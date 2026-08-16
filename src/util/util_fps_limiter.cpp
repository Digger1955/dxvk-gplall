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

    TimerDuration targetInterval;
    TimerDuration deviation;
    TimePoint lastFrameSnapshot;
    bool enabled;

    {
      std::lock_guard<dxvk::mutex> lock(m_mutex);
      enabled = isEnabled();
      if (!enabled)
        return;

      targetInterval = m_targetInterval;
      deviation = m_deviation;
      lastFrameSnapshot = m_lastFrame;
    }

    auto t0 = lastFrameSnapshot;
    auto t1 = dxvk::high_resolution_clock::now();
    auto frameTime = std::chrono::duration_cast<TimerDuration>(t1 - t0);

    if (frameTime * 100 > targetInterval * 103 - deviation * 100) {
      std::lock_guard<dxvk::mutex> lock(m_mutex);
      m_deviation = TimerDuration::zero();
      m_lastFrame = t1;
      return;
    }

    TimerDuration sleepDuration = targetInterval - deviation - frameTime;

    t1 = Sleep::sleepFor(t1, sleepDuration);

    frameTime = std::chrono::duration_cast<TimerDuration>(t1 - t0);

    {
      std::lock_guard<dxvk::mutex> lock(m_mutex);

      m_deviation += frameTime - targetInterval;

      m_deviation = std::min(m_deviation, m_targetInterval / 16);

      m_lastFrame = t1;
    }
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
