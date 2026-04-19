extends Control

@onready var background: TextureRect = $Background
@onready var mic_button: TextureButton = $MicButton
@onready var level_track: Panel = $LevelTrack
@onready var level_fill: ColorRect = $LevelTrack/LevelFill

var _is_active: bool = false
var _level_timer: float = 0.0
var _voice_level: float = 0.0
var _tex_active: Texture2D = null
var _tex_disabled: Texture2D = null


func _ready() -> void:
	_tex_active = UISpriteLibrary.get_texture("mic_button_active")
	_tex_disabled = UISpriteLibrary.get_texture("mic_button_disabled")
	background.texture = UISpriteLibrary.get_texture("mic_bg")
	background.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST

	mic_button.texture_normal = _tex_disabled
	mic_button.texture_pressed = _tex_active
	mic_button.texture_hover = _tex_disabled
	mic_button.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	mic_button.toggle_mode = true
	if not mic_button.toggled.is_connected(_on_mic_toggled):
		mic_button.toggled.connect(_on_mic_toggled)

	var track_style: StyleBoxFlat = StyleBoxFlat.new()
	track_style.bg_color = Color(0.06, 0.08, 0.11, 0.85)
	track_style.border_color = Color(0.32, 0.39, 0.55, 0.9)
	track_style.border_width_left = 1
	track_style.border_width_top = 1
	track_style.border_width_right = 1
	track_style.border_width_bottom = 1
	track_style.corner_radius_top_left = 2
	track_style.corner_radius_top_right = 2
	track_style.corner_radius_bottom_left = 2
	track_style.corner_radius_bottom_right = 2
	level_track.add_theme_stylebox_override("panel", track_style)

	_update_level_bar()


func apply_hud_layout(viewport_size: Vector2) -> void:
	var fallback: Rect2 = Rect2(viewport_size.x - 240.0, 40.0, 220.0, 92.0)
	var rect: Rect2 = UILayoutConfig.get_element_rect("microphone_ui", viewport_size, fallback)
	position = rect.position
	size = rect.size

	var pad_x: float = maxf(8.0, rect.size.x * 0.04)
	var pad_y: float = maxf(8.0, rect.size.y * 0.10)
	var button_size: float = clampf(rect.size.y - pad_y * 2.0, 26.0, rect.size.x * 0.35)
	mic_button.position = Vector2(pad_x, pad_y)
	mic_button.size = Vector2(button_size, button_size)

	var track_x: float = mic_button.position.x + button_size + pad_x
	var track_w: float = maxf(44.0, rect.size.x - track_x - pad_x)
	var track_h: float = clampf(button_size * 0.32, 10.0, 20.0)
	level_track.position = Vector2(track_x, pad_y + (button_size - track_h) * 0.5)
	level_track.size = Vector2(track_w, track_h)
	_update_level_bar()


func _process(delta: float) -> void:
	if _is_active:
		_level_timer += delta
		if _level_timer >= 0.08:
			_level_timer = 0.0
			_voice_level = randf_range(0.15, 0.95)
	else:
		_voice_level = lerpf(_voice_level, 0.0, 0.12)
	_update_level_bar()


func _on_mic_toggled(pressed: bool) -> void:
	_is_active = pressed
	if pressed:
		mic_button.texture_normal = _tex_active
	else:
		mic_button.texture_normal = _tex_disabled


func _update_level_bar() -> void:
	var track_w: float = maxf(2.0, level_track.size.x - 2.0)
	level_fill.size.x = clampf(track_w * _voice_level, 2.0, track_w)
	level_fill.size.y = maxf(1.0, level_track.size.y)
