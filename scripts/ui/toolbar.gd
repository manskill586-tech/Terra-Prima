extends Control

signal item_selected(index: int, item_data: Dictionary)

var items: Array[Dictionary] = [
	{"name": "Hand", "color": Color(0.80, 0.72, 0.62)},
	{"name": "Blade", "color": Color(0.70, 0.72, 0.93)},
	{"name": "Shield", "color": Color(0.58, 0.81, 0.62)},
	{"name": "Torch", "color": Color(0.98, 0.80, 0.33)},
	{"name": "Potion", "color": Color(0.90, 0.47, 0.72)},
	{"name": "Bag", "color": Color(0.91, 0.73, 0.44)},
]

@onready var panel_bg: TextureRect = $PanelBg
@onready var slots_root: Control = $SlotsRoot

var _slot_count: int = 6
var _selected_index: int = 0
var _base_angle: float = 0.0
var _rotation_tween: Tween = null
var _slots: Array[Control] = []

var _front_scale: float = 1.24
var _back_scale: float = 0.74
var _selected_boost: float = 1.18
var _alpha_front: float = 1.0
var _alpha_back: float = 0.28
var _rotation_duration: float = 0.24
var _radius_x_factor: float = 0.36
var _radius_y_factor: float = 0.22
var _center_y_factor: float = 0.60


func _ready() -> void:
	panel_bg.texture = UISpriteLibrary.get_texture("toolbar_bg")
	panel_bg.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	_load_toolbar_settings()
	_build_slots()
	_align_selected_to_front()
	_update_drum(_base_angle)


func apply_hud_layout(viewport_size: Vector2) -> void:
	var fallback: Rect2 = Rect2(viewport_size.x * 0.5 - 260.0, viewport_size.y - 130.0, 520.0, 118.0)
	var rect: Rect2 = UILayoutConfig.get_element_rect("toolbar", viewport_size, fallback)
	position = rect.position
	size = rect.size
	slots_root.position = Vector2.ZERO
	slots_root.size = rect.size
	_update_drum(_base_angle)


func _process(_delta: float) -> void:
	if Input.mouse_mode != Input.MOUSE_MODE_CAPTURED:
		return

	if Input.is_action_just_pressed("toolbar_next"):
		scroll_next()
	elif Input.is_action_just_pressed("toolbar_prev"):
		scroll_prev()

	for index: int in range(min(_slot_count, 9)):
		if Input.is_key_pressed(KEY_1 + index):
			_select_index(index)
			break


func scroll_next() -> void:
	_select_index((_selected_index + 1) % _slot_count)


func scroll_prev() -> void:
	_select_index((_selected_index - 1 + _slot_count) % _slot_count)


func _build_slots() -> void:
	for child_variant: Variant in slots_root.get_children():
		var child: Node = child_variant as Node
		if child != null:
			child.queue_free()
	_slots.clear()

	for index: int in range(_slot_count):
		var slot: Control = _create_slot(index)
		slots_root.add_child(slot)
		_slots.append(slot)


func _create_slot(index: int) -> Control:
	var slot: Control = Control.new()
	slot.custom_minimum_size = Vector2(62.0, 62.0)
	slot.pivot_offset = slot.custom_minimum_size * 0.5

	var panel: Panel = Panel.new()
	panel.anchor_right = 1.0
	panel.anchor_bottom = 1.0
	_apply_slot_style(panel, index)
	slot.add_child(panel)

	var label: Label = Label.new()
	label.anchor_right = 1.0
	label.anchor_bottom = 1.0
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	label.text = str(index + 1)
	var text_cfg: Dictionary = UILayoutConfig.get_text_settings()
	var default_size: int = int(text_cfg.get("default_size", 14))
	label.add_theme_font_size_override("font_size", default_size)
	slot.add_child(label)

	var button: Button = Button.new()
	button.anchor_right = 1.0
	button.anchor_bottom = 1.0
	button.flat = true
	button.tooltip_text = _get_item_name(index)
	button.pressed.connect(_on_slot_pressed.bind(index))
	slot.add_child(button)

	return slot


func _apply_slot_style(panel: Panel, index: int) -> void:
	var item_color: Color = _get_item_color(index)
	var box: StyleBoxFlat = StyleBoxFlat.new()
	box.bg_color = item_color.darkened(0.25)
	box.border_color = item_color.lightened(0.20)
	box.border_width_left = 1
	box.border_width_top = 1
	box.border_width_right = 1
	box.border_width_bottom = 1
	box.corner_radius_top_left = 64
	box.corner_radius_top_right = 64
	box.corner_radius_bottom_left = 64
	box.corner_radius_bottom_right = 64
	panel.add_theme_stylebox_override("panel", box)


