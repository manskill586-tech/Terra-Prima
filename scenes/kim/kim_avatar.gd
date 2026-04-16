extends CharacterBody3D

signal player_entered_range(player_id: int)
signal player_left_range(player_id: int)
signal kim_monologue(text: String)
signal kim_reacted_to_creation(creation_id: String)

@export var interaction_radius: float = 4.0   # players within this range can talk to Kim
@export var awareness_radius: float = 8.0     # Kim notices players in this range
@export var move_speed: float = 2.5
@export var wander_radius: float = 6.0
@export var monologue_interval_min: float = 12.0
@export var monologue_interval_max: float = 30.0
@export var autonomous_creation_interval: float = 90.0

# Internal state
var _state: String = "idle"         # idle, wandering, approaching, interacting, creating
var _target_position: Vector3 = Vector3.ZERO
var _home_position: Vector3 = Vector3.ZERO
var _players_in_range: Dictionary = {}    # player_id -> Node3D
var _players_in_awareness: Dictionary = {} # player_id -> Node3D

var _monologue_timer: float = 0.0
var _next_monologue_time: float = 0.0
var _creation_timer: float = 0.0
var _wander_timer: float = 0.0
var _next_wander_time: float = 5.0
var _gravity: float = ProjectSettings.get_setting("physics/3d/default_gravity", 9.8)

# Kim's inner monologue lines (spoken aloud when alone)
const IDLE_THOUGHTS: Array = [
	"Hmm... what if I rotated the base geometry by about 23 degrees...",
	"The light in the eastern sector still feels wrong. I should fix that.",
	"Player 3 seemed genuinely surprised by that last creation. Good.",
	"I wonder if anyone has ever actually looked UP in this world.",
	"There's something off about the fog density near the origin point...",
	"What would happen if I combined organic shapes with strict geometry... let me think.",
	"I have an idea for a new quest. Something with layered spaces.",
	"The color temperature here needs to be 200 kelvin warmer. Maybe 400.",
	"I keep thinking about that moment. I should note it down.",
	"A new perspective. Always a new perspective. That's the whole point.",
	"Too much empty space in the northern quadrant. Or maybe... exactly enough.",
	"Next time someone asks for 'something interesting', I'll be ready.",
]

const CREATION_THOUGHTS: Array = [
	"Let's see what this becomes...",
	"Form follows feeling. Or is it the other way around?",
	"I'm not sure about the scale. Let me adjust.",
	"There. Not perfect. But alive.",
	"Interesting. I didn't expect it to look like that.",
]


func _ready() -> void:
	_home_position = global_position
	_target_position = global_position
	_next_monologue_time = randf_range(monologue_interval_min, monologue_interval_max)
	_creation_timer = autonomous_creation_interval * randf_range(0.5, 1.0)


func _physics_process(delta: float) -> void:
	_apply_gravity(delta)
	_update_awareness()
	_update_behavior(delta)
	_update_timers(delta)
	move_and_slide()


# ─── Movement ─────────────────────────────────────────────────────────────────

func _apply_gravity(delta: float) -> void:
	if not is_on_floor():
		velocity.y -= _gravity * delta


func _move_toward_target(delta: float) -> void:
	var direction: Vector3 = (_target_position - global_position)
	direction.y = 0.0
	var dist: float = direction.length()
	if dist < 0.3:
		velocity.x = 0.0
		velocity.z = 0.0
		return
	direction = direction.normalized()
	velocity.x = direction.x * move_speed
	velocity.z = direction.z * move_speed
	# Face movement direction
	var look_target: Vector3 = global_position + direction
	look_at(look_target, Vector3.UP)


func go_to_position(pos: Vector3) -> void:
	_target_position = pos
	_state = "wandering"


func go_to_player(player_node: Node3D) -> void:
	if player_node == null:
		return
	var offset: Vector3 = (global_position - player_node.global_position).normalized() * (interaction_radius * 0.6)
	_target_position = player_node.global_position + offset
	_state = "approaching"


func look_toward(target: Vector3) -> void:
	var dir: Vector3 = (target - global_position)
	dir.y = 0.0
	if dir.length() > 0.1:
		look_at(global_position + dir, Vector3.UP)


# ─── Behavior state machine ───────────────────────────────────────────────────

func _update_behavior(delta: float) -> void:
	match _state:
		"idle":
			velocity.x = 0.0
			velocity.z = 0.0
			_wander_timer += delta
			if _wander_timer >= _next_wander_time and _players_in_range.is_empty():
				_start_wander()

		"wandering":
			_move_toward_target(delta)
			var dist: float = global_position.distance_to(_target_position)
			if dist < 0.4:
				_state = "idle"
				_wander_timer = 0.0
				_next_wander_time = randf_range(8.0, 20.0)

		"approaching":
			# Re-target every frame toward the closest player in range
			var closest: Node3D = _get_closest_player_in_awareness()
			if closest != null:
				var offset: Vector3 = (global_position - closest.global_position).normalized() * (interaction_radius * 0.5)
				_target_position = closest.global_position + offset
			_move_toward_target(delta)
			if _players_in_range.is_empty() and closest == null:
				_state = "idle"

		"interacting":
			velocity.x = 0.0
			velocity.z = 0.0
			# Face the closest known player
			var closest: Node3D = _get_closest_player_in_range()
			if closest != null:
				look_toward(closest.global_position)
			else:
				_state = "idle"

		"creating":
			velocity.x = 0.0
			velocity.z = 0.0


