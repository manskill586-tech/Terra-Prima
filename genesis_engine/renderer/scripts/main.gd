extends Node3D

const MockBridge = preload("res://scripts/mock_bridge.gd")

@onready var ui = $UILayer/UI

var bridge
var polling := true
var show_particles := true
var show_chunks := true
var shm_name := ""
var reconnect_interval := 1.0
var reconnect_timer := 0.0
var last_update_age := 0.0
var using_mock := false
var fps_accum := 0.0

const PHASE_GAS := 0
const PHASE_LIQUID := 1
const PHASE_SOLID := 2
const PHASE_PLASMA := 3

const MODE_COLOR := 0
const MODE_PHASE := 1
const MODE_TEMP := 2
const MODE_HARDNESS := 3

var visual_mode := MODE_PHASE

var multimesh_instance: MultiMeshInstance3D
var multimesh: MultiMesh

var chunk_instances: Array = []
var chunk_multimesh: Array = []
var chunk_material: Material
var chunk_sizes := [2.0, 32.0, 256.0]
var render_tiers := [true, false, false]
var tier_scale := [1.0, 0.25, 0.1]

var probe_positions := PackedVector3Array()
var probe_tph := PackedFloat32Array()
var probe_hardness := PackedFloat32Array()
var probe_phase := PackedInt32Array()
var probe_species := PackedInt32Array()

var cam_pivot: Node3D
var camera: Camera3D
var orbit_distance := 6.0
var orbit_yaw := 0.0
var orbit_pitch := -0.4
var rotating := false

const MM_COLOR_8BIT := 1
const MM_CUSTOM_FLOAT := 2

func _ready() -> void:
	_setup_scene()
	shm_name = OS.get_environment("GENESIS_SHM_NAME")
	if shm_name == "":
		shm_name = "GenesisSim"
	if ClassDB.class_exists("GenesisBridge"):
		bridge = ClassDB.instantiate("GenesisBridge")
	else:
		bridge = MockBridge.new()
		using_mock = true
	bridge.open(shm_name)
	ui.set_mode("MOCK" if using_mock else "SHM")
	ui.set_polling(polling)
	ui.set_particles_visible(show_particles)
	ui.set_chunks_visible(show_chunks)
	ui.set_visual_mode(visual_mode)
	ui.set_status(("MOCK" if using_mock else "SHM") + ": connecting...")
	ui.toggle_polling.connect(_on_toggle_polling)
	ui.toggle_particles.connect(_on_toggle_particles)
	ui.toggle_chunks.connect(_on_toggle_chunks)
	ui.visual_mode_changed.connect(_on_visual_mode_changed)
	ui.reset_camera.connect(_on_reset_camera)

func _setup_scene() -> void:
	multimesh_instance = MultiMeshInstance3D.new()
	multimesh = MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	_set_multimesh_color_format(multimesh)
	multimesh.instance_count = 0
	var sphere := SphereMesh.new()
	sphere.radius = 0.03
	sphere.height = 0.06
	multimesh.mesh = sphere
	multimesh_instance.multimesh = multimesh
	var particle_mat := StandardMaterial3D.new()
	particle_mat.vertex_color_use_as_albedo = true
	particle_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	multimesh_instance.material_override = particle_mat
	add_child(multimesh_instance)

	_setup_chunk_renderers()

	cam_pivot = Node3D.new()
	add_child(cam_pivot)
	camera = Camera3D.new()
	camera.position = Vector3(0, 0, orbit_distance)
	cam_pivot.add_child(camera)

	var light := DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-40, 30, 0)
	add_child(light)

	_update_camera()

func _setup_chunk_renderers() -> void:
	var shader_res = load("res://shaders/chunk.gdshader")
	if shader_res is Shader:
		var mat := ShaderMaterial.new()
		mat.shader = shader_res
		chunk_material = mat
	else:
		var fallback := StandardMaterial3D.new()
		fallback.vertex_color_use_as_albedo = true
		fallback.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		chunk_material = fallback

	for phase in range(4):
		var mm := MultiMesh.new()
		mm.transform_format = MultiMesh.TRANSFORM_3D
		_set_multimesh_color_format(mm)
		_set_multimesh_custom_format(mm)
		mm.instance_count = 0
		mm.mesh = _mesh_for_phase(phase)

		var mmi := MultiMeshInstance3D.new()
		mmi.multimesh = mm
		mmi.material_override = chunk_material
		add_child(mmi)

		chunk_multimesh.append(mm)
		chunk_instances.append(mmi)

