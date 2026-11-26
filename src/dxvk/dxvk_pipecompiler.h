#pragma once

#include <atomic>
#include <stack>
#include <vector>

#include "../util/thread.h"
#include "dxvk_include.h"

namespace dxvk {

  class DxvkDevice;
  class DxvkGraphicsPipeline;
  class DxvkGraphicsPipelineStateInfo;

  /**
   * \brief Pipeline compiler
   *
   * Asynchronous pipeline compiler
   * Uses a LIFO stack to minimize shader pop-in
   */
  class DxvkPipelineCompiler : public RcObject {

  public:

    DxvkPipelineCompiler(const DxvkDevice* device);
    ~DxvkPipelineCompiler();

    /**
     * \brief Compiles a pipeline asynchronously
     *
     * This should be used to compile graphics
     * pipeline instances asynchronously.
     * \param [in] pipeline The pipeline object
     * \param [in] state The pipeline state info object
     */
    void queueCompilation(
      DxvkGraphicsPipeline*                   pipeline,
      const DxvkGraphicsPipelineStateInfo&    state);

    /**
     * \brief Stops async compiler threads
     */
    void stopWorkerThreads();

  private:

    struct PipelineEntry {
      DxvkGraphicsPipeline*                   pipeline = nullptr;
      DxvkGraphicsPipelineStateInfo           state;
    };

    std::atomic<bool>           m_compilerStop = { false };
    dxvk::mutex                 m_compilerLock;
    dxvk::condition_variable    m_compilerCond;
    std::stack<PipelineEntry>   m_compilerQueue;
    std::vector<dxvk::thread>   m_compilerThreads;

    void runCompilerThread();

  };

}