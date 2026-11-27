#include <algorithm>
#include <cstring>

#include "dxvk_device.h"
#include "dxvk_graphics.h"
#include "dxvk_pipecompiler.h"

namespace dxvk {

  struct DxvkPipelineCompiler::PipelineEntry {
    DxvkGraphicsPipeline*         pipeline = nullptr;
    DxvkGraphicsPipelineStateInfo state;
  };


  size_t DxvkPipelineCompiler::PipelineEntryHash::operator () (const PipelineEntry* entry) const {
    DxvkHashState hash;
    hash.add(std::hash<DxvkGraphicsPipeline*>()(entry->pipeline));
    hash.add(entry->state.hash());
    return hash;
  }


  bool DxvkPipelineCompiler::PipelineEntryEq::operator () (const PipelineEntry* a, const PipelineEntry* b) const {
    return a->pipeline == b->pipeline
        && std::memcmp(&a->state, &b->state, sizeof(DxvkGraphicsPipelineStateInfo)) == 0;
  }


  DxvkPipelineCompiler::DxvkPipelineCompiler(const DxvkDevice* device) {
      // Determine number of available CPU logical threads, and clamp to a useful
      // range. DXVK is not tested on extremely high core counts, and
      // parallelism may be limited past a certain point.
      // Since application thread is always used by the asynchronous pipeline compilation,
      // it is reasonable to use more than two threads - 
      // by doing that we are lowering load on application thread and potentially reduce stutters.
      uint32_t coreCount = dxvk::thread::hardware_concurrency();

      // Pipeline compilation paralellism is limited by many things,
      // so the best strategy is to prevent starvation of OS/Application.
      // Two threads are always used by synchronous pipeline compilation, 
      // which can happen in parallel with asynchronous pipeline compilation
      // and four threads are reserved for OS/Application.
      // Minimum amount of threads for asynchronous pipeline compilation is 2.
      // Maximum amount of threads for asynchronous pipeline compilation is 8.
      uint32_t numWorkers = coreCount > 8 ? std::clamp(coreCount - 6, 2u, 8u) : 2u;

    m_compilerThreads.resize(numWorkers);

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

    PipelineEntry key = { pipeline, state };

    { std::lock_guard<dxvk::mutex> lock(m_compilerLock);

      if (m_compilerStop.load(std::memory_order_acquire))
        return;

      // Hashes & looks up once: inserts &key temporarily on a miss
      auto [iter, inserted] = m_compilerSet.emplace(&key);
      if (!inserted)
        return;

      // Allocate on the heap only when we know it's a new entry,
      // then swap the stack pointer for the heap pointer in-place.
      auto entry = std::make_unique<PipelineEntry>(std::move(key));
      const_cast<const PipelineEntry*&>(*iter) = entry.get();

      m_compilerQueue.push(std::move(entry));
    }

    m_compilerCond.notify_one();
  }


  void DxvkPipelineCompiler::runCompilerThread() {
    env::setThreadName("dxvk-pcompiler");

    while (true) {
      std::unique_ptr<PipelineEntry> entry;

      { std::unique_lock<dxvk::mutex> lock(m_compilerLock);

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
      entry->pipeline->compilePipeline(entry->state);

      // Remove from active set after compilation finishes so duplicate
      // requests arriving mid-compilation are also filtered out
      { std::lock_guard<dxvk::mutex> lock(m_compilerLock);
        m_compilerSet.erase(entry.get());
      }
    }
  }

}
