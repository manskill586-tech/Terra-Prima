#pragma once

#include "terra/sim/TierState.h"
#include "terra/world/ChunkKey.h"

#include <cstdint>
#include <future>
#include <string>
#include <unordered_map>

namespace terra::io {

struct WorldSnapshot {
  double simTime{0.0};
  std::unordered_map<terra::world::ChunkKey, sim::NearChunkData, terra::world::ChunkKeyHasher> near;
  std::unordered_map<terra::world::ChunkKey, sim::MidChunkData, terra::world::ChunkKeyHasher> mid;
  std::unordered_map<terra::world::ChunkKey, sim::FarTileData, terra::world::ChunkKeyHasher> far;
};

class SnapshotStore {
public:
  std::future<void> SaveSnapshotAsync(const std::string& path, WorldSnapshot snapshot);
};

} // namespace terra::io
