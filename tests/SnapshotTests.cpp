#include "terra/io/SnapshotStore.h"
#include "terra/world/World.h"

#include <cmath>

extern void Expect(bool cond, const char* expr, const char* file, int line);
#define EXPECT_TRUE(x) Expect((x), #x, __FILE__, __LINE__)

namespace {

bool NearlyEqual(float a, float b, float eps = 1e-5f) {
  return std::abs(a - b) <= eps;
}

bool NearlyEqual(double a, double b, double eps = 1e-9) {
  return std::abs(a - b) <= eps;
}

bool ChemEqual(const terra::chem::ChemState& a, const terra::chem::ChemState& b) {
  if (!NearlyEqual(a.temperature, b.temperature)) return false;
  if (!NearlyEqual(a.pressure, b.pressure)) return false;
  if (!NearlyEqual(a.pH, b.pH)) return false;
  if (!NearlyEqual(a.eh, b.eh)) return false;
  if (!NearlyEqual(a.ionicStrength, b.ionicStrength)) return false;
  if (a.dormant != b.dormant) return false;
  if (a.species.size() != b.species.size()) return false;
  for (std::size_t i = 0; i < a.species.size(); ++i) {
    if (a.species[i].id != b.species[i].id) return false;
    if (!NearlyEqual(a.species[i].concentration, b.species[i].concentration)) return false;
  }
  return true;
}

bool NearEqual(const terra::sim::NearChunkData& a, const terra::sim::NearChunkData& b) {
  if (!NearlyEqual(a.temperature, b.temperature)) return false;
  if (!NearlyEqual(a.humidity, b.humidity)) return false;
  for (int i = 0; i < 4; ++i) {
    if (!NearlyEqual(a.concentrations[i], b.concentrations[i])) return false;
  }
  if (a.lastUpdatedStep != b.lastUpdatedStep) return false;
  if (!NearlyEqual(a.lastSimTime, b.lastSimTime)) return false;
  if (a.dirty != b.dirty) return false;
  if (!ChemEqual(a.chem, b.chem)) return false;
  return true;
}

bool MidEqual(const terra::sim::MidChunkData& a, const terra::sim::MidChunkData& b) {
  if (!NearlyEqual(a.temperature, b.temperature)) return false;
  if (!NearlyEqual(a.humidity, b.humidity)) return false;
  if (!NearlyEqual(a.biomass, b.biomass)) return false;
  if (a.lastUpdatedStep != b.lastUpdatedStep) return false;
  if (!NearlyEqual(a.lastSimTime, b.lastSimTime)) return false;
  if (a.dirty != b.dirty) return false;
  if (!ChemEqual(a.chem, b.chem)) return false;
  return true;
}

bool FarEqual(const terra::sim::FarTileData& a, const terra::sim::FarTileData& b) {
  if (!NearlyEqual(a.avgTemperature, b.avgTemperature)) return false;
  if (!NearlyEqual(a.co2, b.co2)) return false;
  if (!NearlyEqual(a.population, b.population)) return false;
  if (a.lastUpdatedStep != b.lastUpdatedStep) return false;
  if (!NearlyEqual(a.lastSimTime, b.lastSimTime)) return false;
  if (a.dirty != b.dirty) return false;
  if (!ChemEqual(a.chem, b.chem)) return false;
  return true;
}

bool SnapshotEqual(const terra::io::WorldSnapshot& a, const terra::io::WorldSnapshot& b) {
  if (!NearlyEqual(a.simTime, b.simTime)) return false;
  if (a.near.size() != b.near.size()) return false;
  if (a.mid.size() != b.mid.size()) return false;
  if (a.far.size() != b.far.size()) return false;

  for (const auto& [key, value] : a.near) {
    auto it = b.near.find(key);
    if (it == b.near.end() || !NearEqual(value, it->second)) return false;
  }
  for (const auto& [key, value] : a.mid) {
    auto it = b.mid.find(key);
    if (it == b.mid.end() || !MidEqual(value, it->second)) return false;
  }
  for (const auto& [key, value] : a.far) {
    auto it = b.far.find(key);
    if (it == b.far.end() || !FarEqual(value, it->second)) return false;
  }
  return true;
}

} // namespace

