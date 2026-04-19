extends Control

const MAX_MESSAGES: int = 50

enum ChatMode {
	WORLD,
	KIM,
}

@onready var chat_bg: TextureRect = $ChatBackground
@onready var chat_history: ScrollContainer = $ContentMargin/ContentVBox/ChatHistory
@onready var messages_container: VBoxContainer = $ContentMargin/ContentVBox/ChatHistory/MessagesContainer
@onready var mode_toggle: Button = $ContentMargin/ContentVBox/InputRow/ModeToggle
@onready var input_bg: TextureRect = $ContentMargin/ContentVBox/InputRow/InputWrap/InputBackground
@onready var chat_input: LineEdit = $ContentMargin/ContentVBox/InputRow/InputWrap/ChatInput
@onready var send_button: TextureButton = $ContentMargin/ContentVBox/InputRow/SendButton
@onready var transport_label: Label = $ContentMargin/ContentVBox/InputRow/TransportLabel

var _mode: ChatMode = ChatMode.WORLD
var _game_server: Node = null
var _chat_message_scene: PackedScene = null
var _transport_refresh_accum: float = 0.0


func _ready() -> void:
	chat_bg.texture = UISpriteLibrary.get_texture("chat_bar_bg")
	chat_bg.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST

	input_bg.texture = UISpriteLibrary.get_texture("chat_input_bg")
	input_bg.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST

	send_button.texture_normal = UISpriteLibrary.get_texture("chat_send_btn")
	send_button.texture_hover = send_button.texture_normal
	send_button.texture_pressed = send_button.texture_normal
	send_button.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST

	mode_toggle.text = UIStrings.CHAT_MODE_WORLD
	chat_input.placeholder_text = UIStrings.CHAT_PLACEHOLDER
	var text_cfg: Dictionary = UILayoutConfig.get_text_settings()
	var default_size: int = int(text_cfg.get("default_size", 14))
	var small_size: int = int(text_cfg.get("small_size", 12))
	mode_toggle.add_theme_font_size_override("font_size", small_size)
	chat_input.add_theme_font_size_override("font_size", default_size)
	transport_label.add_theme_font_size_override("font_size", small_size)
	transport_label.modulate = Color(0.74, 0.88, 0.98, 0.92)
	chat_input.add_theme_color_override("font_color", Color(0.96, 0.97, 1.0, 1.0))
	chat_input.add_theme_color_override("font_placeholder_color", Color(0.62, 0.68, 0.76, 1.0))
	chat_input.add_theme_color_override("font_outline_color", Color(0.04, 0.05, 0.07, 1.0))
	chat_input.add_theme_constant_override("outline_size", 1)
	var input_style: StyleBoxFlat = StyleBoxFlat.new()
	input_style.bg_color = Color(0.03, 0.04, 0.06, 0.70)
	input_style.border_color = Color(0.34, 0.44, 0.62, 0.90)
	input_style.border_width_left = 1
	input_style.border_width_top = 1
	input_style.border_width_right = 1
	input_style.border_width_bottom = 1
	input_style.corner_radius_top_left = 2
	input_style.corner_radius_top_right = 2
	input_style.corner_radius_bottom_left = 2
	input_style.corner_radius_bottom_right = 2
	chat_input.add_theme_stylebox_override("normal", input_style)
	chat_input.add_theme_stylebox_override("focus", input_style)

	var bg_style: StyleBoxFlat = StyleBoxFlat.new()
	bg_style.bg_color = Color(0.05, 0.07, 0.10, 0.80)
	bg_style.border_color = Color(0.44, 0.58, 0.78, 0.85)
	bg_style.border_width_left = 1
	bg_style.border_width_top = 1
	bg_style.border_width_right = 1
	bg_style.border_width_bottom = 1
	bg_style.corner_radius_top_left = 4
	bg_style.corner_radius_top_right = 4
	bg_style.corner_radius_bottom_left = 4
	bg_style.corner_radius_bottom_right = 4
	add_theme_stylebox_override("panel", bg_style)

	mode_toggle.pressed.connect(_on_mode_toggle)
	send_button.pressed.connect(_on_send_pressed)
	chat_input.text_submitted.connect(_on_text_submitted)

	_chat_message_scene = load("res://scenes/ui/chat_message.tscn") as PackedScene

	await get_tree().process_frame
	_game_server = _find_game_server(get_tree().root)
	_update_transport_label()
	if _game_server != null:
		if _game_server.has_signal("world_message_received") and not _game_server.world_message_received.is_connected(_on_world_message):
			_game_server.world_message_received.connect(_on_world_message)
		if _game_server.has_signal("kim_response") and not _game_server.kim_response.is_connected(_on_kim_response):
			_game_server.kim_response.connect(_on_kim_response)