func _mesh_for_phase(phase: int) -> Mesh:
	match phase:
		PHASE_GAS:
			var prism := PrismMesh.new()
			prism.size = Vector3(0.6, 0.6, 0.6)
			return prism
		PHASE_LIQUID:
			var sphere := SphereMesh.new()
			sphere.radius = 0.5
			sphere.height = 1.0
			return sphere
		PHASE_SOLID:
			var box := BoxMesh.new()
			box.size = Vector3(1.0, 1.0, 1.0)
			return box
		PHASE_PLASMA:
			var sphere := SphereMesh.new()
			sphere.radius = 0.5
			sphere.height = 1.0
			return sphere
	return BoxMesh.new()

func _process(delta: float) -> void:
	reconnect_timer += delta
	if not using_mock and not bridge.is_shm_connected() and reconnect_timer >= reconnect_interval:
		bridge.open(shm_name)
		reconnect_timer = 0.0

	if polling and bridge.poll():
		last_update_age = 0.0
		_update_from_bridge()
	else:
		last_update_age += delta
		if bridge.is_shm_connected():
			ui.set_status(("MOCK" if using_mock else "SHM") + ": connected (stale %.1fs)" % last_update_age)
		else:
			ui.set_status(("MOCK" if using_mock else "SHM") + ": disconnected")

	fps_accum += delta
	if fps_accum >= 0.25:
		ui.set_fps(Engine.get_frames_per_second())
		fps_accum = 0.0

func _update_from_bridge() -> void:
	var count: int = int(bridge.get_particle_count())
	var mode_label := "MOCK" if using_mock else "SHM"
	ui.set_status((mode_label + ": connected") if bridge.is_shm_connected() else (mode_label + ": disconnected"))
	ui.set_stats(count, bridge.get_sim_time_seconds())

	if not show_particles:
		multimesh_instance.visible = false
	else:
		multimesh_instance.visible = true
		if multimesh.instance_count != count:
			multimesh.instance_count = count
		var positions: PackedVector3Array = bridge.get_positions() as PackedVector3Array
		var colors := PackedColorArray()
		if bridge.has_method("get_particle_colors"):
			colors = bridge.get_particle_colors() as PackedColorArray
		var safe_count: int = min(count, positions.size())
		for i in range(safe_count):
			var t := Transform3D(Basis(), positions[i])
			multimesh.set_instance_transform(i, t)
			if colors.size() > i:
				multimesh.set_instance_color(i, colors[i])

	_update_chunks_from_bridge()

func _update_chunks_from_bridge() -> void:
	for inst in chunk_instances:
		inst.visible = show_chunks
	if not show_chunks:
		return
	if not bridge.has_method("get_near_chunk_positions"):
		return

	probe_positions = PackedVector3Array()
	probe_tph = PackedFloat32Array()
	probe_hardness = PackedFloat32Array()
	probe_phase = PackedInt32Array()
	probe_species = PackedInt32Array()

	var phase_positions = [PackedVector3Array(), PackedVector3Array(), PackedVector3Array(), PackedVector3Array()]
	var phase_colors = [PackedColorArray(), PackedColorArray(), PackedColorArray(), PackedColorArray()]
	var phase_custom = [PackedColorArray(), PackedColorArray(), PackedColorArray(), PackedColorArray()]
	var phase_scales = [PackedFloat32Array(), PackedFloat32Array(), PackedFloat32Array(), PackedFloat32Array()]

	var tiers: Array = []
	if render_tiers[0]:
		tiers.append(_fetch_tier_data("near", 0))
	if render_tiers[1]:
		tiers.append(_fetch_tier_data("mid", 1))
	if render_tiers[2]:
		tiers.append(_fetch_tier_data("far", 2))

	for tier_data in tiers:
		if tier_data.is_empty():
			continue
		var positions: PackedVector3Array = tier_data["positions"] as PackedVector3Array
		var phases: PackedInt32Array = tier_data["phases"] as PackedInt32Array
		var colors: PackedColorArray = tier_data["colors"] as PackedColorArray
		var custom: PackedColorArray = tier_data["custom"] as PackedColorArray
		var scales: PackedFloat32Array = tier_data["scales"] as PackedFloat32Array
		var hardness: PackedFloat32Array = tier_data["hardness"] as PackedFloat32Array
		var tph: PackedFloat32Array = tier_data["tph"] as PackedFloat32Array
		var species: PackedInt32Array = tier_data["species"] as PackedInt32Array
		var chunk_size: float = float(tier_data["chunk_size"])
		var tier_index: int = int(tier_data["tier_index"])

		var count: int = positions.size()
		for i in range(count):
			var phase: int = phases[i]
			if phase < 0 or phase > 3:
				phase = PHASE_GAS
			var color: Color = colors[i]
			var temp: float = tph[i * 3]
			var hard: float = hardness[i]
			color = _apply_visual_mode(color, phase, temp, hard)
			phase_positions[phase].append(positions[i])
			phase_colors[phase].append(color)
			phase_custom[phase].append(custom[i])
			var abs_scale: float = scales[i] * chunk_size * tier_scale[tier_index]
			phase_scales[phase].append(abs_scale)

			probe_positions.append(positions[i])
			probe_phase.append(phase)
			probe_hardness.append(hard)
			probe_species.append(species[i])
			probe_tph.append(tph[i * 3])
			probe_tph.append(tph[i * 3 + 1])
			probe_tph.append(tph[i * 3 + 2])

	for phase in range(4):
		var mm: MultiMesh = chunk_multimesh[phase]
		var count: int = phase_positions[phase].size()
		mm.instance_count = count
		for i in range(count):
			var pos: Vector3 = phase_positions[phase][i]
			var instance_scale: float = phase_scales[phase][i]
			var instance_basis := Basis().scaled(Vector3(instance_scale, instance_scale, instance_scale))
			mm.set_instance_transform(i, Transform3D(instance_basis, pos))
			mm.set_instance_color(i, phase_colors[phase][i])
			mm.set_instance_custom_data(i, phase_custom[phase][i])

