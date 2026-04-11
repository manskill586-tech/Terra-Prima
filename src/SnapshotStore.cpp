#include "terra/io/SnapshotStore.h"

#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace terra::io {

using terra::chem::ChemState;
using terra::chem::SpeciesAmount;

namespace {

template <typename T>
void WritePOD(std::ofstream& out, const T& value) {
  static_assert(std::is_trivially_copyable_v<T>, "POD required");
  out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
bool ReadPOD(std::ifstream& in, T& value) {
  static_assert(std::is_trivially_copyable_v<T>, "POD required");
  return static_cast<bool>(in.read(reinterpret_cast<char*>(&value), sizeof(T)));
}

template <typename T>
void WriteVector(std::ofstream& out, const std::vector<T>& vec) {
  const uint64_t count = static_cast<uint64_t>(vec.size());
  WritePOD(out, count);
  for (const auto& item : vec) {
    WritePOD(out, item);
  }
}

template <typename T>
bool ReadVector(std::ifstream& in, std::vector<T>& vec) {
  uint64_t count = 0;
  if (!ReadPOD(in, count)) {
    return false;
  }
  vec.resize(count);
  for (uint64_t i = 0; i < count; ++i) {
    if (!ReadPOD(in, vec[i])) {
      return false;
    }
  }
  return true;
}

void WriteChemState(std::ofstream& out, const ChemState& state) {
  WritePOD(out, state.temperature);
  WritePOD(out, state.pressure);
  WritePOD(out, state.pH);
  WritePOD(out, state.eh);
  WritePOD(out, state.ionicStrength);
  const uint8_t dormant = state.dormant ? 1 : 0;
  WritePOD(out, dormant);
  const uint32_t count = static_cast<uint32_t>(state.species.size());
  WritePOD(out, count);
  for (const auto& s : state.species) {
    WritePOD(out, s.id);
    WritePOD(out, s.concentration);
  }
}

bool ReadChemState(std::ifstream& in, ChemState& state) {
  if (!ReadPOD(in, state.temperature)) return false;
  if (!ReadPOD(in, state.pressure)) return false;
  if (!ReadPOD(in, state.pH)) return false;
  if (!ReadPOD(in, state.eh)) return false;
  if (!ReadPOD(in, state.ionicStrength)) return false;
  uint8_t dormant = 0;
  if (!ReadPOD(in, dormant)) return false;
  state.dormant = dormant != 0;
  uint32_t count = 0;
  if (!ReadPOD(in, count)) return false;
  state.species.clear();
  state.species.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    SpeciesAmount s{};
    if (!ReadPOD(in, s.id)) return false;
    if (!ReadPOD(in, s.concentration)) return false;
    state.species.push_back(s);
  }
  return true;
}

void WriteNearChunk(std::ofstream& out, const sim::NearChunkData& data) {
  WritePOD(out, data.temperature);
  WritePOD(out, data.humidity);
  for (float v : data.concentrations) {
    WritePOD(out, v);
  }
  WritePOD(out, data.lastUpdatedStep);
  WritePOD(out, data.lastSimTime);
  const uint8_t dirty = data.dirty ? 1 : 0;
  WritePOD(out, dirty);
  WriteChemState(out, data.chem);
}

bool ReadNearChunk(std::ifstream& in, sim::NearChunkData& data, bool hasChem) {
  if (!ReadPOD(in, data.temperature)) return false;
  if (!ReadPOD(in, data.humidity)) return false;
  for (float& v : data.concentrations) {
    if (!ReadPOD(in, v)) return false;
  }
  if (!ReadPOD(in, data.lastUpdatedStep)) return false;
  if (!ReadPOD(in, data.lastSimTime)) return false;
  uint8_t dirty = 0;
  if (!ReadPOD(in, dirty)) return false;
  data.dirty = dirty != 0;
  if (hasChem) {
    return ReadChemState(in, data.chem);
  }
  data.chem = ChemState{};
  return true;
}

void WriteMidChunk(std::ofstream& out, const sim::MidChunkData& data) {
  WritePOD(out, data.temperature);
  WritePOD(out, data.humidity);
  WritePOD(out, data.biomass);
  WritePOD(out, data.lastUpdatedStep);
  WritePOD(out, data.lastSimTime);
  const uint8_t dirty = data.dirty ? 1 : 0;
  WritePOD(out, dirty);
  WriteChemState(out, data.chem);
}

bool ReadMidChunk(std::ifstream& in, sim::MidChunkData& data, bool hasChem) {
  if (!ReadPOD(in, data.temperature)) return false;
  if (!ReadPOD(in, data.humidity)) return false;
  if (!ReadPOD(in, data.biomass)) return false;
  if (!ReadPOD(in, data.lastUpdatedStep)) return false;
  if (!ReadPOD(in, data.lastSimTime)) return false;
  uint8_t dirty = 0;
  if (!ReadPOD(in, dirty)) return false;
  data.dirty = dirty != 0;
  if (hasChem) {
    return ReadChemState(in, data.chem);
  }
  data.chem = ChemState{};
  return true;
}

void WriteFarChunk(std::ofstream& out, const sim::FarTileData& data) {
  WritePOD(out, data.avgTemperature);
  WritePOD(out, data.co2);
  WritePOD(out, data.population);
  WritePOD(out, data.lastUpdatedStep);
  WritePOD(out, data.lastSimTime);
  const uint8_t dirty = data.dirty ? 1 : 0;
  WritePOD(out, dirty);
  WriteChemState(out, data.chem);
}

bool ReadFarChunk(std::ifstream& in, sim::FarTileData& data, bool hasChem) {
  if (!ReadPOD(in, data.avgTemperature)) return false;
  if (!ReadPOD(in, data.co2)) return false;
  if (!ReadPOD(in, data.population)) return false;
  if (!ReadPOD(in, data.lastUpdatedStep)) return false;
  if (!ReadPOD(in, data.lastSimTime)) return false;
  uint8_t dirty = 0;
  if (!ReadPOD(in, dirty)) return false;
  data.dirty = dirty != 0;
  if (hasChem) {
    return ReadChemState(in, data.chem);
  }
  data.chem = ChemState{};
  return true;
}

void WriteNearMap(std::ofstream& out, const std::unordered_map<world::ChunkKey, sim::NearChunkData, world::ChunkKeyHasher>& map) {
  const uint64_t count = static_cast<uint64_t>(map.size());
  WritePOD(out, count);
  for (const auto& [key, value] : map) {
    WritePOD(out, key);
    WriteNearChunk(out, value);
  }
}

bool ReadNearMap(std::ifstream& in, std::unordered_map<world::ChunkKey, sim::NearChunkData, world::ChunkKeyHasher>& map, bool hasChem) {
  uint64_t count = 0;
  if (!ReadPOD(in, count)) return false;
  map.clear();
  for (uint64_t i = 0; i < count; ++i) {
    world::ChunkKey key{};
    sim::NearChunkData value{};
    if (!ReadPOD(in, key)) return false;
    if (!ReadNearChunk(in, value, hasChem)) return false;
    map.emplace(key, value);
  }
  return true;
}

void WriteMidMap(std::ofstream& out, const std::unordered_map<world::ChunkKey, sim::MidChunkData, world::ChunkKeyHasher>& map) {
  const uint64_t count = static_cast<uint64_t>(map.size());
  WritePOD(out, count);
  for (const auto& [key, value] : map) {
    WritePOD(out, key);
    WriteMidChunk(out, value);
  }
}

bool ReadMidMap(std::ifstream& in, std::unordered_map<world::ChunkKey, sim::MidChunkData, world::ChunkKeyHasher>& map, bool hasChem) {
  uint64_t count = 0;
  if (!ReadPOD(in, count)) return false;
  map.clear();
  for (uint64_t i = 0; i < count; ++i) {
    world::ChunkKey key{};
    sim::MidChunkData value{};
    if (!ReadPOD(in, key)) return false;
    if (!ReadMidChunk(in, value, hasChem)) return false;
    map.emplace(key, value);
  }
  return true;
}

void WriteFarMap(std::ofstream& out, const std::unordered_map<world::ChunkKey, sim::FarTileData, world::ChunkKeyHasher>& map) {
  const uint64_t count = static_cast<uint64_t>(map.size());
  WritePOD(out, count);
  for (const auto& [key, value] : map) {
    WritePOD(out, key);
    WriteFarChunk(out, value);
  }
}

bool ReadFarMap(std::ifstream& in, std::unordered_map<world::ChunkKey, sim::FarTileData, world::ChunkKeyHasher>& map, bool hasChem) {
  uint64_t count = 0;
  if (!ReadPOD(in, count)) return false;
  map.clear();
  for (uint64_t i = 0; i < count; ++i) {
    world::ChunkKey key{};
    sim::FarTileData value{};
    if (!ReadPOD(in, key)) return false;
    if (!ReadFarChunk(in, value, hasChem)) return false;
    map.emplace(key, value);
  }
  return true;
}

bool NearlyEqual(float a, float b, float eps = 1e-6f) {
  return std::abs(a - b) <= eps;
}

bool NearlyEqual(double a, double b, double eps = 1e-9) {
  return std::abs(a - b) <= eps;
}

bool ChemEqual(const ChemState& a, const ChemState& b) {
  if (!NearlyEqual(a.temperature, b.temperature)) return false;
  if (!NearlyEqual(a.pressure, b.pressure)) return false;
  if (!NearlyEqual(a.pH, b.pH)) return false;
  if (!NearlyEqual(a.eh, b.eh)) return false;
  if (!NearlyEqual(a.ionicStrength, b.ionicStrength)) return false;
  if (a.dormant != b.dormant) return false;
  if (a.species.size() != b.species.size()) return false;
  for (std::size_t i = 0; i < a.species.size(); ++i) {
    if (a.species[i].id != b.species[i].id) return false;
    if (!NearlyEqual(a.species[i].concentration, b.species[i].concentration)) return false;
  }
  return true;
}

bool NearEqual(const sim::NearChunkData& a, const sim::NearChunkData& b) {
  if (!NearlyEqual(a.temperature, b.temperature)) return false;
  if (!NearlyEqual(a.humidity, b.humidity)) return false;
  for (int i = 0; i < 4; ++i) {
    if (!NearlyEqual(a.concentrations[i], b.concentrations[i])) return false;
  }
  if (a.lastUpdatedStep != b.lastUpdatedStep) return false;
  if (!NearlyEqual(a.lastSimTime, b.lastSimTime)) return false;
  if (a.dirty != b.dirty) return false;
  if (!ChemEqual(a.chem, b.chem)) return false;
  return true;
}

bool MidEqual(const sim::MidChunkData& a, const sim::MidChunkData& b) {
  if (!NearlyEqual(a.temperature, b.temperature)) return false;
  if (!NearlyEqual(a.humidity, b.humidity)) return false;
  if (!NearlyEqual(a.biomass, b.biomass)) return false;
  if (a.lastUpdatedStep != b.lastUpdatedStep) return false;
  if (!NearlyEqual(a.lastSimTime, b.lastSimTime)) return false;
  if (a.dirty != b.dirty) return false;
  if (!ChemEqual(a.chem, b.chem)) return false;
  return true;
}

bool FarEqual(const sim::FarTileData& a, const sim::FarTileData& b) {
  if (!NearlyEqual(a.avgTemperature, b.avgTemperature)) return false;
  if (!NearlyEqual(a.co2, b.co2)) return false;
  if (!NearlyEqual(a.population, b.population)) return false;
  if (a.lastUpdatedStep != b.lastUpdatedStep) return false;
  if (!NearlyEqual(a.lastSimTime, b.lastSimTime)) return false;
  if (a.dirty != b.dirty) return false;
  if (!ChemEqual(a.chem, b.chem)) return false;
  return true;
}

class JsonParser {
public:
  explicit JsonParser(std::string input) : input_(std::move(input)) {}

