#include <algorithm>
#include <optional>

#include "dxvk_device.h"
#include "dxvk_pipemanager.h"

namespace dxvk {
  
  DxvkPipelineWorkers::DxvkPipelineWorkers(
          DxvkDevice*                     device) {
    // Use a reasonably large number of threads for compiling, but
    // leave some cores to the application to avoid excessive stutter
    uint32_t numCpuCores = dxvk::thread::hardware_concurrency();
    m_workerCount = ((std::max(1u, numCpuCores) - 1) * 5) / 7;

    if (m_workerCount <  1) m_workerCount =  1;
    if (m_workerCount > 32) m_workerCount = 32;

    if (device->config().numCompilerThreads > 0)
      m_workerCount = device->config().numCompilerThreads;
  }


  DxvkPipelineWorkers::~DxvkPipelineWorkers() {
    this->stopWorkers();
  }


  void DxvkPipelineWorkers::compileComputePipeline(
          DxvkComputePipeline*            pipeline,
    const DxvkComputePipelineStateInfo&   state) {
    std::unique_lock lock(m_queueLock);
    this->startWorkers();

    m_pendingTasks += 1;

    PipelineEntry e = { };
    e.computePipeline = pipeline;
    e.computeState = state;

    m_queuedPipelines.push(e);
    m_queueCond.notify_one();
  }


  void DxvkPipelineWorkers::compileGraphicsPipeline(
          DxvkGraphicsPipeline*           pipeline,
    const DxvkGraphicsPipelineStateInfo&  state) {
    std::unique_lock lock(m_queueLock);
    this->startWorkers();

    m_pendingTasks += 1;

    PipelineEntry e = { };
    e.graphicsPipeline = pipeline;
    e.graphicsState = state;

    m_queuedPipelines.push(e);
    m_queueCond.notify_one();
  }


  bool DxvkPipelineWorkers::isBusy() const {
    return m_pendingTasks.load() != 0ull;
  }


  void DxvkPipelineWorkers::stopWorkers() {
    { std::unique_lock lock(m_queueLock);

      if (!m_workersRunning)
        return;

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

      Logger::info(str::format("DXVK: Using ", m_workerCount, " compiler threads"));
      m_workers.resize(m_workerCount);

      for (auto& worker : m_workers) {
        worker = dxvk::thread([this] { runWorker(); });
        worker.set_priority(ThreadPriority::Lowest);
      }
    }
  }

  void DxvkPipelineWorkers::runWorker() {
    env::setThreadName("dxvk-shader");

    while (true) {
      std::optional<PipelineEntry> p;

      { std::unique_lock lock(m_queueLock);

        m_queueCond.wait(lock, [this] {
          return !m_workersRunning
              || !m_queuedPipelines.empty();
        });

        if (!m_workersRunning) {
          // Skip pending work, exiting early is
          // more important in this case.
          break;
        } else if (!m_queuedPipelines.empty()) {
          p = m_queuedPipelines.front();
          m_queuedPipelines.pop();
        }
      }

      if (p) {
        if (p->computePipeline)
          p->computePipeline->compilePipeline(p->computeState);
        else if (p->graphicsPipeline)
          p->graphicsPipeline->compilePipeline(p->graphicsState);

        m_pendingTasks -= 1;
      }
    }
  }


  DxvkPipelineManager::DxvkPipelineManager(
          DxvkDevice*         device)
  : m_device    (device),
    m_workers   (device) {

  }


  DxvkPipelineManager::~DxvkPipelineManager() {

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
