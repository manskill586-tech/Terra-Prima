#pragma once

#include "world/ChunkTypes.h"
#include "terra/chem/ChemState.h"

#include <array>
#include <functional>
#include <unordered_map>
#include <vector>

namespace genesis::world {

struct WorldConfig {
  int world_size_m{1000};
  int near_size_m{2};
  int mid_size_m{32};
  int far_size_m{256};
};

struct Chunk {
  ChunkCoord coord{};
  FieldState field{};
  terra::chem::ChemState chem{};
  VisualProps visual{};
  bool chem_seeded{false};
  bool visual_dirty{true};
  double last_sim_time{0.0};
  uint32_t stable_steps{0};
  bool dirty{true};
  bool sleeping{false};
};

class ChunkStore {
public:
  explicit ChunkStore(WorldConfig config);

  Chunk* Find(Tier tier, ChunkCoord coord);
  const Chunk* Find(Tier tier, ChunkCoord coord) const;
  Chunk& GetOrCreate(Tier tier, ChunkCoord coord);

  void Clear();
  size_t Count(Tier tier) const;
  void ForEachChunk(Tier tier, const std::function<void(Chunk&)>& fn);
  void ForEachChunk(Tier tier, const std::function<void(const Chunk&)>& fn) const;
  std::vector<Chunk*> CollectActive(Tier tier);
  std::vector<const Chunk*> CollectActive(Tier tier) const;

  void EnsureActiveCube(Tier tier, ChunkCoord center, int radius);
  bool InWorld(Tier tier, ChunkCoord coord) const;
  int ChunkSize(Tier tier) const;
  const WorldConfig& config() const { return config_; }

private:
  using Map = std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash>;

  Chunk& EmplaceChunk(Map& map, ChunkCoord coord);
  Map& MapFor(Tier tier);
  const Map& MapFor(Tier tier) const;

  WorldConfig config_{};
  std::array<Map, 3> tiers_;
};

} // namespace genesis::world
