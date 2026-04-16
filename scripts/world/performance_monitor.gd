extends Node

# CLIENT-SIDE: samples FPS and periodically reports the rolling average,
# target FPS cap, and monitor refresh rate to the server.
# The server uses these to compute a player-specific penalty threshold.

@export var report_interval: float = 8.0   # seconds between server reports
@export var sample_window: int = 90        # frames in rolling average

signal fps_dropped_locally(avg_fps: float, threshold: float)
signal fps_recovered_locally(avg_fps: float)

var _samples: Array = []
var _report_timer: float = 0.0
var _was_below_threshold: bool = false


func _process(delta: float) -> void:
	if delta <= 0.0:
		return

	_samples.append(Engine.get_frames_per_second())
	if _samples.size() > sample_window:
		_samples = _samples.slice(_samples.size() - sample_window, _samples.size())

	# Local threshold crossing notification (uses local effective target)
	var avg: float = get_average_fps()
	var local_threshold: float = get_effective_target_fps() * (1.0 - get_tolerance_ratio())
	if avg < local_threshold and not _was_below_threshold:
		_was_below_threshold = true
		fps_dropped_locally.emit(avg, local_threshold)
	elif avg >= get_effective_target_fps() and _was_below_threshold:
		_was_below_threshold = false
		fps_recovered_locally.emit(avg)

	_report_timer += delta
	if _report_timer >= report_interval:
		_report_timer = 0.0
		_send_fps_report(avg)


# ─── FPS / target queries ─────────────────────────────────────────────────────

func get_average_fps() -> float:
	if _samples.is_empty():
		return float(Engine.get_frames_per_second())
	var total: float = 0.0
	for s in _samples:
		total += float(s)
	return total / float(_samples.size())


func get_target_fps() -> float:
	# Engine.get_max_fps() returns 0 when uncapped.
	var cap: int = Engine.get_max_fps()
	if cap <= 0:
		# Uncapped — treat monitor rate as the intent.
		return get_monitor_refresh_rate()
	return float(cap)


func get_monitor_refresh_rate() -> float:
	# DisplayServer.screen_get_refresh_rate returns -1 if unknown.
	var hz: float = DisplayServer.screen_get_refresh_rate()
	if hz <= 0.0:
		return 60.0  # safe fallback
	return hz


func get_effective_target_fps() -> float:
	# Effective = min(target, monitor). A 120-FPS cap on a 60 Hz monitor → 60.
	var target: float = get_target_fps()
	var hz: float = get_monitor_refresh_rate()
	return minf(target, hz)


func get_tolerance_ratio() -> float:
	# Fixed tolerance: 1/6 ≈ 16.7%.
	# At 60 FPS target → threshold 50. At 360 → threshold 300.
	return 1.0 / 6.0


func get_penalty_threshold() -> float:
	return get_effective_target_fps() * (1.0 - get_tolerance_ratio())


# ─── Report to server ─────────────────────────────────────────────────────────

func _send_fps_report(avg_fps: float) -> void:
	var server: Node = _get_game_server()
	if server == null or not server.has_method("client_report_fps"):
		return
	if multiplayer.multiplayer_peer == null:
		return
	# Send avg FPS, the player's target cap, and monitor Hz.
	server.call("client_report_fps", avg_fps, get_target_fps(), get_monitor_refresh_rate())


# ─── Helpers ──────────────────────────────────────────────────────────────────

func _get_game_server() -> Node:
	var direct: Node = get_node_or_null("/root/GameServer")
	if direct != null:
		return direct
	return _find_by_script(get_tree().root, "game_server.gd")


func _find_by_script(node: Node, script_file: String) -> Node:
	var s: Script = node.get_script() as Script
	if s != null and s.resource_path.ends_with(script_file):
		return node
	for child in node.get_children():
		var found: Node = _find_by_script(child, script_file)
		if found != null:
			return found
	return null
