#pragma once

#include "terra/chem/ChemTypes.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace terra::chem {

struct ChemDBConfig {
  std::string dataRoot;
  std::string cachePath;
};

class ChemDB {
public:
  bool LoadCache(const std::string& path);
  bool SaveCache(const std::string& path) const;
  bool LoadOrBuild(const ChemDBConfig& config);

  const Element* GetElement(int atomicNumber) const;
  const Molecule* GetMolecule(int index) const;
  const Reaction* GetReaction(std::size_t index) const;

  const std::vector<Element>& Elements() const { return elements_; }
  const std::vector<Molecule>& Molecules() const { return molecules_; }
  const std::vector<Reaction>& Reactions() const { return reactions_; }
  const std::vector<int>& ReactionsForSpecies(int speciesId) const;

  std::vector<int> DefaultSeedSpecies(std::size_t count) const;

  static bool BuildCacheFromRaw(const ChemDBConfig& config, ChemDB& outDb);

private:
  void BuildReactionIndex();

  std::vector<Element> elements_;
  std::vector<Molecule> molecules_;
  std::vector<Reaction> reactions_;
  std::unordered_map<int, std::size_t> elementByAtomicNumber_;
  std::unordered_map<int, std::size_t> moleculeByIndex_;
  std::vector<std::vector<int>> reactionsBySpecies_;
};

} // namespace terra::chem
