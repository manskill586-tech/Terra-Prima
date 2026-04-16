extends Node

const PORT: int = 7777
const MAX_CLIENTS: int = 16
const DEFAULT_SERVER_HOST: String = "127.0.0.1"

signal kim_response(player_id: int, text: String)
signal player_rated_game(player_id: int, game_id: String, rating: int)

var _personality_module: Node
var _analyst_module: Node
var _pending_player_id: int = 0

# Player node references for proximity checks (player_id -> Node3D)
var _player_nodes: Dictionary = {}
# Kim avatar node reference
var _kim_avatar: Node = null


func _ready() -> void:
	if OS.has_feature("server"):
		_start_server()
	else:
		_start_client()


func _start_server() -> void:
	var peer := ENetMultiplayerPeer.new()
	var err: int = peer.create_server(PORT, MAX_CLIENTS)
	if err != OK:
		push_error("[Server] Failed to start on port %d (err=%d)" % [PORT, err])
		return

	multiplayer.multiplayer_peer = peer
	multiplayer.peer_connected.connect(_on_peer_connected)
	multiplayer.peer_disconnected.connect(_on_peer_disconnected)
	print("[Server] Listening on port %d" % PORT)


func _start_client(host: String = DEFAULT_SERVER_HOST) -> void:
	var peer := ENetMultiplayerPeer.new()
	var err: int = peer.create_client(host, PORT)
	if err != OK:
		push_error("[Client] Failed to connect to %s:%d (err=%d)" % [host, PORT, err])
		return
	multiplayer.multiplayer_peer = peer
	print("[Client] Connecting to %s:%d" % [host, PORT])


func _on_peer_connected(peer_id: int) -> void:
	print("[Server] Peer connected: %d" % peer_id)
	_sync_world_state.rpc_id(peer_id)


func _on_peer_disconnected(peer_id: int) -> void:
	print("[Server] Peer disconnected: %d" % peer_id)


@rpc("authority", "call_remote", "reliable")
func _sync_world_state() -> void:
	# Placeholder for world snapshot sync.
	pass


@rpc("any_peer", "call_local", "unreliable_ordered")
func client_send_to_kim(text: String) -> void:
	if not multiplayer.is_server():
		return

	var sender_id: int = multiplayer.get_remote_sender_id()
	if sender_id <= 0:
		# Host can call this method locally during single-player testing.
		sender_id = multiplayer.get_unique_id()

	# Proximity check: only route to Kim if player is within interaction range
	if not _is_player_in_kim_range(sender_id):
		print("[Server] Player %d sent message but is not in Kim's range — ignored." % sender_id)
		return

	var kim_core := get_node_or_null("/root/KimCore")
	if kim_core == null:
		push_warning("[Server] KimCore autoload is missing.")
		return

	var personality_module: Node = kim_core.activate_module("personality")
	if personality_module != null and personality_module.has_method("send_message"):
		_refresh_profile_from_text(kim_core, sender_id, text)
		_bind_personality_module(personality_module)
		_pending_player_id = sender_id
		personality_module.call("send_message", sender_id, text)


func send_perception_context_to_kim(player_id: int, perception_context: Dictionary) -> void:
	if not multiplayer.is_server():
		return

	var kim_core := get_node_or_null("/root/KimCore")
	if kim_core == null:
		push_warning("[Server] KimCore autoload is missing.")
		return

	var personality_module: Node = kim_core.activate_module("personality")
	var analyst_module: Node = kim_core.activate_module("analyst")
	if analyst_module != null and analyst_module.has_method("track_audio_events"):
		var audio_events: Array = perception_context.get("audio_events", [])
		analyst_module.call("track_audio_events", player_id, audio_events)

	if analyst_module != null and analyst_module.has_method("build_profile_patch") and personality_module != null and personality_module.has_method("update_player_profile"):
		var profile_patch: Dictionary = analyst_module.call("build_profile_patch", player_id)
		if not profile_patch.is_empty():
			personality_module.call("update_player_profile", player_id, profile_patch)

	if personality_module != null and personality_module.has_method("inject_multimodal_context"):
		personality_module.call("inject_multimodal_context", player_id, perception_context)


