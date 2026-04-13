#include "ipc/SharedMemoryBridge.h"
#include "persistence/Snapshot.h"
#include "sim/ParticleSystem.h"
#include "sim/WorldSim.h"
#include "terra/chem/ChemDB.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using genesis::ipc::SharedMemoryBridge;
using genesis::persistence::ApplySnapshot;
using genesis::persistence::CreateDelta;
using genesis::persistence::DeltaSnapshot;
using genesis::persistence::LoadSnapshot;
using genesis::persistence::SaveDelta;
using genesis::persistence::SaveSnapshot;
using genesis::persistence::Snapshot;
using genesis::shared::BeginWrite;
using genesis::shared::EndWrite;
using genesis::shared::SimStateBuffer;
using genesis::shared::WorldState;
using genesis::sim::ParticleSystem;
using genesis::sim::ParticleSystemConfig;
using genesis::sim::WorldSim;
using terra::chem::ChemDB;
using terra::chem::ChemDBConfig;

namespace {

struct Options {
  std::string name = genesis::shared::kDefaultShmName;
  int frames = 0; // 0 = run forever
  int fps = 60;
  int near_particles = 50000;
  uint64_t seed = 1337;
  int steps = 0;
  std::string save_path;
  std::string load_path;
  std::string delta_from;
  std::string delta_out;
  std::string chem_root;
  std::string chem_cache;
  bool chem_rebuild = false;
  int chem_seed = 8;
  double chem_heat_scale = 1e-5;
  bool particle_from_chem = false;
  bool visual_test = false;
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
      opts.near_particles = std::max(1, std::stoi(argv[++i]));
    } else if (arg == "--near-particles" && i + 1 < argc) {
      opts.near_particles = std::max(1, std::stoi(argv[++i]));
    } else if (arg == "--seed" && i + 1 < argc) {
      opts.seed = static_cast<uint64_t>(std::stoull(argv[++i]));
    } else if (arg == "--steps" && i + 1 < argc) {
      opts.steps = std::max(0, std::stoi(argv[++i]));
    } else if (arg == "--save" && i + 1 < argc) {
      opts.save_path = argv[++i];
    } else if (arg == "--load" && i + 1 < argc) {
      opts.load_path = argv[++i];
    } else if (arg == "--delta-from" && i + 1 < argc) {
      opts.delta_from = argv[++i];
    } else if (arg == "--delta-out" && i + 1 < argc) {
      opts.delta_out = argv[++i];
    } else if (arg == "--chem-root" && i + 1 < argc) {
      opts.chem_root = argv[++i];
    } else if (arg == "--chem-cache" && i + 1 < argc) {
      opts.chem_cache = argv[++i];
    } else if (arg == "--chem-rebuild") {
      opts.chem_rebuild = true;
    } else if (arg == "--chem-seed" && i + 1 < argc) {
      opts.chem_seed = std::max(0, std::stoi(argv[++i]));
    } else if (arg == "--chem-heat-scale" && i + 1 < argc) {
      opts.chem_heat_scale = std::stod(argv[++i]);
    } else if (arg == "--particle-from-chem") {
      opts.particle_from_chem = true;
    } else if (arg == "--visual-test") {
      opts.visual_test = true;
    }
  }
  return opts;
}

std::string ResolveChemRoot(const std::string& override_path) {
  namespace fs = std::filesystem;
  if (!override_path.empty()) {
    return override_path;
  }
  const std::vector<std::string> candidates = {
      "data/elements",
      "../data/elements",
      "../../data/elements",
  };
  for (const auto& path : candidates) {
    if (fs::exists(path)) {
      return path;
    }
  }
  return "data/elements";
}

} // namespace

