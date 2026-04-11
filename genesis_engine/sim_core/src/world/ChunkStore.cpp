#include "world/ChunkStore.h"

#include <algorithm>

namespace genesis::world {

ChunkStore::ChunkStore(WorldConfig config) : config_(config) {}

ChunkStore::Map& ChunkStore::MapFor(Tier tier) {
  return tiers_[static_cast<size_t>(tier)];
}

const ChunkStore::Map& ChunkStore::MapFor(Tier tier) const {
  return tiers_[static_cast<size_t>(tier)];
}

Chunk* ChunkStore::Find(Tier tier, ChunkCoord coord) {
  auto& map = MapFor(tier);
  auto it = map.find(coord);
  if (it == map.end()) {
    return nullptr;
  }
  return &it->second;
}

const Chunk* ChunkStore::Find(Tier tier, ChunkCoord coord) const {
  const auto& map = MapFor(tier);
  auto it = map.find(coord);
  if (it == map.end()) {
    return nullptr;
  }
  return &it->second;
}

Chunk& ChunkStore::EmplaceChunk(Map& map, ChunkCoord coord) {
  Chunk chunk;
  chunk.coord = coord;
  auto [it, inserted] = map.emplace(coord, chunk);
  return it->second;
}

Chunk& ChunkStore::GetOrCreate(Tier tier, ChunkCoord coord) {
  auto& map = MapFor(tier);
  auto it = map.find(coord);
  if (it != map.end()) {
    return it->second;
  }
  return EmplaceChunk(map, coord);
}

void ChunkStore::Clear() {
  for (auto& map : tiers_) {
    map.clear();
  }
}

size_t ChunkStore::Count(Tier tier) const {
  return MapFor(tier).size();
}

void ChunkStore::ForEachChunk(Tier tier, const std::function<void(Chunk&)>& fn) {
  auto& map = MapFor(tier);
  for (auto& [coord, chunk] : map) {
    fn(chunk);
  }
}

void ChunkStore::ForEachChunk(Tier tier, const std::function<void(const Chunk&)>& fn) const {
  const auto& map = MapFor(tier);
  for (const auto& [coord, chunk] : map) {
    fn(chunk);
  }
}

std::vector<Chunk*> ChunkStore::CollectActive(Tier tier) {
  std::vector<Chunk*> result;
  auto& map = MapFor(tier);
  result.reserve(map.size());
  for (auto& [coord, chunk] : map) {
    if (!chunk.sleeping) {
      result.push_back(&chunk);
    }
  }
  return result;
}

int ChunkStore::ChunkSize(Tier tier) const {
  switch (tier) {
    case Tier::Near:
      return config_.near_size_m;
    case Tier::Mid:
      return config_.mid_size_m;
    case Tier::Far:
      return config_.far_size_m;
  }
  return config_.near_size_m;
}

bool ChunkStore::InWorld(Tier tier, ChunkCoord coord) const {
  const int size = ChunkSize(tier);
  const int half = config_.world_size_m / 2;
  const int min = -half / size;
  const int max = (half - 1) / size;
  return coord.x >= min && coord.x <= max &&
         coord.y >= min && coord.y <= max &&
         coord.z >= min && coord.z <= max;
}

void ChunkStore::EnsureActiveCube(Tier tier, ChunkCoord center, int radius) {
  for (int dz = -radius; dz <= radius; ++dz) {
    for (int dy = -radius; dy <= radius; ++dy) {
      for (int dx = -radius; dx <= radius; ++dx) {
        ChunkCoord coord{center.x + dx, center.y + dy, center.z + dz};
        if (!InWorld(tier, coord)) {
          continue;
        }
        Chunk& chunk = GetOrCreate(tier, coord);
        chunk.sleeping = false;
        chunk.stable_steps = 0;
        chunk.dirty = true;
      }
    }
  }
}

} // namespace genesis::world
