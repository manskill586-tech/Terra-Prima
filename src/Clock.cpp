#include "terra/core/Clock.h"

namespace terra::core {

void WorldClock::SetTimeScale(double scale) {
  if (scale < 0.0) {
    scale = 0.0;
  }
  timeScale_.store(scale, std::memory_order_relaxed);
}

void WorldClock::Pause(bool paused) {
  paused_.store(paused, std::memory_order_relaxed);
}

void WorldClock::Step(double stepSeconds) {
  if (stepSeconds <= 0.0) {
    return;
  }
  simTimeSeconds_.store(simTimeSeconds_.load(std::memory_order_relaxed) + stepSeconds,
                        std::memory_order_relaxed);
}

void WorldClock::Tick(double realDeltaSeconds) {
  if (realDeltaSeconds < 0.0) {
    return;
  }
  realTimeSeconds_.store(realTimeSeconds_.load(std::memory_order_relaxed) + realDeltaSeconds,
                         std::memory_order_relaxed);
  if (paused_.load(std::memory_order_relaxed)) {
    return;
  }
  const double scale = timeScale_.load(std::memory_order_relaxed);
  simTimeSeconds_.store(simTimeSeconds_.load(std::memory_order_relaxed) + realDeltaSeconds * scale,
                        std::memory_order_relaxed);
}

double WorldClock::SimTime() const {
  return simTimeSeconds_.load(std::memory_order_relaxed);
}

double WorldClock::RealTime() const {
  return realTimeSeconds_.load(std::memory_order_relaxed);
}

double WorldClock::TimeScale() const {
  return timeScale_.load(std::memory_order_relaxed);
}

bool WorldClock::IsPaused() const {
  return paused_.load(std::memory_order_relaxed);
}

} // namespace terra::core
