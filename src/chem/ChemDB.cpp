#include "terra/chem/ChemDB.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

namespace terra::chem {

namespace fs = std::filesystem;

namespace {

constexpr uint32_t kMagic = 0x4348454D; // CHEM
constexpr uint32_t kVersion = 1;

void WriteString(std::ofstream& out, const std::string& value) {
  const uint32_t len = static_cast<uint32_t>(value.size());
  out.write(reinterpret_cast<const char*>(&len), sizeof(len));
  if (len > 0) {
    out.write(value.data(), len);
  }
}

bool ReadString(std::ifstream& in, std::string& value) {
  uint32_t len = 0;
  if (!in.read(reinterpret_cast<char*>(&len), sizeof(len))) {
    return false;
  }
  value.resize(len);
  if (len > 0) {
    if (!in.read(value.data(), len)) {
      return false;
    }
  }
  return true;
}

template <typename T>
void WriteVector(std::ofstream& out, const std::vector<T>& vec) {
  const uint32_t count = static_cast<uint32_t>(vec.size());
  out.write(reinterpret_cast<const char*>(&count), sizeof(count));
  if (!vec.empty()) {
    out.write(reinterpret_cast<const char*>(vec.data()), sizeof(T) * vec.size());
  }
}

template <typename T>
bool ReadVector(std::ifstream& in, std::vector<T>& vec) {
  uint32_t count = 0;
  if (!in.read(reinterpret_cast<char*>(&count), sizeof(count))) {
    return false;
  }
  vec.resize(count);
  if (count > 0) {
    if (!in.read(reinterpret_cast<char*>(vec.data()), sizeof(T) * vec.size())) {
      return false;
    }
  }
  return true;
}

int ExtractIndexFromId(const std::string& line) {
  const auto pos = line.find("index-");
  if (pos == std::string::npos) {
    return -1;
  }
  return std::stoi(line.substr(pos + 6));
}

std::string ExtractMoleculeId(const std::string& line) {
  auto pos = line.find('_');
  if (pos == std::string::npos) {
    return line;
  }
  return line.substr(0, pos);
}

std::string LoadTextFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return {};
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

std::string StripTrailingCommas(const std::string& input) {
  std::string out;
  out.reserve(input.size());
  bool inString = false;
  bool escape = false;
  for (std::size_t i = 0; i < input.size(); ++i) {
    const char c = input[i];
    if (inString) {
      out.push_back(c);
      if (escape) {
        escape = false;
      } else if (c == '\\') {
        escape = true;
      } else if (c == '"') {
        inString = false;
      }
      continue;
    }
    if (c == '"') {
      inString = true;
      out.push_back(c);
      continue;
    }
    if (c == ',') {
      std::size_t j = i + 1;
      while (j < input.size() && std::isspace(static_cast<unsigned char>(input[j]))) {
        ++j;
      }
      if (j < input.size() && (input[j] == '}' || input[j] == ']')) {
        continue;
      }
    }
    out.push_back(c);
  }
  return out;
}

nlohmann::json LoadJson(const std::string& path);

std::string SanitizeJson(const std::string& input) {
  std::string out;
  out.reserve(input.size());
  bool inDouble = false;
  bool inSingle = false;
  bool escape = false;
  for (std::size_t i = 0; i < input.size(); ++i) {
    const char c = input[i];
    if (inDouble) {
      out.push_back(c);
      if (escape) {
        escape = false;
      } else if (c == '\\') {
        escape = true;
      } else if (c == '"') {
        inDouble = false;
      }
      continue;
    }
    if (inSingle) {
      if (escape) {
        out.push_back(c);
        escape = false;
        continue;
      }
      if (c == '\\') {
        out.push_back(c);
        escape = true;
        continue;
      }
      if (c == '\'') {
        out.push_back('"');
        inSingle = false;
        continue;
      }
      if (c == '"') {
        out.push_back('\\');
        out.push_back('"');
        continue;
      }
      out.push_back(c);
      continue;
    }

    if (c == '"') {
      inDouble = true;
      out.push_back(c);
      continue;
    }
    if (c == '\'') {
      inSingle = true;
      out.push_back('"');
      continue;
    }
    out.push_back(c);
  }
  return StripTrailingCommas(out);
}

nlohmann::json LoadJson(const std::string& path) {
  const std::string text = LoadTextFile(path);
  if (text.empty()) {
    return {};
  }
  const std::string sanitized = SanitizeJson(text);
  return nlohmann::json::parse(sanitized, nullptr, true, true);
}

int GetInt(const nlohmann::json& obj, const char* key, int defaultValue) {
  auto it = obj.find(key);
  if (it == obj.end() || it->is_null()) {
    return defaultValue;
  }
  if (it->is_number_integer()) {
    return it->get<int>();
  }
  if (it->is_number()) {
    return static_cast<int>(it->get<double>());
  }
  if (it->is_string()) {
    try {
      return std::stoi(it->get<std::string>());
    } catch (...) {
      return defaultValue;
    }
  }
  return defaultValue;
}

double GetDouble(const nlohmann::json& obj, const char* key, double defaultValue) {
  auto it = obj.find(key);
  if (it == obj.end() || it->is_null()) {
    return defaultValue;
  }
  if (it->is_number()) {
    return it->get<double>();
  }
  if (it->is_string()) {
    try {
      return std::stod(it->get<std::string>());
    } catch (...) {
      return defaultValue;
    }
  }
  return defaultValue;
}

std::string GetString(const nlohmann::json& obj, const char* key, const std::string& defaultValue) {
  auto it = obj.find(key);
  if (it == obj.end() || it->is_null()) {
    return defaultValue;
  }
  if (it->is_string()) {
    return it->get<std::string>();
  }
  return defaultValue;
}

void ParseElementsJson(const std::string& path, std::vector<Element>& elements) {
  nlohmann::json root = LoadJson(path);
  if (!root.is_array()) {
    return;
  }
  for (const auto& item : root) {
    Element element{};
    element.atomicNumber = GetInt(item, "atomic_number", 0);
    element.symbol = GetString(item, "symbol", {});
    element.name = GetString(item, "name", {});
    element.block = GetString(item, "block", {});
    element.atomicWeight = GetDouble(item, "atomic_weight", 0.0);
    element.atomicRadius = GetDouble(item, "atomic_radius", 0.0);
    element.electronegativity = GetDouble(item, "en_pauling", 0.0);
    element.electronAffinity = GetDouble(item, "electron_affinity", 0.0);
    element.density = GetDouble(item, "density", 0.0);
    element.heatCapacity = GetDouble(item, "specific_heat_capacity", 0.0);
    element.thermalConductivity = GetDouble(item, "thermal_conductivity", 0.0);
    elements.push_back(std::move(element));
  }
}

void ParseOxidationStates(const std::string& path, std::unordered_map<int, std::vector<int>>& out) {
  nlohmann::json root = LoadJson(path);
  if (!root.is_array()) {
    return;
  }
  for (const auto& item : root) {
    const int z = GetInt(item, "atomic_number", 0);
    const int state = GetInt(item, "oxidation_state", 0);
    out[z].push_back(state);
  }
}

void ParseIonizationEnergies(const std::string& path, std::unordered_map<int, std::vector<double>>& out) {
  nlohmann::json root = LoadJson(path);
  if (!root.is_array()) {
    return;
  }
  for (const auto& item : root) {
    const int z = GetInt(item, "atomic_number", 0);
    const double energy = GetDouble(item, "ionization_energy", 0.0);
    out[z].push_back(energy);
  }
}

void ParsePhaseTransitions(const std::string& path, std::unordered_map<int, std::pair<double, double>>& out) {
  nlohmann::json root = LoadJson(path);
  if (!root.is_array()) {
    return;
  }
  for (const auto& item : root) {
    const int z = GetInt(item, "atomic_number", 0);
    const double melting = GetDouble(item, "melting_point", 0.0);
    const double boiling = GetDouble(item, "boiling_point", 0.0);
    auto it = out.find(z);
    if (it == out.end()) {
      out[z] = {melting, boiling};
    } else {
      if (melting > 0.0 && (it->second.first <= 0.0 || melting < it->second.first)) {
        it->second.first = melting;
      }
      if (boiling > 0.0 && (it->second.second <= 0.0 || boiling < it->second.second)) {
        it->second.second = boiling;
      }
    }
  }
}

void ParseMoleculeAttributes(const std::string& path, std::unordered_map<int, Molecule>& out) {
  YAML::Node root = YAML::LoadFile(path);
  if (!root.IsSequence()) {
    return;
  }
  for (const auto& node : root) {
    Molecule mol{};
    mol.id = node["id"] ? node["id"].as<std::string>() : std::string{};
    mol.index = node["index"] ? node["index"].as<int>() : 0;
    mol.charge = node["charge"] ? node["charge"].as<int>() : 0;
    mol.freeEnergy = node["free_energy"] ? node["free_energy"].as<double>() : 0.0;
    mol.atomizationFreeEnergy = node["atomization_free_energy"] ? node["atomization_free_energy"].as<double>() : 0.0;
    mol.spinMultiplicity = node["spin_multiplicity"] ? node["spin_multiplicity"].as<int>() : 1;
    out[mol.index] = std::move(mol);
  }
}

void ParseSdf(const std::string& path,
              const std::unordered_map<std::string, int>& symbolToAtomic,
              std::unordered_map<int, Molecule>& out) {
  std::ifstream in(path);
  if (!in) {
    return;
  }
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) {
      continue;
    }
    const std::string header = line;
    const int index = ExtractIndexFromId(header);
    if (index < 0) {
      continue;
    }
    const std::string id = ExtractMoleculeId(header);

