#include "persistence/Snapshot.h"

#include <cmath>
#include <fstream>
#include <unordered_map>
#include <utility>

namespace {

constexpr uint32_t kSnapshotMagic = 0x504E5347; // GSNP
constexpr uint32_t kDeltaMagic = 0x4C414447;    // GDAL
constexpr uint32_t kVersion = 1;

using genesis::persistence::ChunkRecord;
using genesis::persistence::RemovedChunk;

template <typename T>
void Write(std::ostream& os, const T& value) {
  os.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
bool Read(std::istream& is, T& value) {
  return static_cast<bool>(is.read(reinterpret_cast<char*>(&value), sizeof(T)));
}

uint8_t ToTierByte(genesis::world::Tier tier) {
  return static_cast<uint8_t>(tier);
}

genesis::world::Tier FromTierByte(uint8_t v) {
  switch (v) {
    case 0: return genesis::world::Tier::Near;
    case 1: return genesis::world::Tier::Mid;
    case 2: return genesis::world::Tier::Far;
    default: return genesis::world::Tier::Near;
  }
}

struct Key {
  genesis::world::Tier tier{genesis::world::Tier::Near};
  genesis::world::ChunkCoord coord{};
};

struct KeyHash {
  size_t operator()(const Key& k) const {
    size_t h = genesis::world::ChunkCoordHash{}(k.coord);
    return h ^ (static_cast<size_t>(ToTierByte(k.tier)) << 1);
  }
};

struct KeyEq {
  bool operator()(const Key& a, const Key& b) const {
    return a.tier == b.tier && a.coord == b.coord;
  }
};

bool ChemEqual(const terra::chem::ChemState& a, const terra::chem::ChemState& b, float eps = 1e-5f) {
  if (std::abs(a.temperature - b.temperature) > eps ||
      std::abs(a.pressure - b.pressure) > eps ||
      std::abs(a.pH - b.pH) > eps ||
      std::abs(a.eh - b.eh) > eps ||
      std::abs(a.ionicStrength - b.ionicStrength) > eps ||
      a.dormant != b.dormant ||
      a.species.size() != b.species.size()) {
    return false;
  }
  for (size_t i = 0; i < a.species.size(); ++i) {
    if (a.species[i].id != b.species[i].id) {
      return false;
    }
    if (std::abs(a.species[i].concentration - b.species[i].concentration) > eps) {
      return false;
    }
  }
  return true;
}

} // namespace

namespace genesis::persistence {

Snapshot CaptureSnapshot(uint64_t seed, double sim_time, const genesis::world::ChunkStore& store) {
  Snapshot snapshot;
  snapshot.world_seed = seed;
  snapshot.sim_time = sim_time;

  auto capture_tier = [&](genesis::world::Tier tier) {
    store.ForEachChunk(tier, [&](const genesis::world::Chunk& chunk) {
      ChunkRecord record;
      record.tier = tier;
      record.coord = chunk.coord;
      record.field = chunk.field;
      record.chem = chunk.chem;
      record.last_sim_time = chunk.last_sim_time;
      record.stable_steps = chunk.stable_steps;
      record.sleeping = chunk.sleeping;
      snapshot.chunks.push_back(record);
    });
  };

  capture_tier(genesis::world::Tier::Near);
  capture_tier(genesis::world::Tier::Mid);
  capture_tier(genesis::world::Tier::Far);

  return snapshot;
}

void ApplySnapshot(const Snapshot& snapshot, uint64_t& seed, double& sim_time, genesis::world::ChunkStore& store) {
  store.Clear();
  seed = snapshot.world_seed;
  sim_time = snapshot.sim_time;

  for (const auto& record : snapshot.chunks) {
    auto& chunk = store.GetOrCreate(record.tier, record.coord);
    chunk.field = record.field;
    chunk.chem = record.chem;
    chunk.chem_seeded = !chunk.chem.species.empty();
    chunk.last_sim_time = record.last_sim_time;
    chunk.stable_steps = record.stable_steps;
    chunk.sleeping = record.sleeping;
    chunk.dirty = true;
    chunk.visual_dirty = true;
  }
}

DeltaSnapshot CreateDelta(const Snapshot& base, const Snapshot& current) {
  DeltaSnapshot delta;
  delta.base_sim_time = base.sim_time;
  delta.target_sim_time = current.sim_time;

  std::unordered_map<Key, ChunkRecord, KeyHash, KeyEq> base_map;
  base_map.reserve(base.chunks.size());
  for (const auto& record : base.chunks) {
    base_map.emplace(Key{record.tier, record.coord}, record);
  }

  std::unordered_map<Key, ChunkRecord, KeyHash, KeyEq> current_map;
  current_map.reserve(current.chunks.size());
  for (const auto& record : current.chunks) {
    current_map.emplace(Key{record.tier, record.coord}, record);
  }

  for (const auto& [key, record] : current_map) {
    auto it = base_map.find(key);
    if (it == base_map.end()) {
      delta.added.push_back(record);
      continue;
    }
    const auto& base_record = it->second;
    if (record.field.temperature != base_record.field.temperature ||
        record.field.pressure != base_record.field.pressure ||
        record.field.humidity != base_record.field.humidity ||
        record.sleeping != base_record.sleeping ||
        record.stable_steps != base_record.stable_steps ||
        !ChemEqual(record.chem, base_record.chem)) {
      delta.updated.push_back(record);
    }
  }

  for (const auto& [key, record] : base_map) {
    if (current_map.find(key) == current_map.end()) {
      delta.removed.push_back(RemovedChunk{key.tier, key.coord});
    }
  }

  return delta;
}

Snapshot ApplyDelta(const Snapshot& base, const DeltaSnapshot& delta) {
  Snapshot out = base;
  out.sim_time = delta.target_sim_time;

  std::unordered_map<Key, ChunkRecord, KeyHash, KeyEq> map;
  map.reserve(out.chunks.size() + delta.added.size() + delta.updated.size());
  for (const auto& record : out.chunks) {
    map.emplace(Key{record.tier, record.coord}, record);
  }

  for (const auto& removed : delta.removed) {
    map.erase(Key{removed.tier, removed.coord});
  }
  for (const auto& record : delta.added) {
    map[Key{record.tier, record.coord}] = record;
  }
  for (const auto& record : delta.updated) {
    map[Key{record.tier, record.coord}] = record;
  }

  out.chunks.clear();
  out.chunks.reserve(map.size());
  for (const auto& [key, record] : map) {
    out.chunks.push_back(record);
  }

  return out;
}

void WriteChem(std::ostream& os, const terra::chem::ChemState& chem) {
  Write(os, chem.temperature);
  Write(os, chem.pressure);
  Write(os, chem.pH);
  Write(os, chem.eh);
  Write(os, chem.ionicStrength);
  const uint8_t dormant = chem.dormant ? 1 : 0;
  Write(os, dormant);
  const uint32_t count = static_cast<uint32_t>(chem.species.size());
  Write(os, count);
  for (const auto& s : chem.species) {
    Write(os, s.id);
    Write(os, s.concentration);
  }
}

bool ReadChem(std::istream& is, terra::chem::ChemState& chem) {
  if (!Read(is, chem.temperature) ||
      !Read(is, chem.pressure) ||
      !Read(is, chem.pH) ||
      !Read(is, chem.eh) ||
      !Read(is, chem.ionicStrength)) {
    return false;
  }
  uint8_t dormant = 0;
  if (!Read(is, dormant)) {
    return false;
  }
  chem.dormant = dormant != 0;
  uint32_t count = 0;
  if (!Read(is, count)) {
    return false;
  }
  chem.species.clear();
  chem.species.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    terra::chem::SpeciesAmount s;
    if (!Read(is, s.id) || !Read(is, s.concentration)) {
      return false;
    }
    chem.species.push_back(s);
  }
  chem.Compact();
  return true;
}

bool SaveSnapshot(const std::string& path, const Snapshot& snapshot) {
  std::ofstream os(path, std::ios::binary);
  if (!os) {
    return false;
  }
  Write(os, kSnapshotMagic);
  Write(os, kVersion);
  Write(os, snapshot.world_seed);
  Write(os, snapshot.sim_time);
  const uint32_t count = static_cast<uint32_t>(snapshot.chunks.size());
  Write(os, count);
  for (const auto& record : snapshot.chunks) {
    const uint8_t tier = ToTierByte(record.tier);
    Write(os, tier);
    Write(os, record.coord.x);
    Write(os, record.coord.y);
    Write(os, record.coord.z);
    Write(os, record.field.temperature);
    Write(os, record.field.pressure);
    Write(os, record.field.humidity);
    WriteChem(os, record.chem);
    Write(os, record.last_sim_time);
    Write(os, record.stable_steps);
    const uint8_t sleeping = record.sleeping ? 1 : 0;
    Write(os, sleeping);
  }
  return true;
}

bool LoadSnapshot(const std::string& path, Snapshot& out_snapshot) {
  std::ifstream is(path, std::ios::binary);
  if (!is) {
    return false;
  }
  uint32_t magic = 0;
  uint32_t version = 0;
  if (!Read(is, magic) || !Read(is, version) || magic != kSnapshotMagic || version != kVersion) {
    return false;
  }
  Snapshot snapshot;
  if (!Read(is, snapshot.world_seed) || !Read(is, snapshot.sim_time)) {
    return false;
  }
  uint32_t count = 0;
  if (!Read(is, count)) {
    return false;
  }
  snapshot.chunks.clear();
  snapshot.chunks.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    uint8_t tier = 0;
    ChunkRecord record;
    if (!Read(is, tier) ||
        !Read(is, record.coord.x) ||
        !Read(is, record.coord.y) ||
        !Read(is, record.coord.z) ||
        !Read(is, record.field.temperature) ||
        !Read(is, record.field.pressure) ||
        !Read(is, record.field.humidity) ||
        !ReadChem(is, record.chem) ||
        !Read(is, record.last_sim_time) ||
        !Read(is, record.stable_steps)) {
      return false;
    }
    uint8_t sleeping = 0;
    if (!Read(is, sleeping)) {
      return false;
    }
    record.tier = FromTierByte(tier);
    record.sleeping = sleeping != 0;
    snapshot.chunks.push_back(record);
  }
  out_snapshot = std::move(snapshot);
  return true;
}

