extends Node

signal environment_changed(state_name: String, properties: Dictionary)
signal zone_created(zone_id: String, bounds: Dictionary)
signal zone_removed(zone_id: String)
signal player_teleported(player_id: int, position: Vector3)

const ENVIRONMENT_PRESETS: Dictionary = {
	"clear": {
		"ambient_energy":     1.0,
		"ambient_color":      Color(0.9, 0.95, 1.0),
		"fog_enabled":        false,
		"fog_density":        0.0,
		"sky_energy":         1.0,
		"sun_energy":         1.0,
	},
	"night": {
		"ambient_energy":     0.08,
		"ambient_color":      Color(0.1, 0.1, 0.25),
		"fog_enabled":        true,
		"fog_density":        0.005,
		"sky_energy":         0.05,
		"sun_energy":         0.0,
	},
	"foggy": {
		"ambient_energy":     0.5,
		"ambient_color":      Color(0.7, 0.75, 0.8),
		"fog_enabled":        true,
		"fog_density":        0.04,
		"sky_energy":         0.4,
		"sun_energy":         0.3,
	},
	"storm": {
		"ambient_energy":     0.25,
		"ambient_color":      Color(0.3, 0.3, 0.4),
		"fog_enabled":        true,
		"fog_density":        0.06,
		"sky_energy":         0.2,
		"sun_energy":         0.1,
	},
	"ethereal": {
		"ambient_energy":     0.65,
		"ambient_color":      Color(0.6, 0.4, 0.9),
		"fog_enabled":        true,
		"fog_density":        0.02,
		"sky_energy":         0.5,
		"sun_energy":         0.3,
	},
	"dawn": {
		"ambient_energy":     0.6,
		"ambient_color":      Color(1.0, 0.7, 0.4),
		"fog_enabled":        true,
		"fog_density":        0.01,
		"sky_energy":         0.7,
		"sun_energy":         0.5,
	},
}

var current_environment: String = "clear"
var active_zones: Dictionary = {}       # zone_id -> {center, radius, properties}
var _world_environment: WorldEnvironment = null
var _directional_light: DirectionalLight3D = null


func _ready() -> void:
	# Search for WorldEnvironment and DirectionalLight3D in the scene tree
	_find_world_nodes()


func _find_world_nodes() -> void:
	# Walk the scene tree to find WorldEnvironment and DirectionalLight3D
	_world_environment = _find_node_of_type(get_tree().root, "WorldEnvironment") as WorldEnvironment
	_directional_light = _find_node_of_type(get_tree().root, "DirectionalLight3D") as DirectionalLight3D


func _find_node_of_type(node: Node, type_name: String) -> Node:
	if node.is_class(type_name):
		return node
	for child in node.get_children():
		var found: Node = _find_node_of_type(child, type_name)
		if found != null:
			return found
	return null


# ─── Environment control ──────────────────────────────────────────────────────

func apply_environment(preset_name: String) -> void:
	var lower: String = preset_name.to_lower()
	if not ENVIRONMENT_PRESETS.has(lower):
		push_warning("[EnvController] Unknown environment preset: %s" % preset_name)
		return
	current_environment = lower
	var props: Dictionary = ENVIRONMENT_PRESETS[lower]
	_apply_properties(props)
	environment_changed.emit(lower, props)
	print("[EnvController] Environment set to: %s" % lower)


func set_custom_environment(properties: Dictionary) -> void:
	_apply_properties(properties)
	current_environment = "custom"
	environment_changed.emit("custom", properties)


func _apply_properties(props: Dictionary) -> void:
	if _find_world_nodes.is_null() if false else false:
		_find_world_nodes()

	# Apply to WorldEnvironment if found
	if _world_environment != null and _world_environment.environment != null:
		var env: Environment = _world_environment.environment

		if props.has("ambient_energy"):
			env.ambient_light_energy = float(props["ambient_energy"])
		if props.has("ambient_color"):
			var c: Variant = props["ambient_color"]
			if c is Color:
				env.ambient_light_color = c
		if props.has("fog_enabled"):
			env.fog_enabled = bool(props["fog_enabled"])
		if props.has("fog_density"):
			env.fog_density = float(props["fog_density"])
		if props.has("sky_energy") and env.sky != null:
			env.sky_custom_fov = 0.0  # keep default

	# Apply to DirectionalLight3D if found
	if _directional_light != null:
		if props.has("sun_energy"):
			_directional_light.light_energy = float(props["sun_energy"])
		if props.has("ambient_color"):
			var c: Variant = props["ambient_color"]
			if c is Color:
				_directional_light.light_color = c


# ─── Zone management ──────────────────────────────────────────────────────────

func create_zone(
		zone_id: String,
		center: Vector3,
		radius: float,
		properties: Dictionary = {}) -> void:
	active_zones[zone_id] = {
		"center":     center,
		"radius":     radius,
		"properties": properties,
		"created_at": Time.get_ticks_msec(),
	}
	zone_created.emit(zone_id, active_zones[zone_id])
	print("[EnvController] Zone created: %s at %s r=%.1f" % [zone_id, str(center), radius])


func remove_zone(zone_id: String) -> void:
	if not active_zones.has(zone_id):
		return
	active_zones.erase(zone_id)
	zone_removed.emit(zone_id)
	print("[EnvController] Zone removed: %s" % zone_id)


func is_position_in_zone(pos: Vector3, zone_id: String) -> bool:
	if not active_zones.has(zone_id):
		return false
	var zone: Dictionary = active_zones[zone_id]
	var center: Vector3 = zone.get("center", Vector3.ZERO) as Vector3
	var radius: float = float(zone.get("radius", 0.0))
	return pos.distance_to(center) <= radius


func get_zones_at_position(pos: Vector3) -> Array:
	var result: Array = []
	for zone_id in active_zones.keys():
		if is_position_in_zone(pos, zone_id):
			result.append(zone_id)
	return result


# ─── Player teleportation (server-side) ───────────────────────────────────────

func request_teleport_player(player_id: int, target_position: Vector3) -> void:
	# Emits signal; actual teleport is handled by GameServer via RPC
	player_teleported.emit(player_id, target_position)


# ─── Context for LLM ──────────────────────────────────────────────────────────

func get_environment_context() -> String:
	var lines: Array = ["Current environment: %s" % current_environment]
	if not active_zones.is_empty():
		lines.append("Active zones: %s" % str(active_zones.keys()))
	return "\n".join(lines)


func get_available_presets() -> Array:
	return ENVIRONMENT_PRESETS.keys()