  WorldSnapshot ParseSnapshot() {
    WorldSnapshot snapshot{};
    SkipWs();
    Expect('{');
    while (true) {
      SkipWs();
      if (Peek() == '}') {
        Get();
        break;
      }
      std::string key = ParseString();
      SkipWs();
      Expect(':');
      SkipWs();
      if (key == "simTime") {
        snapshot.simTime = ParseNumber();
      } else if (key == "near") {
        ParseNear(snapshot);
      } else if (key == "mid") {
        ParseMid(snapshot);
      } else if (key == "far") {
        ParseFar(snapshot);
      } else {
        SkipValue();
      }
      SkipWs();
      if (Peek() == ',') {
        Get();
        continue;
      }
      if (Peek() == '}') {
        Get();
        break;
      }
    }
    return snapshot;
  }

private:
  char Peek() const {
    if (pos_ >= input_.size()) return '\0';
    return input_[pos_];
  }

  char Get() {
    if (pos_ >= input_.size()) return '\0';
    return input_[pos_++];
  }

  void SkipWs() {
    while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_]))) {
      ++pos_;
    }
  }

  void Expect(char c) {
    SkipWs();
    if (Get() != c) {
      throw std::runtime_error("JSON parse error");
    }
  }

  std::string ParseString() {
    SkipWs();
    if (Get() != '"') {
      throw std::runtime_error("JSON string expected");
    }
    std::string result;
    while (pos_ < input_.size()) {
      char c = Get();
      if (c == '"') break;
      result.push_back(c);
    }
    return result;
  }

  double ParseNumber() {
    SkipWs();
    std::size_t start = pos_;
    while (pos_ < input_.size()) {
      char c = input_[pos_];
      if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E')) {
        break;
      }
      ++pos_;
    }
    return std::stod(input_.substr(start, pos_ - start));
  }

  bool ParseBool() {
    SkipWs();
    if (input_.compare(pos_, 4, "true") == 0) {
      pos_ += 4;
      return true;
    }
    if (input_.compare(pos_, 5, "false") == 0) {
      pos_ += 5;
      return false;
    }
    throw std::runtime_error("JSON bool expected");
  }

  void SkipValue() {
    SkipWs();
    char c = Peek();
    if (c == '{') {
      Expect('{');
      int depth = 1;
      while (depth > 0 && pos_ < input_.size()) {
        char t = Get();
        if (t == '{') depth++;
        if (t == '}') depth--;
      }
      return;
    }
    if (c == '[') {
      Expect('[');
      int depth = 1;
      while (depth > 0 && pos_ < input_.size()) {
        char t = Get();
        if (t == '[') depth++;
        if (t == ']') depth--;
      }
      return;
    }
    if (c == '"') {
      ParseString();
      return;
    }
    if (std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+') {
      ParseNumber();
      return;
    }
    ParseBool();
  }

  ChemState ParseChem() {
    ChemState state{};
    Expect('{');
    while (true) {
      SkipWs();
      if (Peek() == '}') {
        Get();
        break;
      }
      std::string field = ParseString();
      Expect(':');
      if (field == "temperature") state.temperature = ParseNumber();
      else if (field == "pressure") state.pressure = ParseNumber();
      else if (field == "pH") state.pH = ParseNumber();
      else if (field == "eh") state.eh = ParseNumber();
      else if (field == "ionicStrength") state.ionicStrength = ParseNumber();
      else if (field == "dormant") state.dormant = ParseBool();
      else if (field == "species") {
        Expect('[');
        SkipWs();
        if (Peek() != ']') {
          while (true) {
            Expect('{');
            SpeciesAmount s{};
            while (true) {
              std::string skey = ParseString();
              Expect(':');
              if (skey == "id") s.id = static_cast<int>(ParseNumber());
              else if (skey == "concentration") s.concentration = static_cast<float>(ParseNumber());
              else SkipValue();
              SkipWs();
              if (Peek() == ',') {
                Get();
                continue;
              }
              if (Peek() == '}') {
                Get();
                break;
              }
            }
            state.species.push_back(s);
            SkipWs();
            if (Peek() == ',') {
              Get();
              continue;
            }
            if (Peek() == ']') {
              break;
            }
          }
        }
        Expect(']');
      } else {
        SkipValue();
      }

      SkipWs();
      if (Peek() == ',') {
        Get();
        continue;
      }
      if (Peek() == '}') {
        Get();
        break;
      }
    }
    state.Compact();
    return state;
  }

  void ParseNear(WorldSnapshot& snapshot) {
    Expect('[');
    SkipWs();
    if (Peek() == ']') {
      Get();
      return;
    }
    while (true) {
      world::ChunkKey key{};
      sim::NearChunkData data{};
      Expect('{');
      while (true) {
        std::string field = ParseString();
        Expect(':');
        if (field == "face") key.face = static_cast<int>(ParseNumber());
        else if (field == "x") key.x = static_cast<int>(ParseNumber());
        else if (field == "y") key.y = static_cast<int>(ParseNumber());
        else if (field == "lod") key.lod = static_cast<int>(ParseNumber());
        else if (field == "temperature") data.temperature = static_cast<float>(ParseNumber());
        else if (field == "humidity") data.humidity = static_cast<float>(ParseNumber());
        else if (field == "concentrations") {
          Expect('[');
          for (int i = 0; i < 4; ++i) {
            data.concentrations[i] = static_cast<float>(ParseNumber());
            SkipWs();
            if (i < 3) Expect(',');
          }
          Expect(']');
        } else if (field == "lastUpdatedStep") data.lastUpdatedStep = static_cast<uint32_t>(ParseNumber());
        else if (field == "lastSimTime") data.lastSimTime = ParseNumber();
        else if (field == "dirty") data.dirty = ParseBool();
        else if (field == "chem") data.chem = ParseChem();
        else SkipValue();

        SkipWs();
        if (Peek() == ',') {
          Get();
          continue;
        }
        if (Peek() == '}') {
          Get();
          break;
        }
      }
      snapshot.near.emplace(key, data);
      SkipWs();
      if (Peek() == ',') {
        Get();
        continue;
      }
      if (Peek() == ']') {
        Get();
        break;
      }
    }
  }

  void ParseMid(WorldSnapshot& snapshot) {
    Expect('[');
    SkipWs();
    if (Peek() == ']') {
      Get();
      return;
    }
    while (true) {
      world::ChunkKey key{};
      sim::MidChunkData data{};
      Expect('{');
      while (true) {
        std::string field = ParseString();
        Expect(':');
        if (field == "face") key.face = static_cast<int>(ParseNumber());
        else if (field == "x") key.x = static_cast<int>(ParseNumber());
        else if (field == "y") key.y = static_cast<int>(ParseNumber());
        else if (field == "lod") key.lod = static_cast<int>(ParseNumber());
        else if (field == "temperature") data.temperature = static_cast<float>(ParseNumber());
        else if (field == "humidity") data.humidity = static_cast<float>(ParseNumber());
        else if (field == "biomass") data.biomass = static_cast<float>(ParseNumber());
        else if (field == "lastUpdatedStep") data.lastUpdatedStep = static_cast<uint32_t>(ParseNumber());
        else if (field == "lastSimTime") data.lastSimTime = ParseNumber();
        else if (field == "dirty") data.dirty = ParseBool();
        else if (field == "chem") data.chem = ParseChem();
        else SkipValue();

        SkipWs();
        if (Peek() == ',') {
          Get();
          continue;
        }
        if (Peek() == '}') {
          Get();
          break;
        }
      }
      snapshot.mid.emplace(key, data);
      SkipWs();
      if (Peek() == ',') {
        Get();
        continue;
      }
      if (Peek() == ']') {
        Get();
        break;
      }
    }
  }

  void ParseFar(WorldSnapshot& snapshot) {
    Expect('[');
    SkipWs();
    if (Peek() == ']') {
      Get();
      return;
    }
    while (true) {
      world::ChunkKey key{};
      sim::FarTileData data{};
      Expect('{');
      while (true) {
        std::string field = ParseString();
        Expect(':');
        if (field == "face") key.face = static_cast<int>(ParseNumber());
        else if (field == "x") key.x = static_cast<int>(ParseNumber());
        else if (field == "y") key.y = static_cast<int>(ParseNumber());
        else if (field == "lod") key.lod = static_cast<int>(ParseNumber());
        else if (field == "avgTemperature") data.avgTemperature = static_cast<float>(ParseNumber());
        else if (field == "co2") data.co2 = static_cast<float>(ParseNumber());
        else if (field == "population") data.population = static_cast<float>(ParseNumber());
        else if (field == "lastUpdatedStep") data.lastUpdatedStep = static_cast<uint32_t>(ParseNumber());
        else if (field == "lastSimTime") data.lastSimTime = ParseNumber();
        else if (field == "dirty") data.dirty = ParseBool();
        else if (field == "chem") data.chem = ParseChem();
        else SkipValue();

        SkipWs();
        if (Peek() == ',') {
          Get();
          continue;
        }
        if (Peek() == '}') {
          Get();
          break;
        }
      }
      snapshot.far.emplace(key, data);
      SkipWs();
      if (Peek() == ',') {
        Get();
        continue;
      }
      if (Peek() == ']') {
        Get();
        break;
      }
    }
  }

  std::string input_;
  std::size_t pos_{0};
};

