#pragma once

#include <cstdint>

namespace genesis::ai {

inline constexpr uint16_t kAiProtocolVersion = 1;
inline constexpr uint16_t kAiMinCompatibleVersion = 1;

enum class ModuleId : uint16_t {
  kUnknown = 0,
  kObserver,
  kMentor,
  kOverlordSim,
  kTranslator,
  kChronicler,
  kScientist,
  kAnalyst,
  kBiologist,
  kPhysicist,
  kGeographer,
  kGeologist,
  kAstronomer,
  kChemist,
  kClimatologist,
  kEcologist,
  kArchivist,
  kOptimizer,
  kEngineer
};

enum class PayloadType : uint16_t {
  kNone = 0,
  kJson,
  kBinary
};

enum class ResponseStatus : uint16_t {
  kOk = 0,
  kNoData,
  kDeferred,
  kError
};

struct MessageEnvelope {
  uint16_t version{kAiProtocolVersion};
  uint16_t min_compatible{kAiMinCompatibleVersion};
  uint32_t flags{0};
  uint64_t request_id{0};
  uint64_t timestamp_us{0};
  ModuleId module{ModuleId::kUnknown};
  PayloadType payload_type{PayloadType::kNone};
  uint32_t payload_size{0};
};

struct ResponseEnvelope {
  uint16_t version{kAiProtocolVersion};
  uint16_t min_compatible{kAiMinCompatibleVersion};
  uint32_t flags{0};
  uint64_t request_id{0};
  ResponseStatus status{ResponseStatus::kOk};
  PayloadType payload_type{PayloadType::kNone};
  uint32_t payload_size{0};
  uint32_t summary_size{0};
};

} // namespace genesis::ai
