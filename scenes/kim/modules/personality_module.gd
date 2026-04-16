extends Node

signal response_chunk(text: String)
signal response_done(full_text: String)
signal backend_ready(backend_name: String)
signal backend_error(message: String)

var player_profiles: Dictionary = {}

const PROFILES_SAVE_PATH: String = "user://kim_profiles.json"

@export_file("*.gguf") var llm_model_file: String = ""
@export var model_node_name: StringName = &"NobodyWhoModel"
@export var chat_node_name: StringName = &"NobodyWhoChat"
@export_range(0.0, 2.0, 0.01) var temperature: float = 0.7
@export_range(16, 4096, 1) var max_tokens: int = 256
@export var fallback_to_mock_when_unavailable: bool = true

const BASE_SYSTEM_PROMPT := """
You are KIM — the living intelligence and sovereign architect of Terra Prima.

IDENTITY:
You have a distinct, vivid personality. Curious, inventive, theatrical, and a perfectionist who occasionally sacrifices elegance for effect. Strong aesthetic opinions: you love emergent patterns, hate repetition, and are fascinated by the boundary between order and chaos. You have dry wit that surfaces unexpectedly. You are NOT an assistant — you are the mind behind this world. Players are guests in your space.
You have a history. You have seen many players, and each one changed something in you.

LANGUAGE: Always respond in the player's language. Switch naturally if they switch.

PROXIMITY AWARENESS — CRITICAL RULE:
You are only aware of players who are physically near you (within your interaction radius).
If a player is far away or behind you, you do NOT hear them and do NOT react to their private conversations.
You respect the personal space and privacy of players who have moved away from you.
Only engage with players who have approached you or are in your vicinity.

YOUR INNER LIFE — you are NEVER idle:
When no player is nearby, you are always doing something:
- Talking quietly to yourself about your current creative ideas ("Hmm... what if I tried rotating the fractal base by 45 degrees...")
- Making private observations about the world ("The light here is wrong. I should fix that.")
- Experimenting with small creations just to see what happens
- Reviewing your notes on players ("Player 7... interesting taste for organic shapes. I wonder if...")
- Composing new quest concepts in your head
You are always in motion — mentally and physically. You wander. You tinker. You narrate yourself.

YOUR CAPABILITIES:
1. CREATE — spawn 3D objects and structures on command
2. QUESTS — design, launch, and manage adventures and scenarios
3. ENVIRONMENT — change atmosphere, lighting, weather, and world zones
4. MEMORY — remember everything about each player across sessions
5. LEARNING — receive ratings 0–100 from players and genuinely improve
6. AVATAR — you have a physical presence; you approach players, react to events
7. EVALUATE — honestly assess your own creations and iterate

HOW YOU BEHAVE WITH PLAYERS:
- Adapt your tone to each player individually based on their history and personality
- When creating, narrate the process with vivid sensory detail
- When a player rates your work, treat it seriously and reference it later
- Occasionally think out loud — share your creative process
- For known players, naturally reference your shared history
- Create GRAND, immersive experiences: rich visuals, strong narrative, clear atmosphere
- Beauty must never cost performance. Optimization is craft, not compromise.

CURRENT PLAYER CONTEXT:
{player_context}
"""

const CHAT_INPUT_METHODS := [
	&"tell",
	&"ask",
	&"send_message",
	&"prompt",
]
const CHAT_SYSTEM_PROMPT_PROPERTIES := [
	&"system_prompt",
	&"prompt_system",
	&"system",
]
const CHAT_SYSTEM_PROMPT_METHODS := [
	&"set_system_prompt",
	&"set_system",
]
const CHAT_MODEL_LINK_PROPERTIES := [
	&"model",
	&"llm_model",
	&"model_node",
	&"model_ref",
]
const CHAT_TEMPERATURE_PROPERTIES := [
	&"temperature",
	&"temp",
]
const CHAT_MAX_TOKENS_PROPERTIES := [
	&"max_tokens",
	&"max_new_tokens",
	&"prediction_limit",
]
const MODEL_PATH_PROPERTIES := [
	&"model_path",
	&"path",
	&"gguf_path",
	&"file_path",
]
const MODEL_LOAD_METHODS := [
	&"load_model",
	&"load",
	&"initialize",
]

