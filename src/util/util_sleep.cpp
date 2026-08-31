#include "util_sleep.h"
#include "util_string.h"

#include "./log/log.h"

#include <thread>

// x86-specific pause macros to save energy during busy-waiting
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <emmintrin.h>
#define CPU_PAUSE() _mm_pause()
// ARM-specific pause macros to save energy during busy-waiting
#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
#define CPU_PAUSE() __asm__ volatile("isb" ::: "memory")
#else
// Nothing on other CPU architectures
#define CPU_PAUSE() do {} while(0)
#endif

using namespace std::chrono_literals;

namespace dxvk {

  std::once_flag Sleep::s_initFlag;
  Sleep Sleep::s_instance;


  Sleep::Sleep() {

  }


  Sleep::~Sleep() {

  }

  void Sleep::initialize() {
    // Thread-safe platform initialization guaranteed by the runtime library
    std::call_once(s_initFlag, [this]() {
        this->initializePlatformSpecifics(); // NtSetTimerResolution to 1ms by default
    });
}


  void Sleep::initializePlatformSpecifics() {
#ifdef _WIN32
    HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");

    if (ntdll) {
      NtDelayExecution = reinterpret_cast<NtDelayExecutionProc>(
        ::GetProcAddress(ntdll, "NtDelayExecution"));
      auto NtQueryTimerResolution = reinterpret_cast<NtQueryTimerResolutionProc>(
        ::GetProcAddress(ntdll, "NtQueryTimerResolution"));
      auto NtSetTimerResolution = reinterpret_cast<NtSetTimerResolutionProc>(
        ::GetProcAddress(ntdll, "NtSetTimerResolution"));

      ULONG min, max, cur;

      // Wine's implementation of these functions is a stub as of 6.10, which is fine
      // since it uses select() in NtDelayExecution. This is only relevant for Windows.
      if (NtQueryTimerResolution && !NtQueryTimerResolution(&min, &max, &cur)) {
        if (NtSetTimerResolution && !NtSetTimerResolution(10000, TRUE, &cur)) {
          Logger::info(str::format("NtSetTimerResolution: Setting timer interval to 1000 us (1 ms, 1000 Hz)"));
        }
      }
    }
#endif
  }


  Sleep::TimePoint Sleep::sleep(TimePoint t0, TimerDuration duration) {
    if (duration <= TimerDuration::zero())
      return t0;

    initialize();

    // Compile-time constant sleepGranularity = 1 ms
    // Optimal precision and energy efficiency for most systems.
    constexpr TimerDuration sleepGranularity = TimerDuration(1ms);

    // Compile-time constant sleepThreshold = 2 ms
    // Optimal precision and energy efficiency for most systems.
    constexpr TimerDuration sleepThreshold = TimerDuration(2ms);

    TimerDuration remaining = duration;
    TimePoint t1 = t0;

    // Use 2 * sleepGranularity as a sleepThreshold
    while (remaining > sleepThreshold) {
      TimerDuration sleepDuration = remaining - sleepGranularity;

      systemSleep(sleepDuration);

      t1 = dxvk::high_resolution_clock::now();
      remaining -= std::chrono::duration_cast<TimerDuration>(t1 - t0);
      t0 = t1;
    }

    // Counter for intervals between wake up checks
    uint16_t loopCounter = 0;

    // Busy-wait until we have slept long enough
    while (remaining > TimerDuration::zero()) {
      // CPU arch-specific pause macros, which
      // saves energy during busy-waiting.
      // Windows Task Manager will show CPU Load, 
      // but CPU is actually doing less/nothing.
      CPU_PAUSE();

      // Intervals between wake up checks, i.e.
      // Amount of times to do CPU_PAUSE();
      // Before checking if we need to wake up.
      if (++loopCounter >= 1000) {
        t1 = dxvk::high_resolution_clock::now();
        remaining -= std::chrono::duration_cast<TimerDuration>(t1 - t0);
        t0 = t1;
        loopCounter = 0;
      }
    }

    return t1;
}

  void Sleep::systemSleep(TimerDuration duration) {
#ifdef _WIN32
    // Protection from thread loading anomalies
    auto proc = NtDelayExecution.load(std::memory_order_acquire);
    if (proc) {
      LARGE_INTEGER ticks;
      ticks.QuadPart = -duration.count();

      proc(FALSE, &ticks);
    } else {
      std::this_thread::sleep_for(duration);
    }
#else
    std::this_thread::sleep_for(duration);
#endif
  }

}
