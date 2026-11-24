#include <algorithm>
#include <cmath>
#include <limits>
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
      TimerDuration interval = TimerDuration::zero();

      if (std::isfinite(frameRate) && frameRate > 0.0) {
        double intervalCount = double(TimerDuration::period::den) / frameRate;

        if (intervalCount >= 1.0
         && intervalCount < double(std::numeric_limits<int64_t>::max()))
          interval = TimerDuration(int64_t(intervalCount));
      }

      if (m_targetInterval != interval) {
        m_targetInterval = interval;
        m_deviation = TimerDuration::zero();
        m_lastFrame = TimePoint();

        // m_maxCorrection - total correction window - percentage of interval.
        // Set to 12.5% (interval / 8) for any target FPS/interval to filter out oscillations.
        m_maxCorrection = interval > TimerDuration::zero()
          ? interval / 8
          : TimerDuration::zero();
      }
    }
  }


  void FpsLimiter::updateDeviation(TimerDuration rawError) {
    // Limit each sample so a single long scheduling stall cannot dominate the EWMA.
    rawError = std::clamp(rawError, -m_maxCorrection, m_maxCorrection);

    // EWMA-based relative frame-time correction. Measures underlying sleep/scheduling bias.
    // 0.1/0.9 (alphaNumerator = 10/alphaDenominator = 100) favors smoothness and jitter suppression over adjustment speed.

    // Avoids floating-point arithmetic in delay().
    constexpr int64_t alphaNumerator = 10;
    constexpr int64_t alphaDenominator = 100;
    m_deviation = TimerDuration((int64_t(m_deviation.count()) * (alphaDenominator - alphaNumerator) + int64_t(rawError.count()) * alphaNumerator) / alphaDenominator);

    m_deviation = std::clamp(m_deviation, -m_maxCorrection, m_maxCorrection);
  }


  void FpsLimiter::delay(const Rc<DxvkLatencyTracker>& tracker) {
    if (tracker != nullptr && tracker->isFramePacingEnabled())
      return;

    std::unique_lock<dxvk::mutex> lock(m_mutex);
    auto interval = m_targetInterval;

    // Guard against negative values
    if (interval <= TimerDuration::zero()) {
      // Reset
      m_lastFrame = TimePoint();
      m_deviation = TimerDuration::zero();
      return;
    }

    auto now = dxvk::high_resolution_clock::now();

    // First frame after init: present immediately and establish
    // the baseline for relative frame-time calculation.
    if (m_lastFrame == TimePoint()) {
      m_lastFrame = now;
      return;
    }

    TimePoint frameStart = m_lastFrame;
    TimerDuration frameTime = std::chrono::duration_cast<TimerDuration>(now - frameStart);

    // Slow frame - a frame that is 12.5% (interval / 8) over interval.
    // Reset the correction after a slow frame. It has already exceeded the
    // target by more than 12.5%, so do not add a sleep or carry stale state forward.
    if (frameTime > interval + interval / 8) {
      m_deviation = TimerDuration::zero();
      m_lastFrame = now;
      return;
    }

    // Capture current state before unlocking to sleep.
    TimerDuration appliedDeviation = m_deviation;
    // Subtract the relative frame time and learned correction from the target.
    TimerDuration sleepDuration = interval - appliedDeviation - frameTime;

    if (sleepDuration <= TimerDuration::zero()) {
      // Work took longer than (interval - appliedDeviation), so no sleep is needed.
      // Leave m_deviation unchanged since no sleep operation occurred to measure.
      m_lastFrame = now;
      return;
    }

    // Unlock, sleepFor from TimePoint now for TimerDuration sleepDuration,
    // write resulted TimePoint into wakeTime, lock.
    lock.unlock();
    TimePoint wakeTime = Sleep::sleepFor(now, sleepDuration);
    lock.lock();

    // Do not apply a stale sample if another call changed the target or deadline changed while sleeping.
    if (m_targetInterval == interval && m_lastFrame == frameStart) {
      TimerDuration actualFrameTime = std::chrono::duration_cast<TimerDuration>(wakeTime - frameStart);

      // Reconstruct the uncompensated sleep error:
      // actualFrameTime = frameTime + sleepDuration + sleepBias
      // Since sleepDuration = interval - appliedDeviation - frameTime:
      // actualFrameTime - interval + appliedDeviation = sleepBias
      TimerDuration rawError = actualFrameTime - interval + appliedDeviation;

      updateDeviation(rawError);
      m_lastFrame = wakeTime;
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