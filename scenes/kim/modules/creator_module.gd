extends Node

signal creation_requested(spec: Dictionary)
signal creation_completed(creation_id: String, metadata: Dictionary)

var active_creations: Dictionary = {}

const PALETTE_GEOMETRY: Array = [
	Color(0.2, 0.6, 1.0),
	Color(0.8, 0.3, 0.9),
	Color(0.1, 0.9, 0.7),
]
const PALETTE_BIOLOGY: Array = [
	Color(0.2, 0.8, 0.3),
	Color(0.9, 0.7, 0.1),
	Color(0.6, 0.9, 0.2),
]
const PALETTE_SPACE: Array = [
	Color(0.9, 0.9, 1.0),
	Color(0.4, 0.1, 0.8),
	Color(0.1, 0.3, 0.9),
]


func request_creation(spec: Dictionary) -> void:
	creation_requested.emit(spec)


func complete_creation(creation_id: String, metadata: Dictionary = {}) -> void:
	creation_completed.emit(creation_id, metadata)


func parse_creation_command(json_text: String) -> Dictionary:
	var parsed: Variant = JSON.parse_string(json_text)
	if parsed == null or parsed is not Dictionary:
		push_warning("[CreatorModule] Failed to parse creation command JSON: %s" % json_text)
		return {}

	var cmd: Dictionary = parsed as Dictionary

	if not cmd.has("action") or str(cmd["action"]) != "create":
		push_warning("[CreatorModule] Missing or invalid 'action' field.")
		return {}

	var raw_pos: Variant = cmd.get("position", [0.0, 0.0, 0.0])
	var position: Vector3 = Vector3.ZERO
	if raw_pos is Array and (raw_pos as Array).size() >= 3:
		var pos_arr: Array = raw_pos as Array
		position = Vector3(float(pos_arr[0]), float(pos_arr[1]), float(pos_arr[2]))

	var scale_val: float = clampf(float(cmd.get("scale", 1.0)), 0.1, 10.0)

	var theme: String = str(cmd.get("theme", "geometry")).to_lower()
	if theme not in ["geometry", "biology", "space"]:
		theme = "geometry"

	var raw_tags: Variant = cmd.get("tags", [])
	var tags: Array = []
	if raw_tags is Array:
		for t in raw_tags as Array:
			tags.append(str(t))

	return {
		"action":   "create",
		"theme":    theme,
		"scale":    scale_val,
		"position": position,
		"tags":     tags,
	}


func spawn_creation(command: Dictionary, parent: Node3D) -> MeshInstance3D:
	if command.is_empty():
		push_warning("[CreatorModule] Empty command passed to spawn_creation.")
		return null

	var theme: String = str(command.get("theme", "geometry"))
	var scale_val: float = clampf(float(command.get("scale", 1.0)), 0.1, 10.0)
	var raw_position: Variant = command.get("position", Vector3.ZERO)
	var position: Vector3 = raw_position if raw_position is Vector3 else Vector3.ZERO

	var palette: Array = _palette_for_theme(theme)
	var poly_budget: int = 500

	var st: SurfaceTool = SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)

	match theme:
		"geometry":
			_generate_fractal_cube(st, poly_budget, palette)
		"biology":
			_generate_organic_blob(st, poly_budget, palette)
		"space":
			_generate_crystal(st, poly_budget, palette)
		_:
			_generate_fractal_cube(st, poly_budget, palette)

	st.index()
	st.generate_normals()
	st.generate_tangents()

	var mesh: ArrayMesh = st.commit()
	var mesh_instance: MeshInstance3D = MeshInstance3D.new()
	mesh_instance.mesh = mesh

	var mat: StandardMaterial3D = StandardMaterial3D.new()
	mat.vertex_color_use_as_albedo = true
	mat.metallic = 0.3
	mat.roughness = 0.6
	mesh_instance.material_override = mat

	mesh_instance.scale = Vector3.ONE * scale_val
	mesh_instance.position = position
	mesh_instance.visibility_range_end = 50.0 + scale_val * 10.0
	mesh_instance.visibility_range_end_margin = 5.0

	var creation_id: String = _generate_creation_id()
	var metadata: Dictionary = {
		"creation_id": creation_id,
		"theme":       theme,
		"scale":       scale_val,
		"tags":        command.get("tags", []),
		"poly_count":  poly_budget,
	}
	active_creations[creation_id] = metadata

	parent.add_child(mesh_instance)
	creation_completed.emit(creation_id, metadata)

	print("[CreatorModule] Spawned '%s' (id=%s)" % [theme, creation_id])
	return mesh_instance


func _generate_creation_id() -> String:
	return "obj_%d_%d" % [Time.get_ticks_msec(), randi() % 9999]


func _palette_for_theme(theme: String) -> Array:
	match theme:
		"geometry": return PALETTE_GEOMETRY
		"biology":  return PALETTE_BIOLOGY
		"space":    return PALETTE_SPACE
		_:          return PALETTE_GEOMETRY


