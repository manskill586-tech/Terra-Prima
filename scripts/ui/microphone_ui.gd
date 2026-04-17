extends Control

@onready var background: TextureRect  = $Background
@onready var mic_button: TextureButton = $MicButton
@onready var voice_level_bar: ProgressBar = $VoiceLevelBar

var _is_active: bool = false
var _level_timer: float = 0.0
var _tex_active: Texture2D
var _tex_disabled: Texture2D


func _ready() -> void:
	_tex_active   = load("res://assets/UI_player(MicrophoneUI_Microphone_isActive_Btn) _0001.png") as Texture2D
	_tex_disabled = load("res://assets/UI_player(MicrophoneUI_Microphone_isDisabled_Btn) _0001.png") as Texture2D
	var tex_bg: Texture2D = load("res://assets/UI_player(MicrophoneUI) _0001.png") as Texture2D

	if tex_bg != null:
		background.texture = tex_bg
		background.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST

	if _tex_disabled != null:
		mic_button.texture_normal = _tex_disabled
	if _tex_active != null:
		mic_button.texture_pressed = _tex_active
	mic_button.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	mic_button.toggle_mode = true
	mic_button.toggled.connect(_on_mic_toggled)

	voice_level_bar.min_value = 0.0
	voice_level_bar.max_value = 1.0
	voice_level_bar.value = 0.0


func _process(delta: float) -> void:
	if _is_active:
		_level_timer += delta
		if _level_timer >= 0.08:
			_level_timer = 0.0
			voice_level_bar.value = randf_range(0.15, 0.95)
	else:
		voice_level_bar.value = lerpf(voice_level_bar.value, 0.0, 0.12)


func _on_mic_toggled(pressed: bool) -> void:
	_is_active = pressed
	if pressed and _tex_active != null:
		mic_button.texture_normal = _tex_active
	elif not pressed and _tex_disabled != null:
		mic_button.texture_normal = _tex_disabled
