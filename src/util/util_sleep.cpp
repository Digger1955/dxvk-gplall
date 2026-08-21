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

  Sleep Sleep::s_instance;


  Sleep::Sleep() {

  }


  Sleep::~Sleep() {

  }

  void Sleep::initialize() {
    std::lock_guard lock(m_mutex);

    if (m_initialized.load())
      return;

    // NtSetTimerResolution to 2ms by default
    initializePlatformSpecifics();

    m_initialized.store(true, std::memory_order_release);
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
        if (NtSetTimerResolution && !NtSetTimerResolution(20000, TRUE, &cur)) {
          Logger::info(str::format("NtSetTimerResolution: Setting timer interval to 2000 us"));
        }
      }
    }
#endif
  }


  Sleep::TimePoint Sleep::sleep(TimePoint t0, TimerDuration duration) {
    if (duration <= TimerDuration::zero())
      return t0;

    // if necessary, initialize function pointers and some values
    if (!m_initialized.load(std::memory_order_acquire)) 
        initialize();

    // Compile-time constant sleepGranularity = 2 ms
    // Optimal precision and energy efficiency for most systems.
    constexpr TimerDuration sleepGranularity = TimerDuration(2ms);
    const TimePoint targetTime = t0 + duration;

    TimePoint t1 = t0;
    TimerDuration remaining = duration;

    // Use sleepGranularity as a sleepThreshold
    while (remaining > sleepGranularity) {
      TimerDuration sleepDuration = remaining - sleepGranularity;

      // For high precision, try long sleep, only if sleepDuration is
      // longer than sleepGranularity, which equals to 2 ms
      if (sleepDuration > 2ms)
        systemSleep(sleepDuration);

      t1 = dxvk::high_resolution_clock::now();
      remaining = std::chrono::duration_cast<TimerDuration>(targetTime - t1);
      t0 = t1;
    }

    // Counter for intervals between wake up checks
    uint16_t loopCounter = 0;

    // Busy-wait until we have slept long enough
    while (remaining > TimerDuration::zero()) {
      // CPU arch-specific pause macros 
      // to save energy during busy-waiting
      CPU_PAUSE();

      // Intervals between wake up checks, i.e.
      // Amount of times to do CPU_PAUSE();
      // Before checking if we need to wake up.
      if (++loopCounter >= 1000) {
        t1 = dxvk::high_resolution_clock::now();
        remaining = std::chrono::duration_cast<TimerDuration>(targetTime - t1);
        loopCounter = 0;
      }
    }

    return dxvk::high_resolution_clock::now();
}

  void Sleep::systemSleep(TimerDuration duration) {
#ifdef _WIN32
    if (NtDelayExecution) {
      LARGE_INTEGER ticks;
      ticks.QuadPart = -duration.count();

      NtDelayExecution(FALSE, &ticks);
    } else {
      std::this_thread::sleep_for(duration);
    }
#else
    std::this_thread::sleep_for(duration);
#endif
  }

}
