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

struct ColorF {
  float r{0.0f};
  float g{0.0f};
  float b{0.0f};
  float a{1.0f};
};

float Clamp01(float v) {
  return std::clamp(v, 0.0f, 1.0f);
}

ColorF ColorFromPacked(uint32_t rgba) {
  ColorF c{};
  c.r = static_cast<float>(rgba & 0xFF) / 255.0f;
  c.g = static_cast<float>((rgba >> 8) & 0xFF) / 255.0f;
  c.b = static_cast<float>((rgba >> 16) & 0xFF) / 255.0f;
  c.a = static_cast<float>((rgba >> 24) & 0xFF) / 255.0f;
  return c;
}

uint32_t PackColor(const ColorF& c) {
  auto to_u8 = [](float v) -> uint8_t { return static_cast<uint8_t>(Clamp01(v) * 255.0f + 0.5f); };
  const uint8_t r = to_u8(c.r);
  const uint8_t g = to_u8(c.g);
  const uint8_t b = to_u8(c.b);
  const uint8_t a = to_u8(c.a);
  return static_cast<uint32_t>(r) |
         (static_cast<uint32_t>(g) << 8) |
         (static_cast<uint32_t>(b) << 16) |
         (static_cast<uint32_t>(a) << 24);
}

ColorF Lerp(const ColorF& a, const ColorF& b, float t) {
  const float tt = Clamp01(t);
  return {
      a.r + (b.r - a.r) * tt,
      a.g + (b.g - a.g) * tt,
      a.b + (b.b - a.b) * tt,
      a.a + (b.a - a.a) * tt,
  };
}

ColorF Blackbody(float temperature) {
  // Approximate blackbody color in 1000K..40000K.
  const float t = std::clamp(temperature, 1000.0f, 40000.0f) / 100.0f;
  float r, g, b;
  if (t <= 66.0f) {
    r = 1.0f;
    g = std::clamp(0.3900816f * std::log(t) - 0.6318414f, 0.0f, 1.0f);
    if (t <= 19.0f) {
      b = 0.0f;
    } else {
      b = std::clamp(0.5432068f * std::log(t - 10.0f) - 1.1962541f, 0.0f, 1.0f);
    }
  } else {
    r = std::clamp(1.2929362f * std::pow(t - 60.0f, -0.1332048f), 0.0f, 1.0f);
    g = std::clamp(1.1298909f * std::pow(t - 60.0f, -0.0755148f), 0.0f, 1.0f);
    b = 1.0f;
  }
  return {r, g, b, 1.0f};
}

double SpeciesAverageDensity(const terra::chem::ChemDB& db, int species_id) {
  const terra::chem::Molecule* mol = db.GetMolecule(species_id);
  if (!mol || mol->atoms.empty()) {
    const terra::chem::Element* element = db.GetElement(species_id);
    return element ? element->density : 0.0;
  }
  double sum = 0.0;
  int count = 0;
  for (const auto& atom : mol->atoms) {
    const auto* element = db.GetElement(atom.atomicNumber);
    if (!element || element->density <= 0.0) {
      continue;
    }
    sum += element->density;
    ++count;
  }
  return count > 0 ? (sum / static_cast<double>(count)) : 0.0;
}

double SpeciesAverageAtomicVolume(const terra::chem::ChemDB& db, int species_id) {
  const terra::chem::Molecule* mol = db.GetMolecule(species_id);
  if (!mol || mol->atoms.empty()) {
    const terra::chem::Element* element = db.GetElement(species_id);
    return element ? element->atomicVolume : 0.0;
  }
  double sum = 0.0;
  int count = 0;
  for (const auto& atom : mol->atoms) {
    const auto* element = db.GetElement(atom.atomicNumber);
    if (!element || element->atomicVolume <= 0.0) {
      continue;
    }
    sum += element->atomicVolume;
    ++count;
  }
  return count > 0 ? (sum / static_cast<double>(count)) : 0.0;
}

