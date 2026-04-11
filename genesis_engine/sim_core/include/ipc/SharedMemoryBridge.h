#pragma once

#include "shared/sim_state.h"

#include <cstddef>
#include <string>

namespace genesis::ipc {

class SharedMemoryBridge {
public:
  SharedMemoryBridge(const char* name, bool owner);
  ~SharedMemoryBridge();

  SharedMemoryBridge(const SharedMemoryBridge&) = delete;
  SharedMemoryBridge& operator=(const SharedMemoryBridge&) = delete;

  shared::SimStateBuffer* Buffer() const { return buffer_; }
  bool IsValid() const { return buffer_ != nullptr; }

private:
  std::string name_;
  bool owner_{false};
  shared::SimStateBuffer* buffer_{nullptr};

#ifdef _WIN32
  void* handle_{nullptr};
#else
  int fd_{-1};
#endif
};

} // namespace genesis::ipc
