#pragma once

#include <atomic>
#include <memory>
#include <stack>
#include <unordered_set>
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
   * and deduplicates in-flight pipeline states
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

    struct PipelineEntry;

    struct PipelineEntryHash {
      size_t operator () (const PipelineEntry* entry) const;
    };

    struct PipelineEntryEq {
      bool operator () (const PipelineEntry* a, const PipelineEntry* b) const;
    };

    std::atomic<bool>                                                              m_compilerStop = { false };
    dxvk::mutex                                                                    m_compilerLock;
    dxvk::condition_variable                                                       m_compilerCond;
    std::stack<std::unique_ptr<PipelineEntry>>                                     m_compilerQueue;
    std::unordered_set<const PipelineEntry*, PipelineEntryHash, PipelineEntryEq>   m_compilerSet;
    std::vector<dxvk::thread>                                                      m_compilerThreads;

    void runCompilerThread();

  };

}
