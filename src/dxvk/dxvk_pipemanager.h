#pragma once

#include <mutex>
#include <queue>
#include <unordered_map>

#include "dxvk_compute.h"
#include "dxvk_graphics.h"

namespace dxvk {

  class DxvkDevice;

  /**
   * \brief Pipeline count
   * 
   * Stores number of graphics and
   * compute pipelines, individually.
   */
  struct DxvkPipelineCount {
    uint32_t numGraphicsPipelines;
    uint32_t numComputePipelines;
  };

  /**
   * \brief Pipeline stats
   */
  struct DxvkPipelineStats {
    std::atomic<uint32_t> numGraphicsPipelines  = { 0u };
    std::atomic<uint32_t> numComputePipelines   = { 0u };
  };

  /**
   * \brief Pipeline manager worker threads
   *
   * Spawns worker threads to compile shader pipeline
   * libraries and optimized pipelines asynchronously.
   */
  class DxvkPipelineWorkers {

  public:

    DxvkPipelineWorkers(
            DxvkDevice*                     device);

    ~DxvkPipelineWorkers();

    /**
     * \brief Compiles an optimized compute pipeline
     *
     * \param [in] pipeline Compute pipeline
     * \param [in] state Pipeline state
     */
    void compileComputePipeline(
            DxvkComputePipeline*            pipeline,
      const DxvkComputePipelineStateInfo&   state);

    /**
     * \brief Compiles an optimized graphics pipeline
     *
     * \param [in] pipeline Compute pipeline
     * \param [in] state Pipeline state
     */
    void compileGraphicsPipeline(
            DxvkGraphicsPipeline*           pipeline,
      const DxvkGraphicsPipelineStateInfo&  state);

    /**
     * \brief Checks whether workers are busy
     * \returns \c true if there is unfinished work
     */
    bool isBusy() const;

    /**
     * \brief Stops all worker threads
     *
     * Stops threads and waits for their current work
     * to complete. Queued work will be discarded.
     */
    void stopWorkers();

  private:

    struct PipelineEntry {
      DxvkComputePipeline*          computePipeline;
      DxvkGraphicsPipeline*         graphicsPipeline;
      DxvkComputePipelineStateInfo  computeState;
      DxvkGraphicsPipelineStateInfo graphicsState;
    };

    std::atomic<uint64_t>             m_pendingTasks = { 0ull };

    dxvk::mutex                       m_queueLock;
    dxvk::condition_variable          m_queueCond;

    std::queue<PipelineEntry>         m_queuedPipelines;

    uint32_t                          m_workerCount = 0;
    bool                              m_workersRunning = false;
    std::vector<dxvk::thread>         m_workers;

    void startWorkers();

    void runWorker();

  };

  
  /**
   * \brief Pipeline manager
   * 
   * Creates and stores graphics pipelines and compute
   * pipelines for each combination of shaders that is
   * used within the application. This is necessary
   * because DXVK does not expose the concept of shader
   * pipeline objects to the client API.
   */
  class DxvkPipelineManager {
    friend class DxvkComputePipeline;
    friend class DxvkGraphicsPipeline;
  public:
    
    DxvkPipelineManager(
            DxvkDevice*         device);
    
    ~DxvkPipelineManager();
    
    /**
     * \brief Retrieves a compute pipeline object
     * 
     * If a pipeline for the given shader stage object
     * already exists, it will be returned. Otherwise,
     * a new pipeline will be created.
     * \param [in] shaders Shaders for the pipeline
     * \returns Compute pipeline object
     */
    DxvkComputePipeline* createComputePipeline(
      const DxvkComputePipelineShaders& shaders);
    
    /**
     * \brief Retrieves a graphics pipeline object
     * 
     * If a pipeline for the given shader stage objects
     * already exists, it will be returned. Otherwise,
     * a new pipeline will be created.
     * \param [in] shaders Shaders for the pipeline
     * \returns Graphics pipeline object
     */
    DxvkGraphicsPipeline* createGraphicsPipeline(
      const DxvkGraphicsPipelineShaders& shaders);

    /**
     * \brief Creates a descriptor set layout
     *
     * \param [in] key Descriptor set layout key
     * \returns Descriptor set layout object
     */
    const DxvkDescriptorSetLayout* createDescriptorSetLayout(
      const DxvkDescriptorSetLayoutKey& key);

    /**
     * \brief Creates a pipeline layout
     *
     * \param [in] key Pipeline layout key
     * \returns Descriptor set layout object
     */
    const DxvkPipelineLayout* createPipelineLayout(
      const DxvkPipelineLayoutKey& key);

    /**
     * \brief Retrieves total pipeline count
     * \returns Number of compute/graphics pipelines
     */
    DxvkPipelineCount getPipelineCount() const;

    /**
     * \brief Checks whether async compiler is busy
     * \returns \c true if shaders are being compiled
     */
    bool isCompilingShaders() const {
      return m_workers.isBusy();
    }

    /**
     * \brief Stops async compiler threads
     */
    void stopWorkerThreads();
    
  private:
    
    DxvkDevice*               m_device;
    DxvkPipelineWorkers       m_workers;
    DxvkPipelineStats         m_stats;

    dxvk::mutex m_mutex;

    std::unordered_map<
      DxvkDescriptorSetLayoutKey,
      DxvkDescriptorSetLayout,
      DxvkHash, DxvkEq> m_descriptorSetLayouts;

    std::unordered_map<
      DxvkPipelineLayoutKey,
      DxvkPipelineLayout,
      DxvkHash, DxvkEq> m_pipelineLayouts;

    std::unordered_map<
      DxvkComputePipelineShaders,
      DxvkComputePipeline,
      DxvkHash, DxvkEq> m_computePipelines;

    std::unordered_map<
      DxvkGraphicsPipelineShaders,
      DxvkGraphicsPipeline,
      DxvkHash, DxvkEq> m_graphicsPipelines;

  };
}
