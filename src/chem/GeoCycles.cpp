#include "terra/chem/GeoCycles.h"

namespace terra::chem {

void GeoCycles::Step(ChemState& state, double dt) {
  // Placeholder: slow drift of redox potential and pH.
  state.eh += dt * 1e-6;
  state.pH += dt * 1e-7;
  if (state.pH < 0.0) state.pH = 0.0;
  if (state.pH > 14.0) state.pH = 14.0;
}

} // namespace terra::chem