bool SaveDelta(const std::string& path, const DeltaSnapshot& delta) {
  std::ofstream os(path, std::ios::binary);
  if (!os) {
    return false;
  }
  Write(os, kDeltaMagic);
  Write(os, kVersion);
  Write(os, delta.base_sim_time);
  Write(os, delta.target_sim_time);
  const uint32_t added = static_cast<uint32_t>(delta.added.size());
  const uint32_t updated = static_cast<uint32_t>(delta.updated.size());
  const uint32_t removed = static_cast<uint32_t>(delta.removed.size());
  Write(os, added);
  Write(os, updated);
  Write(os, removed);

  auto write_record = [&](const ChunkRecord& record) {
    const uint8_t tier = ToTierByte(record.tier);
    Write(os, tier);
    Write(os, record.coord.x);
    Write(os, record.coord.y);
    Write(os, record.coord.z);
    Write(os, record.field.temperature);
    Write(os, record.field.pressure);
    Write(os, record.field.humidity);
    WriteChem(os, record.chem);
    Write(os, record.last_sim_time);
    Write(os, record.stable_steps);
    const uint8_t sleeping = record.sleeping ? 1 : 0;
    Write(os, sleeping);
  };

  for (const auto& record : delta.added) {
    write_record(record);
  }
  for (const auto& record : delta.updated) {
    write_record(record);
  }
  for (const auto& record : delta.removed) {
    const uint8_t tier = ToTierByte(record.tier);
    Write(os, tier);
    Write(os, record.coord.x);
    Write(os, record.coord.y);
    Write(os, record.coord.z);
  }
  return true;
}

