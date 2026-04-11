#pragma once

#include "shared/sim_state.h"

#include <string>

namespace genesis::bridge {

class BridgeShm {
public:
  BridgeShm() = default;
  ~BridgeShm();

  bool Open(const std::string& name);
  void Close();
  bool IsOpen() const { return buffer_ != nullptr; }

  const shared::SimStateBuffer* Buffer() const { return buffer_; }

private:
  std::string name_;
  shared::SimStateBuffer* buffer_{nullptr};

#ifdef _WIN32
  void* handle_{nullptr};
#else
  int fd_{-1};
#endif
};

} // namespace genesis::bridge
