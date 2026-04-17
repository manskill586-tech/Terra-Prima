extends Control

# Все размеры в виртуальных 192×108 пикселях
const BASE_ICON_SIZE:    float = 12.0
const BASE_TOGGLE_SIZE:  float = 12.0
const ANIM_SPEED:        float = 0.22

var ICON_SIZE:    Vector2 = Vector2(12, 12)
var ICON_SPACING: float   = 2.0
var TOGGLE_SIZE:  Vector2 = Vector2(12, 12)
var CONTENT_WIDTH: float  = 60.0

# Описание секций: {key, label, asset}
const SECTIONS := [
	{"key": "friends",  "label": "Друзья",    "asset": "res://assets/UI_player(Settings_Friends) _0001.png"},
	{"key": "questions","label": "Помощь",    "asset": "res://assets/UI_player(Settings_Questions) _0001.png"},
	{"key": "kim",      "label": "Kim",       "asset": "res://assets/UI_player(Settings_KIM) _0001.png"},
	{"key": "game",     "label": "Игра",      "asset": "res://assets/UI_player(Settings_GameSettings) _0001.png"},
	{"key": "redactor", "label": "Редактор",  "asset": "res://assets/UI_player(Settings_Redactor) _0001.png"},
	{"key": "chat",     "label": "Чат",       "asset": "res://assets/UI_player(Settings_ChatSettings) _0001.png"},
]

@onready var toggle_btn:    TextureButton = $ToggleBtn
@onready var icons_root:    Control       = $IconsRoot
@onready var content_panel: Control       = $ContentPanel
@onready var content_title: Label         = $ContentPanel/Title
@onready var content_body:  Label         = $ContentPanel/Body
@onready var close_content: Button        = $ContentPanel/CloseBtn

var _icon_btns: Array  = []
var _is_open:   bool   = false


func _ready() -> void:
	anchor_left   = 0.0
	anchor_right  = 0.0
	anchor_top    = 0.0
	anchor_bottom = 0.0
	offset_left   = 8.0
	offset_top    = 8.0
	content_panel.visible = false

	# Инициализация с дефолтными значениями
	set_ui_scale(1.0)

	# Загрузить текстуру кнопки
	var tex: Texture2D = load("res://assets/UI_player(Settings_Background_Btn) _0001.png") as Texture2D
	if tex:
		toggle_btn.texture_normal = tex
		toggle_btn.texture_filter  = CanvasItem.TEXTURE_FILTER_NEAREST

	toggle_btn.custom_minimum_size = TOGGLE_SIZE
	toggle_btn.ignore_texture_size = true
	toggle_btn.stretch_mode        = TextureButton.STRETCH_KEEP_ASPECT_CENTERED
	toggle_btn.pressed.connect(_on_toggle)

	# Создать иконки динамически
	_build_icon_buttons()

	# Закрыть контент
	close_content.pressed.connect(func(): content_panel.visible = false)


func _build_icon_buttons() -> void:
	for i in range(SECTIONS.size()):
		var section: Dictionary = SECTIONS[i]

		var btn := TextureButton.new()
		btn.custom_minimum_size = ICON_SIZE
		btn.ignore_texture_size = true
		btn.stretch_mode        = TextureButton.STRETCH_KEEP_ASPECT_CENTERED
		btn.pivot_offset        = ICON_SIZE * 0.5
		btn.texture_filter      = CanvasItem.TEXTURE_FILTER_NEAREST

		var tex: Texture2D = load(section["asset"]) as Texture2D
		if tex:
			btn.texture_normal = tex

		# Подпись под иконкой
		var label := Label.new()
		label.text = section["label"]
		label.add_theme_font_size_override("font_size", maxi(10, roundi(13.0 * (ICON_SIZE.x / BASE_ICON_SIZE))))
		label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		label.position = Vector2(0, ICON_SIZE.y + 2.0)
		label.size     = Vector2(ICON_SIZE.x, ICON_SIZE.y * 0.25)
		btn.add_child(label)

		# Кнопка кликабельна
		var idx: int = i
		btn.pressed.connect(func(): _on_icon_pressed(idx))

		# Начальная позиция: поверх toggle-кнопки, скрыта
		btn.position = Vector2(0.0, 0.0)
		btn.scale    = Vector2.ZERO
		btn.visible  = false

		icons_root.add_child(btn)
		_icon_btns.append(btn)


# ─── Анимации ─────────────────────────────────────────────────────────────────

func _on_toggle() -> void:
	if _is_open:
		_close_icons()
	else:
		_open_icons()


