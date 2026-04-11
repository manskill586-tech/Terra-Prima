#include "terra/job/JobSystem.h"

#include <cassert>
#include <chrono>

namespace terra::job {

namespace {
thread_local int g_workerIndex = -1;
}

JobSystem::JobSystem(std::size_t workerCount) {
  if (workerCount == 0) {
    workerCount = std::max<std::size_t>(1, std::thread::hardware_concurrency());
  }
  workers_.reserve(workerCount);
  for (std::size_t i = 0; i < workerCount; ++i) {
    workers_.emplace_back(std::make_unique<Worker>());
    workers_[i]->thread = std::thread([this, i]() { WorkerLoop(i); });
  }
}

JobSystem::~JobSystem() {
  stop_.store(true, std::memory_order_relaxed);
  cv_.notify_all();
  for (auto& worker : workers_) {
    if (worker && worker->thread.joinable()) {
      worker->thread.join();
    }
  }
}

void JobSystem::Enqueue(Task task) {
  if (!task) {
    return;
  }
  tasksInFlight_.fetch_add(1, std::memory_order_relaxed);

  const int workerIndex = g_workerIndex;
  if (workerIndex >= 0 && static_cast<std::size_t>(workerIndex) < workers_.size()) {
    auto& local = *workers_[static_cast<std::size_t>(workerIndex)];
    {
      std::lock_guard<std::mutex> lock(local.localMutex);
      local.localQueue.push_front(std::move(task));
    }
  } else {
    {
      std::lock_guard<std::mutex> lock(globalMutex_);
      globalQueue_.push_back(std::move(task));
    }
  }
  cv_.notify_one();
}

void JobSystem::EnqueueBulk(const std::vector<Task>& tasks) {
  for (const auto& task : tasks) {
    Enqueue(task);
  }
}

void JobSystem::WaitIdle() {
  while (tasksInFlight_.load(std::memory_order_relaxed) > 0) {
    std::unique_lock<std::mutex> lock(globalMutex_);
    cv_.wait_for(lock, std::chrono::milliseconds(1));
  }
}

std::size_t JobSystem::WorkerCount() const {
  return workers_.size();
}

void JobSystem::WorkerLoop(std::size_t index) {
  g_workerIndex = static_cast<int>(index);
  while (!stop_.load(std::memory_order_relaxed)) {
    Task task;
    if (TryPopLocal(index, task) || TryPopGlobal(task) || TrySteal(index, task)) {
      task();
      tasksInFlight_.fetch_sub(1, std::memory_order_relaxed);
      cv_.notify_all();
      continue;
    }

    std::unique_lock<std::mutex> lock(globalMutex_);
    cv_.wait_for(lock, std::chrono::milliseconds(1));
  }
}

bool JobSystem::TryPopLocal(std::size_t index, Task& outTask) {
  auto& local = *workers_[index];
  std::lock_guard<std::mutex> lock(local.localMutex);
  if (local.localQueue.empty()) {
    return false;
  }
  outTask = std::move(local.localQueue.front());
  local.localQueue.pop_front();
  return true;
}

bool JobSystem::TryPopGlobal(Task& outTask) {
  std::lock_guard<std::mutex> lock(globalMutex_);
  if (globalQueue_.empty()) {
    return false;
  }
  outTask = std::move(globalQueue_.front());
  globalQueue_.pop_front();
  return true;
}

bool JobSystem::TrySteal(std::size_t thiefIndex, Task& outTask) {
  const std::size_t workerCount = workers_.size();
  for (std::size_t i = 0; i < workerCount; ++i) {
    const std::size_t victimIndex = (thiefIndex + i + 1) % workerCount;
    if (victimIndex == thiefIndex) {
      continue;
    }
    auto& victim = *workers_[victimIndex];
    std::lock_guard<std::mutex> lock(victim.localMutex);
    if (victim.localQueue.empty()) {
      continue;
    }
    outTask = std::move(victim.localQueue.back());
    victim.localQueue.pop_back();
    return true;
  }
  return false;
}

} // namespace terra::job
