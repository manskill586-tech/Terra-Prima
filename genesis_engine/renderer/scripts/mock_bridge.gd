extends RefCounted

var connected := true
var sim_time := 0.0
var particle_count := 2000
var positions := PackedVector3Array()
var particle_colors := PackedColorArray()

var near_chunk_positions := PackedVector3Array()
var near_chunk_phases := PackedInt32Array()
var near_chunk_species := PackedInt32Array()
var near_chunk_colors := PackedColorArray()
var near_chunk_custom := PackedColorArray()
var near_chunk_hardness := PackedFloat32Array()
var near_chunk_tph := PackedFloat32Array()
var near_chunk_scale := PackedFloat32Array()

var mid_chunk_positions := PackedVector3Array()
var mid_chunk_phases := PackedInt32Array()
var mid_chunk_species := PackedInt32Array()
var mid_chunk_colors := PackedColorArray()
var mid_chunk_custom := PackedColorArray()
var mid_chunk_hardness := PackedFloat32Array()
var mid_chunk_tph := PackedFloat32Array()
var mid_chunk_scale := PackedFloat32Array()

var far_chunk_positions := PackedVector3Array()
var far_chunk_phases := PackedInt32Array()
var far_chunk_species := PackedInt32Array()
var far_chunk_colors := PackedColorArray()
var far_chunk_custom := PackedColorArray()
var far_chunk_hardness := PackedFloat32Array()
var far_chunk_tph := PackedFloat32Array()
var far_chunk_scale := PackedFloat32Array()

func open(_name: String) -> void:
	connected = true
	_init_positions()
	_init_chunks()

func poll() -> bool:
	sim_time += 1.0 / 60.0
	_update_positions()
	_update_chunks()
	return true

func get_particle_count() -> int:
	return particle_count

func get_positions() -> PackedVector3Array:
	return positions

func get_particle_colors() -> PackedColorArray:
	return particle_colors

func get_sim_time_seconds() -> float:
	return sim_time

func is_shm_connected() -> bool:
	return connected

func get_near_chunk_count() -> int:
	return near_chunk_positions.size()

func get_mid_chunk_count() -> int:
	return mid_chunk_positions.size()

func get_far_chunk_count() -> int:
	return far_chunk_positions.size()

func get_near_chunk_positions() -> PackedVector3Array:
	return near_chunk_positions

func get_mid_chunk_positions() -> PackedVector3Array:
	return mid_chunk_positions

func get_far_chunk_positions() -> PackedVector3Array:
	return far_chunk_positions

func get_near_chunk_phases() -> PackedInt32Array:
	return near_chunk_phases

func get_mid_chunk_phases() -> PackedInt32Array:
	return mid_chunk_phases

func get_far_chunk_phases() -> PackedInt32Array:
	return far_chunk_phases

func get_near_chunk_species() -> PackedInt32Array:
	return near_chunk_species

func get_mid_chunk_species() -> PackedInt32Array:
	return mid_chunk_species

func get_far_chunk_species() -> PackedInt32Array:
	return far_chunk_species

func get_near_chunk_colors() -> PackedColorArray:
	return near_chunk_colors

func get_mid_chunk_colors() -> PackedColorArray:
	return mid_chunk_colors

func get_far_chunk_colors() -> PackedColorArray:
	return far_chunk_colors

func get_near_chunk_custom() -> PackedColorArray:
	return near_chunk_custom

func get_mid_chunk_custom() -> PackedColorArray:
	return mid_chunk_custom

func get_far_chunk_custom() -> PackedColorArray:
	return far_chunk_custom

func get_near_chunk_hardness() -> PackedFloat32Array:
	return near_chunk_hardness

func get_mid_chunk_hardness() -> PackedFloat32Array:
	return mid_chunk_hardness

func get_far_chunk_hardness() -> PackedFloat32Array:
	return far_chunk_hardness

func get_near_chunk_scale() -> PackedFloat32Array:
	return near_chunk_scale

func get_mid_chunk_scale() -> PackedFloat32Array:
	return mid_chunk_scale

func get_far_chunk_scale() -> PackedFloat32Array:
	return far_chunk_scale

