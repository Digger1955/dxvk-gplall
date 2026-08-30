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
      setTargetFrameRate(*override, 0);
      m_envOverride = true;
    }
  }


  FpsLimiter::~FpsLimiter() {

  }


  void FpsLimiter::setTargetFrameRate(double frameRate, uint32_t maxLatency) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (!m_envOverride) {
      m_targetInterval = frameRate != 0.0
        ? TimerDuration(int64_t(double(TimerDuration::period::den) / frameRate))
        : TimerDuration::zero();

      if (isEnabled() && !m_initialized)
        initialize();

        m_maxLatency = maxLatency;
    }
  }


  void FpsLimiter::delay(const Rc<DxvkLatencyTracker>& tracker) {
    FramePacer* framePacer = dynamic_cast<FramePacer*>(tracker.ptr());
    if (framePacer && framePacer->getMode()) {
      return;
    }

    m_isActive.store(false);

    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (!isEnabled())
      return;

    auto t0 = m_lastFrame;
    auto t1 = dxvk::high_resolution_clock::now();

    auto frameTime = std::chrono::duration_cast<TimerDuration>(t1 - t0);

    // FPS-dependent slow frame threshold
    // >= 240FPS - 103; < 240FPS - 101
    int thresholdPercent = (m_targetInterval < 4ms) ? 103 : 101;

    if (frameTime * 100 > m_targetInterval * thresholdPercent - m_deviation * 100) {
      // If we have a slow frame, reset the deviation since we
      // do not want to compensate for low performance later on
      m_deviation = TimerDuration::zero();
    } else {
      // Don't call sleep if the amount of time to sleep is shorter
      // than the time the function calls are likely going to take
      TimerDuration sleepDuration = m_targetInterval - m_deviation - frameTime;
      t1 = Sleep::sleepFor(t1, sleepDuration);

      // Recalculate interval to figure out exact delivery error
      frameTime = std::chrono::duration_cast<TimerDuration>(t1 - t0);
      TimerDuration currentError = frameTime - m_targetInterval;

      // EWMA-based deviation calculation
      m_deviation = std::chrono::duration_cast<TimerDuration>((m_deviation * 0.95) + (currentError * 0.05));
      
      // Total correction window - 10% of target interval.
      TimerDuration maxCap = m_targetInterval / 10;
      m_deviation = std::max(-maxCap, std::min(m_deviation, maxCap));
    }

    m_lastFrame = t1;
  }


  void FpsLimiter::initialize() {
    m_lastFrame = dxvk::high_resolution_clock::now();
    m_initialized = true;
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