    Molecule mol = out.count(index) ? out[index] : Molecule{};
    mol.index = index;
    mol.id = id;

    // Skip 2 lines
    std::getline(in, line);
    std::getline(in, line);

    bool inAtomBlock = false;
    bool inBondBlock = false;
    mol.atoms.clear();
    mol.bonds.clear();

    while (std::getline(in, line)) {
      if (line.rfind("M  V30 BEGIN ATOM", 0) == 0) {
        inAtomBlock = true;
        continue;
      }
      if (line.rfind("M  V30 END ATOM", 0) == 0) {
        inAtomBlock = false;
        continue;
      }
      if (line.rfind("M  V30 BEGIN BOND", 0) == 0) {
        inBondBlock = true;
        continue;
      }
      if (line.rfind("M  V30 END BOND", 0) == 0) {
        inBondBlock = false;
        continue;
      }
      if (line.rfind("M  END", 0) == 0) {
        break;
      }
      if (inAtomBlock) {
        // M  V30 idx symbol x y z ...
        std::istringstream iss(line);
        std::string m, v30;
        int idx = 0;
        std::string symbol;
        float x = 0, y = 0, z = 0;
        if (!(iss >> m >> v30 >> idx >> symbol >> x >> y >> z)) {
          continue;
        }
        Atom atom{};
        auto it = symbolToAtomic.find(symbol);
        atom.atomicNumber = (it != symbolToAtomic.end()) ? it->second : 0;
        atom.x = x;
        atom.y = y;
        atom.z = z;
        mol.atoms.push_back(atom);
      }
      if (inBondBlock) {
        std::istringstream iss(line);
        std::string m, v30;
        int idx = 0;
        int order = 1;
        int a = 0, b = 0;
        if (!(iss >> m >> v30 >> idx >> order >> a >> b)) {
          continue;
        }
        Bond bond{};
        bond.order = order;
        bond.a = a - 1;
        bond.b = b - 1;
        mol.bonds.push_back(bond);
      }
    }