void WriteChemJson(std::ostream& out, const ChemState& state) {
  out << "{\"temperature\": " << state.temperature
      << ", \"pressure\": " << state.pressure
      << ", \"pH\": " << state.pH
      << ", \"eh\": " << state.eh
      << ", \"ionicStrength\": " << state.ionicStrength
      << ", \"dormant\": " << (state.dormant ? "true" : "false")
      << ", \"species\": [";
  bool first = true;
  for (const auto& s : state.species) {
    if (!first) out << ", ";
    first = false;
    out << "{\"id\": " << s.id << ", \"concentration\": " << s.concentration << "}";
  }
  out << "]}";
}

void WriteSnapshotJson(std::ostream& out, const WorldSnapshot& snapshot) {
  out << "{\n";
  out << "  \"simTime\": " << snapshot.simTime << ",\n";

  auto writeNear = [&](const auto& map) {
    out << "  \"near\": [\n";
    bool first = true;
    for (const auto& [key, data] : map) {
      if (!first) out << ",\n";
      first = false;
      out << "    {\"face\": " << key.face
          << ", \"x\": " << key.x
          << ", \"y\": " << key.y
          << ", \"lod\": " << key.lod
          << ", \"temperature\": " << data.temperature
          << ", \"humidity\": " << data.humidity
          << ", \"concentrations\": [" << data.concentrations[0] << ", " << data.concentrations[1]
          << ", " << data.concentrations[2] << ", " << data.concentrations[3] << "]"
          << ", \"lastUpdatedStep\": " << data.lastUpdatedStep
          << ", \"lastSimTime\": " << data.lastSimTime
          << ", \"dirty\": " << (data.dirty ? "true" : "false")
          << ", \"chem\": ";
      WriteChemJson(out, data.chem);
      out << "}";
    }
    out << "\n  ],\n";
  };

  auto writeMid = [&](const auto& map) {
    out << "  \"mid\": [\n";
    bool first = true;
    for (const auto& [key, data] : map) {
      if (!first) out << ",\n";
      first = false;
      out << "    {\"face\": " << key.face
          << ", \"x\": " << key.x
          << ", \"y\": " << key.y
          << ", \"lod\": " << key.lod
          << ", \"temperature\": " << data.temperature
          << ", \"humidity\": " << data.humidity
          << ", \"biomass\": " << data.biomass
          << ", \"lastUpdatedStep\": " << data.lastUpdatedStep
          << ", \"lastSimTime\": " << data.lastSimTime
          << ", \"dirty\": " << (data.dirty ? "true" : "false")
          << ", \"chem\": ";
      WriteChemJson(out, data.chem);
      out << "}";
    }
    out << "\n  ],\n";
  };

  auto writeFar = [&](const auto& map) {
    out << "  \"far\": [\n";
    bool first = true;
    for (const auto& [key, data] : map) {
      if (!first) out << ",\n";
      first = false;
      out << "    {\"face\": " << key.face
          << ", \"x\": " << key.x
          << ", \"y\": " << key.y
          << ", \"lod\": " << key.lod
          << ", \"avgTemperature\": " << data.avgTemperature
          << ", \"co2\": " << data.co2
          << ", \"population\": " << data.population
          << ", \"lastUpdatedStep\": " << data.lastUpdatedStep
          << ", \"lastSimTime\": " << data.lastSimTime
          << ", \"dirty\": " << (data.dirty ? "true" : "false")
          << ", \"chem\": ";
      WriteChemJson(out, data.chem);
      out << "}";
    }
    out << "\n  ]\n";
  };

  writeNear(snapshot.near);
  writeMid(snapshot.mid);
  writeFar(snapshot.far);

  out << "}\n";
}

} // namespace

