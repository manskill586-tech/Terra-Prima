#pragma once

#include "terra/sim/TierState.h"
#include "terra/world/ChunkKey.h"

#include <cstdint>
#include <future>
#include <string>
#include <unordered_map>
#include <vector>

namespace terra::io {

struct WorldSnapshot {
  double simTime{0.0};
  std::unordered_map<terra::world::ChunkKey, sim::NearChunkData, terra::world::ChunkKeyHasher> near;
  std::unordered_map<terra::world::ChunkKey, sim::MidChunkData, terra::world::ChunkKeyHasher> mid;
  std::unordered_map<terra::world::ChunkKey, sim::FarTileData, terra::world::ChunkKeyHasher> far;
};

struct NearDeltaEntry {
  terra::world::ChunkKey key{};
  sim::NearChunkData value{};
};

struct MidDeltaEntry {
  terra::world::ChunkKey key{};
  sim::MidChunkData value{};
};

struct FarDeltaEntry {
  terra::world::ChunkKey key{};
  sim::FarTileData value{};
};

struct DeltaSnapshot {
  double baseSimTime{0.0};
  double targetSimTime{0.0};
  std::vector<NearDeltaEntry> nearUpserts;
  std::vector<terra::world::ChunkKey> nearRemoved;
  std::vector<MidDeltaEntry> midUpserts;
  std::vector<terra::world::ChunkKey> midRemoved;
  std::vector<FarDeltaEntry> farUpserts;
  std::vector<terra::world::ChunkKey> farRemoved;
};

class SnapshotStore {
public:
  std::future<void> SaveSnapshotAsync(const std::string& path, WorldSnapshot snapshot);
  void SaveSnapshot(const std::string& path, const WorldSnapshot& snapshot);
  WorldSnapshot LoadSnapshot(const std::string& path);

  void SaveSnapshotJson(const std::string& path, const WorldSnapshot& snapshot);
  WorldSnapshot LoadSnapshotJson(const std::string& path);

  void SaveDelta(const std::string& path, const DeltaSnapshot& delta);
  DeltaSnapshot LoadDelta(const std::string& path);
};

DeltaSnapshot CreateDelta(const WorldSnapshot& base, const WorldSnapshot& current);
void ApplyDelta(WorldSnapshot& base, const DeltaSnapshot& delta);

} // namespace terra::io
