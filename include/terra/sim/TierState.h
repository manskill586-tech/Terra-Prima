#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace terra::sim {

struct NearChunkData {
  float temperature{288.0f};
  float humidity{0.5f};
  std::array<float, 4> concentrations{{0.25f, 0.25f, 0.25f, 0.25f}};
  uint32_t lastUpdatedStep{0};
};

struct MidChunkData {
  float temperature{285.0f};
  float humidity{0.4f};
  float biomass{0.1f};
  uint32_t lastUpdatedStep{0};
};

struct FarTileData {
  float avgTemperature{280.0f};
  float co2{0.0004f};
  float population{0.0f};
  uint32_t lastUpdatedStep{0};
};

} // namespace terra::sim
