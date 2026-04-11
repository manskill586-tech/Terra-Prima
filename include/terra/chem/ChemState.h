#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace terra::chem {

struct SpeciesAmount {
  int id{0};
  float concentration{0.0f};
};

struct ChemState {
  double temperature{288.0};
  double pressure{101325.0};
  double pH{7.0};
  double eh{0.0};
  double ionicStrength{0.0};
  bool dormant{false};
  std::vector<SpeciesAmount> species;

  float GetConcentration(int id) const {
    for (const auto& s : species) {
      if (s.id == id) {
        return s.concentration;
      }
    }
    return 0.0f;
  }

  void SetConcentration(int id, float value) {
    for (auto& s : species) {
      if (s.id == id) {
        s.concentration = std::max(0.0f, value);
        return;
      }
    }
    if (value > 0.0f) {
      species.push_back({id, value});
    }
  }

  void AddConcentration(int id, float delta) {
    for (auto& s : species) {
      if (s.id == id) {
        s.concentration = std::max(0.0f, s.concentration + delta);
        return;
      }
    }
    if (delta > 0.0f) {
      species.push_back({id, delta});
    }
  }

  void Compact() {
    species.erase(std::remove_if(species.begin(), species.end(),
                                 [](const SpeciesAmount& s) { return s.concentration <= 0.0f; }),
                  species.end());
    std::sort(species.begin(), species.end(), [](const SpeciesAmount& a, const SpeciesAmount& b) {
      return a.id < b.id;
    });
  }
};

} // namespace terra::chem