int main(int argc, char** argv) {
  const Options opts = ParseArgs(argc, argv);
  const char* env_name = std::getenv("GENESIS_SHM_NAME");
  const std::string shm_name = env_name && *env_name ? env_name : opts.name;

  WorldSim world(genesis::world::WorldConfig{});
  world.SetSeed(opts.seed);

  ChemDB chem_db;
  const std::string chem_root = ResolveChemRoot(opts.chem_root);
  ChemDBConfig chem_config;
  chem_config.dataRoot = chem_root;
  chem_config.cachePath = opts.chem_cache.empty() ? (chem_root + "/chemdb.bin") : opts.chem_cache;
  if (opts.chem_rebuild) {
    if (!ChemDB::BuildCacheFromRaw(chem_config, chem_db)) {
      std::cerr << "Failed to build chem cache from raw data.\n";
      return 1;
    }
    if (!chem_config.cachePath.empty()) {
      chem_db.SaveCache(chem_config.cachePath);
    }
  } else if (!chem_db.LoadOrBuild(chem_config)) {
    std::cerr << "Failed to load or build chem database.\n";
    return 1;
  }

  genesis::sim::ChemConfig world_chem;
  world_chem.seed_species = opts.chem_seed;
  world_chem.seed_concentration = 1.0f;
  world_chem.heat_scale = opts.chem_heat_scale;
  world_chem.near_config.maxReactions = 64;
  world_chem.mid_config.maxReactions = 8;
  world.SetChemistry(&chem_db, world_chem);
  world.SetVisualTest(opts.visual_test);

  Snapshot base_snapshot;
  bool has_base_snapshot = false;

  if (!opts.load_path.empty()) {
    Snapshot loaded;
    if (!LoadSnapshot(opts.load_path, loaded)) {
      std::cerr << "Failed to load snapshot: " << opts.load_path << "\n";
      return 1;
    }
    uint64_t seed = world.seed();
    double sim_time = world.sim_time();
    ApplySnapshot(loaded, seed, sim_time, world.store());
    world.SetSeed(seed);
    world.SetSimTime(sim_time);
    world.ResetScheduler();
  }

  if (!opts.delta_from.empty()) {
    if (!LoadSnapshot(opts.delta_from, base_snapshot)) {
      std::cerr << "Failed to load delta base snapshot: " << opts.delta_from << "\n";
      return 1;
    }
    has_base_snapshot = true;
  }

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
  const int particle_count = std::min(opts.near_particles, max_particles);
  const double frame_dt = 1.0 / static_cast<double>(opts.fps);

  ParticleSystemConfig particle_config;
  particle_config.count = particle_count;
  particle_config.use_chem_species = opts.particle_from_chem;
  ParticleSystem particles(particle_config);
  particles.SetChemDB(&chem_db);
  particles.Initialize(opts.seed, world.store().config().world_size_m * 0.5f);

  int frame = 0;
  const int frame_limit = opts.steps > 0 ? opts.steps : opts.frames;
  while (frame_limit == 0 || frame < frame_limit) {
    world.StepFrame(frame_dt);
    particles.Step(frame_dt, world.store(), opts.seed);
    uint32_t write_index = 0;
    WorldState* world_state = BeginWrite(buffer, &write_index);
    world.WriteShmSnapshot(*world_state);
    particles.WriteToShm(world_state->near_particles);

    buffer->header.sim_time_us = static_cast<uint64_t>(world.sim_time() * 1'000'000.0);
    buffer->header.active_near_chunks = world_state->near_chunk_count;
    buffer->header.active_organisms = world_state->organism_count;

    EndWrite(buffer, write_index);

    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(frame_dt * 1000.0)));
    frame++;
  }

  const Snapshot snapshot = genesis::persistence::CaptureSnapshot(world.seed(), world.sim_time(), world.store());

  if (!opts.save_path.empty()) {
    if (!SaveSnapshot(opts.save_path, snapshot)) {
      std::cerr << "Failed to save snapshot: " << opts.save_path << "\n";
      return 1;
    }
  }

  if (!opts.delta_out.empty()) {
    if (!has_base_snapshot) {
      std::cerr << "Delta requested but --delta-from was not provided.\n";
      return 1;
    }
    const DeltaSnapshot delta = CreateDelta(base_snapshot, snapshot);
    if (!SaveDelta(opts.delta_out, delta)) {
      std::cerr << "Failed to save delta: " << opts.delta_out << "\n";
      return 1;
    }
  }

  return 0;
}
