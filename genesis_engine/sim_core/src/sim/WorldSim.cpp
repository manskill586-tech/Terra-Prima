#include "sim/WorldSim.h"

#include "shared/sim_state.h"
#include "terra/chem/ChemDB.h"
#include "terra/chem/ReactionEngine.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

using genesis::world::ChunkCoord;
using genesis::world::FieldState;
using genesis::world::Tier;

struct TierCoeffs {
  float diffusion{0.1f};
  float relax{0.01f};
  float noise_temp{0.05f};
  float noise_pressure{2.0f};
  float noise_humidity{0.01f};
};

struct SleepParams {
  float eps_temp{0.01f};
  float eps_pressure{1.0f};
  float eps_humidity{0.001f};
  uint32_t stable_steps{10};
  float wake_temp{0.03f};
  float wake_pressure{3.0f};
  float wake_humidity{0.003f};
};

TierCoeffs GetCoeffs(Tier tier) {
  switch (tier) {
    case Tier::Near:
      return {0.20f, 0.01f, 0.10f, 4.0f, 0.02f};
    case Tier::Mid:
      return {0.08f, 0.03f, 0.06f, 2.0f, 0.01f};
    case Tier::Far:
      return {0.02f, 0.05f, 0.03f, 1.0f, 0.005f};
  }
  return {};
}

SleepParams GetSleepParams(Tier tier) {
  switch (tier) {
    case Tier::Near:
      return {0.005f, 2.0f, 0.001f, 20, 0.02f, 5.0f, 0.004f};
    case Tier::Mid:
      return {0.01f, 5.0f, 0.002f, 10, 0.05f, 10.0f, 0.01f};
    case Tier::Far:
      return {0.02f, 10.0f, 0.005f, 6, 0.10f, 20.0f, 0.02f};
  }
  return {};
}

float Noise01(uint64_t seed, const ChunkCoord& coord, uint64_t step) {
  uint64_t h = genesis::world::Mix64(seed);
  h ^= genesis::world::Mix64(static_cast<uint64_t>(static_cast<uint32_t>(coord.x)));
  h ^= genesis::world::Mix64(static_cast<uint64_t>(static_cast<uint32_t>(coord.y)) << 1);
  h ^= genesis::world::Mix64(static_cast<uint64_t>(static_cast<uint32_t>(coord.z)) << 2);
  h ^= genesis::world::Mix64(step);
  const double unit = (h >> 11) * (1.0 / 9007199254740992.0);
  return static_cast<float>(unit);
}

FieldState AverageNeighbors(const genesis::world::ChunkStore& store, Tier tier, const ChunkCoord& coord) {
  const int dx[6] = {1, -1, 0, 0, 0, 0};
  const int dy[6] = {0, 0, 1, -1, 0, 0};
  const int dz[6] = {0, 0, 0, 0, 1, -1};
  FieldState sum{};
  int count = 0;
  for (int i = 0; i < 6; ++i) {
    ChunkCoord n{coord.x + dx[i], coord.y + dy[i], coord.z + dz[i]};
    if (!store.InWorld(tier, n)) {
      continue;
    }
    const auto* chunk = store.Find(tier, n);
    if (chunk) {
      sum.temperature += chunk->field.temperature;
      sum.pressure += chunk->field.pressure;
      sum.humidity += chunk->field.humidity;
    } else {
      sum.temperature += genesis::world::kAmbientField.temperature;
      sum.pressure += genesis::world::kAmbientField.pressure;
      sum.humidity += genesis::world::kAmbientField.humidity;
    }
    count++;
  }
  if (count == 0) {
    return genesis::world::kAmbientField;
  }
  sum.temperature /= static_cast<float>(count);
  sum.pressure /= static_cast<float>(count);
  sum.humidity /= static_cast<float>(count);
  return sum;
}

uint64_t ChunkSeed(uint64_t seed, const ChunkCoord& coord, uint64_t step) {
  uint64_t h = genesis::world::Mix64(seed);
  h ^= genesis::world::Mix64(static_cast<uint64_t>(static_cast<uint32_t>(coord.x)));
  h ^= genesis::world::Mix64(static_cast<uint64_t>(static_cast<uint32_t>(coord.y)) << 1);
  h ^= genesis::world::Mix64(static_cast<uint64_t>(static_cast<uint32_t>(coord.z)) << 2);
  h ^= genesis::world::Mix64(step);
  return h;
}

