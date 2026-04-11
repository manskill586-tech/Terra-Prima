#include "terra/sim/SimScheduler.h"

#include <algorithm>

namespace terra::sim {

SimScheduler::SimScheduler() {
  near_ = TierConfig{TierId::Near, 0.05, 0.02, 0.2, 0.4, 0.0, true};
  mid_ = TierConfig{TierId::Mid, 0.5, 0.25, 5.0, 0.8, 0.0, true};
  far_ = TierConfig{TierId::Far, 5.0, 1.0, 60.0, 1.2, 0.0, true};
}

void SimScheduler::SetConfig(TierId id, const TierConfig& config) {
  Config(id) = config;
}

const TierConfig& SimScheduler::GetConfig(TierId id) const {
  switch (id) {
    case TierId::Near: return near_;
    case TierId::Mid: return mid_;
    case TierId::Far: return far_;
  }
  return near_;
}

PlanResult SimScheduler::Plan(double simTimeSec, double frameBudgetMs) {
  PlanResult result{};
  std::vector<TierConfig*> due;
  for (auto* tier : {&near_, &mid_, &far_}) {
    if (!tier->enabled) {
      continue;
    }
    if (simTimeSec + 1e-6 >= tier->nextTimeSec) {
      due.push_back(tier);
    }
  }

  std::sort(due.begin(), due.end(), [](const TierConfig* a, const TierConfig* b) {
    return Priority(a->id) < Priority(b->id);
  });

  double estimated = 0.0;
  for (const auto* tier : due) {
    estimated += std::max(0.1, tier->lastCostMs);
  }

  if (estimated > frameBudgetMs && frameBudgetMs > 0.0) {
    // Adapt intervals for far/mid first.
    for (auto* tier : {&far_, &mid_}) {
      if (tier->enabled && tier->intervalSec < tier->maxIntervalSec) {
        tier->intervalSec = std::min(tier->maxIntervalSec, tier->intervalSec * 1.2);
      }
    }
  } else if (estimated < frameBudgetMs * 0.5) {
    for (auto* tier : {&mid_, &far_}) {
      if (tier->enabled && tier->intervalSec > tier->minIntervalSec) {
        tier->intervalSec = std::max(tier->minIntervalSec, tier->intervalSec * 0.9);
      }
    }
  }

  // Recalculate due after interval changes to avoid double-defer.
  result.estimatedCostMs = 0.0;
  result.overBudget = false;

  // Ensure Near always runs if due.
  auto includeTier = [&](TierConfig* tier) {
    result.tiers.push_back(tier->id);
    result.estimatedCostMs += std::max(0.1, tier->lastCostMs);
  };

  TierConfig* near = &near_;
  TierConfig* mid = &mid_;
  TierConfig* far = &far_;

  if (near->enabled) {
    includeTier(near);
  }

  auto tryInclude = [&](TierConfig* tier) {
    if (!tier->enabled) {
      return;
    }
    if (simTimeSec + 1e-6 < tier->nextTimeSec) {
      return;
    }
    const double cost = std::max(0.1, tier->lastCostMs);
    if (frameBudgetMs > 0.0 && result.estimatedCostMs + cost > frameBudgetMs) {
      result.overBudget = true;
      tier->nextTimeSec = simTimeSec + tier->intervalSec;
      return;
    }
    includeTier(tier);
  };

  tryInclude(mid);
  tryInclude(far);

  if (frameBudgetMs > 0.0 && result.estimatedCostMs > frameBudgetMs) {
    result.overBudget = true;
  }

  return result;
}

void SimScheduler::OnTierUpdated(TierId id, double simTimeSec, double costMs) {
  auto& tier = Config(id);
  tier.lastCostMs = std::max(0.01, costMs);
  tier.nextTimeSec = simTimeSec + tier.intervalSec;
}

void SimScheduler::UpdateIntervals(double frameBudgetMs) {
  if (frameBudgetMs <= 0.0) {
    return;
  }
  const double estimated = std::max(0.1, near_.lastCostMs) +
                           std::max(0.1, mid_.lastCostMs) +
                           std::max(0.1, far_.lastCostMs);
  if (estimated > frameBudgetMs) {
    for (auto* tier : {&far_, &mid_}) {
      if (tier->enabled && tier->intervalSec < tier->maxIntervalSec) {
        tier->intervalSec = std::min(tier->maxIntervalSec, tier->intervalSec * 1.2);
      }
    }
  } else if (estimated < frameBudgetMs * 0.5) {
    for (auto* tier : {&mid_, &far_}) {
      if (tier->enabled && tier->intervalSec > tier->minIntervalSec) {
        tier->intervalSec = std::max(tier->minIntervalSec, tier->intervalSec * 0.9);
      }
    }
  }
}

TierConfig& SimScheduler::Config(TierId id) {
  switch (id) {
    case TierId::Near: return near_;
    case TierId::Mid: return mid_;
    case TierId::Far: return far_;
  }
  return near_;
}

int SimScheduler::Priority(TierId id) {
  switch (id) {
    case TierId::Near: return 0;
    case TierId::Mid: return 1;
    case TierId::Far: return 2;
  }
  return 0;
}

} // namespace terra::sim