func _fetch_tier_data(prefix: String, tier_index: int) -> Dictionary:
	var method_positions := "get_%s_chunk_positions" % prefix
	if not bridge.has_method(method_positions):
		return {}
	var data := {}
	var positions: PackedVector3Array = bridge.call(method_positions) as PackedVector3Array
	data["positions"] = positions
	var count: int = positions.size()

	var phases: PackedInt32Array = PackedInt32Array()
	if bridge.has_method("get_%s_chunk_phases" % prefix):
		phases = bridge.call("get_%s_chunk_phases" % prefix) as PackedInt32Array
	data["phases"] = phases
	var colors: PackedColorArray = PackedColorArray()
	if bridge.has_method("get_%s_chunk_colors" % prefix):
		colors = bridge.call("get_%s_chunk_colors" % prefix) as PackedColorArray
	data["colors"] = colors
	var custom: PackedColorArray = PackedColorArray()
	if bridge.has_method("get_%s_chunk_custom" % prefix):
		custom = bridge.call("get_%s_chunk_custom" % prefix) as PackedColorArray
	data["custom"] = custom
	var scales: PackedFloat32Array = PackedFloat32Array()
	if bridge.has_method("get_%s_chunk_scale" % prefix):
		scales = bridge.call("get_%s_chunk_scale" % prefix) as PackedFloat32Array
	data["scales"] = scales
	var hardness: PackedFloat32Array = PackedFloat32Array()
	if bridge.has_method("get_%s_chunk_hardness" % prefix):
		hardness = bridge.call("get_%s_chunk_hardness" % prefix) as PackedFloat32Array
	data["hardness"] = hardness
	var tph: PackedFloat32Array = PackedFloat32Array()
	if bridge.has_method("get_%s_chunk_tph" % prefix):
		tph = bridge.call("get_%s_chunk_tph" % prefix) as PackedFloat32Array
	data["tph"] = tph
	var species: PackedInt32Array = PackedInt32Array()
	if bridge.has_method("get_%s_chunk_species" % prefix):
		species = bridge.call("get_%s_chunk_species" % prefix) as PackedInt32Array
	data["species"] = species

	if data["phases"].size() != count:
		var phases_fixed := PackedInt32Array()
		phases_fixed.resize(count)
		data["phases"] = phases_fixed
	if data["colors"].size() != count:
		var colors_fixed := PackedColorArray()
		colors_fixed.resize(count)
		for i in range(count):
			colors_fixed[i] = Color(0.7, 0.7, 0.7, 1.0)
		data["colors"] = colors_fixed
	if data["custom"].size() != count:
		var custom_fixed := PackedColorArray()
		custom_fixed.resize(count)
		for i in range(count):
			custom_fixed[i] = Color(0.6, 0.4, 0.0, 0.0)
		data["custom"] = custom_fixed
	if data["scales"].size() != count:
		var scales_fixed := PackedFloat32Array()
		scales_fixed.resize(count)
		for i in range(count):
			scales_fixed[i] = 0.6
		data["scales"] = scales_fixed
	if data["hardness"].size() != count:
		var hardness_fixed := PackedFloat32Array()
		hardness_fixed.resize(count)
		data["hardness"] = hardness_fixed
	if data["species"].size() != count:
		var species_fixed := PackedInt32Array()
		species_fixed.resize(count)
		data["species"] = species_fixed
	if data["tph"].size() != count * 3:
		var tph_fixed := PackedFloat32Array()
		tph_fixed.resize(count * 3)
		data["tph"] = tph_fixed
	data["chunk_size"] = chunk_sizes[tier_index]
	data["tier_index"] = tier_index
	return data

