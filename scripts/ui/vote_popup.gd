extends Control

const VOTE_TIMEOUT: float = 30.0

@onready var question_label: Label = $Panel/Margin/VBox/QuestionLabel
@onready var agree_btn: Button = $Panel/Margin/VBox/ButtonsRow/AgreeButton
@onready var disagree_btn: Button = $Panel/Margin/VBox/ButtonsRow/DisagreeButton
@onready var timer_bar: ProgressBar = $Panel/Margin/VBox/TimerBar
@onready var panel: Control = $Panel

var _penalty_id: String = ""
var _timer: float = 0.0
var _active: bool = false


func _ready() -> void:
	visible = false
	question_label.text = UIStrings.VOTE_DEFAULT_QUESTION
	agree_btn.text = UIStrings.VOTE_AGREE
	disagree_btn.text = UIStrings.VOTE_DISAGREE
	var text_cfg: Dictionary = UILayoutConfig.get_text_settings()
	var default_size: int = int(text_cfg.get("default_size", 14))
	var title_size: int = int(text_cfg.get("title_size", 18))
	question_label.add_theme_font_size_override("font_size", title_size)
	agree_btn.add_theme_font_size_override("font_size", default_size)
	disagree_btn.add_theme_font_size_override("font_size", default_size)
	agree_btn.pressed.connect(_on_agree)
	disagree_btn.pressed.connect(_on_disagree)
	timer_bar.min_value = 0.0
	timer_bar.max_value = VOTE_TIMEOUT


func apply_hud_layout(viewport_size: Vector2) -> void:
	size = viewport_size
	var fallback: Rect2 = Rect2(viewport_size.x * 0.5 - 300.0, viewport_size.y * 0.5 - 160.0, 600.0, 300.0)
	var rect: Rect2 = UILayoutConfig.get_element_rect("vote_popup", viewport_size, fallback)
	panel.position = rect.position
	panel.size = rect.size


func _process(delta: float) -> void:
	if not _active:
		return
	_timer -= delta
	timer_bar.value = maxf(_timer, 0.0)
	if _timer <= 0.0:
		_on_timeout()


func show_for(penalty_id: String, question: String = "") -> void:
	_penalty_id = penalty_id
	_timer = VOTE_TIMEOUT
	_active = true
	question_label.text = question if not question.is_empty() else UIStrings.VOTE_DEFAULT_QUESTION
	timer_bar.value = VOTE_TIMEOUT
	visible = true
	panel.scale = Vector2(0.90, 0.90)
	panel.modulate.a = 0.0
	var tween: Tween = create_tween()
	tween.set_parallel(true)
	tween.tween_property(panel, "scale", Vector2(1.0, 1.0), 0.15)
	tween.tween_property(panel, "modulate:a", 1.0, 0.15)


func _on_agree() -> void:
	_vote(true)


func _on_disagree() -> void:
	_vote(false)


func _vote(agrees: bool) -> void:
	_active = false
	var server: Node = _get_game_server()
	if server != null and server.has_method("vote_optimization"):
		server.call("vote_optimization", _penalty_id, agrees)
	_close()


func _on_timeout() -> void:
	_active = false
	_close()


func _close() -> void:
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
