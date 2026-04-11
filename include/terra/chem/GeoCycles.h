#pragma once

#include "terra/chem/ChemState.h"

namespace terra::chem {

class GeoCycles {
public:
  void Step(ChemState& state, double dt);
};

} // namespace terra::chem