    out[index] = std::move(mol);

    // consume until end of record
    while (std::getline(in, line)) {
      if (line == "$$$$") {
        break;
      }
    }
  }
}

void ParseReactions(const std::string& path, std::vector<Reaction>& out) {
  YAML::Node root = YAML::LoadFile(path);
  if (!root.IsSequence()) {
    return;
  }
  out.reserve(root.size());
  for (const auto& node : root) {
    Reaction reaction{};
    reaction.id = node["id"] ? node["id"].as<std::string>() : std::string{};
    reaction.deltaG = node["value"] ? node["value"].as<double>() : 0.0;
    if (node["reactants"]) {
      for (const auto& r : node["reactants"]) {
        reaction.reactants.push_back(r.as<int>());
      }
    }
    if (node["products"]) {
      for (const auto& p : node["products"]) {
        reaction.products.push_back(p.as<int>());
      }
    }
    out.push_back(std::move(reaction));
  }
}

} // namespace

bool ChemDB::LoadCache(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  uint32_t magic = 0;
  uint32_t version = 0;
  in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
  in.read(reinterpret_cast<char*>(&version), sizeof(version));
  if (magic != kMagic || version != kVersion) {
    return false;
  }
  uint64_t elementCount = 0;
  uint64_t moleculeCount = 0;
  uint64_t reactionCount = 0;
  in.read(reinterpret_cast<char*>(&elementCount), sizeof(elementCount));
  in.read(reinterpret_cast<char*>(&moleculeCount), sizeof(moleculeCount));
  in.read(reinterpret_cast<char*>(&reactionCount), sizeof(reactionCount));

  elements_.clear();
  molecules_.clear();
  reactions_.clear();

  elements_.reserve(static_cast<std::size_t>(elementCount));
  molecules_.reserve(static_cast<std::size_t>(moleculeCount));
  reactions_.reserve(static_cast<std::size_t>(reactionCount));

  for (uint64_t i = 0; i < elementCount; ++i) {
    Element element{};
    in.read(reinterpret_cast<char*>(&element.atomicNumber), sizeof(element.atomicNumber));
    ReadString(in, element.symbol);
    ReadString(in, element.name);
    ReadString(in, element.block);
    in.read(reinterpret_cast<char*>(&element.atomicWeight), sizeof(element.atomicWeight));
    in.read(reinterpret_cast<char*>(&element.atomicRadius), sizeof(element.atomicRadius));
    in.read(reinterpret_cast<char*>(&element.electronegativity), sizeof(element.electronegativity));
    in.read(reinterpret_cast<char*>(&element.electronAffinity), sizeof(element.electronAffinity));
    in.read(reinterpret_cast<char*>(&element.density), sizeof(element.density));
    in.read(reinterpret_cast<char*>(&element.heatCapacity), sizeof(element.heatCapacity));
    in.read(reinterpret_cast<char*>(&element.thermalConductivity), sizeof(element.thermalConductivity));
    in.read(reinterpret_cast<char*>(&element.meltingPoint), sizeof(element.meltingPoint));
    in.read(reinterpret_cast<char*>(&element.boilingPoint), sizeof(element.boilingPoint));
    ReadVector(in, element.oxidationStates);
    ReadVector(in, element.ionizationEnergies);
    elements_.push_back(std::move(element));
  }

  for (uint64_t i = 0; i < moleculeCount; ++i) {
    Molecule mol{};
    in.read(reinterpret_cast<char*>(&mol.index), sizeof(mol.index));
    ReadString(in, mol.id);
    in.read(reinterpret_cast<char*>(&mol.charge), sizeof(mol.charge));
    in.read(reinterpret_cast<char*>(&mol.freeEnergy), sizeof(mol.freeEnergy));
    in.read(reinterpret_cast<char*>(&mol.atomizationFreeEnergy), sizeof(mol.atomizationFreeEnergy));
    in.read(reinterpret_cast<char*>(&mol.spinMultiplicity), sizeof(mol.spinMultiplicity));
    ReadVector(in, mol.atoms);
    ReadVector(in, mol.bonds);
    molecules_.push_back(std::move(mol));
  }

  for (uint64_t i = 0; i < reactionCount; ++i) {
    Reaction reaction{};
    ReadString(in, reaction.id);
    in.read(reinterpret_cast<char*>(&reaction.deltaG), sizeof(reaction.deltaG));
    in.read(reinterpret_cast<char*>(&reaction.activationEnergy), sizeof(reaction.activationEnergy));
    in.read(reinterpret_cast<char*>(&reaction.preExponential), sizeof(reaction.preExponential));
    ReadVector(in, reaction.reactants);
    ReadVector(in, reaction.products);
    reactions_.push_back(std::move(reaction));
  }

  elementByAtomicNumber_.clear();
  moleculeByIndex_.clear();
  for (std::size_t i = 0; i < elements_.size(); ++i) {
    elementByAtomicNumber_[elements_[i].atomicNumber] = i;
  }
  for (std::size_t i = 0; i < molecules_.size(); ++i) {
    moleculeByIndex_[molecules_[i].index] = i;
  }

  BuildReactionIndex();
  return true;
}

