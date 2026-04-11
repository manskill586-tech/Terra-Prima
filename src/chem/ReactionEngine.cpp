#include "terra/chem/ReactionEngine.h"

#include "terra/core/Random.h"

#include <cmath>
#include <limits>
#include <vector>

namespace terra::chem {

namespace {

uint64_t NextRand(uint64_t& state) {
  state = terra::core::SplitMix64(state);
  return state;
}

int PickIndex(uint64_t& state, int maxExclusive) {
  if (maxExclusive <= 0) {
    return 0;
  }
  return static_cast<int>(NextRand(state) % static_cast<uint64_t>(maxExclusive));
}

} // namespace

ReactionEngine::ReactionEngine(const ChemDB& db) : db_(db) {}

void ReactionEngine::Step(ChemState& state, double dt, uint64_t seed, const ReactionEngineConfig& config) {
  if (state.dormant) {
    return;
  }
  if (state.species.empty()) {
    return;
  }

  std::vector<int> activeSpecies;
  activeSpecies.reserve(state.species.size());
  for (const auto& s : state.species) {
    if (s.concentration > 0.0f) {
      activeSpecies.push_back(s.id);
    }
  }
  if (activeSpecies.empty()) {
    return;
  }

  uint64_t rng = seed;
  const double temperature = std::max(1.0, state.temperature);
  bool reacted = false;

  for (int i = 0; i < config.maxReactions; ++i) {
    const int speciesId = activeSpecies[PickIndex(rng, static_cast<int>(activeSpecies.size()))];
    const auto& reactionList = db_.ReactionsForSpecies(speciesId);
    if (reactionList.empty()) {
      continue;
    }
    const int reactionIndex = reactionList[PickIndex(rng, static_cast<int>(reactionList.size()))];
    const Reaction* reaction = db_.GetReaction(static_cast<std::size_t>(reactionIndex));
    if (!reaction) {
      continue;
    }

    float limiting = std::numeric_limits<float>::max();
    for (int reactant : reaction->reactants) {
      const float conc = state.GetConcentration(reactant);
      limiting = std::min(limiting, conc);
    }
    if (!std::isfinite(limiting) || limiting <= 0.0f) {
      continue;
    }

    const double activation = reaction->activationEnergy > 0.0 ? reaction->activationEnergy
                                                                : (config.baseActivationEnergy +
                                                                   std::abs(reaction->deltaG) *
                                                                       config.deltaGScale);
    const double k = reaction->preExponential *
                     std::exp(-activation / (config.gasConstant * temperature));
    const float extent = static_cast<float>(k * dt) * limiting;
    if (extent <= 0.0f) {
      continue;
    }

    for (int reactant : reaction->reactants) {
      state.AddConcentration(reactant, -extent);
    }
    for (int product : reaction->products) {
      state.AddConcentration(product, extent);
    }
    reacted = true;
  }

  if (reacted) {
    state.dormant = false;
  }
  state.Compact();
}

} // namespace terra::chem