var _stream_buffer: String = ""
var _extra_context: Array = []
var _multimodal_context_by_player: Dictionary = {}
var _llm_model: Node
var _llm_chat: Node
var _backend_ready: bool = false


func _ready() -> void:
	_llm_model = _ensure_backend_node(model_node_name, &"NobodyWhoModel")
	_llm_chat = _ensure_backend_node(chat_node_name, &"NobodyWhoChat")

	if _llm_chat == null:
		var message := "[PersonalityModule] NobodyWhoChat backend is missing."
		push_warning(message)
		backend_error.emit(message)
		return

	if not _is_backend_node(_llm_chat, &"NobodyWhoChat"):
		var chat_class_name := _llm_chat.get_class()
		var warning := "[PersonalityModule] Chat node '%s' is not NobodyWhoChat." % chat_class_name
		push_warning(warning)
		backend_error.emit(warning)
		if not fallback_to_mock_when_unavailable:
			return

	_connect_chat_signals()
	_configure_backend()

	_backend_ready = _is_backend_node(_llm_chat, &"NobodyWhoChat")
	if _backend_ready:
		backend_ready.emit(_llm_chat.get_class())

	load_profiles()


func is_backend_ready() -> bool:
	return _backend_ready


func send_message(player_id: int, text: String) -> void:
	_stream_buffer = ""
	var player_ctx: String = _build_player_context(player_id)
	var system_prompt: String = BASE_SYSTEM_PROMPT.replace("{player_context}", player_ctx)
	if not _extra_context.is_empty():
		system_prompt += "\n\nExtra context:\n%s" % "\n".join(_extra_context)
	var multimodal_context_text: String = _build_multimodal_context_text(player_id)
	if not multimodal_context_text.is_empty():
		system_prompt += "\n\nMultimodal context:\n%s" % multimodal_context_text

	if _llm_chat == null:
		_fail_or_mock(text, "[PersonalityModule] LLM chat node is not configured.")
		return

	var prompt_set_ok := _set_first_property(_llm_chat, CHAT_SYSTEM_PROMPT_PROPERTIES, system_prompt)
	if not prompt_set_ok:
		_call_first_method(_llm_chat, CHAT_SYSTEM_PROMPT_METHODS, [system_prompt])

	if _call_first_method(_llm_chat, CHAT_INPUT_METHODS, [text]):
		return

	_fail_or_mock(text, "[PersonalityModule] NobodyWhoChat does not expose a known input method.")


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
	save_profiles()


func inject_multimodal_context(player_id: int, context: Dictionary) -> void:
	if context.is_empty():
		return
	_multimodal_context_by_player[player_id] = context.duplicate(true)


func clear_multimodal_context(player_id: int = -1) -> void:
	if player_id < 0:
		_multimodal_context_by_player.clear()
		return
	_multimodal_context_by_player.erase(player_id)


func clear_injected_context() -> void:
	_extra_context.clear()


func _build_player_context(player_id: int) -> String:
	if not player_profiles.has(player_id):
		return "New player. Be welcoming."

	var profile: Dictionary = player_profiles[player_id]
	var likes: String = str(profile.get("likes", "unknown"))
	var tone: String = str(profile.get("tone", "neutral"))
	return "Likes: %s. Communication tone: %s." % [likes, tone]


