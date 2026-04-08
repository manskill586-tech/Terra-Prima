#pragma once

#include "terra/world/ChunkKey.h"

#include <cstdint>
#include <unordered_map>

namespace terra::world {

struct EntityState {
  uint64_t id{0};
  ChunkKey chunk{};
  float health{1.0f};
  float age{0.0f};
  float skill{0.0f};
};

class EntityLOD {
public:
  void PromoteToNear(uint64_t id);
  void DemoteToMid(uint64_t id);
  void DemoteToFar(uint64_t id);

  void Update(float deltaTime);

  std::size_t NearCount() const;
  std::size_t MidCount() const;
  std::size_t FarCount() const;

  EntityState& GetOrCreate(uint64_t id);

private:
  std::unordered_map<uint64_t, EntityState> near_;
  std::unordered_map<uint64_t, EntityState> mid_;
  std::unordered_map<uint64_t, EntityState> far_;
};

} // namespace terra::world