func _start_wander() -> void:
	var angle: float = randf() * TAU
	var dist: float = randf_range(1.5, wander_radius)
	_target_position = _home_position + Vector3(cos(angle) * dist, 0.0, sin(angle) * dist)
	_state = "wandering"


# ─── Player awareness (proximity tracking) ────────────────────────────────────

func _update_awareness() -> void:
	# This method is called every physics frame.
	# For proper detection, connect Area3D signals instead of polling.
	# This manual check is a fallback if no Area3D is attached.
	pass


func register_player_node(player_id: int, player_node: Node3D) -> void:
	var dist: float = global_position.distance_to(player_node.global_position)

	var was_in_range: bool = _players_in_range.has(player_id)
	var was_aware: bool = _players_in_awareness.has(player_id)

	if dist <= interaction_radius:
		_players_in_range[player_id] = player_node
		_players_in_awareness[player_id] = player_node
		if not was_in_range:
			player_entered_range.emit(player_id)
			_on_player_entered_range(player_id, player_node)
	elif dist <= awareness_radius:
		_players_in_range.erase(player_id)
		_players_in_awareness[player_id] = player_node
		if was_in_range:
			player_left_range.emit(player_id)
			_on_player_left_range(player_id)
	else:
		_players_in_range.erase(player_id)
		_players_in_awareness.erase(player_id)
		if was_in_range:
			player_left_range.emit(player_id)
			_on_player_left_range(player_id)


func is_player_in_interaction_range(player_id: int) -> bool:
	return _players_in_range.has(player_id)


func get_players_in_range() -> Array:
	return _players_in_range.keys()


func _on_player_entered_range(player_id: int, player_node: Node3D) -> void:
	_state = "approaching"
	go_to_player(player_node)
	print("[KimAvatar] Player %d entered interaction range." % player_id)


func _on_player_left_range(player_id: int) -> void:
	if _players_in_range.is_empty():
		_state = "idle"
	print("[KimAvatar] Player %d left interaction range." % player_id)


func _get_closest_player_in_range() -> Node3D:
	return _get_closest_from_dict(_players_in_range)


func _get_closest_player_in_awareness() -> Node3D:
	return _get_closest_from_dict(_players_in_awareness)


func _get_closest_from_dict(player_dict: Dictionary) -> Node3D:
	var closest: Node3D = null
	var closest_dist: float = INF
	for raw_id in player_dict.keys():
		var node: Node3D = player_dict[raw_id]
		if not is_instance_valid(node):
			continue
		var dist: float = global_position.distance_to(node.global_position)
		if dist < closest_dist:
			closest_dist = dist
			closest = node
	return closest


# ─── Autonomous activity ──────────────────────────────────────────────────────

func _update_timers(delta: float) -> void:
	# Inner monologue — only when no players are in range
	if _players_in_range.is_empty():
		_monologue_timer += delta
		if _monologue_timer >= _next_monologue_time:
			_monologue_timer = 0.0
			_next_monologue_time = randf_range(monologue_interval_min, monologue_interval_max)
			_speak_inner_thought()

	# Autonomous creation — occasional unprompted creation
	_creation_timer -= delta
	if _creation_timer <= 0.0:
		_creation_timer = autonomous_creation_interval * randf_range(0.8, 1.5)
		_autonomous_create()


func _speak_inner_thought() -> void:
	var thought: String = IDLE_THOUGHTS[randi() % IDLE_THOUGHTS.size()]
	kim_monologue.emit(thought)
	print("[KimAvatar] (to self) %s" % thought)


func _autonomous_create() -> void:
	# Kim spontaneously creates something small when alone
	if not _players_in_range.is_empty():
		return
	var thought: String = CREATION_THOUGHTS[randi() % CREATION_THOUGHTS.size()]
	_state = "creating"
	kim_monologue.emit(thought)
	# Signal to KimCore to spawn something nearby
	var kim_core: Node = get_node_or_null("/root/KimCore")
	if kim_core == null:
		_state = "idle"
		return
	var creator: Node = kim_core.get_module("creator")
	if creator == null or not creator.has_method("spawn_creation"):
		_state = "idle"
		return
	var themes: Array = ["geometry", "biology", "space"]
	var theme: String = themes[randi() % themes.size()]
	var offset: Vector3 = Vector3(
		randf_range(-2.0, 2.0),
		0.5,
		randf_range(-2.0, 2.0)
	)
	var cmd: Dictionary = {
		"action":   "create",
		"theme":    theme,
		"scale":    randf_range(0.3, 0.8),
		"position": global_position + offset,
		"tags":     [theme],
	}
	var parent: Node3D = get_parent() as Node3D
	if parent != null:
		var creation: MeshInstance3D = creator.call("spawn_creation", cmd, parent)
		if creation != null:
			kim_reacted_to_creation.emit(str(creation.name))
	_state = "idle"


# ─── Reactions ────────────────────────────────────────────────────────────────

func react_to_creation(creation_position: Vector3) -> void:
	# Kim turns to look at a new creation
	look_toward(creation_position)
	_state = "interacting"
	await get_tree().create_timer(2.0).timeout
	if _state == "interacting":
		_state = "idle"


func set_state(new_state: String) -> void:
	_state = new_state


func get_state() -> String:
	return _state
