#include "sim/ParticleSystem.h"

#include "shared/sim_state.h"
#include "terra/chem/ChemState.h"

#include <algorithm>
#include <cmath>

namespace {

using genesis::world::ChunkCoord;
using genesis::world::Tier;

float Clamp(float v, float lo, float hi) {
  return std::max(lo, std::min(hi, v));
}

float SignedNoise(uint64_t seed, uint64_t index, uint64_t step) {
  uint64_t h = genesis::world::Mix64(seed ^ (index * 0x9E3779B97F4A7C15ULL) ^ (step * 0xD1B54A32D192ED03ULL));
  const double unit = (h >> 11) * (1.0 / 9007199254740992.0);
  return static_cast<float>(unit * 2.0 - 1.0);
}

ChunkCoord PositionToCoord(float x, float y, float z, int chunk_size) {
  return {static_cast<int>(std::floor(x / chunk_size)),
          static_cast<int>(std::floor(y / chunk_size)),
          static_cast<int>(std::floor(z / chunk_size))};
}

float PressureAt(const genesis::world::ChunkStore& store, Tier tier, ChunkCoord coord) {
  const auto* chunk = store.Find(tier, coord);
  if (!chunk) {
    return genesis::world::kAmbientField.pressure;
  }
  return chunk->field.pressure;
}

float TemperatureAt(const genesis::world::ChunkStore& store, Tier tier, ChunkCoord coord) {
  const auto* chunk = store.Find(tier, coord);
  if (!chunk) {
    return genesis::world::kAmbientField.temperature;
  }
  return chunk->field.temperature;
}

uint16_t DominantSpeciesId(const terra::chem::ChemState& chem) {
  int id = 0;
  float best = -1.0f;
  for (const auto& s : chem.species) {
    if (s.concentration > best) {
      best = s.concentration;
      id = s.id;
    }
  }
  return static_cast<uint16_t>(id & 0xFFFF);
}

} // namespace