func _apply_visual_mode(color: Color, phase: int, temp: float, hardness: float) -> Color:
	match visual_mode:
		MODE_PHASE:
			var c := _color_for_phase(phase)
			c.a = 0.0
			return c
		MODE_TEMP:
			var c := _color_for_temp(temp)
			c.a = 0.0
			return c
		MODE_HARDNESS:
			var h: float = clamp(hardness, 0.0, 1.0)
			return Color(h, h, h, 0.0)
	return color

func _has_property(obj: Object, prop: String) -> bool:
	for info in obj.get_property_list():
		if info.name == prop:
			return true
	return false

func _set_multimesh_color_format(mm: MultiMesh) -> void:
	if _has_property(mm, "color_format"):
		mm.color_format = MM_COLOR_8BIT
	elif _has_property(mm, "use_colors"):
		mm.use_colors = true

func _set_multimesh_custom_format(mm: MultiMesh) -> void:
	if _has_property(mm, "custom_data_format"):
		mm.custom_data_format = MM_CUSTOM_FLOAT
	elif _has_property(mm, "use_custom_data"):
		mm.use_custom_data = true

func _color_for_phase(phase: int) -> Color:
	match phase:
		PHASE_GAS:
			return Color(0.2, 0.6, 1.0, 1.0)
		PHASE_LIQUID:
			return Color(0.1, 0.4, 1.0, 1.0)
		PHASE_SOLID:
			return Color(0.6, 0.6, 0.6, 1.0)
		PHASE_PLASMA:
			return Color(1.0, 0.4, 0.1, 1.0)
	return Color(0.7, 0.7, 0.7, 1.0)

func _color_for_temp(temp: float) -> Color:
	var t: float = clamp((temp - 200.0) / 1000.0, 0.0, 1.0)
	return Color(0.2 + t * 0.8, 0.2 + t * 0.3, 1.0 - t * 0.7, 1.0)

func _input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_RIGHT:
			rotating = event.pressed
		if event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
			_probe_at(event.position)
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

func _probe_at(screen_pos: Vector2) -> void:
	if probe_positions.size() == 0:
		return
	var origin: Vector3 = camera.project_ray_origin(screen_pos)
	var dir: Vector3 = camera.project_ray_normal(screen_pos)
	var best_idx: int = -1
	var best_dist: float = 1e9
	for i in range(probe_positions.size()):
		var p: Vector3 = probe_positions[i]
		var t: float = dir.dot(p - origin)
		if t < 0.0:
			continue
		var closest: Vector3 = origin + dir * t
		var d: float = (p - closest).length()
		if d < best_dist:
			best_dist = d
			best_idx = i
	if best_idx < 0:
		return
	var temp: float = probe_tph[best_idx * 3]
	var pressure: float = probe_tph[best_idx * 3 + 1]
	var humidity: float = probe_tph[best_idx * 3 + 2]
	var phase: int = probe_phase[best_idx]
	var hardness: float = probe_hardness[best_idx]
	var species: int = probe_species[best_idx]
	ui.set_probe_info("Probe: phase=%d  species=%d  T=%.1fK  P=%.0fPa  H=%.2f  hard=%.2f" % [phase, species, temp, pressure, humidity, hardness])

func _update_camera() -> void:
	cam_pivot.rotation = Vector3(orbit_pitch, orbit_yaw, 0)
	camera.position = Vector3(0, 0, orbit_distance)
	camera.look_at(Vector3.ZERO, Vector3.UP)

func _on_toggle_polling() -> void:
	polling = not polling
	ui.set_polling(polling)

func _on_toggle_particles() -> void:
	show_particles = not show_particles
	ui.set_particles_visible(show_particles)

func _on_toggle_chunks() -> void:
	show_chunks = not show_chunks
	ui.set_chunks_visible(show_chunks)
	_update_chunks_from_bridge()

func _on_visual_mode_changed(mode: int) -> void:
	visual_mode = mode
	ui.set_visual_mode(visual_mode)
	_update_chunks_from_bridge()

func _on_reset_camera() -> void:
	orbit_distance = 6.0
	orbit_yaw = 0.0
	orbit_pitch = -0.4
	_update_camera()
