#include "terra/io/SnapshotStore.h"

#include <fstream>
#include <type_traits>

namespace terra::io {

namespace {

template <typename T>
void WritePOD(std::ofstream& out, const T& value) {
  static_assert(std::is_trivially_copyable_v<T>, "POD required");
  out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename K, typename V, typename H>
void WriteMap(std::ofstream& out, const std::unordered_map<K, V, H>& map) {
  const uint64_t count = static_cast<uint64_t>(map.size());
  WritePOD(out, count);
  for (const auto& [key, value] : map) {
    WritePOD(out, key);
    WritePOD(out, value);
  }
}

} // namespace

std::future<void> SnapshotStore::SaveSnapshotAsync(const std::string& path, WorldSnapshot snapshot) {
  return std::async(std::launch::async, [path, snapshot = std::move(snapshot)]() mutable {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
      return;
    }

    const uint32_t magic = 0x54505241; // "TPRA"
    const uint32_t version = 1;
    WritePOD(out, magic);
    WritePOD(out, version);
    WritePOD(out, snapshot.simTime);

    WriteMap(out, snapshot.near);
    WriteMap(out, snapshot.mid);
    WriteMap(out, snapshot.far);
  });
}

} // namespace terra::io
