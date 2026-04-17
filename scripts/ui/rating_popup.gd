extends Control

@onready var title_label: Label       = $Panel/TitleLabel
@onready var quest_label: Label       = $Panel/QuestInfoLabel
@onready var rating_slider: HSlider   = $Panel/RatingSlider
@onready var rating_label: Label      = $Panel/RatingValueLabel
@onready var submit_btn: Button       = $Panel/SubmitButton
@onready var close_btn: Button        = $Panel/CloseButton
@onready var panel: Control           = $Panel

var _current_quest_id: String = ""


func _ready() -> void:
	visible = false
	rating_slider.min_value = 0
	rating_slider.max_value = 100
	rating_slider.step = 1
	rating_slider.value = 50
	rating_slider.value_changed.connect(_on_slider_changed)
	submit_btn.pressed.connect(_on_submit)
	close_btn.pressed.connect(_on_close)
	_on_slider_changed(50.0)


func show_for(quest_id: String) -> void:
	_current_quest_id = quest_id
	var quest_data: Dictionary = {}
	if QuestManager != null and QuestManager.has_method("get_quest_data"):
		quest_data = QuestManager.call("get_quest_data", quest_id)
	quest_label.text = "Квест: %s" % str(quest_data.get("theme", quest_id))
	rating_slider.value = 50
	_on_slider_changed(50.0)
	visible = true
	# Анимация появления
	panel.scale = Vector2(0.1, 0.1)
	var tw := create_tween()
	tw.set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	tw.tween_property(panel, "scale", Vector2(1.0, 1.0), 0.3)


func _on_slider_changed(value: float) -> void:
	var v: int = int(value)
	rating_label.text = "%d / 100" % v
	var ratio: float = value / 100.0
	if ratio > 0.6:
		rating_label.modulate = Color.GREEN
	elif ratio > 0.3:
		rating_label.modulate = Color.YELLOW
	else:
		rating_label.modulate = Color.RED


func _on_submit() -> void:
	var rating: int = int(rating_slider.value)
	var server: Node = _get_game_server()
	if server != null and server.has_method("rate_game"):
		server.call("rate_game", _current_quest_id, rating)
	_on_close()


func _on_close() -> void:
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