namespace genesis::sim {

ParticleSystem::ParticleSystem(ParticleSystemConfig config) : config_(config) {
  EnsureSize();
}

void ParticleSystem::EnsureSize() {
  const int count = std::max(0, config_.count);
  px_.resize(count);
  py_.resize(count);
  pz_.resize(count);
  vx_.resize(count);
  vy_.resize(count);
  vz_.resize(count);
  element_id_.resize(count);
}

void ParticleSystem::Initialize(uint64_t seed, float world_half_extent) {
  world_half_extent_ = world_half_extent;
  EnsureSize();
  const int count = static_cast<int>(px_.size());
  for (int i = 0; i < count; ++i) {
    const float fx = SignedNoise(seed, static_cast<uint64_t>(i), 1) * world_half_extent_;
    const float fy = SignedNoise(seed, static_cast<uint64_t>(i), 2) * world_half_extent_;
    const float fz = SignedNoise(seed, static_cast<uint64_t>(i), 3) * world_half_extent_;
    px_[i] = fx;
    py_[i] = fy;
    pz_[i] = fz;
    vx_[i] = SignedNoise(seed, static_cast<uint64_t>(i), 4) * 0.1f;
    vy_[i] = SignedNoise(seed, static_cast<uint64_t>(i), 5) * 0.1f;
    vz_[i] = SignedNoise(seed, static_cast<uint64_t>(i), 6) * 0.1f;
    element_id_[i] = static_cast<uint16_t>(1 + (i % 6));
  }
}

void ParticleSystem::Step(double dt, const genesis::world::ChunkStore& store, uint64_t seed) {
  const int count = static_cast<int>(px_.size());
  const float size = static_cast<float>(store.ChunkSize(Tier::Near));

  for (int i = 0; i < count; ++i) {
    ChunkCoord coord = PositionToCoord(px_[i], py_[i], pz_[i], static_cast<int>(size));
    if (!store.InWorld(Tier::Near, coord)) {
      coord = {0, 0, 0};
    }

    const ChunkCoord cxp{coord.x + 1, coord.y, coord.z};
    const ChunkCoord cxm{coord.x - 1, coord.y, coord.z};
    const ChunkCoord cyp{coord.x, coord.y + 1, coord.z};
    const ChunkCoord cym{coord.x, coord.y - 1, coord.z};
    const ChunkCoord czp{coord.x, coord.y, coord.z + 1};
    const ChunkCoord czm{coord.x, coord.y, coord.z - 1};

    const float pxp = PressureAt(store, Tier::Near, cxp);
    const float pxm = PressureAt(store, Tier::Near, cxm);
    const float pyp = PressureAt(store, Tier::Near, cyp);
    const float pym = PressureAt(store, Tier::Near, cym);
    const float pzp = PressureAt(store, Tier::Near, czp);
    const float pzm = PressureAt(store, Tier::Near, czm);

    const float inv = 1.0f / (2.0f * size);
    const float gradx = (pxp - pxm) * inv;
    const float grady = (pyp - pym) * inv;
    const float gradz = (pzp - pzm) * inv;

    const float temperature = TemperatureAt(store, Tier::Near, coord);
    const float temp_scale = config_.temp_noise * (temperature / 300.0f);

    const float nx = SignedNoise(seed, static_cast<uint64_t>(i), step_) * temp_scale;
    const float ny = SignedNoise(seed, static_cast<uint64_t>(i), step_ + 1) * temp_scale;
    const float nz = SignedNoise(seed, static_cast<uint64_t>(i), step_ + 2) * temp_scale;

    vx_[i] += (-gradx * config_.pressure_force + nx) * static_cast<float>(dt);
    vy_[i] += (-grady * config_.pressure_force + ny) * static_cast<float>(dt);
    vz_[i] += (-gradz * config_.pressure_force + nz) * static_cast<float>(dt);

    const float drag = std::max(0.0f, 1.0f - config_.drag * static_cast<float>(dt));
    vx_[i] *= drag;
    vy_[i] *= drag;
    vz_[i] *= drag;

    px_[i] += vx_[i] * static_cast<float>(dt);
    py_[i] += vy_[i] * static_cast<float>(dt);
    pz_[i] += vz_[i] * static_cast<float>(dt);

    if (px_[i] < -world_half_extent_) {
      px_[i] = -world_half_extent_;
      vx_[i] *= -0.5f;
    } else if (px_[i] > world_half_extent_) {
      px_[i] = world_half_extent_;
      vx_[i] *= -0.5f;
    }
    if (py_[i] < -world_half_extent_) {
      py_[i] = -world_half_extent_;
      vy_[i] *= -0.5f;
    } else if (py_[i] > world_half_extent_) {
      py_[i] = world_half_extent_;
      vy_[i] *= -0.5f;
    }
    if (pz_[i] < -world_half_extent_) {
      pz_[i] = -world_half_extent_;
      vz_[i] *= -0.5f;
    } else if (pz_[i] > world_half_extent_) {
      pz_[i] = world_half_extent_;
      vz_[i] *= -0.5f;
    }

    if (config_.use_chem_species) {
      const auto* chunk = store.Find(Tier::Near, coord);
      if (chunk) {
        element_id_[i] = DominantSpeciesId(chunk->chem);
      }
    }
  }

  step_++;
}

void ParticleSystem::WriteToShm(genesis::shared::NearParticles& out) const {
  const int max_particles = static_cast<int>(GENESIS_SHM_MAX_NEAR_PARTICLES);
  const int count = std::min(static_cast<int>(px_.size()), max_particles);
  out.count = static_cast<uint32_t>(count);
  for (int i = 0; i < count; ++i) {
    out.px[i] = px_[i];
    out.py[i] = py_[i];
    out.pz[i] = pz_[i];
    out.vx[i] = vx_[i];
    out.vy[i] = vy_[i];
    out.vz[i] = vz_[i];
    out.element_id[i] = element_id_[i];
    out.flags[i] = 0;
  }
}

} // namespace genesis::sim