void RunSnapshotTests() {
  terra::io::SnapshotStore store;
  terra::io::WorldSnapshot snapshot{};
  snapshot.simTime = 42.0;

  terra::world::ChunkKey key{0, 1, 2, 3};
  terra::sim::NearChunkData near{};
  near.temperature = 290.0f;
  near.humidity = 0.6f;
  near.concentrations = {0.1f, 0.2f, 0.3f, 0.4f};
  near.lastUpdatedStep = 7;
  near.lastSimTime = 12.5;
  near.dirty = true;
  near.chem.temperature = 301.0;
  near.chem.pressure = 95000.0;
  near.chem.pH = 6.8;
  near.chem.eh = -0.2;
  near.chem.ionicStrength = 0.1;
  near.chem.dormant = false;
  near.chem.species.push_back({1, 0.15f});
  near.chem.species.push_back({3, 0.05f});
  snapshot.near.emplace(key, near);

  terra::sim::MidChunkData mid{};
  mid.temperature = 285.0f;
  mid.humidity = 0.4f;
  mid.biomass = 0.2f;
  mid.lastUpdatedStep = 5;
  mid.lastSimTime = 10.0;
  mid.dirty = false;
  mid.chem.temperature = 289.0;
  mid.chem.pressure = 98000.0;
  mid.chem.pH = 7.2;
  mid.chem.eh = 0.05;
  mid.chem.ionicStrength = 0.2;
  mid.chem.dormant = true;
  mid.chem.species.push_back({2, 0.2f});
  snapshot.mid.emplace(key, mid);

  terra::sim::FarTileData far{};
  far.avgTemperature = 278.0f;
  far.co2 = 0.0003f;
  far.population = 0.8f;
  far.lastUpdatedStep = 3;
  far.lastSimTime = 8.0;
  far.dirty = true;
  far.chem.temperature = 275.0;
  far.chem.pressure = 100000.0;
  far.chem.pH = 7.8;
  far.chem.eh = 0.1;
  far.chem.ionicStrength = 0.05;
  far.chem.dormant = false;
  far.chem.species.push_back({5, 0.3f});
  snapshot.far.emplace(key, far);

  store.SaveSnapshot("snapshot_test.bin", snapshot);
  auto loaded = store.LoadSnapshot("snapshot_test.bin");
  EXPECT_TRUE(SnapshotEqual(snapshot, loaded));

  store.SaveSnapshotJson("snapshot_test.json", snapshot);
  auto loadedJson = store.LoadSnapshotJson("snapshot_test.json");
  EXPECT_TRUE(SnapshotEqual(snapshot, loadedJson));

  terra::io::WorldSnapshot current = snapshot;
  auto it = current.near.find(key);
  if (it != current.near.end()) {
    it->second.temperature += 1.0f;
  }
  terra::world::ChunkKey newKey{1, 0, 0, 2};
  current.mid.emplace(newKey, mid);
  current.far.erase(key);
  current.simTime = 45.0;

  auto delta = terra::io::CreateDelta(snapshot, current);
  auto baseCopy = snapshot;
  terra::io::ApplyDelta(baseCopy, delta);
  EXPECT_TRUE(SnapshotEqual(baseCopy, current));

  terra::world::World worldA;
  terra::world::World worldB;
  worldA.SetSeed(1234);
  worldB.SetSeed(1234);
  const double dt = 1.0 / 60.0;
  for (int i = 0; i < 20; ++i) {
    worldA.Update(dt);
    worldB.Update(dt);
  }
  auto snapA = worldA.CreateSnapshot();
  auto snapB = worldB.CreateSnapshot();
  EXPECT_TRUE(SnapshotEqual(snapA, snapB));
}
