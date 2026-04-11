#pragma once

#include "world/ChunkStore.h"

#include <cstdint>
#include <vector>

namespace genesis::shared {
struct NearParticles;
}

namespace genesis::sim {

struct ParticleSystemConfig {
  int count{50000};
  float drag{0.2f};
  float pressure_force{0.00005f};
  float temp_noise{0.4f};
  bool use_chem_species{false};
};

class ParticleSystem {
public:
  explicit ParticleSystem(ParticleSystemConfig config);

  void Initialize(uint64_t seed, float world_half_extent);
  void Step(double dt, const genesis::world::ChunkStore& store, uint64_t seed);
  void WriteToShm(genesis::shared::NearParticles& out) const;

  int count() const { return config_.count; }
  void SetUseChemSpecies(bool value) { config_.use_chem_species = value; }

private:
  ParticleSystemConfig config_{};
  float world_half_extent_{500.0f};
  uint64_t step_{0};

  std::vector<float> px_;
  std::vector<float> py_;
  std::vector<float> pz_;
  std::vector<float> vx_;
  std::vector<float> vy_;
  std::vector<float> vz_;
  std::vector<uint16_t> element_id_;

  void EnsureSize();
};

} // namespace genesis::sim
