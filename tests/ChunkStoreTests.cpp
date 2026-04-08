#include "terra/world/ChunkStore.h"

extern void Expect(bool cond, const char* expr, const char* file, int line);
#define EXPECT_TRUE(x) Expect((x), #x, __FILE__, __LINE__)

void RunChunkStoreTests() {
  terra::world::ChunkStore store;
  terra::world::CameraState camera{};
  camera.nearCenter = {0, 0, 0, 6};
  camera.midCenter = {0, 0, 0, 4};
  camera.farCenter = {0, 0, 0, 2};
  camera.nearRadius = 1;
  camera.midRadius = 2;
  camera.farRadius = 3;

  store.UpdateActive(camera);
  const auto metrics = store.Metrics();
  EXPECT_TRUE(metrics.activeNear > 0);
  EXPECT_TRUE(metrics.activeMid > 0);
  EXPECT_TRUE(metrics.activeFar > 0);

  const auto& near = store.NearChunks();
  EXPECT_TRUE(!near.empty());
}
