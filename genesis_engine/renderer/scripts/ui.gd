extends Control

signal toggle_polling
signal toggle_particles
signal reset_camera

@onready var status_label: Label = $Panel/VBox/StatusLabel
@onready var stats_label: Label = $Panel/VBox/StatsLabel
@onready var fps_label: Label = $Panel/VBox/FpsLabel
@onready var mode_label: Label = $Panel/VBox/ModeLabel
@onready var toggle_button: Button = $Panel/VBox/TogglePollingButton
@onready var particles_button: Button = $Panel/VBox/ToggleParticlesButton
@onready var reset_button: Button = $Panel/VBox/ResetCameraButton

func _ready() -> void:
	toggle_button.pressed.connect(_on_toggle_polling)
	particles_button.pressed.connect(_on_toggle_particles)
	reset_button.pressed.connect(_on_reset_camera)

func set_status(text: String) -> void:
	status_label.text = text

func set_stats(particles: int, time_s: float) -> void:
	stats_label.text = "Particles: %d  Time: %.2f s" % [particles, time_s]

func set_fps(fps: int) -> void:
	fps_label.text = "FPS: %d" % fps

func set_mode(mode: String) -> void:
	mode_label.text = "Mode: %s" % mode

func set_polling(is_polling: bool) -> void:
	toggle_button.text = "Stop" if is_polling else "Start"

func set_particles_visible(is_visible: bool) -> void:
	particles_button.text = "Hide Particles" if is_visible else "Show Particles"

func _on_toggle_polling() -> void:
	emit_signal("toggle_polling")

func _on_toggle_particles() -> void:
	emit_signal("toggle_particles")

func _on_reset_camera() -> void:
	emit_signal("reset_camera")
