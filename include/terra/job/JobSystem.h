#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace terra::job {

class JobSystem {
public:
  using Task = std::function<void()>;

  explicit JobSystem(std::size_t workerCount = 0);
  ~JobSystem();

  JobSystem(const JobSystem&) = delete;
  JobSystem& operator=(const JobSystem&) = delete;

  void Enqueue(Task task);
  void EnqueueBulk(const std::vector<Task>& tasks);
  void WaitIdle();

  std::size_t WorkerCount() const;

private:
  struct Worker {
    std::thread thread;
    std::deque<Task> localQueue;
    std::mutex localMutex;
  };

  void WorkerLoop(std::size_t index);
  bool TryPopLocal(std::size_t index, Task& outTask);
  bool TryPopGlobal(Task& outTask);
  bool TrySteal(std::size_t thiefIndex, Task& outTask);

  std::vector<Worker> workers_;
  std::deque<Task> globalQueue_;
  std::mutex globalMutex_;
  std::condition_variable cv_;

  std::atomic<bool> stop_{false};
  std::atomic<int> tasksInFlight_{0};
};

} // namespace terra::job
