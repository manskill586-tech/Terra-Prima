extends CanvasLayer

const FONT_PATH: String = "res://assets/8bitOperatorPlus-Regular.ttf"
const FONT_BOLD_PATH: String = "res://assets/8bitOperatorPlus-Bold.ttf"

@onready var root_control: Control = $RootControl
@onready var chat_bar: Control = $RootControl/ChatBar
@onready var microphone_ui: Control = $RootControl/MicrophoneUI
@onready var settings_panel: Control = $RootControl/SettingsPanel
@onready var rating_popup: Control = $RootControl/RatingPopup
@onready var vote_popup: Control = $RootControl/VotePopup
@onready var hp_label: Label = $RootControl/HPLabel
@onready var crosshair: TextureRect = $RootControl/Crosshair
@onready var toolbar: Control = $RootControl/Toolbar

var _max_hp: float = 100.0
var _current_hp: float = 100.0
var _corner_bg: TextureRect = null


func _ready() -> void:
	layer = 10
	root_control.set_anchors_preset(Control.PRESET_FULL_RECT)
	root_control.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root_control.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST

	_create_corner_background()
	_configure_crosshair()
	_apply_theme_from_config()
	set_hp(_current_hp, _max_hp)
	_apply_layout()

	var viewport_node: Viewport = get_viewport()
	if not viewport_node.size_changed.is_connected(_on_viewport_resized):
		viewport_node.size_changed.connect(_on_viewport_resized)

	await get_tree().process_frame
	_connect_signals()


func _on_viewport_resized() -> void:
	_apply_layout()


func _apply_layout() -> void:
	var viewport_size: Vector2 = get_viewport().get_visible_rect().size
	root_control.position = Vector2.ZERO
	root_control.size = viewport_size

	_layout_static_hud(viewport_size)

	if chat_bar != null and chat_bar.has_method("apply_hud_layout"):
		chat_bar.call("apply_hud_layout", viewport_size)
	if microphone_ui != null and microphone_ui.has_method("apply_hud_layout"):
		microphone_ui.call("apply_hud_layout", viewport_size)
	if toolbar != null and toolbar.has_method("apply_hud_layout"):
		toolbar.call("apply_hud_layout", viewport_size)
	if settings_panel != null and settings_panel.has_method("apply_hud_layout"):
		settings_panel.call("apply_hud_layout", viewport_size)
	if rating_popup != null and rating_popup.has_method("apply_hud_layout"):
		rating_popup.call("apply_hud_layout", viewport_size)
	if vote_popup != null and vote_popup.has_method("apply_hud_layout"):
		vote_popup.call("apply_hud_layout", viewport_size)


func _layout_static_hud(viewport_size: Vector2) -> void:
	if _corner_bg != null:
		var corner_rect: Rect2 = UILayoutConfig.get_element_rect("hud_corner", viewport_size, Rect2(viewport_size.x - 220.0, 10.0, 200.0, 120.0))
		_corner_bg.position = corner_rect.position
		_corner_bg.size = corner_rect.size

	var hp_rect: Rect2 = UILayoutConfig.get_element_rect("hp_label", viewport_size, Rect2(viewport_size.x - 250.0, 12.0, 220.0, 44.0))
	hp_label.position = hp_rect.position
	hp_label.size = hp_rect.size
	hp_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	hp_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER

	var crosshair_rect: Rect2 = UILayoutConfig.get_element_rect("crosshair", viewport_size, Rect2(viewport_size.x * 0.5 - 8.0, viewport_size.y * 0.5 - 8.0, 16.0, 16.0))
	crosshair.position = crosshair_rect.position
	crosshair.size = crosshair_rect.size


func _create_corner_background() -> void:
	_corner_bg = TextureRect.new()
	_corner_bg.name = "HudCornerBg"
	_corner_bg.texture = UISpriteLibrary.get_texture("ui_top_right_bg")
	_corner_bg.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	_corner_bg.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_corner_bg.stretch_mode = TextureRect.STRETCH_SCALE
	_corner_bg.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root_control.add_child(_corner_bg)
	root_control.move_child(_corner_bg, 0)


func _configure_crosshair() -> void:
	crosshair.texture = UISpriteLibrary.get_texture("crosshair_dot")
	crosshair.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	crosshair.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	crosshair.stretch_mode = TextureRect.STRETCH_SCALE
	crosshair.mouse_filter = Control.MOUSE_FILTER_IGNORE


