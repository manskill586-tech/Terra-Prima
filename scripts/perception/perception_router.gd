extends Node

signal context_routed(player_id: int)
signal routing_failed(message: String)
signal health_changed(status: String, last_error: String)

@export var target_game_server_path: NodePath

var _service_status: String = "UNKNOWN"
var _last_error: String = ""


func route_perception_context(player_id: int, perception_context: Dictionary) -> bool:
	var server: Node = _resolve_game_server()
	if server == null:
		var message: String = "[PerceptionRouter] GameServer not found."
		_last_error = message
		routing_failed.emit(message)
		return false

	if not server.has_method("send_perception_context_to_kim"):
		var unsupported: String = "[PerceptionRouter] GameServer has no send_perception_context_to_kim method."
		_last_error = unsupported
		routing_failed.emit(unsupported)
		return false

	server.call("send_perception_context_to_kim", player_id, perception_context)
	context_routed.emit(player_id)
	return true


func update_service_health(status: String, last_error: String = "") -> void:
	_service_status = status
	_last_error = last_error
	health_changed.emit(_service_status, _last_error)


func is_service_available() -> bool:
	return _service_status == "SERVING"


func get_last_error() -> String:
	return _last_error


func _resolve_game_server() -> Node:
	if target_game_server_path != NodePath():
		var from_path: Node = get_node_or_null(target_game_server_path)
		if from_path != null:
			return from_path

	var root: Node = get_tree().root
	return root.find_child("GameServer", true, false)