double HeatFromChem(const terra::chem::ChemDB& db,
                    const terra::chem::ChemState& chem,
                    double heat_scale,
                    double dt) {
  if (chem.species.empty()) {
    return 0.0;
  }
  const int species_id = chem.species.front().id;
  const auto& reactions = db.ReactionsForSpecies(species_id);
  if (reactions.empty()) {
    return 0.0;
  }
  const auto* reaction = db.GetReaction(static_cast<std::size_t>(reactions.front()));
  if (!reaction) {
    return 0.0;
  }
  const double concentration = static_cast<double>(chem.species.front().concentration);
  const double scaled = -reaction->deltaG * heat_scale * dt * std::min(1.0, concentration);
  return scaled;
}

} // namespace

namespace genesis::sim {

WorldSim::WorldSim(genesis::world::WorldConfig config) : store_(config) {}

void WorldSim::SetSeed(uint64_t seed) {
  seed_ = seed;
}

void WorldSim::ResetScheduler() {
  scheduler_ = SimScheduler();
  tier_step_[0] = 0;
  tier_step_[1] = 0;
  tier_step_[2] = 0;
}

void WorldSim::SetChemistry(const terra::chem::ChemDB* db, ChemConfig config) {
  chem_db_ = db;
  chem_config_ = config;
  if (chem_db_) {
    reaction_engine_ = std::make_unique<terra::chem::ReactionEngine>(*chem_db_);
  } else {
    reaction_engine_.reset();
  }
}

void WorldSim::EnsureActiveRegions() {
  store_.EnsureActiveCube(Tier::Near, {0, 0, 0}, near_radius_);
  store_.EnsureActiveCube(Tier::Mid, {0, 0, 0}, mid_radius_);
  store_.EnsureActiveCube(Tier::Far, {0, 0, 0}, far_radius_);
}

SimMetrics WorldSim::StepFrame(double frame_dt) {
  SimMetrics metrics{};
  EnsureActiveRegions();
  scheduler_.AddTime(frame_dt);

  const TierPlan near_plan = scheduler_.PlanSteps(Tier::Near, true);
  StepTierPlan(Tier::Near, near_plan);
  metrics.near_steps = near_plan.steps + (near_plan.coarse ? 1 : 0);

  const TierPlan mid_plan = scheduler_.PlanSteps(Tier::Mid, false);
  StepTierPlan(Tier::Mid, mid_plan);
  metrics.mid_steps = mid_plan.steps + (mid_plan.coarse ? 1 : 0);

  const TierPlan far_plan = scheduler_.PlanSteps(Tier::Far, false);
  StepTierPlan(Tier::Far, far_plan);
  metrics.far_steps = far_plan.steps + (far_plan.coarse ? 1 : 0);

  sim_time_ += frame_dt;
  return metrics;
}

void WorldSim::StepTierPlan(Tier tier, const TierPlan& plan) {
  for (int i = 0; i < plan.steps; ++i) {
    StepTier(tier, plan.step_dt);
  }
  if (plan.coarse) {
    StepTier(tier, plan.coarse_dt);
  }
}

void WorldSim::StepTier(Tier tier, double dt) {
  auto active = store_.CollectActive(tier);
  const TierCoeffs coeffs = GetCoeffs(tier);
  const SleepParams sleep = GetSleepParams(tier);
  std::vector<ChunkCoord> pending_wake;
  pending_wake.reserve(active.size());

  for (auto* chunk : active) {
    if (!chunk || chunk->sleeping) {
      continue;
    }

    const FieldState avg = AverageNeighbors(store_, tier, chunk->coord);
    const float noise = Noise01(seed_, chunk->coord, tier_step_[static_cast<size_t>(tier)]);
    const float centered = (noise - 0.5f) * 2.0f;

    FieldState next = chunk->field;
    next.temperature += coeffs.diffusion * (avg.temperature - chunk->field.temperature);
    next.pressure += coeffs.diffusion * (avg.pressure - chunk->field.pressure);
    next.humidity += coeffs.diffusion * (avg.humidity - chunk->field.humidity);

    next.temperature += coeffs.relax * (genesis::world::kAmbientField.temperature - chunk->field.temperature) * static_cast<float>(dt);
    next.pressure += coeffs.relax * (genesis::world::kAmbientField.pressure - chunk->field.pressure) * static_cast<float>(dt);
    next.humidity += coeffs.relax * (genesis::world::kAmbientField.humidity - chunk->field.humidity) * static_cast<float>(dt);

    next.temperature += centered * coeffs.noise_temp * static_cast<float>(dt);
    next.pressure += centered * coeffs.noise_pressure * static_cast<float>(dt);
    next.humidity += centered * coeffs.noise_humidity * static_cast<float>(dt);

    next.humidity = std::clamp(next.humidity, 0.0f, 1.0f);
    next.pressure = std::max(next.pressure, 1.0f);

    if (chem_db_ && reaction_engine_) {
      if (!chunk->chem_seeded) {
        const auto species = chem_db_->DefaultSeedSpecies(static_cast<std::size_t>(chem_config_.seed_species));
        chunk->chem.species.clear();
        for (int id : species) {
          chunk->chem.SetConcentration(id, chem_config_.seed_concentration);
        }
        chunk->chem.Compact();
        chunk->chem_seeded = true;
      }

      chunk->chem.temperature = next.temperature;
      chunk->chem.pressure = next.pressure;

      if (tier == Tier::Near) {
        reaction_engine_->Step(chunk->chem, dt, ChunkSeed(seed_, chunk->coord, tier_step_[static_cast<size_t>(tier)]),
                               chem_config_.near_config);
      } else if (tier == Tier::Mid) {
        reaction_engine_->Step(chunk->chem, dt, ChunkSeed(seed_, chunk->coord, tier_step_[static_cast<size_t>(tier)]),
                               chem_config_.mid_config);
      }

      next.temperature += static_cast<float>(HeatFromChem(*chem_db_, chunk->chem, chem_config_.heat_scale, dt));
      chunk->chem.temperature = next.temperature;
      chunk->chem.pressure = next.pressure;
    }

    const float dT = std::abs(next.temperature - chunk->field.temperature);
    const float dP = std::abs(next.pressure - chunk->field.pressure);
    const float dH = std::abs(next.humidity - chunk->field.humidity);

    chunk->field = next;
    chunk->last_sim_time += dt;
    chunk->dirty = true;

    if (dT < sleep.eps_temp && dP < sleep.eps_pressure && dH < sleep.eps_humidity) {
      chunk->stable_steps++;
      if (chunk->stable_steps >= sleep.stable_steps) {
        chunk->sleeping = true;
      }
    } else {
      chunk->stable_steps = 0;
    }

    if (dT > sleep.wake_temp || dP > sleep.wake_pressure || dH > sleep.wake_humidity) {
      const int dx[6] = {1, -1, 0, 0, 0, 0};
      const int dy[6] = {0, 0, 1, -1, 0, 0};
      const int dz[6] = {0, 0, 0, 0, 1, -1};
      for (int i = 0; i < 6; ++i) {
        ChunkCoord n{chunk->coord.x + dx[i], chunk->coord.y + dy[i], chunk->coord.z + dz[i]};
        if (!store_.InWorld(tier, n)) {
          continue;
        }
        auto* neighbor = store_.Find(tier, n);
        if (neighbor && neighbor->sleeping) {
          neighbor->sleeping = false;
          neighbor->stable_steps = 0;
        } else if (!neighbor) {
          pending_wake.push_back(n);
        }
      }
    }
  }

  for (const auto& coord : pending_wake) {
    auto& chunk = store_.GetOrCreate(tier, coord);
    chunk.sleeping = false;
    chunk.stable_steps = 0;
  }

  tier_step_[static_cast<size_t>(tier)]++;
}

void WorldSim::WriteShmSnapshot(genesis::shared::WorldState& out) const {
  auto fill_tier = [&](Tier tier,
                       genesis::shared::ChunkData* dest,
                       uint32_t max_count,
                       uint32_t& out_count,
                       uint8_t lod_level) {
    const auto active = store_.CollectActive(tier);
    const uint32_t count = static_cast<uint32_t>(std::min<size_t>(active.size(), max_count));
    out_count = count;
    for (uint32_t i = 0; i < count; ++i) {
      const auto* chunk = active[i];
      dest[i].coord = {chunk->coord.x, chunk->coord.y, chunk->coord.z};
      dest[i].temperature = chunk->field.temperature;
      dest[i].pressure = chunk->field.pressure;
      dest[i].humidity = chunk->field.humidity;
      dest[i].particle_count = 0;
      dest[i].reaction_count = 0;
      dest[i].lod_level = lod_level;
      std::fill(std::begin(dest[i].concentrations), std::end(dest[i].concentrations), 0.0f);
    }
  };

  fill_tier(Tier::Near, out.near_chunks, GENESIS_SHM_MAX_NEAR_CHUNKS, out.near_chunk_count, 0);
  fill_tier(Tier::Mid, out.mid_chunks, GENESIS_SHM_MAX_MID_CHUNKS, out.mid_chunk_count, 1);
  fill_tier(Tier::Far, out.far_chunks, GENESIS_SHM_MAX_FAR_CHUNKS, out.far_chunk_count, 2);

  out.near_particles.count = 0;

  out.organism_count = 0;
}

} // namespace genesis::sim