std::future<void> SnapshotStore::SaveSnapshotAsync(const std::string& path, WorldSnapshot snapshot) {
  return std::async(std::launch::async, [path, snapshot = std::move(snapshot)]() mutable {
    SnapshotStore store;
    store.SaveSnapshot(path, snapshot);
  });
}

void SnapshotStore::SaveSnapshot(const std::string& path, const WorldSnapshot& snapshot) {
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return;
  }

  const uint32_t magic = 0x54505241; // "TPRA"
  const uint32_t version = 2;
  WritePOD(out, magic);
  WritePOD(out, version);
  WritePOD(out, snapshot.simTime);

  WriteNearMap(out, snapshot.near);
  WriteMidMap(out, snapshot.mid);
  WriteFarMap(out, snapshot.far);
}

WorldSnapshot SnapshotStore::LoadSnapshot(const std::string& path) {
  WorldSnapshot snapshot{};
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return snapshot;
  }

  uint32_t magic = 0;
  uint32_t version = 0;
  if (!ReadPOD(in, magic) || !ReadPOD(in, version)) {
    return snapshot;
  }
  if (magic != 0x54505241 || (version != 1 && version != 2)) {
    return snapshot;
  }
  if (!ReadPOD(in, snapshot.simTime)) {
    return snapshot;
  }

  const bool hasChem = (version >= 2);
  ReadNearMap(in, snapshot.near, hasChem);
  ReadMidMap(in, snapshot.mid, hasChem);
  ReadFarMap(in, snapshot.far, hasChem);
  return snapshot;
}