bool ChemDB::SaveCache(const std::string& path) const {
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return false;
  }
  out.write(reinterpret_cast<const char*>(&kMagic), sizeof(kMagic));
  out.write(reinterpret_cast<const char*>(&kVersion), sizeof(kVersion));
  const uint64_t elementCount = elements_.size();
  const uint64_t moleculeCount = molecules_.size();
  const uint64_t reactionCount = reactions_.size();
  out.write(reinterpret_cast<const char*>(&elementCount), sizeof(elementCount));
  out.write(reinterpret_cast<const char*>(&moleculeCount), sizeof(moleculeCount));
  out.write(reinterpret_cast<const char*>(&reactionCount), sizeof(reactionCount));

  for (const auto& element : elements_) {
    out.write(reinterpret_cast<const char*>(&element.atomicNumber), sizeof(element.atomicNumber));
    WriteString(out, element.symbol);
    WriteString(out, element.name);
    WriteString(out, element.block);
    out.write(reinterpret_cast<const char*>(&element.atomicWeight), sizeof(element.atomicWeight));
    out.write(reinterpret_cast<const char*>(&element.atomicRadius), sizeof(element.atomicRadius));
    out.write(reinterpret_cast<const char*>(&element.electronegativity), sizeof(element.electronegativity));
    out.write(reinterpret_cast<const char*>(&element.electronAffinity), sizeof(element.electronAffinity));
    out.write(reinterpret_cast<const char*>(&element.density), sizeof(element.density));
    out.write(reinterpret_cast<const char*>(&element.heatCapacity), sizeof(element.heatCapacity));
    out.write(reinterpret_cast<const char*>(&element.thermalConductivity), sizeof(element.thermalConductivity));
    out.write(reinterpret_cast<const char*>(&element.meltingPoint), sizeof(element.meltingPoint));
    out.write(reinterpret_cast<const char*>(&element.boilingPoint), sizeof(element.boilingPoint));
    WriteVector(out, element.oxidationStates);
    WriteVector(out, element.ionizationEnergies);
  }

  for (const auto& mol : molecules_) {
    out.write(reinterpret_cast<const char*>(&mol.index), sizeof(mol.index));
    WriteString(out, mol.id);
    out.write(reinterpret_cast<const char*>(&mol.charge), sizeof(mol.charge));
    out.write(reinterpret_cast<const char*>(&mol.freeEnergy), sizeof(mol.freeEnergy));
    out.write(reinterpret_cast<const char*>(&mol.atomizationFreeEnergy), sizeof(mol.atomizationFreeEnergy));
    out.write(reinterpret_cast<const char*>(&mol.spinMultiplicity), sizeof(mol.spinMultiplicity));
    WriteVector(out, mol.atoms);
    WriteVector(out, mol.bonds);
  }

  for (const auto& reaction : reactions_) {
    WriteString(out, reaction.id);
    out.write(reinterpret_cast<const char*>(&reaction.deltaG), sizeof(reaction.deltaG));
    out.write(reinterpret_cast<const char*>(&reaction.activationEnergy), sizeof(reaction.activationEnergy));
    out.write(reinterpret_cast<const char*>(&reaction.preExponential), sizeof(reaction.preExponential));
    WriteVector(out, reaction.reactants);
    WriteVector(out, reaction.products);
  }
  return true;
}

