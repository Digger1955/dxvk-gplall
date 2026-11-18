#include <algorithm>
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
      TimerDuration interval = frameRate > 0.0
        ? TimerDuration(int64_t(double(TimerDuration::period::den) / frameRate))
        : TimerDuration::zero();

      if (m_targetInterval != interval) {
        m_targetInterval = interval;
        m_deviation = TimerDuration::zero();
        m_nextFrame = TimePoint();
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

    // Guard against negative values
    if (interval <= TimerDuration::zero()) {
      m_nextFrame = TimePoint(); // Reset
      m_deviation = TimerDuration::zero();
      return;
    }

    auto now = dxvk::high_resolution_clock::now();

    // FPS-dependent slow frame threshold - improves precision on different FPS
    // m_targetInterval - a frame time to lock to - FPS Limit
    // thresholdPercent - how far a frame time can slip, before resetting deviation
    // if m_targetInterval is Less than 83.3FPS - thresholdPercent=101;
    // if m_targetInterval is More than 83.3FPS - thresholdPercent=104;
    int thresholdPercent = (interval < 12ms) ? 104 : 101;

    // Total correction window - percentage of target interval.
    // Percentage depends on m_targetInterval
    // if m_targetInterval is Less than 83.3FPS - correctionWindow=32 - 3.125%;
    // if m_targetInterval is More than 83.3FPS - correctionWindow=8 - 12.5%;
    int correctionWindow = (interval < 12ms) ? 8 : 32;

    TimerDuration maxCorrection = interval / correctionWindow;

    // Wait until the first deadline without an accumulated correction.
    if (m_nextFrame == TimePoint()) {
      TimePoint sleepTarget = now + interval;
      TimePoint nextFrame = sleepTarget + interval;

      m_nextFrame = nextFrame;

      lock.unlock();
      Sleep::sleepUntil(now, sleepTarget);
      TimePoint wakeTime = dxvk::high_resolution_clock::now();
      lock.lock();

      // Do not apply a stale sample if the target or deadline changed while sleeping.
      if (m_targetInterval == interval && m_nextFrame == nextFrame) {
        TimerDuration sleepError = std::chrono::duration_cast<TimerDuration>(
          wakeTime - sleepTarget);

        sleepError = std::max(sleepError, TimerDuration::zero());

        // EWMA-based sleep overshoot estimate.
        // Measure the sleep itself rather than frame time, which also includes
        // rendering and presentation delays.
        m_deviation = std::chrono::duration_cast<TimerDuration>(
          m_deviation * 0.95 + sleepError * 0.05);

        m_deviation = std::clamp(
          m_deviation, TimerDuration::zero(), maxCorrection);
      }

      return;
    }

    // Capture current sleep target safely under the lock
    TimePoint sleepTarget = m_nextFrame;

    if (now >= sleepTarget) {
      TimerDuration lateness = std::chrono::duration_cast<TimerDuration>(
        now - sleepTarget);

      // Reset the correction after a deadline miss beyond the FPS-dependent
      // threshold. The late frame is still presented immediately.
      if (lateness * 100 > interval * (thresholdPercent - 100))
        m_deviation = TimerDuration::zero();

      // Late: present immediately to avoid adding latency, and re-anchor the
      // deadline so the next frame gets a full interval.
      m_nextFrame = now + interval;
      return;
    }

    // Advance the nominal deadline independently of sleep correction.
    TimePoint nextFrame = sleepTarget + interval;
    m_nextFrame = nextFrame;

    // Apply the accumulated sleep overshoot estimate to the requested wake time.
    TimePoint correctedSleepTarget = sleepTarget - m_deviation;

    if (correctedSleepTarget <= now)
      return;

    lock.unlock();
    Sleep::sleepUntil(now, correctedSleepTarget);
    TimePoint wakeTime = dxvk::high_resolution_clock::now();
    lock.lock();

    // Do not apply a stale sample if another call changed the target or deadline.
    if (m_targetInterval == interval && m_nextFrame == nextFrame) {
      TimerDuration sleepError = std::chrono::duration_cast<TimerDuration>(
        wakeTime - correctedSleepTarget);

      sleepError = std::max(sleepError, TimerDuration::zero());

      // EWMA-based sleep overshoot estimate.
      // 0.05/0.95 favors smoothness and jitter suppression over adjustment speed.
      m_deviation = std::chrono::duration_cast<TimerDuration>(
        m_deviation * 0.95 + sleepError * 0.05);

      m_deviation = std::clamp(
        m_deviation, TimerDuration::zero(), maxCorrection);
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