#include "terra/world/World.h"

#include "terra/core/Profiler.h"
#include "terra/core/Random.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <thread>
#include <utility>
#include <vector>

namespace terra::world {

namespace {

double NowMs() {
  const auto now = std::chrono::high_resolution_clock::now();
  const auto ms = std::chrono::duration<double, std::milli>(now.time_since_epoch());
  return ms.count();
}

uint64_t HashFor(const ChunkKey& key, uint64_t seed, uint64_t step, uint64_t salt) {
  uint64_t h = terra::core::HashCombine(seed, static_cast<uint64_t>(key.face));
  h = terra::core::HashCombine(h, static_cast<uint64_t>(key.x));
  h = terra::core::HashCombine(h, static_cast<uint64_t>(key.y));
  h = terra::core::HashCombine(h, static_cast<uint64_t>(key.lod));
  h = terra::core::HashCombine(h, step);
  h = terra::core::HashCombine(h, salt);
  return h;
}

float Random01(uint64_t value) {
  return static_cast<float>((value & 0xFFFFFF) / static_cast<double>(0xFFFFFF));
}

float NextRand01(uint64_t& state) {
  state = terra::core::SplitMix64(state);
  return Random01(state);
}

float Clamp01(float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

void SeedChemState(terra::chem::ChemState& state, const int* ids, std::size_t count, uint64_t seed) {
  state.species.clear();
  state.dormant = false;
  if (count == 0 || ids == nullptr) {
    return;
  }
  uint64_t rng = seed;
  const float minConc = 0.02f;
  const float maxConc = 0.12f;
  for (std::size_t i = 0; i < count; ++i) {
    const float t = NextRand01(rng);
    const float conc = minConc + (maxConc - minConc) * t;
    state.species.push_back({ids[i], conc});
  }
  state.Compact();
}

} // namespace

World::World()
    : chemDb_{},
      reactionEngine_(chemDb_),
      phaseModel_(chemDb_),
      jobSystem_(std::thread::hardware_concurrency()) {
  metrics_.frameBudgetMs = 16.0;
  clock_.SetTimeScale(10.0);

  chem::ChemDBConfig chemConfig{};
  chemConfig.dataRoot = "data/elements";
  if (!std::filesystem::exists(chemConfig.dataRoot)) {
    for (const auto& candidate : {std::filesystem::path("../data/elements"),
                                  std::filesystem::path("../../data/elements")}) {
      if (std::filesystem::exists(candidate)) {
        chemConfig.dataRoot = candidate.string();
        break;
      }
    }
  }
  chemConfig.cachePath = (std::filesystem::path(chemConfig.dataRoot) / "chemdb.bin").string();
  chemDb_.LoadOrBuild(chemConfig);
  chemSeedSpecies_ = chemDb_.DefaultSeedSpecies(16);

  camera_.nearCenter = {0, 0, 0, 6};
  camera_.midCenter = {0, 0, 0, 4};
  camera_.farCenter = {0, 0, 0, 2};
  camera_.nearRadius = 2;
  camera_.midRadius = 4;
  camera_.farRadius = 6;

  nearRuntime_.maxStepsPerFrame = 8;
  midRuntime_.maxStepsPerFrame = 4;
  farRuntime_.maxStepsPerFrame = 2;
  nearRuntime_.coarseMultiplier = 6.0;
  midRuntime_.coarseMultiplier = 4.0;
  farRuntime_.coarseMultiplier = 3.0;

  chunkStore_.UpdateActive(camera_);
}

void World::Update(double realDeltaSeconds) {
  clock_.Tick(realDeltaSeconds);
  const double simTime = clock_.SimTime();
  metrics_.simTime = simTime;
  const double deltaSim = simTime - lastSimTime_;
  lastSimTime_ = simTime;

  // Simple camera drift to exercise LOD streaming.
  if (static_cast<int>(simTime) % 10 == 0) {
    camera_.nearCenter.x += 1;
    camera_.midCenter.x = camera_.nearCenter.x / 2;
    camera_.farCenter.x = camera_.nearCenter.x / 4;
  }

  chunkStore_.UpdateActive(camera_);

  scheduler_.UpdateIntervals(metrics_.frameBudgetMs);
  nearRuntime_.stepSec = scheduler_.GetConfig(sim::TierId::Near).intervalSec;
  midRuntime_.stepSec = scheduler_.GetConfig(sim::TierId::Mid).intervalSec;
  farRuntime_.stepSec = scheduler_.GetConfig(sim::TierId::Far).intervalSec;

  nearRuntime_.accumulator += deltaSim;
  midRuntime_.accumulator += deltaSim;
  farRuntime_.accumulator += deltaSim;

  auto runTier = [&](TierRuntime& runtime, sim::TierId id, auto updateFn, double& outMs, int& outSteps) {
    const double start = NowMs();
    outSteps = 0;
    while (runtime.accumulator >= runtime.stepSec && outSteps < runtime.maxStepsPerFrame) {
      const uint64_t stepIndex = runtime.stepIndex++;
      updateFn(simTime, runtime.stepSec, stepIndex);
      runtime.accumulator -= runtime.stepSec;
      outSteps++;
    }
    if (runtime.accumulator >= runtime.stepSec * runtime.maxStepsPerFrame) {
      const double coarseStep = runtime.stepSec * runtime.coarseMultiplier;
      const uint64_t stepIndex = runtime.stepIndex++;
      updateFn(simTime, coarseStep, stepIndex);
      runtime.accumulator = std::max(0.0, runtime.accumulator - coarseStep);
      outSteps++;
    }
    outMs = NowMs() - start;
    scheduler_.OnTierUpdated(id, simTime, outMs);
  };

  runTier(nearRuntime_, sim::TierId::Near, [this](double t, double step, uint64_t idx) {
    UpdateNear(t, step, idx);
  }, metrics_.lastNearMs, metrics_.nearSteps);
  runTier(midRuntime_, sim::TierId::Mid, [this](double t, double step, uint64_t idx) {
    UpdateMid(t, step, idx);
  }, metrics_.lastMidMs, metrics_.midSteps);
  runTier(farRuntime_, sim::TierId::Far, [this](double t, double step, uint64_t idx) {
    UpdateFar(t, step, idx);
  }, metrics_.lastFarMs, metrics_.farSteps);

  const auto chunkMetrics = chunkStore_.Metrics();
  metrics_.activeNear = chunkMetrics.activeNear;
  metrics_.activeMid = chunkMetrics.activeMid;
  metrics_.activeFar = chunkMetrics.activeFar;
  metrics_.entitiesNear = entityLod_.NearCount();
  metrics_.entitiesMid = entityLod_.MidCount();
  metrics_.entitiesFar = entityLod_.FarCount();

  renderer_.BeginFrame();
  renderer_.DrawDebugText("SimTime: " + std::to_string(metrics_.simTime));
  renderer_.DrawDebugText("Active Near/Mid/Far: " + std::to_string(metrics_.activeNear) + "/" +
                           std::to_string(metrics_.activeMid) + "/" + std::to_string(metrics_.activeFar));
  renderer_.EndFrame();
}

WorldMetrics World::Metrics() const {
  return metrics_;
}

core::WorldClock& World::Clock() {
  return clock_;
}

void World::SetFrameBudgetMs(double ms) {
  metrics_.frameBudgetMs = ms;
}

sim::SimScheduler& World::Scheduler() {
  return scheduler_;
}

ChunkStore& World::Chunks() {
  return chunkStore_;
}

EntityLOD& World::Entities() {
  return entityLod_;
}

chem::ChemDB& World::ChemDatabase() {
  return chemDb_;
}

io::WorldSnapshot World::CreateSnapshot() const {
  io::WorldSnapshot snapshot{};
  snapshot.simTime = metrics_.simTime;
  snapshot.near = chunkStore_.NearChunks();
  snapshot.mid = chunkStore_.MidChunks();
  snapshot.far = chunkStore_.FarTiles();
  return snapshot;
}

void World::UpdateNear(double simTimeSec, double stepSec, uint64_t stepIndex) {
  ++simStep_;
  const auto& active = chunkStore_.ActiveNear();
  auto& nearMap = chunkStore_.NearChunks();

  std::array<int, 4> tracked{};
  const std::size_t trackedCount = std::min<std::size_t>(tracked.size(), chemSeedSpecies_.size());
  for (std::size_t i = 0; i < trackedCount; ++i) {
    tracked[i] = chemSeedSpecies_[i];
  }

  struct UpdateEntry {
    ChunkKey key;
    sim::NearChunkData data;
  };
  std::vector<UpdateEntry> updates;
  updates.reserve(active.size());

  for (const auto& key : active) {
    auto it = nearMap.find(key);
    if (it == nearMap.end()) {
      continue;
    }
    auto data = it->second;
    const double skipWindow = data.chem.dormant ? stepSec * 4.0 : stepSec * 2.0;
    if (!data.dirty && (simTimeSec - data.lastSimTime) < skipWindow) {
      continue;
    }

    if (data.chem.species.empty() && trackedCount > 0) {
      SeedChemState(data.chem, tracked.data(), trackedCount, HashFor(key, worldSeed_, stepIndex, 11));
    }
    data.chem.temperature = data.temperature;
    data.chem.pressure = 101325.0;

    std::array<float, 4> beforeConc{};
    for (std::size_t i = 0; i < trackedCount; ++i) {
      beforeConc[i] = data.chem.GetConcentration(tracked[i]);
    }
    const double beforePH = data.chem.pH;
    const double beforeEh = data.chem.eh;
    const bool beforeDormant = data.chem.dormant;

    auto config = reactionConfig_;
    config.maxReactions = 32;
    reactionEngine_.Step(data.chem, stepSec, HashFor(key, worldSeed_, stepIndex, 7), config);

    // Diffusion with 4-neighbors for tracked species.
    if (trackedCount > 0) {
      std::array<float, 4> neighborAvg{};
      int neighborCount = 1;
      for (std::size_t i = 0; i < trackedCount; ++i) {
        neighborAvg[i] = data.chem.GetConcentration(tracked[i]);
      }
      for (const auto& offset : std::array<std::pair<int, int>, 4>{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}}) {
        ChunkKey nk = key;
        nk.x += offset.first;
        nk.y += offset.second;
        auto nit = nearMap.find(nk);
        if (nit != nearMap.end()) {
          for (std::size_t i = 0; i < trackedCount; ++i) {
            neighborAvg[i] += nit->second.chem.GetConcentration(tracked[i]);
          }
          neighborCount++;
        }
      }
      for (std::size_t i = 0; i < trackedCount; ++i) {
        neighborAvg[i] /= static_cast<float>(neighborCount);
        const float current = data.chem.GetConcentration(tracked[i]);
        const float blended = current + (neighborAvg[i] - current) * 0.1f;
        data.chem.SetConcentration(tracked[i], blended);
      }
    }

    phaseModel_.Update(data.chem);
    data.chem.Compact();

    float chemDelta = 0.0f;
    for (std::size_t i = 0; i < trackedCount; ++i) {
      chemDelta += std::abs(data.chem.GetConcentration(tracked[i]) - beforeConc[i]);
    }
    chemDelta += static_cast<float>(std::abs(data.chem.pH - beforePH) + std::abs(data.chem.eh - beforeEh));
    if (data.chem.dormant != beforeDormant) {
      chemDelta += 0.1f;
    }

    const float heat = chemDelta * 4.0f;
    const float tempNoise = (Random01(HashFor(key, worldSeed_, stepIndex, 1)) - 0.5f) * 0.05f;
    data.temperature += heat + tempNoise;
    data.humidity = Clamp01(data.humidity - heat * 0.01f);
    data.chem.temperature = data.temperature;

    for (int i = 0; i < 4; ++i) {
      if (static_cast<std::size_t>(i) < trackedCount) {
        data.concentrations[i] = Clamp01(data.chem.GetConcentration(tracked[i]));
      } else {
        data.concentrations[i] = Clamp01(data.concentrations[i] * 0.98f);
      }
    }

    const float delta = std::abs(data.temperature - it->second.temperature) +
                        std::abs(data.humidity - it->second.humidity) +
                        chemDelta;
    data.lastUpdatedStep = static_cast<uint32_t>(stepIndex);
    data.lastSimTime = simTimeSec;
    data.dirty = delta > 1e-4f;
    updates.push_back({key, data});
  }