void SnapshotStore::SaveSnapshotJson(const std::string& path, const WorldSnapshot& snapshot) {
  std::ofstream out(path, std::ios::trunc);
  if (!out) {
    return;
  }
  WriteSnapshotJson(out, snapshot);
}

WorldSnapshot SnapshotStore::LoadSnapshotJson(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    return {};
  }
  std::stringstream buffer;
  buffer << in.rdbuf();
  try {
    JsonParser parser(buffer.str());
    return parser.ParseSnapshot();
  } catch (...) {
    return {};
  }
}

void SnapshotStore::SaveDelta(const std::string& path, const DeltaSnapshot& delta) {
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return;
  }
  const uint32_t magic = 0x54505244; // "TPRD"
  const uint32_t version = 2;
  WritePOD(out, magic);
  WritePOD(out, version);
  WritePOD(out, delta.baseSimTime);
  WritePOD(out, delta.targetSimTime);

  const uint64_t nearCount = static_cast<uint64_t>(delta.nearUpserts.size());
  WritePOD(out, nearCount);
  for (const auto& entry : delta.nearUpserts) {
    WritePOD(out, entry.key);
    WriteNearChunk(out, entry.value);
  }
  WriteVector(out, delta.nearRemoved);

  const uint64_t midCount = static_cast<uint64_t>(delta.midUpserts.size());
  WritePOD(out, midCount);
  for (const auto& entry : delta.midUpserts) {
    WritePOD(out, entry.key);
    WriteMidChunk(out, entry.value);
  }
  WriteVector(out, delta.midRemoved);

  const uint64_t farCount = static_cast<uint64_t>(delta.farUpserts.size());
  WritePOD(out, farCount);
  for (const auto& entry : delta.farUpserts) {
    WritePOD(out, entry.key);
    WriteFarChunk(out, entry.value);
  }
  WriteVector(out, delta.farRemoved);
}

