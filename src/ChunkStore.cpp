#include "terra/world/ChunkStore.h"

#include <algorithm>

namespace terra::world {

void ChunkStore::UpdateActive(const CameraState& camera) {
  desiredNear_.clear();
  desiredMid_.clear();
  desiredFar_.clear();

  ActivateTier(camera.nearCenter, camera.nearRadius, desiredNear_, camera.nearCenter.lod);
  ActivateTier(camera.midCenter, camera.midRadius, desiredMid_, camera.midCenter.lod);
  ActivateTier(camera.farCenter, camera.farRadius, desiredFar_, camera.farCenter.lod);

  SyncActivation();
  AggregateToParent();
}

ChunkMetrics ChunkStore::Metrics() const {
  return {activeNear_.size(), activeMid_.size(), activeFar_.size()};
}

const std::unordered_map<ChunkKey, sim::NearChunkData, ChunkKeyHasher>& ChunkStore::NearChunks() const {
  return nearChunks_;
}

const std::unordered_map<ChunkKey, sim::MidChunkData, ChunkKeyHasher>& ChunkStore::MidChunks() const {
  return midChunks_;
}

const std::unordered_map<ChunkKey, sim::FarTileData, ChunkKeyHasher>& ChunkStore::FarTiles() const {
  return farTiles_;
}

std::unordered_map<ChunkKey, sim::NearChunkData, ChunkKeyHasher>& ChunkStore::NearChunks() {
  return nearChunks_;
}

std::unordered_map<ChunkKey, sim::MidChunkData, ChunkKeyHasher>& ChunkStore::MidChunks() {
  return midChunks_;
}

std::unordered_map<ChunkKey, sim::FarTileData, ChunkKeyHasher>& ChunkStore::FarTiles() {
  return farTiles_;
}

void ChunkStore::ActivateTier(const ChunkKey& center, int radius,
                              std::unordered_set<ChunkKey, ChunkKeyHasher>& activeSet,
                              int targetLod) {
  for (int dy = -radius; dy <= radius; ++dy) {
    for (int dx = -radius; dx <= radius; ++dx) {
      ChunkKey key{center.face, center.x + dx, center.y + dy, targetLod};
      activeSet.insert(key);
    }
  }
}

void ChunkStore::SyncActivation() {
  auto syncTier = [](auto& desired, auto& active, auto& storage, auto factory) {
    for (const auto& key : desired) {
      if (active.find(key) == active.end()) {
        active.insert(key);
      }
      if (storage.find(key) == storage.end()) {
        storage.emplace(key, factory());
      }
    }

    for (auto it = active.begin(); it != active.end();) {
      if (desired.find(*it) == desired.end()) {
        it = active.erase(it);
      } else {
        ++it;
      }
    }
  };

  syncTier(desiredNear_, activeNear_, nearChunks_, [] { return sim::NearChunkData{}; });
  syncTier(desiredMid_, activeMid_, midChunks_, [] { return sim::MidChunkData{}; });
  syncTier(desiredFar_, activeFar_, farTiles_, [] { return sim::FarTileData{}; });
}

void ChunkStore::AggregateToParent() {
  for (const auto& key : desiredNear_) {
    auto parent = ParentKey(key);
    auto midIt = midChunks_.find(parent);
    if (midIt == midChunks_.end()) {
      midIt = midChunks_.emplace(parent, sim::MidChunkData{}).first;
    }
    const auto& nearData = nearChunks_[key];
    midIt->second.temperature = (midIt->second.temperature + nearData.temperature) * 0.5f;
    midIt->second.humidity = (midIt->second.humidity + nearData.humidity) * 0.5f;
  }

  for (const auto& key : desiredMid_) {
    auto parent = ParentKey(key);
    auto farIt = farTiles_.find(parent);
    if (farIt == farTiles_.end()) {
      farIt = farTiles_.emplace(parent, sim::FarTileData{}).first;
    }
    const auto& midData = midChunks_[key];
    farIt->second.avgTemperature = (farIt->second.avgTemperature + midData.temperature) * 0.5f;
    farIt->second.population = (farIt->second.population + midData.biomass) * 0.5f;
  }
}

ChunkKey ChunkStore::ParentKey(const ChunkKey& key) {
  ChunkKey parent = key;
  parent.lod = std::max(0, key.lod - 1);
  parent.x = key.x / 2;
  parent.y = key.y / 2;
  return parent;
}

} // namespace terra::world
