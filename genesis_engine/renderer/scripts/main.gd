extends Node3D

const MockBridge = preload("res://scripts/mock_bridge.gd")

var bridge
var polling := true
var show_particles := true
var shm_name := ""
var reconnect_interval := 1.0
var reconnect_timer := 0.0
var last_update_age := 0.0
var using_mock := false
var fps_accum := 0.0

var multimesh_instance: MultiMeshInstance3D
var multimesh: MultiMesh
var status_label: Label
var stats_label: Label
var fps_label: Label
var mode_label: Label
var toggle_button: Button
var particles_button: Button

var cam_pivot: Node3D
var camera: Camera3D
var orbit_distance := 6.0
var orbit_yaw := 0.0
var orbit_pitch := -0.4
var rotating := false

func _ready() -> void:
	_setup_scene()
	shm_name = OS.get_environment("GENESIS_SHM_NAME")
	if shm_name == "":
		shm_name = "GenesisSim"
	if ClassDB.class_exists("GenesisBridge"):
		bridge = GenesisBridge.new()
	else:
		bridge = MockBridge.new()
		using_mock = true
	bridge.open(shm_name)
	mode_label.text = using_mock ? "Mode: MOCK" : "Mode: SHM"

func _setup_scene() -> void:
	multimesh_instance = MultiMeshInstance3D.new()
	multimesh = MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.instance_count = 0
	var sphere := SphereMesh.new()
	sphere.radius = 0.03
	sphere.height = 0.06
	multimesh.mesh = sphere
	multimesh_instance.multimesh = multimesh
	add_child(multimesh_instance)

	cam_pivot = Node3D.new()
	add_child(cam_pivot)
	camera = Camera3D.new()
	camera.position = Vector3(0, 0, orbit_distance)
	camera.look_at(Vector3.ZERO, Vector3.UP)
	cam_pivot.add_child(camera)

	var light := DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-40, 30, 0)
	add_child(light)

	var ui_layer := CanvasLayer.new()
	add_child(ui_layer)
	var panel := PanelContainer.new()
	panel.position = Vector2(10, 10)
	panel.custom_minimum_size = Vector2(260, 130)
	ui_layer.add_child(panel)
	var vbox := VBoxContainer.new()
	panel.add_child(vbox)

	status_label = Label.new()
	status_label.text = "SHM: connecting..."
	vbox.add_child(status_label)

	stats_label = Label.new()
	stats_label.text = "Particles: 0"
	vbox.add_child(stats_label)

	fps_label = Label.new()
	fps_label.text = "FPS: 0"
	vbox.add_child(fps_label)

	mode_label = Label.new()
	mode_label.text = "Mode: SHM"
	vbox.add_child(mode_label)

	toggle_button = Button.new()
	toggle_button.text = "Stop"
	toggle_button.pressed.connect(_on_toggle_polling)
	vbox.add_child(toggle_button)

	particles_button = Button.new()
	particles_button.text = "Hide Particles"
	particles_button.pressed.connect(_on_toggle_particles)
	vbox.add_child(particles_button)

	var reset_button := Button.new()
	reset_button.text = "Reset Camera"
	reset_button.pressed.connect(_on_reset_camera)
	vbox.add_child(reset_button)

	_update_camera()

func _process(delta: float) -> void:
	reconnect_timer += delta
	if not using_mock and not bridge.is_connected() and reconnect_timer >= reconnect_interval:
		bridge.open(shm_name)
		reconnect_timer = 0.0

	if polling and bridge.poll():
		last_update_age = 0.0
		_update_from_bridge()
	else:
		last_update_age += delta
		if bridge.is_connected():
			status_label.text = (using_mock ? "MOCK" : "SHM") + ": connected (stale %.1fs)" % last_update_age
		else:
			status_label.text = (using_mock ? "MOCK" : "SHM") + ": disconnected"

	fps_accum += delta
	if fps_accum >= 0.25:
		fps_label.text = "FPS: %d" % Engine.get_frames_per_second()
		fps_accum = 0.0

func _update_from_bridge() -> void:
	var count := bridge.get_particle_count()
	status_label.text = bridge.is_connected() ? ((using_mock ? "MOCK" : "SHM") + ": connected") : ((using_mock ? "MOCK" : "SHM") + ": disconnected")
	stats_label.text = "Particles: %d  Time: %.2f s" % [count, bridge.get_sim_time_seconds()]

	if not show_particles:
		multimesh_instance.visible = false
		return
	multimesh_instance.visible = true
	if multimesh.instance_count != count:
		multimesh.instance_count = count
	var positions := bridge.get_positions()
	var safe_count := min(count, positions.size())
	for i in range(safe_count):
		var t := Transform3D(Basis(), positions[i])
		multimesh.set_instance_transform(i, t)

func _input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_RIGHT:
			rotating = event.pressed
		if event.button_index == MOUSE_BUTTON_WHEEL_UP and event.pressed:
			orbit_distance = max(1.0, orbit_distance - 0.5)
			_update_camera()
		if event.button_index == MOUSE_BUTTON_WHEEL_DOWN and event.pressed:
			orbit_distance += 0.5
			_update_camera()
	elif event is InputEventMouseMotion and rotating:
		orbit_yaw -= event.relative.x * 0.01
		orbit_pitch = clamp(orbit_pitch - event.relative.y * 0.01, -1.2, 0.3)
		_update_camera()

func _update_camera() -> void:
	cam_pivot.rotation = Vector3(orbit_pitch, orbit_yaw, 0)
	camera.position = Vector3(0, 0, orbit_distance)
	camera.look_at(Vector3.ZERO, Vector3.UP)

func _on_toggle_polling() -> void:
	polling = not polling
	toggle_button.text = "Start" if not polling else "Stop"

func _on_toggle_particles() -> void:
	show_particles = not show_particles
	particles_button.text = "Show Particles" if not show_particles else "Hide Particles"

func _on_reset_camera() -> void:
	orbit_distance = 6.0
	orbit_yaw = 0.0
	orbit_pitch = -0.4
	_update_camera()