func get_near_chunk_tph() -> PackedFloat32Array:
	return near_chunk_tph

func get_mid_chunk_tph() -> PackedFloat32Array:
	return mid_chunk_tph

func get_far_chunk_tph() -> PackedFloat32Array:
	return far_chunk_tph

func _init_positions() -> void:
	positions.resize(particle_count)
	particle_colors.resize(particle_count)
	_update_positions()

func _update_positions() -> void:
	var t: float = sim_time
	var count: int = particle_count
	for i in range(count):
		var angle := (float(i) / float(count)) * TAU + t * 0.3
		var radius := 2.0 + 0.5 * sin(t * 0.7 + float(i) * 0.13)
		var y := sin(t * 0.9 + float(i) * 0.05) * 0.4
		positions[i] = Vector3(cos(angle) * radius, y, sin(angle) * radius)
		var hue := fmod(float(i) * 0.02 + t * 0.05, 1.0)
		particle_colors[i] = Color.from_hsv(hue, 0.8, 0.9, 1.0)

func _init_chunks() -> void:
	near_chunk_positions.clear()
	near_chunk_phases.clear()
	near_chunk_species.clear()
	near_chunk_colors.clear()
	near_chunk_custom.clear()
	near_chunk_hardness.clear()
	near_chunk_tph.clear()
	near_chunk_scale.clear()

	var size: float = 2.0
	for z in range(-3, 4):
		for x in range(-3, 4):
			var pos := Vector3((x + 0.5) * size, 0.5 * size, (z + 0.5) * size)
			near_chunk_positions.append(pos)
			near_chunk_phases.append(0)
			near_chunk_species.append(1)
			near_chunk_colors.append(Color(0.6, 0.6, 0.6, 1.0))
			near_chunk_custom.append(Color(0.6, 0.4, 0.0, 0.0))
			near_chunk_hardness.append(0.2)
			near_chunk_tph.append_array(PackedFloat32Array([300.0, 101325.0, 0.5]))
			near_chunk_scale.append(0.6)

func _update_chunks() -> void:
	var count: int = near_chunk_positions.size()
	if count == 0:
		return
	var t: float = sim_time
	for i in range(count):
		var x: float = near_chunk_positions[i].x
		var z: float = near_chunk_positions[i].z
		var dist: float = sqrt(x * x + z * z)
		var temp: float = 200.0 + dist * 25.0 + sin(t * 0.8) * 200.0
		var pressure: float = 90000.0 + dist * 500.0
		var humidity: float = clamp(0.2 + dist * 0.02, 0.0, 1.0)
		var phase: int = 0
		if temp < 260.0:
			phase = 2
		elif temp < 360.0:
			phase = 1
		elif temp > 1200.0:
			phase = 3
		near_chunk_phases[i] = phase
		near_chunk_species[i] = 1 + (i % 5)
		var color := Color(0.2, 0.6, 1.0)
		if phase == 1:
			color = Color(0.2, 0.4, 1.0)
		elif phase == 2:
			color = Color(0.6, 0.6, 0.6)
		elif phase == 3:
			color = Color(1.0, 0.4, 0.1)
		near_chunk_colors[i] = color
		var opacity := 0.15 if phase == 0 else (0.6 if phase == 1 else (0.9 if phase == 2 else 0.3))
		var roughness := 1.0 if phase == 0 else (0.15 if phase == 1 else (0.6 if phase == 2 else 0.9))
		var metallic := 0.0
		var emissive := 0.0
		if temp > 700.0:
			emissive = clamp((temp - 700.0) / 2300.0, 0.0, 1.0)
		near_chunk_custom[i] = Color(opacity, roughness, metallic, clamp(pressure / 200000.0, 0.0, 1.0))
		color.a = emissive
		near_chunk_colors[i] = color
		near_chunk_hardness[i] = 0.2 + float(phase == 2) * 0.6
		near_chunk_scale[i] = 0.6 + float(phase == 2) * 0.2
		var base := i * 3
		near_chunk_tph[base + 0] = temp
		near_chunk_tph[base + 1] = pressure
		near_chunk_tph[base + 2] = humidity
