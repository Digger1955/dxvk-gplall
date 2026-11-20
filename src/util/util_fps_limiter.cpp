#include <algorithm>
#include <thread>

#include "thread.h"
#include "util_env.h"
#include "util_fps_limiter.h"
#include "util_sleep.h"
#include "util_string.h"
#include "../dxvk/dxvk_latency.h"

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

        if (interval > TimerDuration::zero()) {
          // m_maxCorrection - total correction window - percentage of interval.
          // Set to 12.5% for any target FPS/interval to filter out oscillations.
          m_maxCorrection      = interval / 8;
        } else {
          m_maxCorrection      = TimerDuration::zero();
        }
      }
    }
  }


  void FpsLimiter::updateDeviation(TimePoint target, TimePoint wake) {
    TimerDuration sleepError = std::clamp(std::chrono::duration_cast<TimerDuration>(wake - target), TimerDuration::zero(), m_maxCorrection);

    // EWMA-based sleep overshoot estimate.
    // 0.1/0.9 (alphaNumerator = 10/alphaDenominator = 100) favors smoothness and jitter suppression over adjustment speed.
    // Measures the sleep time itself, rather than frame time, which also includes rendering and presentation delays.

    // Avoids floating-point arithmetic in delay().
    constexpr int64_t alphaNumerator = 10;
    constexpr int64_t alphaDenominator = 100;
    m_deviation = TimerDuration((int64_t(m_deviation.count()) * (alphaDenominator - alphaNumerator) + int64_t(sleepError.count()) * alphaNumerator) / alphaDenominator);

    m_deviation = std::clamp(m_deviation, TimerDuration::zero(), m_maxCorrection);
  }


  void FpsLimiter::delay(const Rc<DxvkLatencyTracker>& tracker) {
    if (tracker != nullptr && tracker->isFramePacingEnabled())
      return;

    std::unique_lock<dxvk::mutex> lock(m_mutex);
    auto interval = m_targetInterval;

    // Guard against negative values
    if (interval <= TimerDuration::zero()) {
      // Reset
      m_nextFrame = TimePoint();
      m_deviation = TimerDuration::zero();
      return;
    }

    auto now = dxvk::high_resolution_clock::now();

    // First frame after init or rate change: present immediately and anchor next deadline.
    if (m_nextFrame == TimePoint()) {
      m_nextFrame = now + interval;
      return;
    }

    // Capture current sleep target safely under the lock
    TimePoint sleepTarget = m_nextFrame;

    if (now >= sleepTarget) {
      TimePoint nextFrame = sleepTarget + interval;

      if (nextFrame <= now) {
        // Stall (>= 1 full frame lost): reset overshoot estimate and
        // re-anchor the deadline grid to prevent a catch-up burst.
        m_deviation = TimerDuration::zero();
        m_nextFrame = now + interval;
      } else {
        // Sub-frame slip (< 1 interval): preserve both the phase grid and the
        // learned OS sleep overshoot estimate so brief spikes don't cause oscillation.
        m_nextFrame = nextFrame;
      }

      return;
    }

    // Advance the nominal deadline independently of sleep correction.
    TimePoint nextFrame = sleepTarget + interval;
    m_nextFrame = nextFrame;

    // Apply the accumulated sleep overshoot estimate to the requested wake time.
    TimePoint correctedSleepTarget = sleepTarget - m_deviation;

    if (correctedSleepTarget <= now) {
      // Already within the wakeup margin of the deadline: present immediately
      // without sleeping, and feed a zero-overshoot sample into the EWMA so
      // m_deviation does not remain frozen if frame times hover near the limit.
      updateDeviation(now, now);
      return;
    }

    lock.unlock();
    Sleep::sleepUntil(now, correctedSleepTarget);
    TimePoint wakeTime = dxvk::high_resolution_clock::now();
    lock.lock();

    // Do not apply a stale sample if another call changed the target or deadline changed while sleeping.
    if (m_targetInterval == interval && m_nextFrame == nextFrame) {
      updateDeviation(correctedSleepTarget, wakeTime);
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
