#include "ipc/SharedMemoryBridge.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <string>
#include <thread>

using genesis::ipc::SharedMemoryBridge;
using genesis::shared::BeginWrite;
using genesis::shared::EndWrite;
using genesis::shared::SimStateBuffer;
using genesis::shared::WorldState;

namespace {

struct Options {
  std::string name = genesis::shared::kDefaultShmName;
  int frames = 0; // 0 = run forever
  int fps = 60;
  int particles = 50000;
  uint64_t seed = 1337;
};

Options ParseArgs(int argc, char** argv) {
  Options opts;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--name" && i + 1 < argc) {
      opts.name = argv[++i];
    } else if (arg == "--frames" && i + 1 < argc) {
      opts.frames = std::stoi(argv[++i]);
    } else if (arg == "--fps" && i + 1 < argc) {
      opts.fps = std::max(1, std::stoi(argv[++i]));
    } else if (arg == "--particles" && i + 1 < argc) {
      opts.particles = std::max(1, std::stoi(argv[++i]));
    } else if (arg == "--seed" && i + 1 < argc) {
      opts.seed = static_cast<uint64_t>(std::stoull(argv[++i]));
    }
  }
  return opts;
}

} // namespace

int main(int argc, char** argv) {
  const Options opts = ParseArgs(argc, argv);
  const char* env_name = std::getenv("GENESIS_SHM_NAME");
  const std::string shm_name = env_name && *env_name ? env_name : opts.name;

  SharedMemoryBridge shm(shm_name.c_str(), true);
  if (!shm.IsValid()) {
    std::cerr << "Failed to create shared memory: " << shm_name << "\n";
    return 1;
  }

  SimStateBuffer* buffer = shm.Buffer();
  buffer->header.write_idx.store(0, std::memory_order_relaxed);
  buffer->header.read_idx.store(0, std::memory_order_relaxed);
  buffer->header.frame_id.store(0, std::memory_order_relaxed);
  buffer->header.frame_ready.store(false, std::memory_order_relaxed);

  const int max_particles = static_cast<int>(GENESIS_SHM_MAX_NEAR_PARTICLES);
  const int particle_count = std::min(opts.particles, max_particles);
  const double dt = 1.0 / static_cast<double>(opts.fps);
  const uint64_t step_us = static_cast<uint64_t>(1'000'000.0 * dt);

  int frame = 0;
  while (opts.frames == 0 || frame < opts.frames) {
    uint32_t write_index = 0;
    WorldState* world = BeginWrite(buffer, &write_index);

    world->near_chunk_count = 1;
    world->mid_chunk_count = 0;
    world->far_chunk_count = 0;
    world->near_chunks[0].coord = {0, 0, 0};
    world->near_chunks[0].temperature = 288.0f + static_cast<float>(frame) * 0.01f;
    world->near_chunks[0].pressure = 101325.0f;
    world->near_chunks[0].particle_count = static_cast<uint32_t>(particle_count);
    world->near_chunks[0].reaction_count = 0;

    world->near_particles.count = static_cast<uint32_t>(particle_count);
    const float time = static_cast<float>(frame) * static_cast<float>(dt);
    const float base = static_cast<float>(opts.seed % 1000) * 0.001f;
    for (int i = 0; i < particle_count; ++i) {
      const float angle = base + 0.02f * static_cast<float>(i) + time * 0.8f;
      const float radius = 1.0f + (i % 100) * 0.01f;
      const float z = (i % 50) * 0.02f;
      world->near_particles.px[i] = std::cos(angle) * radius;
      world->near_particles.py[i] = std::sin(angle) * radius;
      world->near_particles.pz[i] = z;
      world->near_particles.vx[i] = -std::sin(angle) * radius * 0.8f;
      world->near_particles.vy[i] = std::cos(angle) * radius * 0.8f;
      world->near_particles.vz[i] = 0.0f;
      world->near_particles.element_id[i] = static_cast<uint16_t>(1 + (i % 6));
      world->near_particles.flags[i] = 0;
    }

    world->organism_count = 0;

    buffer->header.sim_time_us = static_cast<uint64_t>(frame) * step_us;
    buffer->header.active_near_chunks = world->near_chunk_count;
    buffer->header.active_organisms = world->organism_count;

    EndWrite(buffer, write_index);

    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(dt * 1000.0)));
    frame++;
  }

  return 0;
}
