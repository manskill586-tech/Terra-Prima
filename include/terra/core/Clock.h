#pragma once

#include <atomic>
#include <cstdint>

namespace terra::core {

class WorldClock {
public:
  void SetTimeScale(double scale);
  void Pause(bool paused);
  void Step(double stepSeconds);
  void Reset(double simTimeSeconds, double realTimeSeconds = 0.0);

  void Tick(double realDeltaSeconds);

  double SimTime() const;
  double RealTime() const;
  double TimeScale() const;
  bool IsPaused() const;

private:
  std::atomic<double> simTimeSeconds_{0.0};
  std::atomic<double> realTimeSeconds_{0.0};
  std::atomic<double> timeScale_{1.0};
  std::atomic<bool> paused_{false};
};

} // namespace terra::core
