extends Control

# Базовые значения в виртуальных 192×108 пикселях
const SLOT_COUNT:     int   = 6
const BASE_SLOT:      float = 14.0   # px в виртуальном пространстве
const BASE_RADIUS_X:  float = 56.0
const BASE_RADIUS_Y:  float = 10.0
const SELECTED_SCALE: float = 1.28
const SPIN_DURATION:  float = 0.22
const PEEK_RATIO:     float = 0.20

signal item_selected(index: int, item_data: Dictionary)

var _slot_size:     float = BASE_SLOT
var _radius_x:      float = BASE_RADIUS_X
var _radius_y:      float = BASE_RADIUS_Y
var _panel_height:  float = 18.0
var _peek_height:   float = 4.0
var _virtual_w:     float = 192.0
var _virtual_h:     float = 108.0

var _selected_index: int   = 0
var _base_angle:     float = 0.0
var _target_angle:   float = 0.0
var _slots:          Array = []
var _spin_tween:     Tween = null

var items: Array = [
	{"name": "Рука",    "color": Color(0.8, 0.7, 0.6)},
	{"name": "Меч",     "color": Color(0.7, 0.7, 0.9)},
	{"name": "Щит",     "color": Color(0.6, 0.8, 0.6)},
	{"name": "Факел",   "color": Color(1.0, 0.8, 0.3)},
	{"name": "Зелье",   "color": Color(0.9, 0.4, 0.7)},
	{"name": "Сумка",   "color": Color(0.9, 0.7, 0.4)},
]

@onready var drum_container: Control = $DrumContainer
@onready var panel_bg:       Panel   = $PanelBg


func _ready() -> void:
	# Инициализация с дефолтными виртуальными значениями
	# player_hud вызовет set_ui_scale позже с правильными параметрами
	set_ui_scale(1.0, 192.0, 108.0)


func set_ui_scale(screen_scale: float, virt_w: float, virt_h: float) -> void:
	_virtual_w    = virt_w
	_virtual_h    = virt_h
	# Все размеры уже в виртуальных единицах — масштаб не нужен
	_slot_size    = BASE_SLOT
	_radius_x     = BASE_RADIUS_X
	_radius_y     = BASE_RADIUS_Y
	_panel_height = 18.0
	_peek_height  = _panel_height * PEEK_RATIO

	# Позиция в виртуальном 192×108 пространстве: прилеплена к низу
	set_anchors_preset(Control.PRESET_TOP_LEFT)
	position = Vector2(0.0, virt_h - _peek_height)
	size     = Vector2(virt_w, _panel_height)

	# Пересоздать слоты
	for slot in _slots:
		slot.queue_free()
	_slots.clear()
	_build_slots()
	_update_drum(0.0)


func _build_slots() -> void:
	for i in range(SLOT_COUNT):
		var slot := _create_slot(i)
		drum_container.add_child(slot)
		_slots.append(slot)


func _create_slot(index: int) -> Control:
	var slot := Control.new()
	slot.custom_minimum_size = Vector2(_slot_size, _slot_size)
	slot.pivot_offset = Vector2(_slot_size * 0.5, _slot_size * 0.5)

	# Круглый фон
	var panel := Panel.new()
	panel.anchor_right  = 1.0
	panel.anchor_bottom = 1.0
	var style := StyleBoxFlat.new()
	var r := int(_slot_size * 0.5)
	style.corner_radius_top_left     = r
	style.corner_radius_top_right    = r
	style.corner_radius_bottom_left  = r
	style.corner_radius_bottom_right = r
	var col: Color = items[index]["color"] if index < items.size() else Color.GRAY
	style.bg_color     = col.darkened(0.3)
	style.border_width_left   = maxi(1, int(_slot_size * 0.03))
	style.border_width_right  = maxi(1, int(_slot_size * 0.03))
	style.border_width_top    = maxi(1, int(_slot_size * 0.03))
	style.border_width_bottom = maxi(1, int(_slot_size * 0.03))
	style.border_color = col.lightened(0.2)
	panel.add_theme_stylebox_override("panel", style)
	slot.add_child(panel)

	# Номер / иконка
	var label := Label.new()
	label.anchor_right  = 1.0
	label.anchor_bottom = 1.0
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	label.vertical_alignment   = VERTICAL_ALIGNMENT_CENTER
	label.text = str(index + 1)
	# Шрифт и размер наследуются из Theme (player_hud.gd)
	# Принудительный размер подписи под слотом
	label.add_theme_font_size_override("font_size", maxi(3, int(_slot_size * 0.3)))
	slot.add_child(label)

	# Кнопка-overlay
	var btn := Button.new()
	btn.anchor_right  = 1.0
	btn.anchor_bottom = 1.0
	btn.flat = true
	btn.pressed.connect(func(): _select_index(index))
	slot.add_child(btn)

	return slot


