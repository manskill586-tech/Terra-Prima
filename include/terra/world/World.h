#pragma once

#include "terra/core/Clock.h"
#include "terra/io/SnapshotStore.h"
#include "terra/job/JobSystem.h"
#include "terra/render/Renderer.h"
#include "terra/sim/SimScheduler.h"
#include "terra/world/ChunkStore.h"
#include "terra/world/EntityLOD.h"

namespace terra::world {

struct WorldMetrics {
  double simTime{0.0};
  double frameBudgetMs{16.0};
  double lastNearMs{0.0};
  double lastMidMs{0.0};
  double lastFarMs{0.0};
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

  core::WorldClock& Clock();
  sim::SimScheduler& Scheduler();
  ChunkStore& Chunks();
  EntityLOD& Entities();

  io::WorldSnapshot CreateSnapshot() const;

private:
  void UpdateNear(double simTimeSec);
  void UpdateMid(double simTimeSec);
  void UpdateFar(double simTimeSec);

  core::WorldClock clock_;
  sim::SimScheduler scheduler_;
  ChunkStore chunkStore_;
  EntityLOD entityLod_;
  io::SnapshotStore snapshotStore_;
  job::JobSystem jobSystem_;
  render::NullRenderer renderer_;

  WorldMetrics metrics_{};
  uint32_t simStep_{0};
  CameraState camera_{};
};

} // namespace terra::world