func _on_slot_pressed(index: int) -> void:
	_select_index(index)


func _select_index(index: int) -> void:
	if index < 0 or index >= _slot_count:
		return
	if index == _selected_index:
		return

	_selected_index = index
	_animate_to_selected()

	if index < items.size():
		item_selected.emit(index, items[index])


func _animate_to_selected() -> void:
	var front_angle: float = -PI * 0.5
	var target_angle: float = front_angle - float(_selected_index) * (TAU / float(_slot_count))
	var start_angle: float = _base_angle
	var delta: float = wrapf(target_angle - start_angle, -PI, PI)
	target_angle = start_angle + delta

	if _rotation_tween != null and _rotation_tween.is_valid():
		_rotation_tween.kill()

	_rotation_tween = create_tween()
	_rotation_tween.set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	_rotation_tween.tween_method(_update_drum, start_angle, target_angle, _rotation_duration)
	_rotation_tween.tween_callback(func() -> void:
		_base_angle = target_angle
	)


func _align_selected_to_front() -> void:
	var front_angle: float = -PI * 0.5
	_base_angle = front_angle - float(_selected_index) * (TAU / float(_slot_count))


func _update_drum(angle: float) -> void:
	if _slots.is_empty():
		return

	_base_angle = angle
	var center_x: float = size.x * 0.5
	var center_y: float = size.y * _center_y_factor
	var radius_x: float = size.x * _radius_x_factor
	var radius_y: float = size.y * _radius_y_factor
	var base_slot: float = clampf(size.y * 0.44, 42.0, 72.0)
	var selected_scale: float = _front_scale * _selected_boost
	var normal_scale: float = _back_scale

	for index: int in range(_slots.size()):
		var slot: Control = _slots[index]
		var slot_angle: float = angle + float(index) * (TAU / float(_slot_count))
		var x: float = center_x + cos(slot_angle) * radius_x
		var y: float = center_y + sin(slot_angle) * radius_y
		var is_selected: bool = index == _selected_index
		var scale_value: float = selected_scale if is_selected else normal_scale
		var alpha_value: float = 1.0 if is_selected else _alpha_back

		slot.size = Vector2(base_slot, base_slot)
		slot.pivot_offset = slot.size * 0.5
		slot.scale = Vector2.ONE * scale_value
		slot.position = Vector2(round(x - slot.size.x * 0.5), round(y - slot.size.y * 0.5))
		slot.modulate.a = alpha_value
		slot.z_index = 100 if is_selected else 0


func _load_toolbar_settings() -> void:
	var cfg: Dictionary = UILayoutConfig.get_toolbar_settings()
	_slot_count = int(cfg.get("slot_count", 6))
	_front_scale = float(cfg.get("front_scale", 1.24))
	_back_scale = float(cfg.get("back_scale", 0.74))
	_selected_boost = float(cfg.get("selected_boost", 1.18))
	_alpha_front = float(cfg.get("alpha_front", 1.0))
	_alpha_back = float(cfg.get("alpha_back", 0.28))
	_rotation_duration = float(cfg.get("rotation_duration", 0.24))
	_radius_x_factor = float(cfg.get("radius_x_factor", 0.36))
	_radius_y_factor = float(cfg.get("radius_y_factor", 0.22))
	_center_y_factor = float(cfg.get("center_y_factor", 0.60))

	_slot_count = maxi(1, _slot_count)
	if items.is_empty():
		_slot_count = 1
	elif items.size() < _slot_count:
		_slot_count = items.size()


func _get_item_name(index: int) -> String:
	if index >= 0 and index < items.size():
		return str(items[index].get("name", "Item"))
	return "Item"


func _get_item_color(index: int) -> Color:
	if index >= 0 and index < items.size():
		var item: Dictionary = items[index]
		var color_variant: Variant = item.get("color")
		if typeof(color_variant) == TYPE_COLOR:
			return color_variant as Color
	return Color(0.60, 0.60, 0.60)


func get_selected_item() -> Dictionary:
	if _selected_index >= 0 and _selected_index < items.size():
		return items[_selected_index]
	return {}


func set_item_icon(slot_index: int, texture: Texture2D) -> void:
	if slot_index < 0 or slot_index >= _slots.size():
		return
	var slot: Control = _slots[slot_index]
	for child_variant: Variant in slot.get_children():
		var child: TextureRect = child_variant as TextureRect
		if child != null:
			child.texture = texture
			return

	var icon_rect: TextureRect = TextureRect.new()
	icon_rect.texture = texture
	icon_rect.anchor_right = 1.0
	icon_rect.anchor_bottom = 1.0
	icon_rect.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	icon_rect.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	icon_rect.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	slot.add_child(icon_rect)
	slot.move_child(icon_rect, 1)
