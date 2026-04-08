#pragma once

#include <cstddef>
#include <cstdint>

namespace terra::world {

struct ChunkKey {
  int face{0};
  int x{0};
  int y{0};
  int lod{0};

  bool operator==(const ChunkKey& other) const {
    return face == other.face && x == other.x && y == other.y && lod == other.lod;
  }
};

struct ChunkKeyHasher {
  std::size_t operator()(const ChunkKey& key) const noexcept {
    std::size_t h = 1469598103934665603ULL;
    h ^= static_cast<std::size_t>(key.face) + 0x9e3779b97f4a7c15ULL + (h << 6U) + (h >> 2U);
    h ^= static_cast<std::size_t>(key.x) + 0x9e3779b97f4a7c15ULL + (h << 6U) + (h >> 2U);
    h ^= static_cast<std::size_t>(key.y) + 0x9e3779b97f4a7c15ULL + (h << 6U) + (h >> 2U);
    h ^= static_cast<std::size_t>(key.lod) + 0x9e3779b97f4a7c15ULL + (h << 6U) + (h >> 2U);
    return h;
  }
};

} // namespace terra::world
