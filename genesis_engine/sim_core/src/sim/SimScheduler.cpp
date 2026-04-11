#include "sim/SimScheduler.h"

namespace genesis::sim {

SimScheduler::SimScheduler() {
  schedules_[0] = {1.0 / 20.0, 0.0, 4}; // Near
  schedules_[1] = {1.0 / 2.0, 0.0, 2};  // Mid
  schedules_[2] = {5.0, 0.0, 1};        // Far
}

void SimScheduler::AddTime(double frame_dt) {
  for (auto& schedule : schedules_) {
    schedule.accumulator += frame_dt;
  }
}

TierPlan SimScheduler::PlanSteps(genesis::world::Tier tier, bool force_min_step) {
  const size_t index = static_cast<size_t>(tier);
  TierSchedule& schedule = schedules_[index];
  TierPlan plan;
  plan.step_dt = schedule.dt;

  while (schedule.accumulator >= schedule.dt && plan.steps < schedule.max_steps) {
    schedule.accumulator -= schedule.dt;
    plan.steps++;
  }

  if (schedule.accumulator >= schedule.dt) {
    plan.coarse = true;
    plan.coarse_dt = schedule.accumulator;
    schedule.accumulator = 0.0;
  }

  if (force_min_step && plan.steps == 0 && !plan.coarse) {
    plan.steps = 1;
  }

  return plan;
}

} // namespace genesis::sim
