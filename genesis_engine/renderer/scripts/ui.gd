extends Control

signal toggle_polling
signal toggle_particles
signal toggle_chunks
signal visual_mode_changed
signal reset_camera

@onready var status_label: Label = $Panel/VBox/StatusLabel
@onready var stats_label: Label = $Panel/VBox/StatsLabel
@onready var fps_label: Label = $Panel/VBox/FpsLabel
@onready var mode_label: Label = $Panel/VBox/ModeLabel
@onready var toggle_button: Button = $Panel/VBox/TogglePollingButton
@onready var particles_button: Button = $Panel/VBox/ToggleParticlesButton
@onready var chunks_button: Button = $Panel/VBox/ToggleChunksButton
@onready var mode_button: OptionButton = $Panel/VBox/ModeOption
@onready var reset_button: Button = $Panel/VBox/ResetCameraButton
@onready var probe_label: Label = $Panel/VBox/ProbeLabel

func _ready() -> void:
	toggle_button.pressed.connect(_on_toggle_polling)
	particles_button.pressed.connect(_on_toggle_particles)
	chunks_button.pressed.connect(_on_toggle_chunks)
	mode_button.item_selected.connect(_on_mode_selected)
	reset_button.pressed.connect(_on_reset_camera)
	mode_button.clear()
	mode_button.add_item("Color", 0)
	mode_button.add_item("Phase", 1)
	mode_button.add_item("Temp", 2)
	mode_button.add_item("Hardness", 3)

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

func set_particles_visible(visible_flag: bool) -> void:
	particles_button.text = "Hide Particles" if visible_flag else "Show Particles"

func set_chunks_visible(visible_flag: bool) -> void:
	chunks_button.text = "Hide Chunks" if visible_flag else "Show Chunks"

func set_visual_mode(mode: int) -> void:
	mode_button.select(mode)

func set_probe_info(text: String) -> void:
	probe_label.text = text

func _on_toggle_polling() -> void:
	emit_signal("toggle_polling")

func _on_toggle_particles() -> void:
	emit_signal("toggle_particles")

func _on_toggle_chunks() -> void:
	emit_signal("toggle_chunks")

func _on_mode_selected(index: int) -> void:
	emit_signal("visual_mode_changed", index)

func _on_reset_camera() -> void:
	emit_signal("reset_camera")
