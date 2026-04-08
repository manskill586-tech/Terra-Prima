#pragma once

#include <chrono>
#include <functional>
#include <string_view>

namespace terra::core {

class ScopedTimer {
public:
  using Callback = std::function<void(std::string_view, double)>;

  ScopedTimer(std::string_view label, Callback callback)
      : label_(label), callback_(std::move(callback)), start_(Clock::now()) {}

  ~ScopedTimer() {
    const auto end = Clock::now();
    const std::chrono::duration<double, std::milli> elapsed = end - start_;
    if (callback_) {
      callback_(label_, elapsed.count());
    }
  }

private:
  using Clock = std::chrono::high_resolution_clock;

  std::string_view label_;
  Callback callback_;
  Clock::time_point start_;
};

} // namespace terra::core
