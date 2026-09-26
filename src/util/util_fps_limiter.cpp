#include <thread>
#include <cmath>
#include <algorithm>

#include "thread.h"
#include "util_env.h"
#include "util_fps_limiter.h"
#include "util_sleep.h"
#include "util_string.h"
#include "../dxvk/framepacer/dxvk_framepacer.h"

#include "./log/log.h"

using namespace std::chrono_literals;

namespace dxvk {

  FpsLimiter::FpsLimiter() 
    : m_deviationNs_History(0.0) {
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

    double targetMs = std::chrono::duration<double, std::milli>(m_targetInterval).count();
    double thresholdScalar = 1.0 + std::clamp(1.0 / targetMs, 0.01, 0.08); 

    if (double(frameTime.count()) > double(m_targetInterval.count()) * thresholdScalar - double(m_deviation.count())) {
      m_deviation = TimerDuration::zero();
      m_deviationNs_History = 0.0;
    } else {
      TimerDuration sleepDuration = m_targetInterval - m_deviation - frameTime;
      t1 = Sleep::sleepFor(t1, sleepDuration);

      frameTime = std::chrono::duration_cast<TimerDuration>(t1 - t0);
      TimerDuration currentError = frameTime - m_targetInterval;

      double frameTimeSec = std::chrono::duration<double>(frameTime).count();
      double currentErrorNs = std::chrono::duration<double, std::nano>(currentError).count();
      double targetSec = std::chrono::duration<double>(m_targetInterval).count();

      double maxClampBound = std::clamp(targetSec * 3.0, 0.033, 0.150); 
      double clampedFrameTimeSec = std::clamp(frameTimeSec, 0.001, maxClampBound);

      const double targetWindowSec = 0.30;

      double alpha = 1.0 - std::exp(-clampedFrameTimeSec / targetWindowSec);
      double beta = 1.0 - alpha;

      m_deviationNs_History = (m_deviationNs_History * beta) + (currentErrorNs * alpha);

      m_deviation = std::chrono::duration_cast<TimerDuration>(std::chrono::duration<double, std::nano>(m_deviationNs_History));

      double dynamicCorrection = std::clamp(targetMs * 2.0, 4.0, 32.0); 
      TimerDuration maxCap = m_targetInterval / int(dynamicCorrection);
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
