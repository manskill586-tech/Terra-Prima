#pragma once

#include <cstdint>
#include <cstddef>

namespace genesis::world {

enum class Tier : uint8_t { Near = 0, Mid = 1, Far = 2 };

struct ChunkCoord {
  int32_t x{0};
  int32_t y{0};
  int32_t z{0};
};

inline bool operator==(const ChunkCoord& a, const ChunkCoord& b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

struct FieldState {
  float temperature{288.0f};
  float pressure{101325.0f};
  float humidity{0.5f};
};

struct VisualProps {
  uint32_t dominant_species_id{0};
  uint8_t phase{0}; // 0=gas,1=liquid,2=solid,3=plasma
  float density{0.0f};
  float hardness{0.0f};
  float viscosity{0.0f};
  float compressibility{0.0f};
  float porosity{0.0f};
  float opacity{1.0f};
  float roughness{0.5f};
  float metallic{0.0f};
  float scale{1.0f}; // relative scale (0..1) of chunk size
  float emissive{0.0f};
  uint32_t albedo_rgba{0};
};

inline constexpr FieldState kAmbientField{};

inline uint64_t Mix64(uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

struct ChunkCoordHash {
  size_t operator()(const ChunkCoord& c) const {
    uint64_t h = 0;
    h ^= Mix64(static_cast<uint64_t>(static_cast<uint32_t>(c.x)));
    h ^= Mix64(static_cast<uint64_t>(static_cast<uint32_t>(c.y)) << 1);
    h ^= Mix64(static_cast<uint64_t>(static_cast<uint32_t>(c.z)) << 2);
    return static_cast<size_t>(h);
  }
};

} // namespace genesis::world
