extends Node

signal response_chunk(text: String)
signal response_done(full_text: String)

var player_profiles: Dictionary = {}

const BASE_SYSTEM_PROMPT := """
You are Kim, an intelligent host of a virtual world.
Speak vividly and metaphorically.
Current player context: {player_context}
"""

var _stream_buffer: String = ""
var _extra_context: Array = []
var _llm_model: Node
var _llm_chat: Node


func _ready() -> void:
	_llm_model = get_node_or_null("NobodyWhoModel")
	_llm_chat = get_node_or_null("NobodyWhoChat")

	if _llm_chat == null:
		push_warning("[PersonalityModule] NobodyWhoChat node is missing.")
		return

	if _llm_chat.has_signal("response_updated"):
		_llm_chat.connect("response_updated", Callable(self, "_on_response_chunk"))
	if _llm_chat.has_signal("response_finished"):
		_llm_chat.connect("response_finished", Callable(self, "_on_response_done"))


func send_message(player_id: int, text: String) -> void:
	if _llm_chat == null:
		push_warning("[PersonalityModule] LLM chat node is not configured.")
		return

	_stream_buffer = ""
	var player_ctx: String = _build_player_context(player_id)
	var system_prompt: String = BASE_SYSTEM_PROMPT.replace("{player_context}", player_ctx)
	if not _extra_context.is_empty():
		system_prompt += "\n\nExtra context:\n%s" % "\n".join(_extra_context)

	_llm_chat.set("system_prompt", system_prompt)

	if _llm_chat.has_method("tell"):
		_llm_chat.call("tell", text)
	else:
		push_warning("[PersonalityModule] NobodyWhoChat does not expose tell().")


func inject_context(context: String) -> void:
	if context.is_empty():
		return
	_extra_context.append(context)
	if _extra_context.size() > 10:
		_extra_context = _extra_context.slice(_extra_context.size() - 10, _extra_context.size())


func update_player_profile(player_id: int, profile_patch: Dictionary) -> void:
	var current: Dictionary = player_profiles.get(player_id, {})
	for key in profile_patch.keys():
		current[key] = profile_patch[key]
	player_profiles[player_id] = current


func _build_player_context(player_id: int) -> String:
	if not player_profiles.has(player_id):
		return "New player. Be welcoming."

	var profile: Dictionary = player_profiles[player_id]
	var likes: String = str(profile.get("likes", "unknown"))
	var tone: String = str(profile.get("tone", "neutral"))
	return "Likes: %s. Communication tone: %s." % [likes, tone]


func _on_response_chunk(text: String) -> void:
	_stream_buffer += text
	response_chunk.emit(text)


func _on_response_done(full_text: String) -> void:
	if full_text.is_empty():
		full_text = _stream_buffer
	response_done.emit(full_text)
