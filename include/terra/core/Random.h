#pragma once

#include <cstdint>

namespace terra::core {

inline uint64_t SplitMix64(uint64_t x) {
  uint64_t z = x + 0x9E3779B97F4A7C15ULL;
  z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31U);
}

inline uint64_t HashCombine(uint64_t a, uint64_t b) {
  return SplitMix64(a ^ (b + 0x9E3779B97F4A7C15ULL + (a << 6U) + (a >> 2U)));
}

} // namespace terra::core
