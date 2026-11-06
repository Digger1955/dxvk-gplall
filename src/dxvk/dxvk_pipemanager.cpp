#include <algorithm>
#include <optional>

#include "dxvk_device.h"
#include "dxvk_pipemanager.h"

namespace dxvk {

  DxvkPipelineWorkers::DxvkPipelineWorkers(
          DxvkDevice*                     device) {
    // Determine number of available CPU logical threads, and clamp to a useful
    // range. DXVK is not tested on extremely high core counts, and
    // parallelism may be limited past a certain point.
    // Since application thread is always used by the synchronous pipeline compilation,
    // we can at least try to change a thread on which it happens - 
    // by doing that we are lowering load on application thread and potentially reduce stutters.
    uint32_t coreCount = dxvk::thread::hardware_concurrency();

    // Use up to half of CPU logical threads
    // The only reason not to leave synchronous pipeline compilation single-threaded -
    // Is to win the time which one thread spends compiling one pipeline,
    // and tell another thread to go for another pipeline.
    // Also, there are internal Vulkan driver locks exist which are limiting too.
    coreCount = coreCount/2;

    // Minimum amount of threads for synchronous pipeline compilation is 2.
    // Maximum amount of threads for synchronous pipeline compilation is 16.
    coreCount = std::clamp(coreCount, 2u, 16u);

    m_workerCount = coreCount;

    Logger::info(str::format("DXVK: Using ", m_workerCount, " threads for synchronous pipeline compilation."));
    }


  DxvkPipelineWorkers::~DxvkPipelineWorkers() {
    this->stopWorkers();
  }


  void DxvkPipelineWorkers::compileComputePipeline(
          DxvkComputePipeline*            pipeline,
    const DxvkComputePipelineStateInfo&   state) {
    if (pipeline == nullptr)
      return;

    std::unique_lock lock(m_queueLock);
    this->startWorkers();

    // Constructs the PipelineEntry directly inside the queue container slot.
    m_queuedPipelines.emplace(PipelineEntry{
      pipeline,  // computePipeline
      nullptr,   // graphicsPipeline
      state,     // computeState
      {}         // graphicsState (Default empty constructor)
  });

    m_pendingTasks.fetch_add(1, std::memory_order_relaxed);

    m_queueCond.notify_one();
  }


  void DxvkPipelineWorkers::compileGraphicsPipeline(
          DxvkGraphicsPipeline*           pipeline,
    const DxvkGraphicsPipelineStateInfo&  state) {
    if (pipeline == nullptr)
      return;

    std::unique_lock lock(m_queueLock);
    this->startWorkers();

    // Constructs the PipelineEntry directly inside the queue container slot.
     m_queuedPipelines.emplace(PipelineEntry{
       nullptr,   // computePipeline
       pipeline,  // graphicsPipeline
       {},        // computeState (Default empty constructor)
       state      // graphicsState
  });

    m_pendingTasks.fetch_add(1, std::memory_order_relaxed);

    m_queueCond.notify_one();
  }


  bool DxvkPipelineWorkers::isBusy() const {
    return m_pendingTasks.load(std::memory_order_relaxed) != 0ull;
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

      // Resize the vector safely first while no child threads are active
      m_workers.resize(m_workerCount);

      // Spawn the threads completely
      for (uint32_t i = 0; i < m_workerCount; i++) {
        m_workers[i] = dxvk::thread([this] { runWorker(); });
      }

      // Set priorities only after all handles are registered in the vector
      // Keeps background work from choking the application
      for (uint32_t i = 0; i < m_workerCount; i++) {
        m_workers[i].set_priority(ThreadPriority::Lowest);
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

        // During shutdown, finish queued work before exiting.
        if (!m_workersRunning && m_queuedPipelines.empty())
          break;

        // Direct 1-by-1 retrieval, std::move casts the item to an rvalue reference,
        // moving a few raw pointer addresses around, 
        // which is faster than copying, i.e. without std::move
        p = std::move(m_queuedPipelines.front());
        m_queuedPipelines.pop();

        // Chained wake-up: If there are still items left in the queue, 
        // instantly flag the next thread while we hold the lock.
        if (!m_queuedPipelines.empty()) {
          m_queueCond.notify_one();
        }
      } // Lock is cleanly released here

      // Compile exactly 1 pipeline safely outside the lock
      if (p) {
        if (p->computePipeline)
          p->computePipeline->compilePipeline(p->computeState);
        else if (p->graphicsPipeline)
          p->graphicsPipeline->compilePipeline(p->graphicsState);

        m_pendingTasks.fetch_sub(1, std::memory_order_relaxed);
      }
    }
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
