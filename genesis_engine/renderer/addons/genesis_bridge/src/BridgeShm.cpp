#include "BridgeShm.h"

#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace genesis::bridge {

BridgeShm::~BridgeShm() {
  Close();
}

bool BridgeShm::Open(const std::string& name) {
  if (name.empty()) {
    return false;
  }
  if (IsOpen() && name == name_) {
    return true;
  }
  Close();
  name_ = name;

#ifdef _WIN32
  const std::size_t size = sizeof(shared::SimStateBuffer);
  HANDLE hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name_.c_str());
  if (!hMap) {
    return false;
  }
  void* view = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, size);
  if (!view) {
    CloseHandle(hMap);
    return false;
  }
  handle_ = hMap;
  buffer_ = static_cast<shared::SimStateBuffer*>(view);
#else
  const std::size_t size = sizeof(shared::SimStateBuffer);
  fd_ = shm_open(name_.c_str(), O_RDWR, 0666);
  if (fd_ == -1) {
    return false;
  }
  void* view = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
  if (view == MAP_FAILED) {
    close(fd_);
    fd_ = -1;
    return false;
  }
  buffer_ = static_cast<shared::SimStateBuffer*>(view);
#endif

  return true;
}

void BridgeShm::Close() {
#ifdef _WIN32
  if (buffer_) {
    UnmapViewOfFile(buffer_);
    buffer_ = nullptr;
  }
  if (handle_) {
    CloseHandle(static_cast<HANDLE>(handle_));
    handle_ = nullptr;
  }
#else
  if (buffer_) {
    munmap(buffer_, sizeof(shared::SimStateBuffer));
    buffer_ = nullptr;
  }
  if (fd_ != -1) {
    close(fd_);
    fd_ = -1;
  }
#endif
}

} // namespace genesis::bridge
