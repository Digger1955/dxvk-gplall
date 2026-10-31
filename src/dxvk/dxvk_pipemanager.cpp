#include <algorithm>
#include <optional>
#include <atomic>
#include <thread>

#include "dxvk_device.h"
#include "dxvk_pipemanager.h"

namespace dxvk {
  
  DxvkPipelineWorkers::DxvkPipelineWorkers(
          DxvkDevice*                     device) {
    (void)device;

    // One background worker is enough for the single-slot design.
    m_workerCount = 1;
  }


  DxvkPipelineWorkers::~DxvkPipelineWorkers() {
    this->stopWorkers();
  }


  void DxvkPipelineWorkers::compileComputePipeline(
          DxvkComputePipeline*            pipeline,
    const DxvkComputePipelineStateInfo&   state) {
    if (pipeline == nullptr)
      return;

    auto* slot = std::launder(reinterpret_cast<PipelineEntry*>(&m_taskStorage));

    // Reserve the single slot atomically.
    // If the slot is already occupied, fall back to synchronous compile.
    if (m_hasTask.exchange(true, std::memory_order_acq_rel)) {
      pipeline->compilePipeline(state);
      return;
    }

    // Write task into the single slot.
    slot->computePipeline = pipeline;
    slot->graphicsPipeline = nullptr;
    slot->computeState = state;

    m_pendingTasks.fetch_add(1u, std::memory_order_release);

    if (!m_workersRunning.load(std::memory_order_acquire))
      startWorkers();
  }


  void DxvkPipelineWorkers::compileGraphicsPipeline(
          DxvkGraphicsPipeline*           pipeline,
    const DxvkGraphicsPipelineStateInfo&  state) {
    if (pipeline == nullptr)
      return;

    auto* slot = std::launder(reinterpret_cast<PipelineEntry*>(&m_taskStorage));

    // Reserve the single slot atomically.
    // If the slot is already occupied, fall back to synchronous compile.
    if (m_hasTask.exchange(true, std::memory_order_acq_rel)) {
      pipeline->compilePipeline(state);
      return;
    }

    // Write task into the single slot.
    slot->computePipeline = nullptr;
    slot->graphicsPipeline = pipeline;
    slot->graphicsState = state;

    m_pendingTasks.fetch_add(1u, std::memory_order_release);

    if (!m_workersRunning.load(std::memory_order_acquire))
      startWorkers();
  }


  bool DxvkPipelineWorkers::isBusy() const {
    return m_pendingTasks.load(std::memory_order_acquire) != 0ull;
  }


  void DxvkPipelineWorkers::stopWorkers() {
    m_workersRunning.store(false, std::memory_order_release);

    if (m_worker.joinable())
      m_worker.join();
  }


  void DxvkPipelineWorkers::startWorkers() {
    if (m_workersRunning.load(std::memory_order_acquire))
      return;

    m_workersRunning.store(true, std::memory_order_release);

    m_worker = dxvk::thread([this] { this->runWorker(); });
  }

  void DxvkPipelineWorkers::runWorker() {
    env::setThreadName("dxvk-shader");

    while (m_workersRunning.load(std::memory_order_acquire)
        || m_hasTask.load(std::memory_order_acquire)) {
      if (!m_hasTask.load(std::memory_order_acquire)) {
        std::this_thread::yield();
        continue;
      }

      auto* slot = std::launder(reinterpret_cast<PipelineEntry*>(&m_taskStorage));

      if (slot->computePipeline) {
        slot->computePipeline->compilePipeline(slot->computeState);
        slot->computePipeline = nullptr;
      } else if (slot->graphicsPipeline) {
        slot->graphicsPipeline->compilePipeline(slot->graphicsState);
        slot->graphicsPipeline = nullptr;
      }

      // Clear the slot only after the task is finished.
      m_hasTask.store(false, std::memory_order_release);
      m_pendingTasks.fetch_sub(1u, std::memory_order_acq_rel);
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