bool ChemDB::LoadOrBuild(const ChemDBConfig& config) {
  if (!config.cachePath.empty() && fs::exists(config.cachePath)) {
    return LoadCache(config.cachePath);
  }
  if (!BuildCacheFromRaw(config, *this)) {
    return false;
  }
  if (!config.cachePath.empty()) {
    SaveCache(config.cachePath);
  }
  return true;
}

const Element* ChemDB::GetElement(int atomicNumber) const {
  auto it = elementByAtomicNumber_.find(atomicNumber);
  if (it == elementByAtomicNumber_.end()) {
    return nullptr;
  }
  return &elements_[it->second];
}

const Molecule* ChemDB::GetMolecule(int index) const {
  auto it = moleculeByIndex_.find(index);
  if (it == moleculeByIndex_.end()) {
    return nullptr;
  }
  return &molecules_[it->second];
}

const Reaction* ChemDB::GetReaction(std::size_t index) const {
  if (index >= reactions_.size()) {
    return nullptr;
  }
  return &reactions_[index];
}

const std::vector<int>& ChemDB::ReactionsForSpecies(int speciesId) const {
  static const std::vector<int> kEmpty;
  if (speciesId < 0 || static_cast<std::size_t>(speciesId) >= reactionsBySpecies_.size()) {
    return kEmpty;
  }
  return reactionsBySpecies_[static_cast<std::size_t>(speciesId)];
}

std::vector<int> ChemDB::DefaultSeedSpecies(std::size_t count) const {
  std::vector<int> seed;
  seed.reserve(count);
  for (const auto& mol : molecules_) {
    seed.push_back(mol.index);
    if (seed.size() >= count) {
      break;
    }
  }
  return seed;
}

