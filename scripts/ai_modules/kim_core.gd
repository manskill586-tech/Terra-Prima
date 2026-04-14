extends Node
class_name KimCore

var active_modules: Dictionary = {}

const MODULE_PATHS: Dictionary = {
	"personality": "res://scenes/kim/modules/personality_module.tscn",
	"creator": "res://scenes/kim/modules/creator_module.tscn",
	"analyst": "res://scenes/kim/modules/analyst_module.tscn",
	"idea_gen": "res://scenes/kim/modules/idea_generator_module.tscn",
}


func activate_module(module_name: String) -> Node:
	if active_modules.has(module_name):
		return active_modules[module_name]

	if not MODULE_PATHS.has(module_name):
		push_error("[KimCore] Unknown module: %s" % module_name)
		return null

	var scene_path: String = MODULE_PATHS[module_name]
	var packed: PackedScene = load(scene_path)
	if packed == null:
		push_error("[KimCore] Failed to load scene: %s" % scene_path)
		return null

	var instance: Node = packed.instantiate()
	add_child(instance)
	active_modules[module_name] = instance
	print("[KimCore] Activated module: %s" % module_name)
	return instance


func deactivate_module(module_name: String) -> void:
	if not active_modules.has(module_name):
		return

	var module_node: Node = active_modules[module_name]
	if is_instance_valid(module_node):
		module_node.queue_free()
	active_modules.erase(module_name)
	print("[KimCore] Deactivated module: %s" % module_name)


func get_module(module_name: String) -> Node:
	if active_modules.has(module_name):
		return active_modules[module_name]
	return null


func is_module_active(module_name: String) -> bool:
	return active_modules.has(module_name)


func deactivate_all() -> void:
	var names: Array = active_modules.keys()
	for module_name in names:
		deactivate_module(String(module_name))
