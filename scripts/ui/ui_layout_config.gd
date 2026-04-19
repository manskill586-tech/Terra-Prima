class_name UILayoutConfig
extends RefCounted

const LAYOUT_PATH: String = "res://assets/ui_layout.json"

static var _loaded: bool = false
static var _layout: Dictionary = {}
static var _hot_reload_enabled: bool = true
static var _last_mtime: int = -1


static func reload_config() -> void:
	_loaded = false
	_layout.clear()
	_last_mtime = -1
	_ensure_loaded()


static func set_hot_reload_enabled(enabled: bool) -> void:
	_hot_reload_enabled = enabled


static func get_element_rect(element_id: String, viewport_size: Vector2, fallback: Rect2) -> Rect2:
	_ensure_loaded()
	_refresh_if_needed()

	var elements_variant: Variant = _layout.get("elements", {})
	if typeof(elements_variant) != TYPE_DICTIONARY:
		return _snap_rect(fallback)

	var elements: Dictionary = elements_variant as Dictionary
	var entry_variant: Variant = elements.get(element_id)
	if typeof(entry_variant) != TYPE_DICTIONARY:
		return _snap_rect(fallback)

	var entry: Dictionary = entry_variant as Dictionary
	var pos: Vector2 = _read_vec2(entry.get("position"), Vector2(fallback.position.x / maxf(viewport_size.x, 1.0), fallback.position.y / maxf(viewport_size.y, 1.0)))
	var size_ratio: Vector2 = _read_vec2(entry.get("size"), Vector2(fallback.size.x / maxf(viewport_size.x, 1.0), fallback.size.y / maxf(viewport_size.y, 1.0)))
	var pivot: Vector2 = _read_vec2(entry.get("pivot"), Vector2.ZERO)
	var min_size: Vector2 = _read_vec2(entry.get("min_size"), fallback.size)
	var max_size: Vector2 = _read_vec2(entry.get("max_size"), fallback.size)

	pos.x = clampf(pos.x, -2.0, 3.0)
	pos.y = clampf(pos.y, -2.0, 3.0)
	pivot.x = clampf(pivot.x, -1.0, 2.0)
	pivot.y = clampf(pivot.y, -1.0, 2.0)
	size_ratio.x = maxf(0.0, size_ratio.x)
	size_ratio.y = maxf(0.0, size_ratio.y)
	min_size.x = maxf(1.0, min_size.x)
	min_size.y = maxf(1.0, min_size.y)
	max_size.x = maxf(min_size.x, max_size.x)
	max_size.y = maxf(min_size.y, max_size.y)

	var width: float = clampf(viewport_size.x * size_ratio.x, min_size.x, maxf(min_size.x, max_size.x))
	var height: float = clampf(viewport_size.y * size_ratio.y, min_size.y, maxf(min_size.y, max_size.y))

	var x: float = viewport_size.x * pos.x - width * pivot.x
	var y: float = viewport_size.y * pos.y - height * pivot.y

	return _snap_rect(Rect2(Vector2(x, y), Vector2(width, height)))


static func get_text_settings() -> Dictionary:
	_ensure_loaded()
	_refresh_if_needed()
	var text_variant: Variant = _layout.get("text", {})
	if typeof(text_variant) == TYPE_DICTIONARY:
		return text_variant as Dictionary
	return {}


static func get_toolbar_settings() -> Dictionary:
	_ensure_loaded()
	_refresh_if_needed()
	var toolbar_variant: Variant = _layout.get("toolbar", {})
	if typeof(toolbar_variant) == TYPE_DICTIONARY:
		return toolbar_variant as Dictionary
	return {}


static func _ensure_loaded() -> void:
	if _loaded:
		return
	_loaded = true

	var absolute_path: String = ProjectSettings.globalize_path(LAYOUT_PATH)
	if not FileAccess.file_exists(LAYOUT_PATH):
		printerr("[UILayoutConfig] Missing layout config: %s" % LAYOUT_PATH)
		return

	var raw: String = FileAccess.get_file_as_string(LAYOUT_PATH)
	if raw.is_empty():
		printerr("[UILayoutConfig] Empty layout config: %s" % LAYOUT_PATH)
		return

	var parsed: Variant = JSON.parse_string(raw)
	if typeof(parsed) != TYPE_DICTIONARY:
		printerr("[UILayoutConfig] Failed to parse layout JSON")
		return

	_layout = parsed as Dictionary
	_last_mtime = FileAccess.get_modified_time(absolute_path)


static func _refresh_if_needed() -> void:
	if not _hot_reload_enabled:
		return
	if not OS.is_debug_build():
		return
	var absolute_path: String = ProjectSettings.globalize_path(LAYOUT_PATH)
	if absolute_path.is_empty():
		return
	var mtime: int = FileAccess.get_modified_time(absolute_path)
	if mtime <= 0:
		return
	if _last_mtime <= 0:
		_last_mtime = mtime
		return
	if mtime == _last_mtime:
		return
	reload_config()


static func _read_vec2(value: Variant, fallback: Vector2) -> Vector2:
	if typeof(value) == TYPE_ARRAY:
		var arr: Array = value as Array
		if arr.size() >= 2:
			return Vector2(float(arr[0]), float(arr[1]))
	return fallback


static func _snap_rect(rect: Rect2) -> Rect2:
	return Rect2(Vector2(round(rect.position.x), round(rect.position.y)), Vector2(maxf(1.0, round(rect.size.x)), maxf(1.0, round(rect.size.y))))
