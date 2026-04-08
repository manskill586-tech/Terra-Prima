#pragma once

#include "terra/sim/TierState.h"
#include "terra/world/ChunkKey.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace terra::world {

struct CameraState {
  ChunkKey nearCenter{};
  ChunkKey midCenter{};
  ChunkKey farCenter{};
  int nearRadius{2};
  int midRadius{4};
  int farRadius{6};
};

struct ChunkMetrics {
  std::size_t activeNear{0};
  std::size_t activeMid{0};
  std::size_t activeFar{0};
};

class ChunkStore {
public:
  void UpdateActive(const CameraState& camera);

  ChunkMetrics Metrics() const;

  const std::unordered_map<ChunkKey, sim::NearChunkData, ChunkKeyHasher>& NearChunks() const;
  const std::unordered_map<ChunkKey, sim::MidChunkData, ChunkKeyHasher>& MidChunks() const;
  const std::unordered_map<ChunkKey, sim::FarTileData, ChunkKeyHasher>& FarTiles() const;

  std::unordered_map<ChunkKey, sim::NearChunkData, ChunkKeyHasher>& NearChunks();
  std::unordered_map<ChunkKey, sim::MidChunkData, ChunkKeyHasher>& MidChunks();
  std::unordered_map<ChunkKey, sim::FarTileData, ChunkKeyHasher>& FarTiles();

private:
  void ActivateTier(const ChunkKey& center, int radius,
                    std::unordered_set<ChunkKey, ChunkKeyHasher>& activeSet,
                    int targetLod);
  void SyncActivation();
  void AggregateToParent();

  static ChunkKey ParentKey(const ChunkKey& key);

  std::unordered_map<ChunkKey, sim::NearChunkData, ChunkKeyHasher> nearChunks_;
  std::unordered_map<ChunkKey, sim::MidChunkData, ChunkKeyHasher> midChunks_;
  std::unordered_map<ChunkKey, sim::FarTileData, ChunkKeyHasher> farTiles_;

  std::unordered_set<ChunkKey, ChunkKeyHasher> activeNear_;
  std::unordered_set<ChunkKey, ChunkKeyHasher> activeMid_;
  std::unordered_set<ChunkKey, ChunkKeyHasher> activeFar_;

  std::unordered_set<ChunkKey, ChunkKeyHasher> desiredNear_;
  std::unordered_set<ChunkKey, ChunkKeyHasher> desiredMid_;
  std::unordered_set<ChunkKey, ChunkKeyHasher> desiredFar_;
};

} // namespace terra::world
