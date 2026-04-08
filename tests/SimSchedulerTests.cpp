#include "terra/sim/SimScheduler.h"

#include <algorithm>

extern void Expect(bool cond, const char* expr, const char* file, int line);
#define EXPECT_TRUE(x) Expect((x), #x, __FILE__, __LINE__)

void RunSimSchedulerTests() {
  terra::sim::SimScheduler scheduler;

  auto plan = scheduler.Plan(0.0, 5.0);
  EXPECT_TRUE(!plan.tiers.empty());
  EXPECT_TRUE(std::find(plan.tiers.begin(), plan.tiers.end(), terra::sim::TierId::Near) != plan.tiers.end());

  scheduler.OnTierUpdated(terra::sim::TierId::Near, 0.0, 1.0);
  scheduler.OnTierUpdated(terra::sim::TierId::Mid, 0.0, 2.0);
  scheduler.OnTierUpdated(terra::sim::TierId::Far, 0.0, 2.5);

  plan = scheduler.Plan(0.0, 1.0);
  EXPECT_TRUE(std::find(plan.tiers.begin(), plan.tiers.end(), terra::sim::TierId::Near) != plan.tiers.end());
}
