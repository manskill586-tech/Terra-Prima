class_name UISpriteLibrary
extends RefCounted

const REGIONS_PATH := "res://assets/ui_regions.json"

static var _loaded: bool = false
static var _regions: Dictionary = {}
static var _cache: Dictionary = {}
static var _placeholder_texture: Texture2D = null
static var _reported_errors: Dictionary = {}
static var _missing_sources: Dictionary = {}


static func reload_regions() -> void:
	_loaded = false
	_regions.clear()
	_cache.clear()
	_reported_errors.clear()
	_missing_sources.clear()


static func has_sprite(sprite_id: String) -> bool:
	_ensure_loaded()
	return _regions.has(sprite_id)


static func get_texture(sprite_id: String) -> Texture2D:
	_ensure_loaded()
	if _cache.has(sprite_id):
		return _cache[sprite_id] as Texture2D

	var entry_variant: Variant = _regions.get(sprite_id)
	if typeof(entry_variant) != TYPE_DICTIONARY:
		_log_once("unknown:%s" % sprite_id, "[UISpriteLibrary] Unknown sprite id: %s" % sprite_id)
		return _get_placeholder()

	var entry: Dictionary = entry_variant as Dictionary
	var source_path: String = str(entry.get("source_path", ""))
	var rect_variant: Variant = entry.get("rect", [])
	if source_path.is_empty() or typeof(rect_variant) != TYPE_ARRAY:
		_log_once("invalid-entry:%s" % sprite_id, "[UISpriteLibrary] Invalid region entry: %s" % sprite_id)
		return _get_placeholder()

	var atlas_tex: Texture2D = load(source_path) as Texture2D
	if atlas_tex == null:
		_report_missing_source(source_path)
		return _get_placeholder()

	var rect_data: Array = rect_variant as Array
	if rect_data.size() != 4:
		_log_once("invalid-rect:%s" % sprite_id, "[UISpriteLibrary] Invalid rect size for: %s" % sprite_id)
		return _get_placeholder()

	var x: int = int(rect_data[0])
	var y: int = int(rect_data[1])
	var w: int = int(rect_data[2])
	var h: int = int(rect_data[3])
	if w <= 0 or h <= 0:
		_log_once("non-positive:%s" % sprite_id, "[UISpriteLibrary] Non-positive region for: %s" % sprite_id)
		return _get_placeholder()

	var atlas: AtlasTexture = AtlasTexture.new()
	atlas.atlas = atlas_tex
	atlas.region = Rect2(x, y, w, h)
	atlas.filter_clip = true
	_cache[sprite_id] = atlas
	return atlas


static func _ensure_loaded() -> void:
	if _loaded:
		return
	_loaded = true

	if not FileAccess.file_exists(REGIONS_PATH):
		_log_once("missing-regions", "[UISpriteLibrary] Missing region map: %s" % REGIONS_PATH)
		return

	var raw: String = FileAccess.get_file_as_string(REGIONS_PATH)
	if raw.is_empty():
		_log_once("empty-regions", "[UISpriteLibrary] Empty region map: %s" % REGIONS_PATH)
		return

	var parsed: Variant = JSON.parse_string(raw)
	if typeof(parsed) != TYPE_DICTIONARY:
		_log_once("parse-regions", "[UISpriteLibrary] Failed to parse JSON region map")
		return

	_regions = parsed as Dictionary


static func _get_placeholder() -> Texture2D:
	if _placeholder_texture != null:
		return _placeholder_texture

	var image: Image = Image.create(8, 8, false, Image.FORMAT_RGBA8)
	image.fill(Color(1.0, 0.0, 1.0, 1.0))
	for y: int in range(8):
		for x: int in range(8):
			if (x + y) % 2 == 0:
				image.set_pixel(x, y, Color(0.0, 0.0, 0.0, 1.0))

	_placeholder_texture = ImageTexture.create_from_image(image)
	return _placeholder_texture


static func _log_once(key: String, message: String) -> void:
	if _reported_errors.has(key):
		return
	_reported_errors[key] = true
	printerr(message)


static func _report_missing_source(source_path: String) -> void:
	if _missing_sources.has(source_path):
		return
	_missing_sources[source_path] = true

	var paths: Array = _missing_sources.keys()
	paths.sort()
	var chunks: PackedStringArray = PackedStringArray()
	for item_variant: Variant in paths:
		chunks.append(str(item_variant))
	var summary: String = ", ".join(chunks)
	_log_once("missing-summary:%d" % chunks.size(), "[UISpriteLibrary] Missing atlas textures (%d): %s" % [chunks.size(), summary])