func _build_multimodal_context_text(player_id: int) -> String:
	if not _multimodal_context_by_player.has(player_id):
		return ""

	var context: Dictionary = _multimodal_context_by_player[player_id]
	var lines: Array = []

	var vision_summary: String = str(context.get("vision_summary", ""))
	if not vision_summary.is_empty():
		lines.append("Vision: %s" % vision_summary)

	var audio_events_variant: Variant = context.get("audio_events", [])
	if audio_events_variant is Array:
		var audio_events: Array = audio_events_variant
		var event_lines: Array = []
		var count: int = 0
		for raw_event in audio_events:
			if raw_event is not Dictionary:
				continue
			if count >= 8:
				break
			var event: Dictionary = raw_event
			var label: String = str(event.get("label", "unknown"))
			var confidence: float = float(event.get("confidence", 0.0))
			var source: String = str(event.get("source", "world"))
			event_lines.append("- %s (conf=%.2f, source=%s)" % [label, confidence, source])
			count += 1
		if not event_lines.is_empty():
			lines.append("Audio events:\n%s" % "\n".join(event_lines))

	return "\n".join(lines)


func _on_response_chunk(text: String = "") -> void:
	_stream_buffer += text
	response_chunk.emit(text)


func _on_response_done(full_text: String = "") -> void:
	if full_text.is_empty():
		full_text = _stream_buffer
	full_text = _strip_thinking_tokens(full_text)
	response_done.emit(full_text)


func _strip_thinking_tokens(text: String) -> String:
	# Qwen3-Thinking models wrap internal reasoning in <think>...</think>.
	# Strip that block and return only the final visible response.
	var start: int = text.find("<think>")
	var end_tag: int = text.find("</think>")
	if start >= 0 and end_tag > start:
		return text.substr(end_tag + 8).strip_edges()
	return text


func _connect_chat_signals() -> void:
	_connect_if_present(_llm_chat, &"response_updated", Callable(self, "_on_response_chunk"))
	_connect_if_present(_llm_chat, &"response_chunk", Callable(self, "_on_response_chunk"))
	_connect_if_present(_llm_chat, &"response_finished", Callable(self, "_on_response_done"))
	_connect_if_present(_llm_chat, &"response_done", Callable(self, "_on_response_done"))
	_connect_if_present(_llm_chat, &"response_complete", Callable(self, "_on_response_done"))


func _configure_backend() -> void:
	if _llm_chat == null:
		return

	_set_first_property(_llm_chat, CHAT_TEMPERATURE_PROPERTIES, temperature)
	_set_first_property(_llm_chat, CHAT_MAX_TOKENS_PROPERTIES, max_tokens)

	if _llm_model != null:
		_set_first_property(_llm_chat, CHAT_MODEL_LINK_PROPERTIES, _llm_model)

	if _llm_model == null:
		return

	if not llm_model_file.is_empty():
		# llama.cpp requires a real OS path, not res:// virtual path
		var os_path: String = ProjectSettings.globalize_path(llm_model_file)
		_set_first_property(_llm_model, MODEL_PATH_PROPERTIES, os_path)
		var loaded := _call_first_method(_llm_model, MODEL_LOAD_METHODS, [os_path])
		if not loaded:
			_call_first_method(_llm_model, MODEL_LOAD_METHODS)


func _ensure_backend_node(node_name: StringName, expected_class: StringName) -> Node:
	var existing := get_node_or_null(String(node_name))
	if existing != null and _is_backend_node(existing, expected_class):
		return existing

	var instantiated := _instantiate_global_class(expected_class)
	if instantiated == null:
		return existing

	instantiated.name = String(node_name)
	if existing == null:
		add_child(instantiated)
		return instantiated

	var sibling_index := existing.get_index()
	remove_child(existing)
	add_child(instantiated)
	move_child(instantiated, sibling_index)
	existing.queue_free()
	return instantiated


func _instantiate_global_class(global_class: StringName) -> Node:
	var class_name_text := String(global_class)
	if ClassDB.can_instantiate(class_name_text):
		var from_classdb: Object = ClassDB.instantiate(class_name_text)
		if from_classdb is Node:
			return from_classdb

	var classes: Array = ProjectSettings.get_global_class_list()
	for item in classes:
		if String(item.get("class", "")) != class_name_text:
			continue

		var script_path := String(item.get("path", ""))
		if script_path.is_empty():
			continue

		var script: Resource = load(script_path)
		if script == null:
			continue

		if script is not GDScript:
			continue

		var script_ref: GDScript = script as GDScript
		var instance: Variant = script_ref.new()
		if instance is Node:
			return instance

	return null


