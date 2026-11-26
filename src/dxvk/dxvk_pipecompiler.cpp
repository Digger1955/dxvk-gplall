#include <algorithm>

#include "dxvk_device.h"
#include "dxvk_graphics.h"
#include "dxvk_pipecompiler.h"

namespace dxvk {

  DxvkPipelineCompiler::DxvkPipelineCompiler(const DxvkDevice* device) {
      // Determine number of available CPU logical threads, and clamp to a useful
      // range. DXVK is not tested on extremely high core counts, and
      // parallelism may be limited past a certain point.
      // Since application thread is always used by the asynchronous pipeline compilation,
      // it is reasonable to use more than two threads - 
      // by doing that we are lowering load on application thread and potentially reduce stutters.
      uint32_t coreCount = dxvk::thread::hardware_concurrency();

      // Pipeline compilation is not very parallelizable - so keeping it reasonable
      // Two threads are always used by synchronous pipeline compilation, 
      // which can happen in parallel with asynchronous pipeline compilation
      // and two threads are reserved for OS/Application
      // Minimum amount of threads for asynchronous pipeline compilation is 2.
      // Maximum amount of threads for asynchronous pipeline compilation is 16.
      uint32_t numWorkers = coreCount > 4 ? std::clamp(coreCount - 4, 2u, 16u) : 2u;

    // Resize the vector safely first while no child threads are active
    m_compilerThreads.resize(numWorkers);

    // Spawn the threads completely with the normal priority (by not specifying priority)
    for (uint32_t i = 0; i < numWorkers; i++) {
      m_compilerThreads.at(i) = dxvk::thread([this] { this->runCompilerThread(); });
    }
    Logger::info(str::format("DXVK: Using ", numWorkers, " threads for asynchronous pipeline compilation."));
  }


  DxvkPipelineCompiler::~DxvkPipelineCompiler() {
    this->stopWorkerThreads();
  }

  void DxvkPipelineCompiler::stopWorkerThreads() {
    { std::lock_guard<dxvk::mutex> lock(m_compilerLock);
      if (m_compilerStop.exchange(true, std::memory_order_release))
        return;
    }

    m_compilerCond.notify_all();
    for (auto& thread : m_compilerThreads)
      thread.join();

    m_compilerThreads.clear();
  }

  void DxvkPipelineCompiler::queueCompilation(
    DxvkGraphicsPipeline*                   pipeline,
    const DxvkGraphicsPipelineStateInfo&    state) {
    if (pipeline == nullptr)
      return;

    {
      std::lock_guard<dxvk::mutex> lock(m_compilerLock);

      if (m_compilerStop.load(std::memory_order_acquire))
        return;

    m_compilerQueue.push(PipelineEntry{ pipeline, state });
    }

    // Wake up one thread, let him take his part of queue,
    // then, if queue is not empty - wake up another (any) thread.
    m_compilerCond.notify_one();
  }


  void DxvkPipelineCompiler::runCompilerThread() {
    env::setThreadName("dxvk-pcompiler");

    while (true) {
      PipelineEntry entry;

      {
        std::unique_lock<dxvk::mutex> lock(m_compilerLock);

        m_compilerCond.wait(lock, [this] {
          return m_compilerStop.load(std::memory_order_acquire)
              || !m_compilerQueue.empty();
        });

        // Exit cleanly if shutdown was requested and the queue is empty.
        if (m_compilerStop.load(std::memory_order_acquire) && m_compilerQueue.empty())
          break;

        entry = std::move(m_compilerQueue.top());
        m_compilerQueue.pop();
      } // Lock is cleanly released here

      // Compile safely outside the mutex lock
      entry.pipeline->compilePipeline(entry.state);
    }
  }

}