bool LoadDelta(const std::string& path, DeltaSnapshot& out_delta) {
  std::ifstream is(path, std::ios::binary);
  if (!is) {
    return false;
  }
  uint32_t magic = 0;
  uint32_t version = 0;
  if (!Read(is, magic) || !Read(is, version) || magic != kDeltaMagic || version != kVersion) {
    return false;
  }
  DeltaSnapshot delta;
  if (!Read(is, delta.base_sim_time) || !Read(is, delta.target_sim_time)) {
    return false;
  }
  uint32_t added = 0;
  uint32_t updated = 0;
  uint32_t removed = 0;
  if (!Read(is, added) || !Read(is, updated) || !Read(is, removed)) {
    return false;
  }

  auto read_record = [&](ChunkRecord& record) {
    uint8_t tier = 0;
    if (!Read(is, tier) ||
        !Read(is, record.coord.x) ||
        !Read(is, record.coord.y) ||
        !Read(is, record.coord.z) ||
        !Read(is, record.field.temperature) ||
        !Read(is, record.field.pressure) ||
        !Read(is, record.field.humidity) ||
        !ReadChem(is, record.chem) ||
        !Read(is, record.last_sim_time) ||
        !Read(is, record.stable_steps)) {
      return false;
    }
    uint8_t sleeping = 0;
    if (!Read(is, sleeping)) {
      return false;
    }
    record.tier = FromTierByte(tier);
    record.sleeping = sleeping != 0;
    return true;
  };

  delta.added.resize(added);
  for (uint32_t i = 0; i < added; ++i) {
    if (!read_record(delta.added[i])) {
      return false;
    }
  }
  delta.updated.resize(updated);
  for (uint32_t i = 0; i < updated; ++i) {
    if (!read_record(delta.updated[i])) {
      return false;
    }
  }
  delta.removed.resize(removed);
  for (uint32_t i = 0; i < removed; ++i) {
    uint8_t tier = 0;
    if (!Read(is, tier) ||
        !Read(is, delta.removed[i].coord.x) ||
        !Read(is, delta.removed[i].coord.y) ||
        !Read(is, delta.removed[i].coord.z)) {
      return false;
    }
    delta.removed[i].tier = FromTierByte(tier);
  }

  out_delta = std::move(delta);
  return true;
}

} // namespace genesis::persistence