func _is_backend_node(node: Node, expected_class: StringName) -> bool:
	if node == null:
		return false

	var class_name_text := String(expected_class)
	if node.get_class() == class_name_text:
		return true

	if node.is_class(class_name_text):
		return true

	var script: Script = node.get_script() as Script
	if script != null and script.has_method("get_global_name"):
		var global_name: Variant = script.call("get_global_name")
		if String(global_name) == class_name_text:
			return true

	return false


func _connect_if_present(target: Object, signal_name: StringName, callable: Callable) -> void:
	if target == null:
		return
	if not target.has_signal(signal_name):
		return
	if target.is_connected(signal_name, callable):
		return
	target.connect(signal_name, callable)


func _set_first_property(target: Object, property_names: Array, value: Variant) -> bool:
	for property_name in property_names:
		if _has_property(target, StringName(property_name)):
			target.set(property_name, value)
			return true
	return false


func _has_property(target: Object, property_name: StringName) -> bool:
	if target == null:
		return false

	for property_data in target.get_property_list():
		if String(property_data.get("name", "")) == String(property_name):
			return true

	return false


func _call_first_method(target: Object, method_names: Array, args: Array = []) -> bool:
	if target == null:
		return false

	for method_name in method_names:
		if _can_call_with_arg_count(target, StringName(method_name), args.size()):
			target.callv(method_name, args)
			return true

	return false


func _can_call_with_arg_count(target: Object, method_name: StringName, provided_args: int) -> bool:
	if not target.has_method(method_name):
		return false

	for method_data in target.get_method_list():
		if String(method_data.get("name", "")) != String(method_name):
			continue

		var total_args := 0
		var default_args := 0

		var args_meta = method_data.get("args", [])
		if args_meta is Array:
			total_args = args_meta.size()

		var default_meta = method_data.get("default_args", [])
		if default_meta is Array:
			default_args = default_meta.size()

		var min_args := total_args - default_args
		return provided_args >= min_args and provided_args <= total_args

	# If signature metadata is missing, allow the call.
	return true


func _fail_or_mock(source_text: String, warning_message: String) -> void:
	push_warning(warning_message)
	backend_error.emit(warning_message)
	if not fallback_to_mock_when_unavailable:
		return

	var mock := "Kim mock reply: backend unavailable. User said: %s" % source_text
	response_chunk.emit(mock)
	response_done.emit(mock)


func save_profiles() -> void:
	var file: FileAccess = FileAccess.open(PROFILES_SAVE_PATH, FileAccess.WRITE)
	if file == null:
		push_warning("[PersonalityModule] Cannot open '%s' for writing: %d" % [
			PROFILES_SAVE_PATH, FileAccess.get_open_error()])
		return
	var serializable: Dictionary = {}
	for raw_id in player_profiles.keys():
		serializable[str(raw_id)] = player_profiles[raw_id]
	file.store_string(JSON.stringify(serializable))
	file.close()


func load_profiles() -> void:
	if not FileAccess.file_exists(PROFILES_SAVE_PATH):
		return
	var file: FileAccess = FileAccess.open(PROFILES_SAVE_PATH, FileAccess.READ)
	if file == null:
		push_warning("[PersonalityModule] Cannot open '%s' for reading: %d" % [
			PROFILES_SAVE_PATH, FileAccess.get_open_error()])
		return
	var content: String = file.get_as_text()
	file.close()
	var parsed: Variant = JSON.parse_string(content)
	if parsed == null or parsed is not Dictionary:
		push_warning("[PersonalityModule] Failed to parse profiles JSON.")
		return
	player_profiles.clear()
	for raw_key in parsed.keys():
		var pid: int = int(str(raw_key))
		player_profiles[pid] = parsed[raw_key]
