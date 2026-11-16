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

    // Guard against negative values
    if (interval <= TimerDuration::zero()) {
      m_nextFrame = TimePoint(); // Reset
      return;
    }

    auto t1 = dxvk::high_resolution_clock::now();

    // First frame: initialize cleanly without a burst
    if (m_nextFrame == TimePoint()) {
      TimePoint sleepTarget = t1 + interval;
      m_nextFrame = sleepTarget + interval;
      lock.unlock();
      Sleep::sleepUntil(t1, sleepTarget);
      return;
    }

    // Capture current sleep target safely under the lock
    TimePoint sleepTarget = m_nextFrame;

    if (t1 < sleepTarget) {
      // On time: maintain strict phase grid to prevent clock drift
      m_nextFrame = sleepTarget + interval;
      lock.unlock();
      Sleep::sleepUntil(t1, sleepTarget);
    } else {
      // Late (minor or major stall): present immediately to avoid adding latency,
      // and re-anchor the deadline to t1 so the next frame gets a full interval
      m_nextFrame = t1 + interval;
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
