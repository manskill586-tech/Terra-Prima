#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>

// Tunable limits for the shared-memory "viewport snapshot".
// Keep these small for development; scale up per target machine.
#ifndef GENESIS_MAX_MOLECULE_TYPES
#define GENESIS_MAX_MOLECULE_TYPES 256
#endif

#ifndef GENESIS_SHM_MAX_NEAR_CHUNKS
#define GENESIS_SHM_MAX_NEAR_CHUNKS 4096
#endif

#ifndef GENESIS_SHM_MAX_MID_CHUNKS
#define GENESIS_SHM_MAX_MID_CHUNKS 8192
#endif

#ifndef GENESIS_SHM_MAX_FAR_CHUNKS
#define GENESIS_SHM_MAX_FAR_CHUNKS 2048
#endif

#ifndef GENESIS_SHM_MAX_NEAR_PARTICLES
#define GENESIS_SHM_MAX_NEAR_PARTICLES 200000
#endif

#ifndef GENESIS_SHM_MAX_ORGANISMS
#define GENESIS_SHM_MAX_ORGANISMS 50000
#endif

namespace genesis::shared {

inline constexpr const char* kDefaultShmName = "GenesisSim";

struct ChunkCoord {
  int32_t x{0};
  int32_t y{0};
  int32_t z{0};
};

struct ChunkData {
  ChunkCoord coord{};
  uint32_t particle_count{0};
  uint32_t reaction_count{0};
  float temperature{0.0f};
  float pressure{0.0f};
  float concentrations[GENESIS_MAX_MOLECULE_TYPES]{};
  uint8_t lod_level{0}; // 0=Near,1=Mid,2=Far
};

struct NearParticles {
  uint32_t count{0};
  float px[GENESIS_SHM_MAX_NEAR_PARTICLES]{};
  float py[GENESIS_SHM_MAX_NEAR_PARTICLES]{};
  float pz[GENESIS_SHM_MAX_NEAR_PARTICLES]{};
  float vx[GENESIS_SHM_MAX_NEAR_PARTICLES]{};
  float vy[GENESIS_SHM_MAX_NEAR_PARTICLES]{};
  float vz[GENESIS_SHM_MAX_NEAR_PARTICLES]{};
  uint16_t element_id[GENESIS_SHM_MAX_NEAR_PARTICLES]{};
  uint8_t flags[GENESIS_SHM_MAX_NEAR_PARTICLES]{};
};

struct OrganismRef {
  uint64_t genome_hash{0};
  uint32_t genome_offset{0};
  uint16_t genome_size_kb{0};
  uint8_t lod_level{0};
  uint8_t flags{0};
  float x{0.0f};
  float y{0.0f};
  float z{0.0f};
  uint16_t species_id{0};
  uint16_t neural_net_id{0};
};

struct WorldState {
  uint32_t near_chunk_count{0};
  uint32_t mid_chunk_count{0};
  uint32_t far_chunk_count{0};

  ChunkData near_chunks[GENESIS_SHM_MAX_NEAR_CHUNKS]{};
  ChunkData mid_chunks[GENESIS_SHM_MAX_MID_CHUNKS]{};
  ChunkData far_chunks[GENESIS_SHM_MAX_FAR_CHUNKS]{};

  NearParticles near_particles{};

  uint32_t organism_count{0};
  OrganismRef organisms[GENESIS_SHM_MAX_ORGANISMS]{};
};

struct SHMHeader {
  std::atomic<uint64_t> frame_id{0}; // odd=writing, even=ready
  std::atomic<uint32_t> write_idx{0};
  std::atomic<uint32_t> read_idx{0};
  std::atomic<bool> frame_ready{false};

  uint64_t sim_time_us{0};
  uint32_t active_near_chunks{0};
  uint32_t active_organisms{0};
};

struct SimStateBuffer {
  SHMHeader header{};
  WorldState buffers[2]{};
};

inline WorldState* BeginWrite(SimStateBuffer* buffer, uint32_t* out_index) {
  auto& header = buffer->header;
  const uint32_t w = header.write_idx.load(std::memory_order_relaxed);
  header.frame_id.fetch_add(1, std::memory_order_release); // begin (odd)
  if (out_index) {
    *out_index = w;
  }
  return &buffer->buffers[w];
}

inline void EndWrite(SimStateBuffer* buffer, uint32_t write_index) {
  auto& header = buffer->header;
  header.frame_id.fetch_add(1, std::memory_order_release); // end (even)
  header.read_idx.store(write_index, std::memory_order_release);
  header.write_idx.store(write_index ^ 1U, std::memory_order_relaxed);
  header.frame_ready.store(true, std::memory_order_release);
}

inline bool TryReadSnapshot(const SimStateBuffer* buffer, WorldState* out, uint64_t* out_frame_id = nullptr) {
  const auto& header = buffer->header;
  const uint64_t id_before = header.frame_id.load(std::memory_order_acquire);
  if (id_before & 1ULL) {
    return false; // writer active
  }
  const uint32_t r = header.read_idx.load(std::memory_order_acquire);
  if (out) {
    std::memcpy(out, &buffer->buffers[r], sizeof(WorldState));
  }
  const uint64_t id_after = header.frame_id.load(std::memory_order_acquire);
  if (id_before != id_after) {
    return false;
  }
  if (out_frame_id) {
    *out_frame_id = id_after;
  }
  return true;
}

} // namespace genesis::shared