func send_text_to_kim(text: String) -> void:
	if multiplayer.is_server():
		client_send_to_kim(text)
	else:
		client_send_to_kim.rpc_id(1, text)


@rpc("authority", "call_remote", "reliable")
func server_send_kim_response(player_id: int, text: String) -> void:
	kim_response.emit(player_id, text)
	print("[Kim -> %d] %s" % [player_id, text])


func _bind_personality_module(personality_module: Node) -> void:
	if _personality_module == personality_module:
		return

	if _personality_module != null:
		var old_callback := Callable(self, "_on_personality_response_done")
		if _personality_module.has_signal("response_done") and _personality_module.is_connected("response_done", old_callback):
			_personality_module.disconnect("response_done", old_callback)

	_personality_module = personality_module
	var callback := Callable(self, "_on_personality_response_done")
	if _personality_module.has_signal("response_done") and not _personality_module.is_connected("response_done", callback):
		_personality_module.connect("response_done", callback)


func _on_personality_response_done(full_text: String) -> void:
	if not multiplayer.is_server():
		return

	var target_player := _pending_player_id
	if target_player <= 0:
		target_player = multiplayer.get_unique_id()

	kim_response.emit(target_player, full_text)
	server_send_kim_response.rpc(target_player, full_text)


func _refresh_profile_from_text(kim_core: Node, player_id: int, text: String) -> void:
	_analyst_module = kim_core.activate_module("analyst")
	if _analyst_module == null:
		return
	if not _analyst_module.has_method("track_interaction"):
		return

	var topic: String = _guess_topic_from_text(text)
	_analyst_module.call("track_interaction", player_id, topic, 0.0)
	if not _analyst_module.has_method("build_profile_patch"):
		return

	var profile_patch: Dictionary = _analyst_module.call("build_profile_patch", player_id)
	if profile_patch.is_empty():
		return

	var personality_module: Node = kim_core.activate_module("personality")
	if personality_module == null or not personality_module.has_method("update_player_profile"):
		return
	personality_module.call("update_player_profile", player_id, profile_patch)


# ─── Rating system ────────────────────────────────────────────────────────────

@rpc("any_peer", "call_local", "reliable")
func client_rate_game(game_id: String, rating: int) -> void:
	if not multiplayer.is_server():
		return
	var sender_id: int = multiplayer.get_remote_sender_id()
	if sender_id <= 0:
		sender_id = multiplayer.get_unique_id()
	var clamped: int = clampi(rating, 0, 100)
	var feedback_learner: Node = get_node_or_null("/root/FeedbackLearner")
	if feedback_learner != null and feedback_learner.has_method("rate_game"):
		feedback_learner.call("rate_game", game_id, sender_id, clamped)
	player_rated_game.emit(sender_id, game_id, clamped)
	print("[Server] Player %d rated game '%s': %d/100" % [sender_id, game_id, clamped])


func rate_game(game_id: String, rating: int) -> void:
	if multiplayer.is_server():
		client_rate_game(game_id, rating)
	else:
		client_rate_game.rpc_id(1, game_id, rating)


# ─── Teleport (server-authoritative) ─────────────────────────────────────────

func teleport_player(player_id: int, position: Vector3) -> void:
	if not multiplayer.is_server():
		return
	_do_teleport_player.rpc_id(player_id, position)


@rpc("authority", "call_remote", "reliable")
func _do_teleport_player(position: Vector3) -> void:
	# Executed on the client — move local player to position
	var local_player: Node = _find_local_player_node()
	if local_player != null and local_player.has_method("set_global_position"):
		local_player.call("set_global_position", position)
	elif local_player is Node3D:
		(local_player as Node3D).global_position = position
	print("[Client] Teleported to %s" % str(position))


func _find_local_player_node() -> Node:
	# Override or extend this to return the local player's CharacterBody3D/Node3D.
	return null


# ─── Proximity helpers ────────────────────────────────────────────────────────

func register_player_node(player_id: int, player_node: Node3D) -> void:
	_player_nodes[player_id] = player_node
	_update_avatar_ref()
	if _kim_avatar != null and _kim_avatar.has_method("register_player_node"):
		_kim_avatar.call("register_player_node", player_id, player_node)


func unregister_player_node(player_id: int) -> void:
	_player_nodes.erase(player_id)


