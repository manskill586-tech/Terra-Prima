#include "register_types.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

using namespace genesis::bridge;

extern "C" {

GDExtensionBool GDE_EXPORT genesis_bridge_init(const GDExtensionInterface* p_interface,
                                               GDExtensionClassLibraryPtr p_library,
                                               GDExtensionInitialization* r_initialization) {
  godot::GDExtensionBinding::InitObject init_obj(p_interface, p_library, r_initialization);
  init_obj.register_initializer(initialize_genesis_bridge_module);
  init_obj.register_terminator(uninitialize_genesis_bridge_module);
  init_obj.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
  return init_obj.init();
}

}
