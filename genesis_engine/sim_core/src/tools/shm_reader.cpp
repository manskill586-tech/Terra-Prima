#include "ipc/SharedMemoryBridge.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

using genesis::ipc::SharedMemoryBridge;
using genesis::shared::SimStateBuffer;
using genesis::shared::TryReadSnapshot;
using genesis::shared::WorldState;

namespace {

struct Options {
  std::string name = genesis::shared::kDefaultShmName;
  int polls = 200;
  int sleep_ms = 16;
};

Options ParseArgs(int argc, char** argv) {
  Options opts;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--name" && i + 1 < argc) {
      opts.name = argv[++i];
    } else if (arg == "--polls" && i + 1 < argc) {
      opts.polls = std::stoi(argv[++i]);
    } else if (arg == "--sleep-ms" && i + 1 < argc) {
      opts.sleep_ms = std::stoi(argv[++i]);
    }
  }
  return opts;
}

} // namespace

int main(int argc, char** argv) {
  const Options opts = ParseArgs(argc, argv);
  const char* env_name = std::getenv("GENESIS_SHM_NAME");
  const std::string shm_name = env_name && *env_name ? env_name : opts.name;
  SharedMemoryBridge shm(shm_name.c_str(), false);
  if (!shm.IsValid()) {
    std::cerr << "Failed to open shared memory: " << shm_name << "\n";
    return 1;
  }

  const SimStateBuffer* buffer = shm.Buffer();
  WorldState snapshot{};
  uint64_t frame_id = 0;

  for (int i = 0; i < opts.polls; ++i) {
    if (TryReadSnapshot(buffer, &snapshot, &frame_id)) {
      std::cout << "frame_id=" << frame_id
                << " sim_time_us=" << buffer->header.sim_time_us
                << " near_chunks=" << snapshot.near_chunk_count
                << " near_particles=" << snapshot.near_particles.count
                << " organisms=" << snapshot.organism_count
                << "\n";
    }
    if (opts.sleep_ms > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(opts.sleep_ms));
    }
  }

  return 0;
}
