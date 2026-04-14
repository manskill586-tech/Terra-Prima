extends Node3D

@export var preload_modules: Array[String] = [
	"personality",
	"analyst",
	"idea_gen",
]


func _ready() -> void:
	var kim_core := get_node_or_null("/root/KimCore")
	if kim_core == null:
		push_warning("[WorldBootstrap] KimCore autoload is missing.")
		return

	for module_name in preload_modules:
		kim_core.activate_module(module_name)