  for (const auto& update : updates) {
    nearMap[update.key] = update.data;
  }

  entityLod_.Update(static_cast<float>(stepSec));
}

void World::UpdateMid(double simTimeSec, double stepSec, uint64_t stepIndex) {
  const auto& active = chunkStore_.ActiveMid();
  auto& midMap = chunkStore_.MidChunks();

  std::array<int, 3> tracked{};
  const std::size_t trackedCount = std::min<std::size_t>(tracked.size(), chemSeedSpecies_.size());
  for (std::size_t i = 0; i < trackedCount; ++i) {
    tracked[i] = chemSeedSpecies_[i];
  }

  struct UpdateEntry {
    ChunkKey key;
    sim::MidChunkData data;
  };
  std::vector<UpdateEntry> updates;
  updates.reserve(active.size());

  for (const auto& key : active) {
    auto it = midMap.find(key);
    if (it == midMap.end()) {
      continue;
    }
    auto data = it->second;
    const double skipWindow = data.chem.dormant ? stepSec * 5.0 : stepSec * 2.5;
    if (!data.dirty && (simTimeSec - data.lastSimTime) < skipWindow) {
      continue;
    }

    if (data.chem.species.empty() && trackedCount > 0) {
      SeedChemState(data.chem, tracked.data(), trackedCount, HashFor(key, worldSeed_, stepIndex, 17));
    }
    data.chem.temperature = data.temperature;
    data.chem.pressure = 101325.0;
    std::array<float, 3> beforeConc{};
    for (std::size_t i = 0; i < trackedCount; ++i) {
      beforeConc[i] = data.chem.GetConcentration(tracked[i]);
    }
    const double beforePH = data.chem.pH;
    const double beforeEh = data.chem.eh;
    const bool beforeDormant = data.chem.dormant;

    if ((stepIndex % 3) == 0) {
      auto config = reactionConfig_;
      config.maxReactions = 12;
      reactionEngine_.Step(data.chem, stepSec, HashFor(key, worldSeed_, stepIndex, 13), config);
    }

    float tempAvg = data.temperature;
    float humAvg = data.humidity;
    float bioAvg = data.biomass;
    double phAvg = data.chem.pH;
    double ehAvg = data.chem.eh;
    int count = 1;
    for (const auto& offset : std::array<std::pair<int, int>, 4>{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}}) {
      ChunkKey nk = key;
      nk.x += offset.first;
      nk.y += offset.second;
      auto nit = midMap.find(nk);
      if (nit != midMap.end()) {
        tempAvg += nit->second.temperature;
        humAvg += nit->second.humidity;
        bioAvg += nit->second.biomass;
        phAvg += nit->second.chem.pH;
        ehAvg += nit->second.chem.eh;
        count++;
      }
    }
    tempAvg /= static_cast<float>(count);
    humAvg /= static_cast<float>(count);
    bioAvg /= static_cast<float>(count);
    phAvg /= static_cast<double>(count);
    ehAvg /= static_cast<double>(count);

    const float weatherNoise = (Random01(HashFor(key, worldSeed_, stepIndex, 2)) - 0.5f) * 0.05f;
    data.temperature += (tempAvg - data.temperature) * 0.15f + weatherNoise;
    data.humidity = Clamp01(data.humidity + (humAvg - data.humidity) * 0.2f);

    const float growth = 0.2f * data.biomass * (1.0f - data.biomass);
    const float stress = std::abs(data.temperature - 288.0f) * 0.0008f;
    data.biomass = Clamp01(data.biomass + (growth - stress) * static_cast<float>(stepSec));

    data.chem.pH += (phAvg - data.chem.pH) * 0.2;
    data.chem.eh += (ehAvg - data.chem.eh) * 0.2;

    if (trackedCount > 0) {
      std::array<float, 3> neighborAvg{};
      int neighborCount = 1;
      for (std::size_t i = 0; i < trackedCount; ++i) {
        neighborAvg[i] = data.chem.GetConcentration(tracked[i]);
      }
      for (const auto& offset : std::array<std::pair<int, int>, 4>{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}}) {
        ChunkKey nk = key;
        nk.x += offset.first;
        nk.y += offset.second;
        auto nit = midMap.find(nk);
        if (nit != midMap.end()) {
          for (std::size_t i = 0; i < trackedCount; ++i) {
            neighborAvg[i] += nit->second.chem.GetConcentration(tracked[i]);
          }
          neighborCount++;
        }
      }
      for (std::size_t i = 0; i < trackedCount; ++i) {
        neighborAvg[i] /= static_cast<float>(neighborCount);
        const float current = data.chem.GetConcentration(tracked[i]);
        data.chem.SetConcentration(tracked[i], current + (neighborAvg[i] - current) * 0.25f);
      }
    }