func apply_hud_layout(viewport_size: Vector2) -> void:
	var fallback: Rect2 = Rect2(16.0, viewport_size.y - 220.0, 520.0, 200.0)
	var rect: Rect2 = UILayoutConfig.get_element_rect("chat_bar", viewport_size, fallback)
	position = rect.position
	size = rect.size


func _process(delta: float) -> void:
	_transport_refresh_accum += delta
	if _transport_refresh_accum >= 0.5:
		_transport_refresh_accum = 0.0
		_update_transport_label()


func _on_mode_toggle() -> void:
	_mode = ChatMode.KIM if _mode == ChatMode.WORLD else ChatMode.WORLD
	mode_toggle.text = UIStrings.CHAT_MODE_KIM if _mode == ChatMode.KIM else UIStrings.CHAT_MODE_WORLD
	mode_toggle.modulate = Color(0.72, 0.95, 1.0) if _mode == ChatMode.KIM else Color(1.0, 1.0, 1.0)


func _on_send_pressed() -> void:
	_send_current_text()


func _on_text_submitted(_text: String) -> void:
	_send_current_text()


func _send_current_text() -> void:
	var text: String = chat_input.text.strip_edges()
	if text.is_empty() or _game_server == null:
		return
	chat_input.text = ""

	if _mode == ChatMode.KIM:
		if _game_server.has_method("send_text_to_kim"):
			_game_server.call("send_text_to_kim", text)
	else:
		if _game_server.has_method("send_world_message"):
			_game_server.call("send_world_message", text)
	_update_transport_label()


func add_kim_message(text: String) -> void:
	if text.is_empty():
		return
	var msg: Node = _spawn_message()
	if msg == null:
		return
	if msg.has_method("set_kim_message"):
		msg.call("set_kim_message", text)
	_trim_messages()
	_scroll_to_bottom()


func add_player_message(player_id: int, text: String) -> void:
	if text.is_empty():
		return
	var msg: Node = _spawn_message()
	if msg == null:
		return
	if msg.has_method("set_player_message"):
		msg.call("set_player_message", player_id, text)
	_trim_messages()
	_scroll_to_bottom()


func add_system_message(text: String) -> void:
	if text.is_empty():
		return
	var label: Label = Label.new()
	label.text = "- %s -" % text
	label.modulate = Color(0.72, 0.78, 0.90)
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	messages_container.add_child(label)
	_trim_messages()
	_scroll_to_bottom()


func _spawn_message() -> Node:
	if _chat_message_scene == null:
		return null
	var msg: Node = _chat_message_scene.instantiate()
	messages_container.add_child(msg)
	return msg


func _on_world_message(sender_id: int, text: String) -> void:
	add_player_message(sender_id, text)


func _on_kim_response(_player_id: int, text: String) -> void:
	add_kim_message(text)


func _trim_messages() -> void:
	var children: Array = messages_container.get_children()
	while children.size() > MAX_MESSAGES:
		var node: Node = children[0] as Node
		if node != null:
			node.queue_free()
		children.remove_at(0)


func _scroll_to_bottom() -> void:
	await get_tree().process_frame
	var scrollbar: VScrollBar = chat_history.get_v_scroll_bar()
	if scrollbar != null:
		chat_history.scroll_vertical = int(scrollbar.max_value)


func _find_game_server(node: Node) -> Node:
	var script_ref: Script = node.get_script() as Script
	if script_ref != null and script_ref.resource_path.ends_with("game_server.gd"):
		return node
	for child_variant: Variant in node.get_children():
		var child: Node = child_variant as Node
		if child == null:
			continue
		var found: Node = _find_game_server(child)
		if found != null:
			return found
	return null


func _update_transport_label() -> void:
	if transport_label == null:
		return
	if _game_server == null:
		transport_label.text = UIStrings.CHAT_TRANSPORT_OFFLINE
		return
	if _game_server.has_method("get_transport_label"):
		transport_label.text = str(_game_server.call("get_transport_label"))
		return
	transport_label.text = UIStrings.CHAT_TRANSPORT_NETWORK
