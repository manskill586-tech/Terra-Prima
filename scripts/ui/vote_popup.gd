extends Control

@onready var question_label: Label  = $Panel/QuestionLabel
@onready var agree_btn: Button      = $Panel/AgreeButton
@onready var disagree_btn: Button   = $Panel/DisagreeButton
@onready var timer_bar: ProgressBar = $Panel/TimerBar
@onready var panel: Control         = $Panel

const VOTE_TIMEOUT: float = 30.0

var _penalty_id: String = ""
var _timer: float = 0.0
var _active: bool = false


func _ready() -> void:
	visible = false
	agree_btn.pressed.connect(_on_agree)
	disagree_btn.pressed.connect(_on_disagree)
	timer_bar.min_value = 0
	timer_bar.max_value = VOTE_TIMEOUT


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
	question_label.text = question if not question.is_empty() else \
		"Ким оспаривает штраф за производительность.\nОптимизация была хорошей?"
	timer_bar.value = VOTE_TIMEOUT
	visible = true

	panel.scale = Vector2(0.1, 0.1)
	var tw := create_tween()
	tw.set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	tw.tween_property(panel, "scale", Vector2(1.0, 1.0), 0.25)


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
	var tw := create_tween()
	tw.tween_property(panel, "scale", Vector2(0.1, 0.1), 0.15)
	tw.tween_callback(func(): visible = false)


func _get_game_server() -> Node:
	var direct: Node = get_node_or_null("/root/GameServer")
	if direct != null:
		return direct
	return _find_by_script(get_tree().root, "game_server.gd")


func _find_by_script(node: Node, script_file: String) -> Node:
	var s: Script = node.get_script() as Script
	if s != null and s.resource_path.ends_with(script_file):
		return node
	for child in node.get_children():
		var found: Node = _find_by_script(child, script_file)
		if found != null:
			return found
	return null
