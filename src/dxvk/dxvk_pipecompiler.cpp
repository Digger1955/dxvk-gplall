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
      // there is no reason to limit the amount of used threads - 
      // by doing that we are lowering load on application thread and potentially reduce stutters.
      uint32_t coreCount = dxvk::thread::hardware_concurrency();

      // Minimum amount of threads for asynchronous pipeline compilation is 2.
      // Maximum amount of threads for asynchronous pipeline compilation is 64.
      coreCount = std::clamp(coreCount, 2u, 64u);

      uint32_t numWorkers = coreCount;

    Logger::info(str::format("DXVK: Using ", numWorkers, " threads for asynchronous pipeline compilation."));

    // Start the compiler threads
    m_compilerThreads.resize(numWorkers);

    for (uint32_t i = 0; i < numWorkers; i++) {
      m_compilerThreads.at(i) = dxvk::thread([this] { this->runCompilerThread(); });
      m_compilerThreads.at(i).set_priority(ThreadPriority::Lowest); // Keeps background work from choking the game
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

    // Wake up one thread, let him take his part of queue,
    // then, if queue is not empty - wake up another (any) thread.
    m_compilerCond.notify_one();
  }


  void DxvkPipelineCompiler::runCompilerThread() {
    env::setThreadName("dxvk-pcompiler");

    std::vector<PipelineEntry> tasks;
    const size_t MAX_BATCH_PER_THREAD = 2;
    tasks.reserve(MAX_BATCH_PER_THREAD);

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

        // --- DYNAMIC BATCH CALCULATION ---
        size_t currentQueueSize = m_compilerQueue.size();

        // Default fallback for small bursts
        // Spreads shader compilation load evenly
        size_t targetBatchSize = 1; 

        if (currentQueueSize > 1) {
          // Distribute work proportionally among available threads
          // IMPORTANT: does not guarantee that each worker gets an equal share. 
          // It only computes a chunk size based on the queue size at the moment a worker wakes and locks.
          targetBatchSize = currentQueueSize / m_compilerThreads.size();
          
          // Clamp to keep lock contention low during massive loading sequences
          // Clamp boundaries. Minimum bulk chunk is 1, Max is 2.
          // It is more efficient to give each thread a small part of queue, 
          // than to load a bigger part of queue, into less amount of threads, 
          // in order to avoid stuttering.
          targetBatchSize = std::clamp(targetBatchSize, size_t(1), MAX_BATCH_PER_THREAD);
        }
        // ----------------------------------

        // Pull the dynamically calculated number of tasks from the queue
        while (!m_compilerQueue.empty() && tasks.size() < targetBatchSize) {
          tasks.push_back(std::move(m_compilerQueue.front()));
          m_compilerQueue.pop();
        }

        // Notify the next thread WHILE HOLDING the lock. 
        // This ensures the OS readies Thread 2 immediately, preventing Thread 1 
        // from looping around and stealing the work again.
        if (!m_compilerQueue.empty()) {
          m_compilerCond.notify_one();
        }
      } // Lock is cleanly released here

      // Compile safely outside the mutex lock
      for (auto& entry : tasks) {
        if (entry.pipeline != nullptr)
          entry.pipeline->compilePipeline(entry.state);
      }
      tasks.clear();
    }
  }

}
