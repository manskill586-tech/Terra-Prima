extends NinePatchRect

const PATCH_MARGIN := 4

@onready var sender_label: Label = $SenderLabel
@onready var message_label: Label = $MessageLabel

var _tex_kim: Texture2D
var _tex_player: Texture2D


func _ready() -> void:
	patch_margin_left   = PATCH_MARGIN
	patch_margin_top    = PATCH_MARGIN
	patch_margin_right  = PATCH_MARGIN
	patch_margin_bottom = PATCH_MARGIN
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	custom_minimum_size = Vector2(80, 28)

	_tex_kim    = load("res://assets/UI_player(ChatBubble) _0001.png") as Texture2D
	_tex_player = load("res://assets/UI_player(ChatBubble_PlayerColor_ПравыйВерх_Угол) _0001.png") as Texture2D


func set_kim_message(text: String) -> void:
	if _tex_kim != null:
		texture = _tex_kim
	sender_label.text = "Kim"
	sender_label.modulate = Color(0.7, 0.9, 1.0)

	if message_label.has_method("define_text"):
		message_label.call("define_text", text)
		message_label.call("start")
	else:
		message_label.text = text

	modulate.a = 0.0
	var tw := create_tween()
	tw.tween_property(self, "modulate:a", 1.0, 0.25)


func set_player_message(player_id: int, text: String) -> void:
	if _tex_player != null:
		texture = _tex_player
	sender_label.text = "Player %d" % player_id
	sender_label.modulate = Color(0.9, 0.85, 1.0)
	message_label.text = text

	modulate.a = 0.0
	var tw := create_tween()
	tw.tween_property(self, "modulate:a", 1.0, 0.15)
