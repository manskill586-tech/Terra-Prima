#include "terra/world/World.h"

#include "terra/core/Profiler.h"
#include "terra/core/Random.h"

#include <algorithm>
#include <chrono>
#include <thread>

namespace terra::world {

namespace {

double NowMs() {
  const auto now = std::chrono::high_resolution_clock::now();
  const auto ms = std::chrono::duration<double, std::milli>(now.time_since_epoch());
  return ms.count();
}

float NoiseFromKey(const ChunkKey& key, uint32_t step) {
  uint64_t h = terra::core::HashCombine(static_cast<uint64_t>(key.face), static_cast<uint64_t>(key.x));
  h = terra::core::HashCombine(h, static_cast<uint64_t>(key.y));
  h = terra::core::HashCombine(h, static_cast<uint64_t>(key.lod));
  h = terra::core::HashCombine(h, static_cast<uint64_t>(step));
  return static_cast<float>((h % 1000) / 1000.0);
}

} // namespace

World::World() : jobSystem_(std::thread::hardware_concurrency()) {
  metrics_.frameBudgetMs = 16.0;
  clock_.SetTimeScale(10.0);

  camera_.nearCenter = {0, 0, 0, 6};
  camera_.midCenter = {0, 0, 0, 4};
  camera_.farCenter = {0, 0, 0, 2};
  camera_.nearRadius = 2;
  camera_.midRadius = 4;
  camera_.farRadius = 6;

  chunkStore_.UpdateActive(camera_);
}

void World::Update(double realDeltaSeconds) {
  clock_.Tick(realDeltaSeconds);
  const double simTime = clock_.SimTime();
  metrics_.simTime = simTime;

  // Simple camera drift to exercise LOD streaming.
  if (static_cast<int>(simTime) % 10 == 0) {
    camera_.nearCenter.x += 1;
    camera_.midCenter.x = camera_.nearCenter.x / 2;
    camera_.farCenter.x = camera_.nearCenter.x / 4;
  }

  chunkStore_.UpdateActive(camera_);

  const auto plan = scheduler_.Plan(simTime, metrics_.frameBudgetMs);
  for (auto tier : plan.tiers) {
    const double start = NowMs();
    switch (tier) {
      case sim::TierId::Near:
        UpdateNear(simTime);
        metrics_.lastNearMs = NowMs() - start;
        scheduler_.OnTierUpdated(tier, simTime, metrics_.lastNearMs);
        break;
      case sim::TierId::Mid:
        UpdateMid(simTime);
        metrics_.lastMidMs = NowMs() - start;
        scheduler_.OnTierUpdated(tier, simTime, metrics_.lastMidMs);
        break;
      case sim::TierId::Far:
        UpdateFar(simTime);
        metrics_.lastFarMs = NowMs() - start;
        scheduler_.OnTierUpdated(tier, simTime, metrics_.lastFarMs);
        break;
    }
  }

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

sim::SimScheduler& World::Scheduler() {
  return scheduler_;
}

ChunkStore& World::Chunks() {
  return chunkStore_;
}

EntityLOD& World::Entities() {
  return entityLod_;
}

io::WorldSnapshot World::CreateSnapshot() const {
  io::WorldSnapshot snapshot{};
  snapshot.simTime = metrics_.simTime;
  snapshot.near = chunkStore_.NearChunks();
  snapshot.mid = chunkStore_.MidChunks();
  snapshot.far = chunkStore_.FarTiles();
  return snapshot;
}

void World::UpdateNear(double simTimeSec) {
  ++simStep_;
  for (auto& [key, data] : chunkStore_.NearChunks()) {
    const float noise = NoiseFromKey(key, simStep_);
    data.temperature += (noise - 0.5f) * 0.1f;
    data.humidity = std::clamp(data.humidity + (noise - 0.5f) * 0.02f, 0.0f, 1.0f);
    data.concentrations[0] = std::clamp(data.concentrations[0] + (noise - 0.5f) * 0.01f, 0.0f, 1.0f);
    data.lastUpdatedStep = simStep_;
  }

  entityLod_.Update(static_cast<float>(simTimeSec * 0.001));
}

void World::UpdateMid(double simTimeSec) {
  for (auto& [key, data] : chunkStore_.MidChunks()) {
    const float noise = NoiseFromKey(key, simStep_ + 13);
    data.temperature += (noise - 0.5f) * 0.05f;
    data.humidity = std::clamp(data.humidity + (noise - 0.5f) * 0.01f, 0.0f, 1.0f);
    data.biomass = std::clamp(data.biomass + (noise - 0.5f) * 0.02f, 0.0f, 1.0f);
    data.lastUpdatedStep = simStep_;
  }
}

void World::UpdateFar(double simTimeSec) {
  for (auto& [key, data] : chunkStore_.FarTiles()) {
    const float noise = NoiseFromKey(key, simStep_ + 37);
    data.avgTemperature += (noise - 0.5f) * 0.02f;
    data.population = std::clamp(data.population + (noise - 0.5f) * 0.01f, 0.0f, 1.0f);
    data.lastUpdatedStep = simStep_;
  }
}

} // namespace terra::world
