#include "terra/chem/PhaseModel.h"

#include <algorithm>

namespace terra::chem {

PhaseModel::PhaseModel(const ChemDB& db) : db_(db) {}

bool PhaseModel::IsSolid(const Molecule& molecule, double temperature) const {
  double total = 0.0;
  int count = 0;
  for (const auto& atom : molecule.atoms) {
    const Element* element = db_.GetElement(atom.atomicNumber);
    if (!element) {
      continue;
    }
    if (element->meltingPoint <= 0.0) {
      continue;
    }
    total += element->meltingPoint;
    ++count;
  }
  if (count == 0) {
    return false;
  }
  const double avgMelt = total / static_cast<double>(count);
  return temperature < avgMelt;
}

void PhaseModel::Update(ChemState& state) {
  if (state.species.empty()) {
    return;
  }

  bool allSolid = true;
  for (const auto& s : state.species) {
    const Molecule* mol = db_.GetMolecule(s.id);
    if (!mol) {
      allSolid = false;
      break;
    }
    if (!IsSolid(*mol, state.temperature)) {
      allSolid = false;
      break;
    }
  }
  state.dormant = allSolid;
}

} // namespace terra::chem
