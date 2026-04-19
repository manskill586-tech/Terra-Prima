extends Control

@onready var title_label: Label = $Panel/Margin/VBox/TitleLabel
@onready var quest_label: Label = $Panel/Margin/VBox/QuestInfoLabel
@onready var rating_slider: HSlider = $Panel/Margin/VBox/RatingSlider
@onready var rating_label: Label = $Panel/Margin/VBox/RatingValueLabel
@onready var submit_btn: Button = $Panel/Margin/VBox/ButtonsRow/SubmitButton
@onready var close_btn: Button = $Panel/Margin/VBox/ButtonsRow/CloseButton
@onready var panel: Control = $Panel

var _current_quest_id: String = ""


func _ready() -> void:
	visible = false
	title_label.text = UIStrings.RATING_TITLE
	submit_btn.text = UIStrings.RATING_SUBMIT
	close_btn.text = UIStrings.RATING_SKIP
	var text_cfg: Dictionary = UILayoutConfig.get_text_settings()
	var title_size: int = int(text_cfg.get("title_size", 18))
	var default_size: int = int(text_cfg.get("default_size", 14))
	title_label.add_theme_font_size_override("font_size", title_size)
	quest_label.add_theme_font_size_override("font_size", default_size)
	rating_label.add_theme_font_size_override("font_size", title_size)

	rating_slider.min_value = 0
	rating_slider.max_value = 100
	rating_slider.step = 1
	rating_slider.value = 50
	rating_slider.value_changed.connect(_on_slider_changed)
	submit_btn.pressed.connect(_on_submit)
	close_btn.pressed.connect(_on_close)
	_on_slider_changed(50.0)


func apply_hud_layout(viewport_size: Vector2) -> void:
	size = viewport_size
	var fallback: Rect2 = Rect2(viewport_size.x * 0.5 - 260.0, viewport_size.y * 0.5 - 180.0, 520.0, 320.0)
	var rect: Rect2 = UILayoutConfig.get_element_rect("rating_popup", viewport_size, fallback)
	panel.position = rect.position
	panel.size = rect.size


func show_for(quest_id: String) -> void:
	_current_quest_id = quest_id
	var quest_data: Dictionary = {}
	if QuestManager != null and QuestManager.has_method("get_quest_data"):
		quest_data = QuestManager.call("get_quest_data", quest_id) as Dictionary
	var quest_name: String = str(quest_data.get("theme", quest_id))
	quest_label.text = UIStrings.RATING_QUEST_LABEL % quest_name
	rating_slider.value = 50
	_on_slider_changed(50.0)
	visible = true
	panel.scale = Vector2(0.85, 0.85)
	panel.modulate.a = 0.0
	var tween: Tween = create_tween()
	tween.set_parallel(true)
	tween.tween_property(panel, "scale", Vector2(1.0, 1.0), 0.18)
	tween.tween_property(panel, "modulate:a", 1.0, 0.18)


func _on_slider_changed(value: float) -> void:
	var as_int: int = int(value)
	rating_label.text = "%d / 100" % as_int
	var ratio: float = value / 100.0
	if ratio > 0.6:
		rating_label.modulate = Color(0.35, 0.95, 0.40)
	elif ratio > 0.3:
		rating_label.modulate = Color(0.95, 0.88, 0.22)
	else:
		rating_label.modulate = Color(0.95, 0.35, 0.28)


func _on_submit() -> void:
	var rating: int = int(rating_slider.value)
	var server: Node = _get_game_server()
	if server != null and server.has_method("rate_game"):
		server.call("rate_game", _current_quest_id, rating)
	_on_close()


func _on_close() -> void:
	var tween: Tween = create_tween()
	tween.tween_property(panel, "modulate:a", 0.0, 0.12)
	tween.tween_callback(func() -> void:
		visible = false
	)


func _get_game_server() -> Node:
	var direct: Node = get_node_or_null("/root/GameServer")
	if direct != null:
		return direct
	return _find_by_script(get_tree().root, "game_server.gd")


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
