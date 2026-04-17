extends CanvasLayer

const VIRTUAL_W: float = 192.0
const VIRTUAL_H: float = 108.0

const FONT_PATH := "res://assets/8bitOperatorPlus-Regular.ttf"
const FONT_BOLD_PATH := "res://assets/8bitOperatorPlusSC-Bold.ttf"
const FONT_SIZE_DEFAULT: int = 6   # в виртуальных пикселях

@onready var root_control:   Control     = $RootControl
@onready var chat_bar:       Control     = $RootControl/ChatBar
@onready var microphone_ui:  Control     = $RootControl/MicrophoneUI
@onready var settings_panel: Control     = $RootControl/SettingsPanel
@onready var rating_popup:   Control     = $RootControl/RatingPopup
@onready var vote_popup:     Control     = $RootControl/VotePopup
@onready var hp_label:       Label       = $RootControl/HPLabel
@onready var crosshair:      TextureRect = $RootControl/Crosshair
@onready var toolbar:        Control     = $RootControl/Toolbar

var _max_hp: float  = 100.0
var _current_hp: float = 100.0
var _ui_scale: float   = 1.0


func _ready() -> void:
	layer = 1
	await get_tree().process_frame

	var vp: Vector2 = get_viewport().get_visible_rect().size

	# Целый масштаб: 6× при 1280×720, 10× при 1920×1080
	_ui_scale = maxf(1.0, floor(minf(vp.x / VIRTUAL_W, vp.y / VIRTUAL_H)))

	# Центрировать виртуальный canvas
	var ui_size: Vector2 = Vector2(VIRTUAL_W, VIRTUAL_H) * _ui_scale
	var origin:  Vector2 = ((vp - ui_size) * 0.5).floor()  # целые пиксели!
	transform = Transform2D(0.0, Vector2(_ui_scale, _ui_scale), 0.0, origin)

	# RootControl = ровно виртуальный размер
	root_control.set_anchors_preset(Control.PRESET_TOP_LEFT)
	root_control.size = Vector2(VIRTUAL_W, VIRTUAL_H)

	# NEAREST-фильтр на весь RootControl — дочерние узлы наследуют
	root_control.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST

	# Создать тему с пиксельным шрифтом (без anti-aliasing)
	_apply_pixel_theme()

	# Текстура прицела
	_load_tex(crosshair, "res://assets/UI_player(Crosshair_Dot) _0001.png")

	set_hp(_current_hp, _max_hp)

	# Сообщить дочерним модулям о масштабе
	if toolbar != null and toolbar.has_method("set_ui_scale"):
		toolbar.call("set_ui_scale", _ui_scale, VIRTUAL_W, VIRTUAL_H)
	if settings_panel != null and settings_panel.has_method("set_ui_scale"):
		settings_panel.call("set_ui_scale", _ui_scale)

	await get_tree().process_frame
	_connect_signals()


func _apply_pixel_theme() -> void:
	var raw: FontFile = load(FONT_PATH) as FontFile
	if raw == null:
		return

	# Дублируем шрифт чтобы не менять оригинал
	var font: FontFile = raw.duplicate() as FontFile

	# Отключить сглаживание и хинтинг — только пиксели
	font.antialiasing           = TextServer.FONT_ANTIALIASING_NONE
	font.hinting                = TextServer.HINTING_NONE
	font.subpixel_positioning   = TextServer.SUBPIXEL_POSITIONING_DISABLED
	font.generate_mipmaps       = false

	# Тема с дефолтным шрифтом и размером
	var theme := Theme.new()
	theme.default_font      = font
	theme.default_font_size = FONT_SIZE_DEFAULT
	theme.set_font_size("font_size", "Label",    FONT_SIZE_DEFAULT)
	theme.set_font_size("font_size", "Button",   FONT_SIZE_DEFAULT)
	theme.set_font_size("font_size", "LineEdit", FONT_SIZE_DEFAULT)

	root_control.theme = theme


func _load_tex(node: TextureRect, path: String) -> void:
	if node == null:
		return
	var t: Texture2D = load(path) as Texture2D
	if t:
		node.texture      = t
		# texture_filter уже наследуется NEAREST от RootControl
		node.expand_mode  = TextureRect.EXPAND_IGNORE_SIZE
		node.stretch_mode = TextureRect.STRETCH_SCALE


# ─── HP ───────────────────────────────────────────────────────────────────────

func set_hp(current: float, max_hp: float) -> void:
	_current_hp = current
	_max_hp = max_hp
	if hp_label == null:
		return
	hp_label.text = "HP %d" % int(current)
	var ratio: float = current / maxf(max_hp, 1.0)
	if ratio > 0.5:
		hp_label.modulate = Color.GREEN.lerp(Color.YELLOW, (1.0 - ratio) * 2.0)
	else:
		hp_label.modulate = Color.YELLOW.lerp(Color.RED, (0.5 - ratio) * 2.0)


# ─── Сигналы ──────────────────────────────────────────────────────────────────

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
		chat_bar.call("add_system_message", "Kim is nearby")


func _find_by_script(node: Node, script_file: String) -> Node:
	var s: Script = node.get_script() as Script
	if s != null and s.resource_path.ends_with(script_file):
		return node
	for child in node.get_children():
		var found: Node = _find_by_script(child, script_file)
		if found != null:
			return found
	return null
