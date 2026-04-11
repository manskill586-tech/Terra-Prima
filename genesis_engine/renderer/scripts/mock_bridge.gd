extends RefCounted

var connected := true
var sim_time := 0.0
var particle_count := 2000
var positions := PackedVector3Array()

func open(_name: String) -> void:
	connected = true
	_init_positions()

func poll() -> bool:
	sim_time += 1.0 / 60.0
	_update_positions()
	return true

func get_particle_count() -> int:
	return particle_count

func get_positions() -> PackedVector3Array:
	return positions

func get_sim_time_seconds() -> float:
	return sim_time

func is_connected() -> bool:
	return connected

func _init_positions() -> void:
	positions.resize(particle_count)
	_update_positions()

func _update_positions() -> void:
	var t := sim_time
	var count := particle_count
	for i in range(count):
		var angle := (float(i) / float(count)) * TAU + t * 0.3
		var radius := 2.0 + 0.5 * sin(t * 0.7 + float(i) * 0.13)
		var y := sin(t * 0.9 + float(i) * 0.05) * 0.4
		positions[i] = Vector3(cos(angle) * radius, y, sin(angle) * radius)