func _generate_fractal_cube(st: SurfaceTool, polys: int, palette: Array) -> void:
	var subdivisions: int = mini(roundi(sqrt(float(polys) / 6.0)), 20)
	if subdivisions < 1:
		subdivisions = 1
	var step: float = 1.0 / float(subdivisions)
	var noise: FastNoiseLite = FastNoiseLite.new()
	noise.noise_type = FastNoiseLite.TYPE_SIMPLEX

	for x in range(subdivisions):
		for y in range(subdivisions):
			var u: float = float(x) * step - 0.5
			var v: float = float(y) * step - 0.5
			var noise_val: float = noise.get_noise_2d(u * 3.0, v * 3.0) * 0.15
			var ci: int = (int(u * 10.0) + int(v * 10.0)) % palette.size()
			if ci < 0:
				ci = 0
			var c: Color = palette[ci]
			st.set_color(c)
			st.add_vertex(Vector3(u,        0.5 + noise_val, v))
			st.add_vertex(Vector3(u + step, 0.5 + noise_val, v))
			st.add_vertex(Vector3(u,        0.5 + noise_val, v + step))
			st.set_color(c)
			st.add_vertex(Vector3(u + step, 0.5 + noise_val, v))
			st.add_vertex(Vector3(u + step, 0.5 + noise_val, v + step))
			st.add_vertex(Vector3(u,        0.5 + noise_val, v + step))


func _generate_organic_blob(st: SurfaceTool, polys: int, palette: Array) -> void:
	var rings: int = mini(maxi(4, roundi(sqrt(float(polys) / 4.0))), 16)
	var segments: int = rings * 2
	var noise: FastNoiseLite = FastNoiseLite.new()
	noise.noise_type = FastNoiseLite.TYPE_SIMPLEX
	noise.frequency = 0.8

	for r in range(rings):
		for s in range(segments):
			var theta0: float = (float(r) / float(rings)) * PI
			var theta1: float = (float(r + 1) / float(rings)) * PI
			var phi0: float = (float(s) / float(segments)) * TAU
			var phi1: float = (float(s + 1) / float(segments)) * TAU

			var v0: Vector3 = _sphere_point(theta0, phi0)
			var v1: Vector3 = _sphere_point(theta0, phi1)
			var v2: Vector3 = _sphere_point(theta1, phi0)
			var v3: Vector3 = _sphere_point(theta1, phi1)

			var n0: float = 1.0 + noise.get_noise_3d(v0.x, v0.y, v0.z) * 0.25
			var n1: float = 1.0 + noise.get_noise_3d(v1.x, v1.y, v1.z) * 0.25
			var n2: float = 1.0 + noise.get_noise_3d(v2.x, v2.y, v2.z) * 0.25
			var n3: float = 1.0 + noise.get_noise_3d(v3.x, v3.y, v3.z) * 0.25

			var ci: int = (r + s) % palette.size()
			st.set_color(palette[ci])
			st.add_vertex(v0 * n0)
			st.add_vertex(v1 * n1)
			st.add_vertex(v2 * n2)
			st.set_color(palette[ci])
			st.add_vertex(v1 * n1)
			st.add_vertex(v3 * n3)
			st.add_vertex(v2 * n2)


func _sphere_point(theta: float, phi: float) -> Vector3:
	return Vector3(
		sin(theta) * cos(phi),
		cos(theta),
		sin(theta) * sin(phi)
	) * 0.5


func _generate_crystal(st: SurfaceTool, polys: int, palette: Array) -> void:
	var faces: int = mini(maxi(4, roundi(float(polys) / 8.0)), 12)
	var height: float = 1.0
	var radius: float = 0.35

	var apex_top: Vector3 = Vector3(0.0, height * 0.5, 0.0)
	var apex_bot: Vector3 = Vector3(0.0, -height * 0.5, 0.0)

	for i in range(faces):
		var angle0: float = (float(i) / float(faces)) * TAU
		var angle1: float = (float(i + 1) / float(faces)) * TAU
		var r_mid: float = radius * 1.2

		var pm0: Vector3 = Vector3(cos(angle0) * r_mid, 0.0, sin(angle0) * r_mid)
		var pm1: Vector3 = Vector3(cos(angle1) * r_mid, 0.0, sin(angle1) * r_mid)
		var p0: Vector3 = Vector3(cos(angle0) * radius, 0.0, sin(angle0) * radius)
		var p1: Vector3 = Vector3(cos(angle1) * radius, 0.0, sin(angle1) * radius)

		var ci: int = i % palette.size()
		st.set_color(palette[ci])
		st.add_vertex(apex_top)
		st.add_vertex(pm1)
		st.add_vertex(pm0)

		st.set_color(palette[ci])
		st.add_vertex(apex_bot)
		st.add_vertex(p0)
		st.add_vertex(p1)

		st.set_color(palette[(ci + 1) % palette.size()])
		st.add_vertex(pm0)
		st.add_vertex(pm1)
		st.add_vertex(p0)
		st.set_color(palette[(ci + 1) % palette.size()])
		st.add_vertex(pm1)
		st.add_vertex(p1)
		st.add_vertex(p0)
