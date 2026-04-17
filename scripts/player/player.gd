extends CharacterBody3D

const SPEED: float = 4.5
const SPRINT_SPEED: float = 8.0
const JUMP_VELOCITY: float = 4.8
const MOUSE_SENSITIVITY: float = 0.002

var _gravity: float = ProjectSettings.get_setting("physics/3d/default_gravity", 9.8)
var _hp: float = 100.0
var _max_hp: float = 100.0

@onready var camera: Camera3D = $Head/Camera3D
@onready var head: Node3D = $Head


func _ready() -> void:
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	camera.make_current()
	# Регистрация в GameServer и KimAvatar после полной загрузки сцены
	call_deferred("_register_player")


func _register_player() -> void:
	var gs: Node = _find_by_script(get_tree().root, "game_server.gd")
	if gs != null and gs.has_method("register_player_node"):
		var pid: int = multiplayer.get_unique_id() if multiplayer.multiplayer_peer != null else 1
		gs.call("register_player_node", pid, self)

	# Сообщить HUD начальный HP
	var hud: Node = _find_by_script(get_tree().root, "player_hud.gd")
	if hud != null and hud.has_method("set_hp"):
		hud.call("set_hp", _hp, _max_hp)


func _unhandled_input(event: InputEvent) -> void:
	# Вращение камеры мышью
	if event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		rotate_y(-event.relative.x * MOUSE_SENSITIVITY)
		head.rotate_x(-event.relative.y * MOUSE_SENSITIVITY)
		head.rotation.x = clampf(head.rotation.x, -PI * 0.45, PI * 0.45)

	# Переключение захвата мыши
	if event.is_action_pressed("toggle_cursor"):
		if Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
			Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
		else:
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED


func _physics_process(delta: float) -> void:
	# Гравитация
	if not is_on_floor():
		velocity.y -= _gravity * delta

	# Прыжок
	if Input.is_action_just_pressed("player_jump") and is_on_floor():
		velocity.y = JUMP_VELOCITY

	# Движение WASD
	var input_dir: Vector2 = Input.get_vector(
		"move_left", "move_right", "move_forward", "move_back"
	)
	var direction: Vector3 = (
		transform.basis * Vector3(input_dir.x, 0.0, input_dir.y)
	).normalized()

	var speed: float = SPRINT_SPEED if Input.is_key_pressed(KEY_SHIFT) else SPEED

	if direction.length() > 0.0:
		velocity.x = direction.x * speed
		velocity.z = direction.z * speed
	else:
		velocity.x = move_toward(velocity.x, 0.0, speed)
		velocity.z = move_toward(velocity.z, 0.0, speed)

	move_and_slide()


func take_damage(amount: float) -> void:
	_hp = clampf(_hp - amount, 0.0, _max_hp)
	_notify_hud_hp()
	if _hp <= 0.0:
		_on_death()


func heal(amount: float) -> void:
	_hp = clampf(_hp + amount, 0.0, _max_hp)
	_notify_hud_hp()


func _notify_hud_hp() -> void:
	var hud: Node = _find_by_script(get_tree().root, "player_hud.gd")
	if hud != null and hud.has_method("set_hp"):
		hud.call("set_hp", _hp, _max_hp)


func _on_death() -> void:
	print("[Player] Game over.")


func _find_by_script(node: Node, script_file: String) -> Node:
	var s: Script = node.get_script() as Script
	if s != null and s.resource_path.ends_with(script_file):
		return node
	for child in node.get_children():
		var found: Node = _find_by_script(child, script_file)
		if found != null:
			return found
	return null
