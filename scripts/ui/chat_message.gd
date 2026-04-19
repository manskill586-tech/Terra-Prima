extends PanelContainer

@onready var sender_label: Label = $Margin/VBox/SenderLabel
@onready var message_label: Label = $Margin/VBox/MessageLabel


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	custom_minimum_size = Vector2(0.0, 24.0)
	var text_cfg: Dictionary = UILayoutConfig.get_text_settings()
	var default_size: int = int(text_cfg.get("default_size", 14))
	var small_size: int = int(text_cfg.get("small_size", 12))
	sender_label.add_theme_font_size_override("font_size", small_size)
	message_label.add_theme_font_size_override("font_size", default_size)
	_set_visual_style(false)


func set_kim_message(text: String) -> void:
	sender_label.text = UIStrings.CHAT_SENDER_KIM
	sender_label.modulate = Color(0.72, 0.95, 1.0)
	message_label.text = text
	_set_visual_style(false)
	_fade_in(0.22)


func set_player_message(player_id: int, text: String) -> void:
	sender_label.text = UIStrings.CHAT_SENDER_PLAYER % player_id
	sender_label.modulate = Color(0.95, 0.88, 1.0)
	message_label.text = text
	_set_visual_style(true)
	_fade_in(0.15)


func _set_visual_style(is_player: bool) -> void:
	var box: StyleBoxFlat = StyleBoxFlat.new()
	box.bg_color = Color(0.10, 0.12, 0.17, 0.88) if is_player else Color(0.08, 0.10, 0.15, 0.92)
	box.border_color = Color(0.45, 0.52, 0.72, 0.85) if is_player else Color(0.42, 0.62, 0.80, 0.9)
	box.border_width_left = 1
	box.border_width_top = 1
	box.border_width_right = 1
	box.border_width_bottom = 1
	box.corner_radius_top_left = 3
	box.corner_radius_top_right = 3
	box.corner_radius_bottom_left = 3
	box.corner_radius_bottom_right = 3
	add_theme_stylebox_override("panel", box)


func _fade_in(duration: float) -> void:
	modulate.a = 0.0
	var tween: Tween = create_tween()
	tween.tween_property(self, "modulate:a", 1.0, duration)