double SpeciesMeltingPoint(const terra::chem::ChemDB& db, int species_id) {
  const terra::chem::Molecule* mol = db.GetMolecule(species_id);
  if (!mol || mol->atoms.empty()) {
    const terra::chem::Element* element = db.GetElement(species_id);
    return element ? element->meltingPoint : 0.0;
  }
  double sum = 0.0;
  int count = 0;
  for (const auto& atom : mol->atoms) {
    const auto* element = db.GetElement(atom.atomicNumber);
    if (!element || element->meltingPoint <= 0.0) {
      continue;
    }
    sum += element->meltingPoint;
    ++count;
  }
  return count > 0 ? (sum / static_cast<double>(count)) : 0.0;
}

double SpeciesBoilingPoint(const terra::chem::ChemDB& db, int species_id) {
  const terra::chem::Molecule* mol = db.GetMolecule(species_id);
  if (!mol || mol->atoms.empty()) {
    const terra::chem::Element* element = db.GetElement(species_id);
    return element ? element->boilingPoint : 0.0;
  }
  double sum = 0.0;
  int count = 0;
  for (const auto& atom : mol->atoms) {
    const auto* element = db.GetElement(atom.atomicNumber);
    if (!element || element->boilingPoint <= 0.0) {
      continue;
    }
    sum += element->boilingPoint;
    ++count;
  }
  return count > 0 ? (sum / static_cast<double>(count)) : 0.0;
}

uint8_t PhaseFromTemp(const terra::chem::ChemDB* db, int species_id, double temperature) {
  if (temperature >= 3500.0) {
    return 3; // plasma
  }
  if (!db) {
    if (temperature < 260.0) {
      return 2;
    }
    if (temperature < 360.0) {
      return 1;
    }
    return 0;
  }
  const double melt = SpeciesMeltingPoint(*db, species_id);
  const double boil = SpeciesBoilingPoint(*db, species_id);
  if (melt > 0.0 && temperature < melt) {
    return 2;
  }
  if (boil > 0.0 && temperature < boil) {
    return 1;
  }
  return 0;
}

