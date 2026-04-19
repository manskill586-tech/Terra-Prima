extends Control

const ICON_SIZE: Vector2 = Vector2(22.0, 22.0)

const SECTIONS: Array[Dictionary] = [
	{"key": "friends", "label": UIStrings.SETTINGS_FRIENDS, "sprite": "settings_friends"},
	{"key": "help", "label": UIStrings.SETTINGS_HELP, "sprite": "settings_help"},
	{"key": "kim", "label": UIStrings.SETTINGS_KIM, "sprite": "settings_kim"},
	{"key": "game", "label": UIStrings.SETTINGS_GAME, "sprite": "settings_game"},
	{"key": "editor", "label": UIStrings.SETTINGS_EDITOR, "sprite": "settings_editor"},
	{"key": "chat", "label": UIStrings.SETTINGS_CHAT, "sprite": "settings_chat"},
]

@onready var toggle_btn: TextureButton = $ToggleBtn
@onready var content_panel: PanelContainer = $ContentPanel
@onready var content_title: Label = $ContentPanel/ContentMargin/ContentVBox/HeaderRow/Title
@onready var close_btn: Button = $ContentPanel/ContentMargin/ContentVBox/HeaderRow/CloseBtn
@onready var icons_column: VBoxContainer = $ContentPanel/ContentMargin/ContentVBox/SectionsRow/IconsColumn
@onready var body_label: Label = $ContentPanel/ContentMargin/ContentVBox/SectionsRow/Body

var _is_open: bool = false
var _icon_buttons: Array[Button] = []
var _selected_key: String = "help"


func _ready() -> void:
	toggle_btn.texture_normal = UISpriteLibrary.get_texture("settings_toggle")
	toggle_btn.texture_hover = toggle_btn.texture_normal
	toggle_btn.texture_pressed = toggle_btn.texture_normal
	toggle_btn.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	toggle_btn.ignore_texture_size = true
	toggle_btn.stretch_mode = TextureButton.STRETCH_KEEP_ASPECT_CENTERED

	content_title.text = UIStrings.SETTINGS_PANEL_TITLE
	body_label.text = ""
	content_panel.visible = false
	var text_cfg: Dictionary = UILayoutConfig.get_text_settings()
	var default_size: int = int(text_cfg.get("default_size", 14))
	var small_size: int = int(text_cfg.get("small_size", 12))
	content_title.add_theme_font_size_override("font_size", default_size)
	body_label.add_theme_font_size_override("font_size", small_size)

	var panel_style: StyleBoxFlat = StyleBoxFlat.new()
	panel_style.bg_color = Color(0.07, 0.09, 0.13, 0.88)
	panel_style.border_color = Color(0.38, 0.50, 0.70, 0.95)
	panel_style.border_width_left = 1
	panel_style.border_width_top = 1
	panel_style.border_width_right = 1
	panel_style.border_width_bottom = 1
	panel_style.corner_radius_top_left = 4
	panel_style.corner_radius_top_right = 4
	panel_style.corner_radius_bottom_left = 4
	panel_style.corner_radius_bottom_right = 4
	content_panel.add_theme_stylebox_override("panel", panel_style)

	if not toggle_btn.pressed.is_connected(_on_toggle_pressed):
		toggle_btn.pressed.connect(_on_toggle_pressed)
	if not close_btn.pressed.is_connected(_on_close_pressed):
		close_btn.pressed.connect(_on_close_pressed)

	_build_section_buttons()
	_select_section(_selected_key)


func apply_hud_layout(viewport_size: Vector2) -> void:
	var fallback: Rect2 = Rect2(20.0, 20.0, 280.0, 180.0)
	var rect: Rect2 = UILayoutConfig.get_element_rect("settings_panel", viewport_size, fallback)
	position = rect.position
	size = rect.size

	var toggle_size: float = clampf(rect.size.y * 0.22, 26.0, 46.0)
	toggle_btn.position = Vector2.ZERO
	toggle_btn.size = Vector2(toggle_size, toggle_size)

	var panel_w: float = clampf(rect.size.x, 240.0, 420.0)
	var panel_h: float = clampf(rect.size.y * 1.2, 150.0, 300.0)
	content_panel.position = Vector2(toggle_size + 10.0, 0.0)
	content_panel.size = Vector2(panel_w, panel_h)


func _build_section_buttons() -> void:
	for child_variant: Variant in icons_column.get_children():
		var child: Node = child_variant as Node
		if child != null:
			child.queue_free()
	_icon_buttons.clear()

	for section: Dictionary in SECTIONS:
		var button: Button = Button.new()
		button.custom_minimum_size = ICON_SIZE
		button.flat = true
		button.clip_text = true
		button.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
		button.alignment = HORIZONTAL_ALIGNMENT_LEFT
		button.expand_icon = true
		button.text = str(section.get("label", ""))
		button.icon = UISpriteLibrary.get_texture(str(section.get("sprite", "")))
		button.pressed.connect(_on_section_pressed.bind(str(section.get("key", ""))))

		icons_column.add_child(button)
		_icon_buttons.append(button)


func _on_toggle_pressed() -> void:
	_is_open = not _is_open
	content_panel.visible = _is_open
	if _is_open:
		content_panel.modulate.a = 0.0
		var tween: Tween = create_tween()
		tween.tween_property(content_panel, "modulate:a", 1.0, 0.12)


func _on_close_pressed() -> void:
	_is_open = false
	content_panel.visible = false


func _on_section_pressed(key: String) -> void:
	_select_section(key)


func _select_section(key: String) -> void:
	_selected_key = key
	for idx: int in range(SECTIONS.size()):
		var section: Dictionary = SECTIONS[idx]
		var button: Button = _icon_buttons[idx]
		var selected: bool = str(section.get("key", "")) == key
		button.modulate = Color(0.72, 0.93, 1.0) if selected else Color(1.0, 1.0, 1.0)

	body_label.text = _get_section_text(key)


func _get_section_text(key: String) -> String:
	match key:
		"friends":
			return UIStrings.SETTINGS_PANEL_FRIENDS
		"help":
			return UIStrings.SETTINGS_PANEL_HELP
		"kim":
			var learner: Node = get_node_or_null("/root/FeedbackLearner")
			if learner != null:
				var rating: String = ""
				var perf: String = ""
				if learner.has_method("get_rating_context"):
					rating = str(learner.call("get_rating_context"))
				if learner.has_method("get_performance_context"):
					perf = str(learner.call("get_performance_context"))
				var combined: String = "%s\n\n%s" % [rating, perf]
				if not combined.strip_edges().is_empty():
					return combined
			return UIStrings.SETTINGS_PANEL_KIM_EMPTY
		"game":
			var cap: int = Engine.get_max_fps()
			var cap_text: String = "Unlimited" if cap == 0 else str(cap)
			return UIStrings.SETTINGS_PANEL_GAME % [cap_text, str(DisplayServer.screen_get_size())]
		"editor":
			return UIStrings.SETTINGS_PANEL_EDITOR
		"chat":
			return UIStrings.SETTINGS_PANEL_CHAT
		_:
			return ""


func show_panel() -> void:
	_is_open = true
	content_panel.visible = true


func hide_panel() -> void:
	_is_open = false
	content_panel.visible = false
