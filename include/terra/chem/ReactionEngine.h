#pragma once

#include "terra/chem/ChemDB.h"
#include "terra/chem/ChemState.h"

#include <cstdint>

namespace terra::chem {

struct ReactionEngineConfig {
  int maxReactions{64};
  double gasConstant{8.314462618};
  double baseActivationEnergy{50000.0};
  double deltaGScale{1000.0};
  double preExponential{1.0};
};

class ReactionEngine {
public:
  explicit ReactionEngine(const ChemDB& db);

  void Step(ChemState& state, double dt, uint64_t seed, const ReactionEngineConfig& config);

private:
  const ChemDB& db_;
};

} // namespace terra::chem
