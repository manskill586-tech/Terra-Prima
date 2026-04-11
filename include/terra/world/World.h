#pragma once

#include "terra/core/Clock.h"
#include "terra/chem/ChemDB.h"
#include "terra/chem/GeoCycles.h"
#include "terra/chem/PhaseModel.h"
#include "terra/chem/ReactionEngine.h"
#include "terra/io/SnapshotStore.h"
#include "terra/job/JobSystem.h"
#include "terra/render/Renderer.h"
#include "terra/sim/SimScheduler.h"
#include "terra/world/ChunkStore.h"
#include "terra/world/EntityLOD.h"

#include <cstdint>
#include <vector>

namespace terra::world {

struct WorldMetrics {
  double simTime{0.0};
  double frameBudgetMs{16.0};
  double lastNearMs{0.0};
  double lastMidMs{0.0};
  double lastFarMs{0.0};
  int nearSteps{0};
  int midSteps{0};
  int farSteps{0};
  std::size_t activeNear{0};
  std::size_t activeMid{0};
  std::size_t activeFar{0};
  std::size_t entitiesNear{0};
  std::size_t entitiesMid{0};
  std::size_t entitiesFar{0};
};

class World {
public:
  World();

  void Update(double realDeltaSeconds);
  WorldMetrics Metrics() const;
  void SetSeed(uint64_t seed);
  void SetFrameBudgetMs(double ms);

  core::WorldClock& Clock();
  sim::SimScheduler& Scheduler();
  ChunkStore& Chunks();
  EntityLOD& Entities();

  io::WorldSnapshot CreateSnapshot() const;
  void ApplySnapshot(const io::WorldSnapshot& snapshot);
  io::DeltaSnapshot CreateDelta(const io::WorldSnapshot& base) const;
  void ApplyDelta(const io::DeltaSnapshot& delta);
  chem::ChemDB& ChemDatabase();

private:
  void UpdateNear(double simTimeSec, double stepSec, uint64_t stepIndex);
  void UpdateMid(double simTimeSec, double stepSec, uint64_t stepIndex);
  void UpdateFar(double simTimeSec, double stepSec, uint64_t stepIndex);

  struct TierRuntime {
    double accumulator{0.0};
    double stepSec{0.1};
    double coarseMultiplier{4.0};
    uint64_t stepIndex{0};
    int maxStepsPerFrame{4};
    int stepsLastFrame{0};
  };

  core::WorldClock clock_;
  chem::ChemDB chemDb_;
  chem::ReactionEngine reactionEngine_;
  chem::PhaseModel phaseModel_;
  chem::GeoCycles geoCycles_;
  chem::ReactionEngineConfig reactionConfig_{};
  sim::SimScheduler scheduler_;
  ChunkStore chunkStore_;
  EntityLOD entityLod_;
  io::SnapshotStore snapshotStore_;
  job::JobSystem jobSystem_;
  render::NullRenderer renderer_;

  WorldMetrics metrics_{};
  uint32_t simStep_{0};
  double lastSimTime_{0.0};
  uint64_t worldSeed_{0xC0FFEE1234ULL};
  TierRuntime nearRuntime_{};
  TierRuntime midRuntime_{};
  TierRuntime farRuntime_{};
  std::vector<int> chemSeedSpecies_;
  CameraState camera_{};
};

} // namespace terra::world
