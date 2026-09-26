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
      TimerDuration interval = frameRate != 0.0
        ? TimerDuration(int64_t(double(TimerDuration::period::den) / frameRate))
        : TimerDuration::zero();

      if (m_targetInterval != interval) {
        m_targetInterval = interval;
        m_maxLatency = maxLatency;
      }
    }
  }


  void FpsLimiter::delay(const Rc<DxvkLatencyTracker>& tracker) {
    FramePacer* framePacer = dynamic_cast<FramePacer*>(tracker.ptr());
    if (framePacer && framePacer->getMode()) {
      return;
    }

    m_isActive.store(false);

    std::unique_lock<dxvk::mutex> lock(m_mutex);
    auto interval = m_targetInterval;
    auto latency = m_maxLatency;

    if (interval == TimerDuration::zero()) {
      m_nextFrame = TimePoint();
      return;
    }

    auto t1 = dxvk::high_resolution_clock::now();

    if (interval < TimerDuration::zero()) {
      interval = -interval;
    }

    // Subsequent code must not access any class members
    // that can be written by setTargetFrameRate
    lock.unlock();

    if (t1 < m_nextFrame) {
      m_isActive.store(true);
      m_lastActive.store(high_resolution_clock::now());
      Sleep::sleepUntil(t1, m_nextFrame);
    }

    m_nextFrame = (t1 < m_nextFrame + interval)
      ? m_nextFrame + interval
      : t1 + interval;
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
