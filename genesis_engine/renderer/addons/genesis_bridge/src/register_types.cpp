#include "register_types.h"

#include "GenesisBridge.h"

#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace genesis::bridge {

void initialize_genesis_bridge_module(godot::ModuleInitializationLevel level) {
  if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
    return;
  }
  godot::ClassDB::register_class<GenesisBridge>();
}

void uninitialize_genesis_bridge_module(godot::ModuleInitializationLevel level) {
  if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
    return;
  }
}

} // namespace genesis::bridge
