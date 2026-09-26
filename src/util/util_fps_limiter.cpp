#include <thread>

#include "thread.h"
#include "util_env.h"
#include "util_fps_limiter.h"
#include "util_sleep.h"
#include "util_string.h"
#include "../dxvk/framepacer/dxvk_framepacer.h"

#include "./log/log.h"

using namespace std::chrono_literals;

namespace dxvk {

  FpsLimiter::FpsLimiter() {
    auto override = getEnvironmentOverride();

    if (override) {
      setTargetFrameRate(*override);
      m_envOverride = true;
    }
  }


  FpsLimiter::~FpsLimiter() {

  }


  void FpsLimiter::setTargetFrameRate(double frameRate) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (!m_envOverride) {
      TimerDuration interval = frameRate != 0.0
        ? TimerDuration(int64_t(double(TimerDuration::period::den) / frameRate))
        : TimerDuration::zero();

      if (m_targetInterval != interval) {
        m_targetInterval = interval;
      }
    }
  }


  void FpsLimiter::delay(const Rc<DxvkLatencyTracker>& tracker) {
    FramePacer* framePacer = dynamic_cast<FramePacer*>(tracker.ptr());
    if (framePacer && framePacer->getMode()) {
      return;
    }

    std::unique_lock<dxvk::mutex> lock(m_mutex);
    auto interval = m_targetInterval;

    if (interval == TimerDuration::zero()) {
      m_nextFrame = TimePoint(); // Reset back to uninitialized epoch
      return;
    }

    auto t1 = dxvk::high_resolution_clock::now();

    // 1. Check for massive long-term engine stall / drop
    if (t1 > m_nextFrame + (interval * 2)) {
      m_nextFrame = t1;
    }

    // 2. Capture current sleep target safely under the lock
    TimePoint sleepTarget = m_nextFrame;

    // 3. Increment the baseline. If t1 is early, advance by exactly one interval.
    // If t1 is already late, advance the baseline relative to t1 to prevent pipeline stall cascades.
    m_nextFrame = (t1 < sleepTarget + interval)
      ? sleepTarget + interval
      : t1 + interval;

    // 4. Decide whether to sleep based on the calculated timeline
    if (t1 < sleepTarget) {
      // Safe to unlock: m_nextFrame has already been pushed forward for concurrent threads
      lock.unlock();
      Sleep::sleepUntil(t1, sleepTarget);
    }
  }


  std::optional<double> FpsLimiter::getEnvironmentOverride() {
    std::string env = env::getEnvVar("DXVK_FRAME_RATE");

    if (!env.empty()) {
      try {
        return std::stod(env);
      } catch (const std::invalid_argument&) {
        // no op
      }
    }

    return std::nullopt;
  }

}