DeltaSnapshot SnapshotStore::LoadDelta(const std::string& path) {
  DeltaSnapshot delta{};
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return delta;
  }
  uint32_t magic = 0;
  uint32_t version = 0;
  if (!ReadPOD(in, magic) || !ReadPOD(in, version)) {
    return delta;
  }
  if (magic != 0x54505244 || (version != 1 && version != 2)) {
    return delta;
  }
  if (!ReadPOD(in, delta.baseSimTime) || !ReadPOD(in, delta.targetSimTime)) {
    return delta;
  }

  const bool hasChem = (version >= 2);
  uint64_t nearCount = 0;
  if (!ReadPOD(in, nearCount)) return delta;
  delta.nearUpserts.clear();
  delta.nearUpserts.reserve(nearCount);
  for (uint64_t i = 0; i < nearCount; ++i) {
    NearDeltaEntry entry{};
    if (!ReadPOD(in, entry.key)) return delta;
    if (!ReadNearChunk(in, entry.value, hasChem)) return delta;
    delta.nearUpserts.push_back(entry);
  }
  ReadVector(in, delta.nearRemoved);

  uint64_t midCount = 0;
  if (!ReadPOD(in, midCount)) return delta;
  delta.midUpserts.clear();
  delta.midUpserts.reserve(midCount);
  for (uint64_t i = 0; i < midCount; ++i) {
    MidDeltaEntry entry{};
    if (!ReadPOD(in, entry.key)) return delta;
    if (!ReadMidChunk(in, entry.value, hasChem)) return delta;
    delta.midUpserts.push_back(entry);
  }
  ReadVector(in, delta.midRemoved);

  uint64_t farCount = 0;
  if (!ReadPOD(in, farCount)) return delta;
  delta.farUpserts.clear();
  delta.farUpserts.reserve(farCount);
  for (uint64_t i = 0; i < farCount; ++i) {
    FarDeltaEntry entry{};
    if (!ReadPOD(in, entry.key)) return delta;
    if (!ReadFarChunk(in, entry.value, hasChem)) return delta;
    delta.farUpserts.push_back(entry);
  }
  ReadVector(in, delta.farRemoved);
  return delta;
}

