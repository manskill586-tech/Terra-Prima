# Terra Prima / Kim Prototype

Initial implementation was started from `План.md`, stage 1.

## Implemented

- Godot 4 project bootstrap with `forward_plus` renderer in `project.godot`.
- `KimCore` autoload with dynamic module activation.
- Base AI module scenes:
  - `personality_module.tscn`
  - `creator_module.tscn`
  - `analyst_module.tscn`
  - `idea_generator_module.tscn`
- Authoritative multiplayer skeleton in `scripts/networking/game_server.gd`.
- Main world scene in `scenes/world/world_scene.tscn`.

## Next in Godot Editor

1. Open this folder as a Godot 4.5+ project.
2. Install and enable plugins:
   - `NobodyWho`
   - `godot-xr-tools`
3. Replace placeholder nodes in `personality_module.tscn`:
   - `NobodyWhoModel` -> plugin model node
   - `NobodyWhoChat` -> plugin chat node
4. Wire model/chat settings in Inspector and set the GGUF path.
