# Terra Prima / Kim Prototype

Initial implementation was started from the stage-1 plan document.

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
3. Open `scenes/kim/modules/personality_module.tscn`.
4. Set `llm_model_file` in the inspector to your `.gguf` model path.
5. Run and call `KimCore.activate_module("personality").send_message(0, "hello")`.
