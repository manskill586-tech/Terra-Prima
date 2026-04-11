#pragma once

#include "sim/SimScheduler.h"
#include "world/ChunkStore.h"
#include "terra/chem/ReactionEngine.h"

#include <cstdint>
#include <memory>

namespace genesis::shared {
struct WorldState;
}

namespace terra::chem {
class ChemDB;
class ReactionEngine;
}

namespace genesis::sim {

struct ChemConfig {
  int seed_species{8};
  float seed_concentration{1.0f};
  double heat_scale{1e-5};
  terra::chem::ReactionEngineConfig near_config{};
  terra::chem::ReactionEngineConfig mid_config{};
};

struct SimMetrics {
  int near_steps{0};
  int mid_steps{0};
  int far_steps{0};
};

class WorldSim {
public:
  explicit WorldSim(genesis::world::WorldConfig config);

  void SetSeed(uint64_t seed);
  void SetSimTime(double sim_time) { sim_time_ = sim_time; }
  void ResetScheduler();
  void SetChemistry(const terra::chem::ChemDB* db, ChemConfig config);
  bool HasChemistry() const { return chem_db_ != nullptr; }
  const ChemConfig& chem_config() const { return chem_config_; }
  uint64_t seed() const { return seed_; }
  double sim_time() const { return sim_time_; }

  genesis::world::ChunkStore& store() { return store_; }
  const genesis::world::ChunkStore& store() const { return store_; }

  SimMetrics StepFrame(double frame_dt);

  void WriteShmSnapshot(genesis::shared::WorldState& out) const;

private:
  void EnsureActiveRegions();
  void StepTier(genesis::world::Tier tier, double dt);
  void StepTierPlan(genesis::world::Tier tier, const TierPlan& plan);

  genesis::world::ChunkStore store_;
  SimScheduler scheduler_;
  uint64_t seed_{1337};
  double sim_time_{0.0};
  uint64_t tier_step_[3]{0, 0, 0};
  int near_radius_{3};
  int mid_radius_{2};
  int far_radius_{1};
  const terra::chem::ChemDB* chem_db_{nullptr};
  std::unique_ptr<terra::chem::ReactionEngine> reaction_engine_;
  ChemConfig chem_config_{};
};

} // namespace genesis::sim
