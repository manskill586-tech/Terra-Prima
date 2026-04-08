#include "terra/world/World.h"

#include <chrono>
#include <iostream>
#include <thread>

int main() {
  terra::world::World world;

  const int frames = 120;
  const double dt = 1.0 / 60.0;

  for (int i = 0; i < frames; ++i) {
    world.Update(dt);
    const auto metrics = world.Metrics();
    std::cout << "Frame " << i
              << " | SimTime " << metrics.simTime
              << " | Near " << metrics.activeNear
              << " Mid " << metrics.activeMid
              << " Far " << metrics.activeFar
              << " | NearMs " << metrics.lastNearMs
              << " MidMs " << metrics.lastMidMs
              << " FarMs " << metrics.lastFarMs
              << '\n';

    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  auto snapshot = world.CreateSnapshot();
  terra::io::SnapshotStore store;
  auto future = store.SaveSnapshotAsync("snapshot.tpra", std::move(snapshot));
  future.wait();

  std::cout << "Snapshot saved: snapshot.tpra" << '\n';
  return 0;
}
