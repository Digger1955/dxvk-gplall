#include <algorithm>

#include "dxvk_device.h"
#include "dxvk_graphics.h"
#include "dxvk_pipecompiler.h"

namespace dxvk {

  DxvkPipelineCompiler::DxvkPipelineCompiler(const DxvkDevice* device) {
      // Determine number of available CPU logical threads, and clamp to a useful
      // range. DXVK is not tested on extremely high core counts, and
      // parallelism may be limited past a certain point.
      uint32_t coreCount = dxvk::thread::hardware_concurrency();

      // Use ((number of CPU logical threads/2) - 2) CPU logical threads.
      // Less stuttering when compiling shaders while playing,
      // in comparison to using all CPU logical threads.
      coreCount = (coreCount/2) - 2;

      // Minimum amount of threads for asynchonous pipeline compilation is 2.
      // Maximum amount of threads for asynchonous pipeline compilation is 8.
      coreCount = std::clamp(coreCount, 2u, 8u);

      uint32_t numWorkers = coreCount;

    Logger::info(str::format("DXVK: Using ", numWorkers, " threads for asynchonous pipeline compilation."));

    // Start the compiler threads
    m_compilerThreads.resize(numWorkers);

    for (uint32_t i = 0; i < numWorkers; i++) {
      m_compilerThreads.at(i) = dxvk::thread(
        [this] { this->runCompilerThread(); });
    }
  }


  DxvkPipelineCompiler::~DxvkPipelineCompiler() {
    this->stopWorkerThreads();
  }

  void DxvkPipelineCompiler::stopWorkerThreads() {
    { std::lock_guard<std::mutex> lock(m_compilerLock);
      m_compilerStop.store(true, std::memory_order_release);
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
      std::lock_guard<std::mutex> lock(m_compilerLock);

      if (m_compilerStop.load(std::memory_order_acquire))
        return;

      m_compilerQueue.emplace(PipelineEntry{ pipeline, state });
    }

    m_compilerCond.notify_one();
  }


  void DxvkPipelineCompiler::runCompilerThread() {
    env::setThreadName("dxvk-pcompiler");

      std::vector<PipelineEntry> tasks;
      tasks.reserve(1024);

    while (true) {
      {
        std::unique_lock<std::mutex> lock(m_compilerLock);

        m_compilerCond.wait(lock, [this] {
          return m_compilerStop.load(std::memory_order_acquire)
              || !m_compilerQueue.empty();
        });

        // Exit cleanly if shutdown was requested and the queue is empty.
        if (m_compilerStop.load(std::memory_order_acquire) && m_compilerQueue.empty())
          break;

        // Drain the queue to minimize lock time and reduce contention.
        while (!m_compilerQueue.empty()) {
          tasks.push_back(std::move(m_compilerQueue.front()));
          m_compilerQueue.pop();
        }
      }

      // Compile outside the lock.
      for (auto& entry : tasks) {
        if (entry.pipeline != nullptr)
          entry.pipeline->compilePipeline(entry.state);
      }
      tasks.clear();
    }
  }

}