#include <iostream>

int g_failures = 0;

void Expect(bool cond, const char* expr, const char* file, int line) {
  if (!cond) {
    ++g_failures;
    std::cerr << "EXPECT FAILED: " << expr << " at " << file << ":" << line << "\n";
  }
}

#define EXPECT_TRUE(x) Expect((x), #x, __FILE__, __LINE__)

void RunSimSchedulerTests();
void RunChunkStoreTests();
void RunSnapshotTests();

int main() {
  RunSimSchedulerTests();
  RunChunkStoreTests();
  RunSnapshotTests();

  if (g_failures == 0) {
    std::cout << "All tests passed.\n";
    return 0;
  }
  std::cout << g_failures << " tests failed.\n";
  return 1;
}
