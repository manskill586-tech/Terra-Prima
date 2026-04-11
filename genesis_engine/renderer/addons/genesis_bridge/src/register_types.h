#pragma once

#include <godot_cpp/core/class_db.hpp>

namespace genesis::bridge {

void initialize_genesis_bridge_module(godot::ModuleInitializationLevel level);
void uninitialize_genesis_bridge_module(godot::ModuleInitializationLevel level);

} // namespace genesis::bridge