func _is_player_in_kim_range(player_id: int) -> bool:
	_update_avatar_ref()
	if _kim_avatar == null:
		# No avatar — allow all messages (single-player / no-avatar fallback)
		return true
	if _kim_avatar.has_method("is_player_in_interaction_range"):
		return _kim_avatar.call("is_player_in_interaction_range", player_id)
	return true


func _update_avatar_ref() -> void:
	if _kim_avatar != null and is_instance_valid(_kim_avatar):
		return
	# Search for KimAvatar in the scene tree
	_kim_avatar = _find_node_by_class(get_tree().root, "KimAvatar")


func _find_node_by_class(node: Node, class_name_str: String) -> Node:
	if node.get_script() != null:
		var script_path: String = str(node.get_script().resource_path)
		if script_path.ends_with("kim_avatar.gd"):
			return node
	for child in node.get_children():
		var found: Node = _find_node_by_class(child, class_name_str)
		if found != null:
			return found
	return null


# ─── FPS reporting ────────────────────────────────────────────────────────────

@rpc("any_peer", "call_local", "unreliable_ordered")
func client_report_fps(avg_fps: float, target_fps: float = 60.0, monitor_hz: float = 60.0) -> void:
	if not multiplayer.is_server():
		return
	var sender_id: int = multiplayer.get_remote_sender_id()
	if sender_id <= 0:
		sender_id = multiplayer.get_unique_id()
	var pm: Node = get_node_or_null("/root/PerformanceManager")
	if pm != null and pm.has_method("receive_fps_report"):
		pm.call("receive_fps_report", sender_id, avg_fps, target_fps, monitor_hz)


# ─── Optimization vote ────────────────────────────────────────────────────────

# Called by PerformanceManager — sends vote request to a specific client
func server_request_optimization_vote(target_player_id: int, penalty_id: String, question: String) -> void:
	if not multiplayer.is_server():
		return
	_ask_player_optimization_vote.rpc_id(target_player_id, penalty_id, question)


@rpc("authority", "call_remote", "reliable")
func _ask_player_optimization_vote(penalty_id: String, question: String) -> void:
	# Executed on the client. The UI should display question and let player respond.
	# Emit a signal that the game's UI layer can connect to.
	print("[Client] Kim's optimization dispute: %s" % question)
	# Game UI connects to this signal to show a dialog.
	# Automatic self-vote after a short delay if no UI is connected:
	_auto_vote_if_no_ui.call_deferred(penalty_id)


func _auto_vote_if_no_ui(penalty_id: String) -> void:
	# Fallback: if no UI connected, player is assumed to agree after 5 seconds
	await get_tree().create_timer(5.0).timeout
	client_vote_on_optimization.rpc_id(1, penalty_id, true)


@rpc("any_peer", "call_local", "reliable")
func client_vote_on_optimization(penalty_id: String, agrees: bool) -> void:
	if not multiplayer.is_server():
		return
	var sender_id: int = multiplayer.get_remote_sender_id()
	if sender_id <= 0:
		sender_id = multiplayer.get_unique_id()
	var pm: Node = get_node_or_null("/root/PerformanceManager")
	if pm != null and pm.has_method("receive_vote"):
		pm.call("receive_vote", sender_id, penalty_id, agrees)


# Called by client UI to manually vote (true = good optimization, false = bad)
func vote_optimization(penalty_id: String, agrees: bool) -> void:
	if multiplayer.is_server():
		client_vote_on_optimization(penalty_id, agrees)
	else:
		client_vote_on_optimization.rpc_id(1, penalty_id, agrees)


func _guess_topic_from_text(text: String) -> String:
	var source: String = text.to_lower()
	if source.find("build") >= 0 or source.find("create") >= 0:
		return "creation"
	if source.find("idea") >= 0 or source.find("concept") >= 0:
		return "ideas"
	if source.find("help") >= 0:
		return "support"
	if source.find("создай") >= 0 or source.find("построй") >= 0 or source.find("сделай") >= 0:
		return "creation"
	if source.find("идея") >= 0 or source.find("концепт") >= 0 or source.find("придумай") >= 0:
		return "ideas"
	if source.find("помоги") >= 0 or source.find("помощь") >= 0:
		return "support"
	return "general"
