extends Control

const MAX_MESSAGES := 50

enum ChatMode { WORLD, KIM }

@onready var chat_bg: TextureRect          = $ChatBackground
@onready var chat_history: ScrollContainer = $ChatHistory
@onready var messages_container: VBoxContainer = $ChatHistory/MessagesContainer
@onready var mode_toggle: Button           = $InputRow/ModeToggle
@onready var chat_input: LineEdit          = $InputRow/ChatInput
@onready var send_button: TextureButton    = $InputRow/SendButton

var _mode: ChatMode = ChatMode.WORLD
var _game_server: Node = null
var _chat_message_scene: PackedScene = null


func _ready() -> void:
	# Загрузка текстур — безопасно, без preload
	var tex_bg: Texture2D = load("res://assets/UI_player(Chat_Bar_Background) _0001.png") as Texture2D
	if tex_bg != null and chat_bg != null:
		chat_bg.texture        = tex_bg
		chat_bg.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
		chat_bg.expand_mode    = TextureRect.EXPAND_IGNORE_SIZE
		chat_bg.stretch_mode   = TextureRect.STRETCH_SCALE

	# Шрифт применяется через Theme от RootControl (player_hud.gd)

	var tex_send: Texture2D = load("res://assets/UI_player(Chat_Send_Btn) _0001.png") as Texture2D
	if tex_send != null and send_button != null:
		send_button.texture_normal = tex_send
		send_button.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST

	# Загрузка сцены сообщения
	_chat_message_scene = load("res://scenes/ui/chat_message.tscn") as PackedScene

	mode_toggle.text = "World"
	mode_toggle.pressed.connect(_on_mode_toggle)
	send_button.pressed.connect(_on_send_pressed)
	chat_input.text_submitted.connect(_on_text_submitted)

	await get_tree().process_frame
	_game_server = _find_game_server(get_tree().root)
	if _game_server != null:
		if _game_server.has_signal("world_message_received"):
			_game_server.world_message_received.connect(_on_world_message)
		if _game_server.has_signal("kim_response"):
			_game_server.kim_response.connect(_on_kim_response)


func _on_mode_toggle() -> void:
	_mode = ChatMode.KIM if _mode == ChatMode.WORLD else ChatMode.WORLD
	mode_toggle.text = "Kim" if _mode == ChatMode.KIM else "World"
	mode_toggle.modulate = Color(0.7, 0.95, 1.0) if _mode == ChatMode.KIM else Color.WHITE


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
	var msg: Node = _spawn_message()
	if msg == null:
		return
	if msg.has_method("set_player_message"):
		msg.call("set_player_message", player_id, text)
	_trim_messages()
	_scroll_to_bottom()


func add_system_message(text: String) -> void:
	var label := Label.new()
	label.text = "— %s —" % text
	label.modulate = Color(0.6, 0.6, 0.8)
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	messages_container.add_child(label)
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
		children[0].queue_free()
		children.remove_at(0)


func _scroll_to_bottom() -> void:
	await get_tree().process_frame
	var scrollbar := chat_history.get_v_scroll_bar()
	if scrollbar != null:
		chat_history.scroll_vertical = int(scrollbar.max_value)


func _find_game_server(node: Node) -> Node:
	var s: Script = node.get_script() as Script
	if s != null and s.resource_path.ends_with("game_server.gd"):
		return node
	for child in node.get_children():
		var found: Node = _find_game_server(child)
		if found != null:
			return found
	return null
