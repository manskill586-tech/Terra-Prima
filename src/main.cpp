#include "terra/world/World.h"
#include "terra/io/SnapshotStore.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

struct Options {
  int steps = 120;
  std::string savePath;
  std::string saveJsonPath;
  std::string loadPath;
  std::string deltaFromPath;
  std::string deltaOutPath;
  uint64_t seed = 0;
  bool seedSet = false;
};

Options ParseArgs(int argc, char** argv) {
  Options opts;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    auto next = [&](std::string& out) {
      if (i + 1 < argc) {
        out = argv[++i];
      }
    };
    if (arg == "--steps" && i + 1 < argc) {
      opts.steps = std::stoi(argv[++i]);
    } else if (arg == "--save") {
      next(opts.savePath);
    } else if (arg == "--save-json") {
      next(opts.saveJsonPath);
    } else if (arg == "--load") {
      next(opts.loadPath);
    } else if (arg == "--delta-from") {
      next(opts.deltaFromPath);
    } else if (arg == "--delta-out") {
      next(opts.deltaOutPath);
    } else if (arg == "--seed" && i + 1 < argc) {
      opts.seed = static_cast<uint64_t>(std::stoull(argv[++i]));
      opts.seedSet = true;
    }
  }
  return opts;
}

int main(int argc, char** argv) {
  Options opts = ParseArgs(argc, argv);

  terra::world::World world;
  if (opts.seedSet) {
    world.SetSeed(opts.seed);
  }

  terra::io::SnapshotStore store;
  terra::io::WorldSnapshot baseSnapshot{};
  bool hasBaseSnapshot = false;

  if (!opts.deltaFromPath.empty()) {
    baseSnapshot = store.LoadSnapshot(opts.deltaFromPath);
    world.ApplySnapshot(baseSnapshot);
    hasBaseSnapshot = true;
  } else if (!opts.loadPath.empty()) {
    auto snapshot = store.LoadSnapshot(opts.loadPath);
    world.ApplySnapshot(snapshot);
  }

  if (!opts.deltaOutPath.empty() && !hasBaseSnapshot) {
    baseSnapshot = world.CreateSnapshot();
    hasBaseSnapshot = true;
  }

  const double dt = 1.0 / 60.0;
  for (int i = 0; i < opts.steps; ++i) {
    world.Update(dt);
    const auto metrics = world.Metrics();
    std::cout << "Frame " << i
              << " | SimTime " << metrics.simTime
              << " | Near/Mid/Far " << metrics.activeNear << "/" << metrics.activeMid << "/" << metrics.activeFar
              << " | Steps " << metrics.nearSteps << "/" << metrics.midSteps << "/" << metrics.farSteps
              << " | NearMs " << metrics.lastNearMs
              << " MidMs " << metrics.lastMidMs
              << " FarMs " << metrics.lastFarMs
              << '\n';

    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  auto snapshot = world.CreateSnapshot();
  if (!opts.savePath.empty()) {
    store.SaveSnapshot(opts.savePath, snapshot);
  }
  if (!opts.saveJsonPath.empty()) {
    store.SaveSnapshotJson(opts.saveJsonPath, snapshot);
  }
  if (!opts.deltaOutPath.empty() && hasBaseSnapshot) {
    auto delta = world.CreateDelta(baseSnapshot);
    store.SaveDelta(opts.deltaOutPath, delta);
  }

  return 0;
}
