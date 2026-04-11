#pragma once

#include "world/ChunkStore.h"
#include "terra/chem/ChemState.h"

#include <cstdint>
#include <string>
#include <vector>

namespace genesis::persistence {

struct ChunkRecord {
  genesis::world::Tier tier{genesis::world::Tier::Near};
  genesis::world::ChunkCoord coord{};
  genesis::world::FieldState field{};
  terra::chem::ChemState chem{};
  double last_sim_time{0.0};
  uint32_t stable_steps{0};
  bool sleeping{false};
};

struct Snapshot {
  uint64_t world_seed{0};
  double sim_time{0.0};
  std::vector<ChunkRecord> chunks;
};

struct RemovedChunk {
  genesis::world::Tier tier{genesis::world::Tier::Near};
  genesis::world::ChunkCoord coord{};
};

struct DeltaSnapshot {
  double base_sim_time{0.0};
  double target_sim_time{0.0};
  std::vector<ChunkRecord> added;
  std::vector<ChunkRecord> updated;
  std::vector<RemovedChunk> removed;
};

Snapshot CaptureSnapshot(uint64_t seed, double sim_time, const genesis::world::ChunkStore& store);
void ApplySnapshot(const Snapshot& snapshot, uint64_t& seed, double& sim_time, genesis::world::ChunkStore& store);

DeltaSnapshot CreateDelta(const Snapshot& base, const Snapshot& current);
Snapshot ApplyDelta(const Snapshot& base, const DeltaSnapshot& delta);

bool SaveSnapshot(const std::string& path, const Snapshot& snapshot);
bool LoadSnapshot(const std::string& path, Snapshot& out_snapshot);

bool SaveDelta(const std::string& path, const DeltaSnapshot& delta);
bool LoadDelta(const std::string& path, DeltaSnapshot& out_delta);

} // namespace genesis::persistence