bool ChemDB::BuildCacheFromRaw(const ChemDBConfig& config, ChemDB& outDb) {
  const std::string jsonRoot = config.dataRoot + "/json";
  std::vector<Element> elements;
  std::cerr << "chemdb: loading elements\n";
  ParseElementsJson(jsonRoot + "/elements.json", elements);

  std::unordered_map<int, std::vector<int>> oxidation;
  std::unordered_map<int, std::vector<double>> ionization;
  std::unordered_map<int, std::pair<double, double>> phase;
  std::cerr << "chemdb: loading oxidation states\n";
  ParseOxidationStates(jsonRoot + "/oxidationstates.json", oxidation);
  std::cerr << "chemdb: loading ionization energies\n";
  ParseIonizationEnergies(jsonRoot + "/ionizationenergies.json", ionization);
  std::cerr << "chemdb: loading phase transitions\n";
  ParsePhaseTransitions(jsonRoot + "/phasetransitions.json", phase);

  for (auto& element : elements) {
    auto ox = oxidation.find(element.atomicNumber);
    if (ox != oxidation.end()) {
      element.oxidationStates = ox->second;
    }
    auto ion = ionization.find(element.atomicNumber);
    if (ion != ionization.end()) {
      element.ionizationEnergies = ion->second;
    }
    auto ph = phase.find(element.atomicNumber);
    if (ph != phase.end()) {
      element.meltingPoint = ph->second.first;
      element.boilingPoint = ph->second.second;
    }
  }

  std::unordered_map<int, Molecule> molecules;
  std::cerr << "chemdb: loading molecule attributes\n";
  ParseMoleculeAttributes(config.dataRoot + "/molecule_attributes.yaml", molecules);

  std::unordered_map<std::string, int> symbolToAtomic;
  symbolToAtomic.reserve(elements.size());
  for (const auto& element : elements) {
    if (!element.symbol.empty()) {
      symbolToAtomic[element.symbol] = element.atomicNumber;
    }
  }

  std::cerr << "chemdb: loading molecule structures\n";
  ParseSdf(config.dataRoot + "/molecules.sdf", symbolToAtomic, molecules);

  std::vector<Molecule> moleculeVec;
  moleculeVec.reserve(molecules.size());
  for (auto& [idx, mol] : molecules) {
    moleculeVec.push_back(std::move(mol));
  }
  std::sort(moleculeVec.begin(), moleculeVec.end(), [](const Molecule& a, const Molecule& b) {
    return a.index < b.index;
  });

  std::vector<Reaction> reactions;
  std::cerr << "chemdb: loading reactions\n";
  ParseReactions(config.dataRoot + "/reactions.yaml", reactions);

  for (auto& reaction : reactions) {
    reaction.preExponential = 1.0;
    reaction.activationEnergy = std::abs(reaction.deltaG) * 1000.0 + 50000.0;
  }

  outDb.elements_ = std::move(elements);
  outDb.molecules_ = std::move(moleculeVec);
  outDb.reactions_ = std::move(reactions);

  outDb.elementByAtomicNumber_.clear();
  outDb.moleculeByIndex_.clear();
  for (std::size_t i = 0; i < outDb.elements_.size(); ++i) {
    outDb.elementByAtomicNumber_[outDb.elements_[i].atomicNumber] = i;
  }
  for (std::size_t i = 0; i < outDb.molecules_.size(); ++i) {
    outDb.moleculeByIndex_[outDb.molecules_[i].index] = i;
  }

  outDb.BuildReactionIndex();
  return true;
}

void ChemDB::BuildReactionIndex() {
  reactionsBySpecies_.clear();
  int maxIndex = 0;
  for (const auto& mol : molecules_) {
    maxIndex = std::max(maxIndex, mol.index);
  }
  reactionsBySpecies_.resize(static_cast<std::size_t>(maxIndex + 1));
  for (std::size_t i = 0; i < reactions_.size(); ++i) {
    const auto& reaction = reactions_[i];
    for (int species : reaction.reactants) {
      if (species >= 0 && static_cast<std::size_t>(species) < reactionsBySpecies_.size()) {
        reactionsBySpecies_[static_cast<std::size_t>(species)].push_back(static_cast<int>(i));
      }
    }
  }
}

} // namespace terra::chem