func _apply_theme_from_config() -> void:
	var regular_raw: FontFile = load(FONT_PATH) as FontFile
	if regular_raw == null:
		return
	var bold_raw: FontFile = load(FONT_BOLD_PATH) as FontFile

	var regular_font: FontFile = regular_raw.duplicate() as FontFile
	regular_font.antialiasing = TextServer.FONT_ANTIALIASING_NONE
	regular_font.hinting = TextServer.HINTING_NONE
	regular_font.subpixel_positioning = TextServer.SUBPIXEL_POSITIONING_DISABLED
	regular_font.generate_mipmaps = false

	var bold_font: FontFile = null
	if bold_raw != null:
		bold_font = bold_raw.duplicate() as FontFile
		bold_font.antialiasing = TextServer.FONT_ANTIALIASING_NONE
		bold_font.hinting = TextServer.HINTING_NONE
		bold_font.subpixel_positioning = TextServer.SUBPIXEL_POSITIONING_DISABLED
		bold_font.generate_mipmaps = false

	var text_cfg: Dictionary = UILayoutConfig.get_text_settings()
	var default_size: int = int(text_cfg.get("default_size", 16))
	var outline_size: int = int(text_cfg.get("outline_size", 1))
	var font_color: Color = _read_color(text_cfg.get("font_color"), Color(0.96, 0.97, 1.0, 1.0))
	var outline_color: Color = _read_color(text_cfg.get("outline_color"), Color(0.04, 0.05, 0.07, 1.0))

	var theme: Theme = Theme.new()
	theme.default_font = regular_font
	theme.default_font_size = default_size
	theme.set_font_size("font_size", "Label", default_size)
	theme.set_font_size("font_size", "Button", default_size)
	theme.set_font_size("font_size", "LineEdit", default_size)
	theme.set_font_size("font_size", "RichTextLabel", default_size)
	theme.set_color("font_color", "Label", font_color)
	theme.set_color("font_color", "Button", font_color)
	theme.set_color("font_color", "LineEdit", font_color)
	theme.set_color("font_outline_color", "Label", outline_color)
	theme.set_color("font_outline_color", "Button", outline_color)
	theme.set_color("font_outline_color", "LineEdit", outline_color)
	theme.set_constant("outline_size", "Label", outline_size)
	theme.set_constant("outline_size", "Button", outline_size)
	theme.set_constant("outline_size", "LineEdit", outline_size)

	if bold_font != null:
		theme.set_font("font", "Button", bold_font)

	root_control.theme = theme
	hp_label.add_theme_font_size_override("font_size", default_size)


func _read_color(value: Variant, fallback: Color) -> Color:
	if typeof(value) == TYPE_ARRAY:
		var arr: Array = value as Array
		if arr.size() >= 4:
			return Color(float(arr[0]), float(arr[1]), float(arr[2]), float(arr[3]))
	return fallback


func set_hp(current: float, max_hp: float) -> void:
	_current_hp = current
	_max_hp = max_hp
	if hp_label == null:
		return

	var current_int: int = int(round(current))
	var max_int: int = int(max(1.0, round(max_hp)))
	hp_label.text = UIStrings.HUD_HP_FORMAT % [current_int, max_int]
	var ratio: float = current / maxf(max_hp, 1.0)
	if ratio > 0.5:
		hp_label.modulate = Color.GREEN.lerp(Color.YELLOW, (1.0 - ratio) * 2.0)
	else:
		hp_label.modulate = Color.YELLOW.lerp(Color.RED, (0.5 - ratio) * 2.0)


func _connect_signals() -> void:
	var gs: Node = _find_by_script(get_tree().root, "game_server.gd")
	if gs != null and gs.has_signal("kim_response"):
		if not gs.is_connected("kim_response", _on_kim_response):
			gs.kim_response.connect(_on_kim_response)

	var qm: Node = get_node_or_null("/root/QuestManager")
	if qm != null and qm.has_signal("quest_ended"):
		if not qm.is_connected("quest_ended", _on_quest_ended):
			qm.quest_ended.connect(_on_quest_ended)

	var pm: Node = get_node_or_null("/root/PerformanceManager")
	if pm != null and pm.has_signal("optimization_vote_requested"):
		if not pm.is_connected("optimization_vote_requested", _on_vote_requested):
			pm.optimization_vote_requested.connect(_on_vote_requested)

	var avatar: Node = _find_by_script(get_tree().root, "kim_avatar.gd")
	if avatar != null and avatar.has_signal("player_entered_range"):
		if not avatar.is_connected("player_entered_range", _on_player_entered_kim_range):
			avatar.player_entered_range.connect(_on_player_entered_kim_range)


func _on_kim_response(_player_id: int, text: String) -> void:
	if chat_bar != null and chat_bar.has_method("add_kim_message"):
		chat_bar.call("add_kim_message", text)


func _on_quest_ended(quest_id: String, _state: int, _score: float) -> void:
	if rating_popup != null and rating_popup.has_method("show_for"):
		rating_popup.call("show_for", quest_id)


func _on_vote_requested(penalty_id: String) -> void:
	if vote_popup != null and vote_popup.has_method("show_for"):
		vote_popup.call("show_for", penalty_id)


func _on_player_entered_kim_range(_player_id: int) -> void:
	if chat_bar != null and chat_bar.has_method("add_system_message"):
		chat_bar.call("add_system_message", UIStrings.CHAT_SYSTEM_KIM_NEARBY)


func _find_by_script(node: Node, script_file: String) -> Node:
	var script_ref: Script = node.get_script() as Script
	if script_ref != null and script_ref.resource_path.ends_with(script_file):
		return node
	for child_variant: Variant in node.get_children():
		var child: Node = child_variant as Node
		if child == null:
			continue
		var found: Node = _find_by_script(child, script_file)
		if found != null:
			return found
	return null