    phaseModel_.Update(data.chem);
    data.chem.Compact();
    data.chem.temperature = data.temperature;

    float chemDelta = 0.0f;
    for (std::size_t i = 0; i < trackedCount; ++i) {
      chemDelta += std::abs(data.chem.GetConcentration(tracked[i]) - beforeConc[i]);
    }
    chemDelta += static_cast<float>(std::abs(data.chem.pH - beforePH) + std::abs(data.chem.eh - beforeEh));
    if (data.chem.dormant != beforeDormant) {
      chemDelta += 0.1f;
    }

    const float delta = std::abs(data.temperature - it->second.temperature) +
                        std::abs(data.humidity - it->second.humidity) +
                        std::abs(data.biomass - it->second.biomass) +
                        chemDelta;
    data.lastUpdatedStep = static_cast<uint32_t>(stepIndex);
    data.lastSimTime = simTimeSec;
    data.dirty = delta > 1e-4f;
    updates.push_back({key, data});
  }

  for (const auto& update : updates) {
    midMap[update.key] = update.data;
  }
}

void World::UpdateFar(double simTimeSec, double stepSec, uint64_t stepIndex) {
  const auto& active = chunkStore_.ActiveFar();
  auto& farMap = chunkStore_.FarTiles();

  std::array<int, 2> tracked{};
  const std::size_t trackedCount = std::min<std::size_t>(tracked.size(), chemSeedSpecies_.size());
  for (std::size_t i = 0; i < trackedCount; ++i) {
    tracked[i] = chemSeedSpecies_[i];
  }

  struct UpdateEntry {
    ChunkKey key;
    sim::FarTileData data;
  };
  std::vector<UpdateEntry> updates;
  updates.reserve(active.size());

  for (const auto& key : active) {
    auto it = farMap.find(key);
    if (it == farMap.end()) {
      continue;
    }
    auto data = it->second;
    const double skipWindow = data.chem.dormant ? stepSec * 6.0 : stepSec * 3.0;
    if (!data.dirty && (simTimeSec - data.lastSimTime) < skipWindow) {
      continue;
    }

    if (data.chem.species.empty() && trackedCount > 0) {
      SeedChemState(data.chem, tracked.data(), trackedCount, HashFor(key, worldSeed_, stepIndex, 23));
    }
    data.chem.temperature = data.avgTemperature;
    data.chem.pressure = 101325.0;
    std::array<float, 2> beforeConc{};
    for (std::size_t i = 0; i < trackedCount; ++i) {
      beforeConc[i] = data.chem.GetConcentration(tracked[i]);
    }
    const double beforePH = data.chem.pH;
    const double beforeEh = data.chem.eh;
    const bool beforeDormant = data.chem.dormant;

    float popAvg = data.population;
    float tempAvg = data.avgTemperature;
    double phAvg = data.chem.pH;
    double ehAvg = data.chem.eh;
    int count = 1;
    for (const auto& offset : std::array<std::pair<int, int>, 4>{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}}) {
      ChunkKey nk = key;
      nk.x += offset.first;
      nk.y += offset.second;
      auto nit = farMap.find(nk);
      if (nit != farMap.end()) {
        popAvg += nit->second.population;
        tempAvg += nit->second.avgTemperature;
        phAvg += nit->second.chem.pH;
        ehAvg += nit->second.chem.eh;
        count++;
      }
    }
    popAvg /= static_cast<float>(count);
    tempAvg /= static_cast<float>(count);
    phAvg /= static_cast<double>(count);
    ehAvg /= static_cast<double>(count);

    const float growth = 0.1f * data.population * (1.0f - data.population);
    const float migration = (popAvg - data.population) * 0.05f;
    data.population = Clamp01(data.population + (growth + migration) * static_cast<float>(stepSec));

    const float tempNoise = (Random01(HashFor(key, worldSeed_, stepIndex, 3)) - 0.5f) * 0.02f;
    data.avgTemperature += (tempAvg - data.avgTemperature) * 0.05f + tempNoise;

    data.chem.pH += (phAvg - data.chem.pH) * 0.1;
    data.chem.eh += (ehAvg - data.chem.eh) * 0.1;

    if ((stepIndex % 5) == 0) {
      auto config = reactionConfig_;
      config.maxReactions = 4;
      reactionEngine_.Step(data.chem, stepSec, HashFor(key, worldSeed_, stepIndex, 19), config);
    }

    if (trackedCount > 0) {
      std::array<float, 2> neighborAvg{};
      int neighborCount = 1;
      for (std::size_t i = 0; i < trackedCount; ++i) {
        neighborAvg[i] = data.chem.GetConcentration(tracked[i]);
      }
      for (const auto& offset : std::array<std::pair<int, int>, 4>{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}}) {
        ChunkKey nk = key;
        nk.x += offset.first;
        nk.y += offset.second;
        auto nit = farMap.find(nk);
        if (nit != farMap.end()) {
          for (std::size_t i = 0; i < trackedCount; ++i) {
            neighborAvg[i] += nit->second.chem.GetConcentration(tracked[i]);
          }
          neighborCount++;
        }
      }
      for (std::size_t i = 0; i < trackedCount; ++i) {
        neighborAvg[i] /= static_cast<float>(neighborCount);
        const float current = data.chem.GetConcentration(tracked[i]);
        data.chem.SetConcentration(tracked[i], current + (neighborAvg[i] - current) * 0.05f);
      }
    }

    geoCycles_.Step(data.chem, stepSec);
    phaseModel_.Update(data.chem);
    data.chem.Compact();
    data.chem.temperature = data.avgTemperature;

    float chemDelta = 0.0f;
    for (std::size_t i = 0; i < trackedCount; ++i) {
      chemDelta += std::abs(data.chem.GetConcentration(tracked[i]) - beforeConc[i]);
    }
    chemDelta += static_cast<float>(std::abs(data.chem.pH - beforePH) + std::abs(data.chem.eh - beforeEh));
    if (data.chem.dormant != beforeDormant) {
      chemDelta += 0.1f;
    }

    const float delta = std::abs(data.population - it->second.population) +
                        std::abs(data.avgTemperature - it->second.avgTemperature) +
                        chemDelta;
    data.lastUpdatedStep = static_cast<uint32_t>(stepIndex);
    data.lastSimTime = simTimeSec;
    data.dirty = delta > 1e-4f;
    updates.push_back({key, data});
  }

  for (const auto& update : updates) {
    farMap[update.key] = update.data;
  }
}

