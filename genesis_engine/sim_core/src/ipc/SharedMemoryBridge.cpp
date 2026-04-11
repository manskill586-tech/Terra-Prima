#include "ipc/SharedMemoryBridge.h"

#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace genesis::ipc {

SharedMemoryBridge::SharedMemoryBridge(const char* name, bool owner)
    : name_(name ? name : ""), owner_(owner) {
  if (name_.empty()) {
    return;
  }

#ifdef _WIN32
  const std::size_t size = sizeof(shared::SimStateBuffer);
  HANDLE hMap = nullptr;
  if (owner_) {
    hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                              static_cast<DWORD>(size >> 32),
                              static_cast<DWORD>(size & 0xFFFFFFFF), name_.c_str());
  } else {
    hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name_.c_str());
  }
  if (!hMap) {
    return;
  }
  void* view = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, size);
  if (!view) {
    CloseHandle(hMap);
    return;
  }
  handle_ = hMap;
  buffer_ = static_cast<shared::SimStateBuffer*>(view);
  if (owner_) {
    std::memset(buffer_, 0, sizeof(shared::SimStateBuffer));
  }
#else
  const std::size_t size = sizeof(shared::SimStateBuffer);
  const int flags = owner_ ? (O_CREAT | O_RDWR) : O_RDWR;
  fd_ = shm_open(name_.c_str(), flags, 0666);
  if (fd_ == -1) {
    return;
  }
  if (owner_) {
    if (ftruncate(fd_, static_cast<off_t>(size)) != 0) {
      close(fd_);
      fd_ = -1;
      return;
    }
  }
  void* view = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
  if (view == MAP_FAILED) {
    close(fd_);
    fd_ = -1;
    return;
  }
  buffer_ = static_cast<shared::SimStateBuffer*>(view);
  if (owner_) {
    std::memset(buffer_, 0, sizeof(shared::SimStateBuffer));
  }
#endif
}

SharedMemoryBridge::~SharedMemoryBridge() {
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
  if (owner_ && !name_.empty()) {
    shm_unlink(name_.c_str());
  }
#endif
}

} // namespace genesis::ipc