genesis::world::VisualProps ComputeVisual(const genesis::world::Chunk& chunk,
                                          Tier tier,
                                          const terra::chem::ChemDB* db,
                                          int chunk_size) {
  (void)tier;
  (void)chunk_size;
  genesis::world::VisualProps out{};
  out.phase = 0;
  out.opacity = 1.0f;
  out.roughness = 0.5f;
  out.metallic = 0.0f;
  out.scale = 0.6f;
  out.emissive = 0.0f;

  const auto& species = chunk.chem.species;
  int dominant_id = 0;
  float dominant_val = -1.0f;
  ColorF mix{0.6f, 0.6f, 0.6f, 1.0f};

  if (db && !species.empty()) {
    float total = 0.0f;
    for (const auto& s : species) {
      total += std::max(0.0f, s.concentration);
      if (s.concentration > dominant_val) {
        dominant_val = s.concentration;
        dominant_id = s.id;
      }
    }
    if (total > 0.0f) {
      ColorF accum{0, 0, 0, 1.0f};
      for (const auto& s : species) {
        if (s.concentration <= 0.0f) {
          continue;
        }
        const float w = s.concentration / total;
        const ColorF c = ColorFromPacked(db->ColorForSpecies(s.id));
        accum.r += c.r * w;
        accum.g += c.g * w;
        accum.b += c.b * w;
      }
      mix = accum;
    }
  }

  if (dominant_id == 0 && !species.empty()) {
    dominant_id = species.front().id;
  }

  out.dominant_species_id = static_cast<uint32_t>(dominant_id);

  const double temperature = chunk.field.temperature;
  out.phase = PhaseFromTemp(db, dominant_id, temperature);

  double density = 0.0;
  double atomic_volume = 0.0;
  if (db && dominant_id != 0) {
    density = SpeciesAverageDensity(*db, dominant_id);
    atomic_volume = SpeciesAverageAtomicVolume(*db, dominant_id);
  }

  const float density_norm = density > 0.0 ? std::clamp(static_cast<float>(density / 20.0), 0.0f, 1.0f) : 0.1f;
  out.density = static_cast<float>(density);

  if (out.phase == 2) { // solid
    out.hardness = 0.3f + 0.7f * density_norm;
    out.viscosity = 0.0f;
    out.compressibility = 0.05f;
    out.porosity = 0.1f + 0.2f * (1.0f - density_norm);
    out.opacity = 0.85f + 0.1f * density_norm;
    out.roughness = 0.5f + 0.3f * out.porosity;
    if (db) {
      const auto* element = db->GetElement(dominant_id);
      if (element && (element->block == "d" || element->block == "f")) {
        out.metallic = 0.7f;
      }
    }
  } else if (out.phase == 1) { // liquid
    out.hardness = 0.1f;
    out.viscosity = 0.2f + 0.6f * density_norm;
    out.compressibility = 0.4f;
    out.porosity = 0.0f;
    out.opacity = 0.5f + 0.4f * density_norm;
    out.roughness = 0.15f;
    out.metallic = 0.0f;
  } else if (out.phase == 3) { // plasma
    out.hardness = 0.02f;
    out.viscosity = 0.05f;
    out.compressibility = 0.9f;
    out.porosity = 1.0f;
    out.opacity = 0.2f;
    out.roughness = 1.0f;
    out.metallic = 0.0f;
  } else { // gas
    out.hardness = 0.02f;
    out.viscosity = 0.05f;
    out.compressibility = 1.0f;
    out.porosity = 1.0f;
    out.opacity = 0.05f + 0.2f * density_norm;
    out.roughness = 1.0f;
    out.metallic = 0.0f;
  }

  // Scale (relative to chunk size)
  float vol_scale = 1.0f;
  if (atomic_volume > 0.0) {
    const float vol = static_cast<float>(atomic_volume);
    vol_scale = std::clamp(0.3f + 0.08f * std::log1p(vol), 0.2f, 1.2f);
  }
  if (out.phase == 0) {
    const float pressure = std::max(1.0f, chunk.field.pressure);
    const float factor = std::clamp(101325.0f / pressure, 0.2f, 4.0f);
    vol_scale *= factor;
  }
  out.scale = std::clamp(vol_scale, 0.2f, 1.5f);

  // Temperature glow
  if (temperature > 700.0) {
    const float t = Clamp01(static_cast<float>((temperature - 700.0) / 2300.0));
    const ColorF glow = Blackbody(static_cast<float>(temperature));
    mix = Lerp(mix, glow, t * t);
    out.emissive = t;
  }

  mix.a = Clamp01(out.emissive);
  out.albedo_rgba = PackColor(mix);
  return out;
}

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

    if (visual_test_) {
      const float dist = std::sqrt(static_cast<float>(chunk->coord.x * chunk->coord.x +
                                                      chunk->coord.y * chunk->coord.y +
                                                      chunk->coord.z * chunk->coord.z));
      const float wave = std::sin(static_cast<float>(sim_time_) * 0.5f);
      next.temperature = 220.0f + dist * 35.0f + wave * 200.0f;
      next.pressure = 90000.0f + dist * 800.0f;
      next.humidity = std::clamp(0.2f + 0.05f * dist, 0.0f, 1.0f);
    }

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
      chunk->visual_dirty = true;
    }

    const float dT = std::abs(next.temperature - chunk->field.temperature);
    const float dP = std::abs(next.pressure - chunk->field.pressure);
    const float dH = std::abs(next.humidity - chunk->field.humidity);

    chunk->field = next;
    chunk->last_sim_time += dt;
    chunk->dirty = true;
    chunk->visual_dirty = true;

    if (chunk->visual_dirty) {
      chunk->visual = ComputeVisual(*chunk, tier, chem_db_, store_.ChunkSize(tier));
      chunk->visual_dirty = false;
    }

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
      dest[i].dominant_species_id = chunk->visual.dominant_species_id;
      dest[i].phase = chunk->visual.phase;
      dest[i].albedo_rgba = chunk->visual.albedo_rgba;
      dest[i].opacity = chunk->visual.opacity;
      dest[i].roughness = chunk->visual.roughness;
      dest[i].metallic = chunk->visual.metallic;
      dest[i].scale = chunk->visual.scale;
      dest[i].hardness = chunk->visual.hardness;
      dest[i].emissive = chunk->visual.emissive;
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