void World::SetSeed(uint64_t seed) {
  worldSeed_ = seed;
}

void World::ApplySnapshot(const io::WorldSnapshot& snapshot) {
  chunkStore_.NearChunks() = snapshot.near;
  chunkStore_.MidChunks() = snapshot.mid;
  chunkStore_.FarTiles() = snapshot.far;
  metrics_.simTime = snapshot.simTime;
  clock_.Reset(snapshot.simTime);
  lastSimTime_ = snapshot.simTime;
  simStep_ = 0;
  nearRuntime_.accumulator = 0.0;
  midRuntime_.accumulator = 0.0;
  farRuntime_.accumulator = 0.0;
  nearRuntime_.stepIndex = 0;
  midRuntime_.stepIndex = 0;
  farRuntime_.stepIndex = 0;

  for (auto& [key, data] : chunkStore_.NearChunks()) {
    data.dirty = true;
  }
  for (auto& [key, data] : chunkStore_.MidChunks()) {
    data.dirty = true;
  }
  for (auto& [key, data] : chunkStore_.FarTiles()) {
    data.dirty = true;
  }

  chunkStore_.UpdateActive(camera_);
}

io::DeltaSnapshot World::CreateDelta(const io::WorldSnapshot& base) const {
  return io::CreateDelta(base, CreateSnapshot());
}

void World::ApplyDelta(const io::DeltaSnapshot& delta) {
  io::WorldSnapshot snapshot = CreateSnapshot();
  io::ApplyDelta(snapshot, delta);
  ApplySnapshot(snapshot);
  clock_.Reset(delta.targetSimTime);
  lastSimTime_ = delta.targetSimTime;
}

} // namespace terra::world
