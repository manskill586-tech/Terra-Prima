#pragma once

#include "world/ChunkTypes.h"

namespace genesis::sim {

struct TierSchedule {
  double dt{0.0};
  double accumulator{0.0};
  int max_steps{1};
};

struct TierPlan {
  int steps{0};
  double step_dt{0.0};
  bool coarse{false};
  double coarse_dt{0.0};
};

class SimScheduler {
public:
  SimScheduler();

  void AddTime(double frame_dt);
  TierPlan PlanSteps(genesis::world::Tier tier, bool force_min_step);

private:
  TierSchedule schedules_[3]{};
};

} // namespace genesis::sim