func _process(_delta: float) -> void:
	if Input.mouse_mode != Input.MOUSE_MODE_CAPTURED:
		return
	if Input.is_action_just_pressed("toolbar_next"):
		scroll_next()
	elif Input.is_action_just_pressed("toolbar_prev"):
		scroll_prev()
	for i in range(min(SLOT_COUNT, 9)):
		if Input.is_key_pressed(KEY_1 + i):
			_select_index(i)
			break


func scroll_next() -> void:
	_select_index((_selected_index + 1) % SLOT_COUNT)


func scroll_prev() -> void:
	_select_index((_selected_index - 1 + SLOT_COUNT) % SLOT_COUNT)


func _select_index(new_index: int) -> void:
	if new_index == _selected_index:
		return
	var prev: int = _selected_index
	_selected_index = new_index

	var diff: int = new_index - prev
	if diff > SLOT_COUNT / 2.0:
		diff -= SLOT_COUNT
	elif diff < -SLOT_COUNT / 2.0:
		diff += SLOT_COUNT

	_target_angle = _base_angle - float(diff) * (TAU / float(SLOT_COUNT))
	_animate_spin()

	if new_index < items.size():
		item_selected.emit(new_index, items[new_index])


func _animate_spin() -> void:
	if _spin_tween != null and _spin_tween.is_valid():
		_spin_tween.kill()
	_spin_tween = create_tween()
	_spin_tween.set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	_spin_tween.tween_method(_update_drum, _base_angle, _target_angle, SPIN_DURATION)
	_spin_tween.tween_callback(func(): _base_angle = _target_angle)


func _update_drum(angle: float) -> void:
	if _slots.is_empty():
		return
	# Центр барабана — в виртуальных координатах
	var center_x: float = _virtual_w * 0.5
	var center_y: float = _panel_height * 0.5 + _peek_height

	for i in range(SLOT_COUNT):
		var slot_angle: float = angle + float(i) * (TAU / float(SLOT_COUNT))
		var x: float = _radius_x * cos(slot_angle)
		var y: float = _radius_y * sin(slot_angle)
		var depth: float  = (sin(slot_angle - PI * 0.5) + 1.0) * 0.5
		var alpha: float  = clampf(1.0 - depth * 0.8, 0.15, 1.0)
		var is_front: bool = i == _selected_index
		var sv: float     = SELECTED_SCALE if is_front else lerpf(1.0, 0.65, depth)
		var slot_z: int   = int((1.0 - depth) * 100.0)

		var slot: Control = _slots[i]
		slot.position = Vector2(
			center_x + x - _slot_size * sv * 0.5,
			center_y + y - _slot_size * sv * 0.5
		)
		slot.scale       = Vector2.ONE * sv
		slot.modulate.a  = alpha
		slot.z_index     = slot_z


func get_selected_item() -> Dictionary:
	if _selected_index < items.size():
		return items[_selected_index]
	return {}


func set_item_icon(slot_index: int, texture: Texture2D) -> void:
	if slot_index >= _slots.size():
		return
	var slot: Control = _slots[slot_index]
	for child in slot.get_children():
		if child is TextureRect:
			child.texture = texture
			return
	var icon_rect := TextureRect.new()
	icon_rect.texture      = texture
	icon_rect.anchor_right  = 1.0
	icon_rect.anchor_bottom = 1.0
	icon_rect.expand_mode   = TextureRect.EXPAND_IGNORE_SIZE
	icon_rect.stretch_mode  = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	icon_rect.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	slot.add_child(icon_rect)
	slot.move_child(icon_rect, 1)
