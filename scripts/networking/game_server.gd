extends Node

const PORT: int = 7777
const MAX_CLIENTS: int = 16
const DEFAULT_SERVER_HOST: String = "127.0.0.1"


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

	var kim_core := get_node_or_null("/root/KimCore")
	if kim_core == null:
		push_warning("[Server] KimCore autoload is missing.")
		return

	var personality_module: Node = kim_core.activate_module("personality")
	if personality_module != null and personality_module.has_method("send_message"):
		personality_module.call("send_message", sender_id, text)


func send_text_to_kim(text: String) -> void:
	if multiplayer.is_server():
		client_send_to_kim(text)
	else:
		client_send_to_kim.rpc_id(1, text)