DeltaSnapshot CreateDelta(const WorldSnapshot& base, const WorldSnapshot& current) {
  DeltaSnapshot delta{};
  delta.baseSimTime = base.simTime;
  delta.targetSimTime = current.simTime;

  for (const auto& [key, value] : current.near) {
    auto it = base.near.find(key);
    if (it == base.near.end() || !NearEqual(it->second, value)) {
      delta.nearUpserts.push_back({key, value});
    }
  }
  for (const auto& [key, value] : base.near) {
    if (current.near.find(key) == current.near.end()) {
      delta.nearRemoved.push_back(key);
    }
  }

  for (const auto& [key, value] : current.mid) {
    auto it = base.mid.find(key);
    if (it == base.mid.end() || !MidEqual(it->second, value)) {
      delta.midUpserts.push_back({key, value});
    }
  }
  for (const auto& [key, value] : base.mid) {
    if (current.mid.find(key) == current.mid.end()) {
      delta.midRemoved.push_back(key);
    }
  }

  for (const auto& [key, value] : current.far) {
    auto it = base.far.find(key);
    if (it == base.far.end() || !FarEqual(it->second, value)) {
      delta.farUpserts.push_back({key, value});
    }
  }
  for (const auto& [key, value] : base.far) {
    if (current.far.find(key) == current.far.end()) {
      delta.farRemoved.push_back(key);
    }
  }

  return delta;
}

void ApplyDelta(WorldSnapshot& base, const DeltaSnapshot& delta) {
  for (const auto& entry : delta.nearUpserts) {
    base.near[entry.key] = entry.value;
  }
  for (const auto& key : delta.nearRemoved) {
    base.near.erase(key);
  }

  for (const auto& entry : delta.midUpserts) {
    base.mid[entry.key] = entry.value;
  }
  for (const auto& key : delta.midRemoved) {
    base.mid.erase(key);
  }

  for (const auto& entry : delta.farUpserts) {
    base.far[entry.key] = entry.value;
  }
  for (const auto& key : delta.farRemoved) {
    base.far.erase(key);
  }

  base.simTime = delta.targetSimTime;
}

} // namespace terra::io
