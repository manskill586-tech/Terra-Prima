# CLAS API Contract (Alpha Placeholder)

This document defines the future contract between the game runtime and the CLAS Python service. No runtime integration exists in alpha.

## Snapshot Input (from game to CLAS)
The game provides a compact snapshot with only the data required for analysis. Example shape:

```json
{
  "protocol_version": 1,
  "request_id": 42,
  "module": "observer",
  "timestamp_us": 123456789,
  "sim": {
    "sim_time_s": 12.5,
    "fps": 60.0,
    "tick_ms": 5.0,
    "lod": {"near": 1, "mid": 8, "far": 4},
    "active_chunks": 12,
    "active_organisms": 1024
  },
  "player": {
    "camera_pos": [0.0, 4.0, -12.0],
    "focus": "near_particles",
    "session_time_s": 1800,
    "input_intensity": 0.3
  },
  "events": [
    {"type": "tutorial_step", "value": "movement"},
    {"type": "metric_shift", "value": "temperature_spike"}
  ]
}
```

## Response Output (from CLAS to game UI)
The response is a compact envelope plus an optional structured payload.

```json
{
  "protocol_version": 1,
  "request_id": 42,
  "status": "ok",
  "summary": "Температура поднялась в двух чанках. Рекомендуется проверить источник тепла.",
  "ui": {
    "severity": "info",
    "tags": ["climate", "near"],
    "suggestions": ["Open Climate Panel", "Show Heat Map"]
  },
  "data": {
    "hot_chunks": 2,
    "trend": "up"
  }
}
```

## Versioning
The envelope fields are defined in `shared/ai_protocol.h`. The game should refuse incompatible versions and fall back to no-AI behavior.

## Training Hooks (Disabled in Alpha)
Training data capture is explicitly disabled during alpha. The contract only reserves fields for future use.

Future fields (reserved):
- `training_opt_in` (bool)
- `training_tags` (array)
- `privacy_level` (enum)

No logging is performed until this is explicitly enabled.
