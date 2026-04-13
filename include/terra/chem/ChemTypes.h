#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace terra::chem {

struct Element {
  int atomicNumber{0};
  std::string symbol;
  std::string name;
  std::string block;
  double atomicWeight{0.0};
  double atomicRadius{0.0};
  double atomicVolume{0.0};
  double electronegativity{0.0};
  double electronAffinity{0.0};
  double density{0.0};
  double heatCapacity{0.0};
  double thermalConductivity{0.0};
  double meltingPoint{0.0};
  double boilingPoint{0.0};
  uint32_t colorRgba{0};
  std::vector<int> oxidationStates;
  std::vector<double> ionizationEnergies;
};

struct Atom {
  int atomicNumber{0};
  float x{0.0f};
  float y{0.0f};
  float z{0.0f};
};

struct Bond {
  int a{0};
  int b{0};
  int order{1};
};

struct Molecule {
  int index{0};
  std::string id;
  int charge{0};
  double freeEnergy{0.0};
  double atomizationFreeEnergy{0.0};
  int spinMultiplicity{1};
  std::vector<Atom> atoms;
  std::vector<Bond> bonds;
};

struct Reaction {
  std::string id;
  std::vector<int> reactants;
  std::vector<int> products;
  double deltaG{0.0};
  double activationEnergy{0.0};
  double preExponential{1.0};
};

} // namespace terra::chem