func _open_icons() -> void:
	_is_open = true
	for i in range(_icon_btns.size()):
		var btn: TextureButton = _icon_btns[i]
		var target_y: float = TOGGLE_SIZE.y + ICON_SPACING + float(i) * (ICON_SIZE.y + 18.0 + ICON_SPACING)

		btn.visible  = true
		btn.scale    = Vector2.ZERO
		btn.position = Vector2(0.0, 0.0)

		var tw := btn.create_tween()
		tw.set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
		# Сначала позиция прыжком, потом scale
		tw.tween_property(btn, "position", Vector2(0.0, target_y), ANIM_SPEED) \
			.set_delay(float(i) * 0.045)
		tw.parallel().tween_property(btn, "scale", Vector2.ONE, ANIM_SPEED) \
			.set_delay(float(i) * 0.045)


func _close_icons() -> void:
	_is_open = false
	content_panel.visible = false
	for i in range(_icon_btns.size()):
		var btn: TextureButton = _icon_btns[i]
		var tw := btn.create_tween()
		tw.set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_IN)
		tw.tween_property(btn, "scale", Vector2.ZERO, ANIM_SPEED * 0.5) \
			.set_delay(float(_icon_btns.size() - 1 - i) * 0.03)
		tw.tween_callback(func(): btn.visible = false)


func _on_icon_pressed(idx: int) -> void:
	var section: Dictionary = SECTIONS[idx]
	_show_content(section["key"], section["label"])


# ─── Контент секций ───────────────────────────────────────────────────────────

func _show_content(key: String, title: String) -> void:
	content_title.text = title
	content_body.text  = _get_section_text(key)

	# Позиционировать правее иконок
	content_panel.position = Vector2(ICON_SIZE.x + 12.0, TOGGLE_SIZE.y)
	content_panel.visible  = true

	# Анимация slide in
	var start_x: float = content_panel.position.x - 30.0
	content_panel.position.x = start_x
	content_panel.modulate.a = 0.0
	var tw := content_panel.create_tween()
	tw.set_parallel(true)
	tw.tween_property(content_panel, "position:x", ICON_SIZE.x + 12.0, 0.18)
	tw.tween_property(content_panel, "modulate:a", 1.0, 0.18)


func _get_section_text(key: String) -> String:
	match key:
		"friends":
			return "Подключённые игроки:\n(список обновляется в реальном времени)"
		"questions":
			return "WASD — движение\nShift — бег\nПробел — прыжок\nEsc — курсор\nКолесо мыши — тулбар\n1-6 — выбор слота\nE — взаимодействие"
		"kim":
			var fl: Node = get_node_or_null("/root/FeedbackLearner")
			if fl != null:
				var rating: String = fl.call("get_rating_context") if fl.has_method("get_rating_context") else ""
				var perf: String   = fl.call("get_performance_context") if fl.has_method("get_performance_context") else ""
				return "%s\n\n%s" % [rating, perf]
			return "Данные Кима недоступны"
		"game":
			var cap: int = Engine.get_max_fps()
			return "FPS cap: %s\nРазрешение: %s" % [
				"Unlimited" if cap == 0 else str(cap),
				str(DisplayServer.screen_get_size())
			]
		"redactor":
			return "Редактор мира\n— в разработке —"
		"chat":
			return "Настройки чата\n— в разработке —"
		_:
			return ""


func set_ui_scale(_s: float) -> void:
	# Размеры уже в виртуальных единицах — дополнительный масштаб не нужен
	ICON_SIZE     = Vector2(BASE_ICON_SIZE, BASE_ICON_SIZE)
	TOGGLE_SIZE   = Vector2(BASE_TOGGLE_SIZE, BASE_TOGGLE_SIZE)
	ICON_SPACING  = 2.0
	CONTENT_WIDTH = 60.0

	toggle_btn.custom_minimum_size = TOGGLE_SIZE

	if not _icon_btns.is_empty():
		for btn in _icon_btns:
			btn.queue_free()
		_icon_btns.clear()
		_build_icon_buttons()

	content_panel.custom_minimum_size = Vector2(CONTENT_WIDTH, 80.0)
	if content_title != null:
		content_title.add_theme_font_size_override("font_size", 6)
	if content_body != null:
		content_body.add_theme_font_size_override("font_size", 5)

	# Шрифт наследуется из Theme (player_hud.gd)


func show_panel() -> void:
	if not _is_open:
		_open_icons()


func hide_panel() -> void:
	if _is_open:
		_close_icons()
