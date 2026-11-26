#include <algorithm>

#include "dxvk_device.h"
#include "dxvk_pipemanager.h"

namespace dxvk {

  DxvkPipelineWorkers::DxvkPipelineWorkers(
          DxvkDevice*                     device) {
    // Since application thread is always used by the synchronous pipeline compilation,
    // we can at least try to change a thread on which it happens - 
    // by doing that we are lowering load on application thread and potentially reduce stutters.

    // Use 2 CPU logical threads.
    // The only reason not to leave synchronous pipeline compilation single-threaded -
    // is to win the time which one thread spends compiling one pipeline,
    // and tell another thread to go for another pipeline.
    // Also, there are internal Vulkan driver locks exist which are limiting too.
    m_workerCount = 2;

    Logger::info(str::format("DXVK: Using ", m_workerCount, " threads for synchronous pipeline compilation."));
    }


  DxvkPipelineWorkers::~DxvkPipelineWorkers() {
    this->stopWorkers();
  }


  void DxvkPipelineWorkers::compileGraphicsPipeline(
          DxvkGraphicsPipeline*           pipeline,
    const DxvkGraphicsPipelineStateInfo&  state) {
    if (pipeline == nullptr)
      return;

    std::unique_lock lock(m_queueLock);
    this->startWorkers();

    m_queuedPipelines.push(PipelineEntry{
      pipeline,
      state
    });

    m_pendingTasks++;
    m_queueCond.notify_one();
  }


  bool DxvkPipelineWorkers::isBusy() const {
    std::unique_lock lock(m_queueLock);
    return m_pendingTasks != 0ull;
  }

  void DxvkPipelineWorkers::waitIdle() {
    std::unique_lock lock(m_queueLock);

    while (m_pendingTasks > 0ull) {
      if (!m_queuedPipelines.empty()) {
        // Assist workers: steal a task from the queue and compile it locally
        PipelineEntry p = std::move(m_queuedPipelines.front());
        m_queuedPipelines.pop();

        lock.unlock();
        compileEntry(p);
        lock.lock();

        if (--m_pendingTasks == 0ull)
          m_idleCond.notify_all();
      } else {
        // Queue is empty, but workers are still compiling the last few tasks.
        // Sleep without spinning until they finish.
        m_idleCond.wait(lock, [this] {
          return m_pendingTasks == 0ull || !m_queuedPipelines.empty();
        });
      }
    }
  }


  void DxvkPipelineWorkers::stopWorkers() {
    { std::unique_lock lock(m_queueLock);

      if (!m_workersRunning)
        return;

      // Workers will finish queued tasks before exiting.
      m_workersRunning = false;
      m_queueCond.notify_all();
    }

    for (auto& worker : m_workers)
      worker.join();

    m_workers.clear();
  }


  void DxvkPipelineWorkers::startWorkers() {
    if (!m_workersRunning) {
      m_workersRunning = true;
      m_workers.reserve(m_workerCount);

      for (uint32_t i = 0; i < m_workerCount; i++) {
        m_workers.emplace_back([this] { runWorker(); });
      }
    }
  }

  void DxvkPipelineWorkers::runWorker() {
    env::setThreadName("dxvk-shader");

    while (true) {
      PipelineEntry p;

      { std::unique_lock lock(m_queueLock);

        m_queueCond.wait(lock, [this] {
          return !m_workersRunning
              || !m_queuedPipelines.empty();
        });

        if (!m_workersRunning && m_queuedPipelines.empty())
          break;

        p = std::move(m_queuedPipelines.front());
        m_queuedPipelines.pop();
      }

      compileEntry(p);

      { std::unique_lock lock(m_queueLock);
        if (--m_pendingTasks == 0ull)
          m_idleCond.notify_all();
      }
    }
  }

  void DxvkPipelineWorkers::compileEntry(const PipelineEntry& entry) {
    if (entry.graphicsPipeline)
      entry.graphicsPipeline->compilePipeline(entry.graphicsState);
  }

  DxvkPipelineManager::DxvkPipelineManager(
          DxvkDevice*         device)
  : m_device    (device),
    m_workers   (device) {
    if (device->config().enableAsync)
      m_compiler = new DxvkPipelineCompiler(device);

  }


  DxvkPipelineManager::~DxvkPipelineManager() {
    this->stopWorkerThreads();

  }


  DxvkComputePipeline* DxvkPipelineManager::createComputePipeline(
    const DxvkComputePipelineShaders& shaders) {
    if (shaders.cs == nullptr)
      return nullptr;
    
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    
    auto pair = m_computePipelines.find(shaders);
    if (pair != m_computePipelines.end())
      return &pair->second;

    auto iter = m_computePipelines.emplace(
      std::piecewise_construct,
      std::tuple(shaders),
      std::tuple(m_device, this, shaders));
    return &iter.first->second;
  }


  DxvkGraphicsPipeline* DxvkPipelineManager::createGraphicsPipeline(
    const DxvkGraphicsPipelineShaders& shaders) {
    if (shaders.vs == nullptr)
      return nullptr;
    
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    
    auto pair = m_graphicsPipelines.find(shaders);
    if (pair != m_graphicsPipelines.end())
      return &pair->second;

    auto iter = m_graphicsPipelines.emplace(
      std::piecewise_construct,
      std::tuple(shaders),
      std::tuple(m_device, this, shaders));
    return &iter.first->second;
  }


  DxvkPipelineCount DxvkPipelineManager::getPipelineCount() const {
    DxvkPipelineCount result;
    result.numGraphicsPipelines = m_stats.numGraphicsPipelines.load();
    result.numComputePipelines  = m_stats.numComputePipelines.load();
    return result;
  }


  void DxvkPipelineManager::stopWorkerThreads() {
    if (m_compiler != nullptr)
      m_compiler->stopWorkerThreads();
    m_workers.stopWorkers();
  }


  const DxvkDescriptorSetLayout* DxvkPipelineManager::createDescriptorSetLayout(
    const DxvkDescriptorSetLayoutKey& key) {
    auto pair = m_descriptorSetLayouts.find(key);
    if (pair != m_descriptorSetLayouts.end())
      return &pair->second;

    auto iter = m_descriptorSetLayouts.emplace(
      std::piecewise_construct,
      std::tuple(key),
      std::tuple(m_device, key));
    return &iter.first->second;
  }


  const DxvkPipelineLayout* DxvkPipelineManager::createPipelineLayout(
    const DxvkPipelineLayoutKey& key) {
    auto pair = m_pipelineLayouts.find(key);
    if (pair != m_pipelineLayouts.end())
      return &pair->second;

    auto iter = m_pipelineLayouts.emplace(
      std::piecewise_construct,
      std::tuple(key),
      std::tuple(m_device, key));
    return &iter.first->second;
  }


}
