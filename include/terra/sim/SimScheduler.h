#pragma once

#include <cstdint>
#include <vector>

namespace terra::sim {

enum class TierId : uint8_t {
  Near = 0,
  Mid = 1,
  Far = 2
};

struct TierConfig {
  TierId id{};
  double intervalSec{1.0};
  double minIntervalSec{0.1};
  double maxIntervalSec{30.0};
  double lastCostMs{0.5};
  double nextTimeSec{0.0};
  bool enabled{true};
};

struct PlanResult {
  std::vector<TierId> tiers;
  double estimatedCostMs{0.0};
  bool overBudget{false};
};

class SimScheduler {
public:
  SimScheduler();

  void SetConfig(TierId id, const TierConfig& config);
  const TierConfig& GetConfig(TierId id) const;

  PlanResult Plan(double simTimeSec, double frameBudgetMs);
  void OnTierUpdated(TierId id, double simTimeSec, double costMs);
  void UpdateIntervals(double frameBudgetMs);

private:
  TierConfig& Config(TierId id);
  static int Priority(TierId id);

  TierConfig near_;
  TierConfig mid_;
  TierConfig far_;
};

} // namespace terra::sim
