#pragma once

#include "terra/chem/ChemDB.h"
#include "terra/chem/ChemState.h"

namespace terra::chem {

class PhaseModel {
public:
  explicit PhaseModel(const ChemDB& db);

  void Update(ChemState& state);
  bool IsSolid(const Molecule& molecule, double temperature) const;

private:
  const ChemDB& db_;
};

} // namespace terra::chem
