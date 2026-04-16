extends Node

# --- Interaction tracking ---
var interactions_by_player: Dictionary = {}
var recent_audio_events_by_player: Dictionary = {}

# --- Extended player memory ---
var player_notes: Dictionary = {}       # player_id -> Array[String]  (Kim's private observations)
var social_graph: Dictionary = {}       # player_id -> {connections: {other_id -> {strength, type}}}
var player_session_start: Dictionary = {} # player_id -> timestamp_ms

# --- Persistence ---
const ANALYTICS_SAVE_PATH: String = "user://kim_analytics.json"

const MAX_RECENT_AUDIO_EVENTS: int = 20
const MAX_NOTES_PER_PLAYER: int = 30

const DEFAULT_AUDIO_TOPIC_MAP: Dictionary = {
	"dog_bark":  "animals",
	"cat_meow":  "animals",
	"bird_chirp":"nature",
	"wind":      "nature",
	"rain":      "nature",
	"water":     "nature",
	"fire":      "danger",
	"alarm":     "danger",
	"siren":     "danger",
	"metal_hit": "mechanics",
	"glass_break":"danger",
	"footsteps": "movement",
	"applause":  "social",
	"crowd":     "social",
	"speech":    "social",
}


func _ready() -> void:
	load_analytics()


# ─── Interaction tracking ────────────────────────────────────────────────────

func track_interaction(player_id: int, topic: String, sentiment: float = 0.0) -> void:
	var summary: Dictionary = interactions_by_player.get(player_id, {
		"count": 0,
		"topics": {},
		"avg_sentiment": 0.0,
	})

	summary["count"] += 1

	var topics: Dictionary = summary["topics"]
	topics[topic] = int(topics.get(topic, 0)) + 1
	summary["topics"] = topics

	var count: int = summary["count"]
	var prev_avg: float = float(summary["avg_sentiment"])
	summary["avg_sentiment"] = ((prev_avg * float(count - 1)) + sentiment) / float(count)

	interactions_by_player[player_id] = summary


func get_player_summary(player_id: int) -> Dictionary:
	return interactions_by_player.get(player_id, {})


func track_audio_events(player_id: int, audio_events: Array) -> Dictionary:
	var accepted_events: Array = []
	for raw_event in audio_events:
		if raw_event is not Dictionary:
			continue

		var event: Dictionary = raw_event
		var label: String = str(event.get("label", "unknown"))
		var confidence: float = clampf(float(event.get("confidence", 0.0)), 0.0, 1.0)
		var source: String = str(event.get("source", "world"))
		var topic: String = _map_audio_label_to_topic(label)
		var sentiment: float = (confidence * 0.4) - 0.1

		track_interaction(player_id, topic, sentiment)
		accepted_events.append({
			"label": label,
			"confidence": confidence,
			"source": source,
			"topic": topic,
		})

	var existing_events: Array = recent_audio_events_by_player.get(player_id, [])
	existing_events.append_array(accepted_events)
	if existing_events.size() > MAX_RECENT_AUDIO_EVENTS:
		existing_events = existing_events.slice(
			existing_events.size() - MAX_RECENT_AUDIO_EVENTS,
			existing_events.size()
		)
	recent_audio_events_by_player[player_id] = existing_events

	return get_player_summary(player_id)


func get_recent_audio_events(player_id: int) -> Array:
	return recent_audio_events_by_player.get(player_id, [])


func build_profile_patch(player_id: int) -> Dictionary:
	var summary: Dictionary = get_player_summary(player_id)
	if summary.is_empty():
		return {}

	var topics: Dictionary = summary.get("topics", {})
	var topic_weights: Dictionary = _normalize_topic_weights(topics)
	var likes: Array = _extract_top_topics(topics, 3)
	var avg_sentiment: float = float(summary.get("avg_sentiment", 0.0))
	var count: int = int(summary.get("count", 0))
	var tone: String = _tone_from_sentiment(avg_sentiment)
	var response_length: String = _response_length_from_activity(count)

	return {
		"likes": likes,
		"tone": tone,
		"response_length": response_length,
		"topic_weights": topic_weights,
	}


# ─── Player notes (Kim's private observations) ───────────────────────────────

func add_player_note(player_id: int, note: String) -> void:
	if note.is_empty():
		return
	var notes: Array = player_notes.get(player_id, [])
	notes.append({
		"text": note,
		"timestamp": Time.get_ticks_msec(),
	})
	if notes.size() > MAX_NOTES_PER_PLAYER:
		notes = notes.slice(notes.size() - MAX_NOTES_PER_PLAYER, notes.size())
	player_notes[player_id] = notes
	save_analytics()


func get_player_notes(player_id: int) -> Array:
	return player_notes.get(player_id, [])


func get_player_notes_text(player_id: int) -> String:
	var notes: Array = get_player_notes(player_id)
	if notes.is_empty():
		return ""
	var lines: Array = []
	for note_entry in notes:
		lines.append(str(note_entry.get("text", "")))
	return "\n".join(lines)


# ─── Session tracking ─────────────────────────────────────────────────────────

func record_session_start(player_id: int) -> void:
	player_session_start[player_id] = Time.get_ticks_msec()


func get_session_duration_seconds(player_id: int) -> float:
	if not player_session_start.has(player_id):
		return 0.0
	return float(Time.get_ticks_msec() - int(player_session_start[player_id])) / 1000.0


# ─── Social graph ─────────────────────────────────────────────────────────────

func observe_player_relationship(
		player_a: int,
		player_b: int,
		relation_type: String = "known",
		strength_delta: float = 0.1) -> void:
	# Record relationship from A's perspective
	_update_connection(player_a, player_b, relation_type, strength_delta)
	# Reciprocal (weaker)
	_update_connection(player_b, player_a, relation_type, strength_delta * 0.5)
	save_analytics()


func get_social_context(player_id: int) -> String:
	var graph_entry: Dictionary = social_graph.get(player_id, {})
	var connections: Dictionary = graph_entry.get("connections", {})
	if connections.is_empty():
		return ""
	var lines: Array = []
	for raw_other_id in connections.keys():
		var other_id: int = int(str(raw_other_id))
		var conn: Dictionary = connections[raw_other_id]
		var strength: float = float(conn.get("strength", 0.0))
		var rel_type: String = str(conn.get("type", "known"))
		lines.append("Player %d (%s, strength=%.2f)" % [other_id, rel_type, strength])
	return "Social connections: " + ", ".join(lines)


func _update_connection(
		from_id: int,
		to_id: int,
		relation_type: String,
		delta: float) -> void:
	if not social_graph.has(from_id):
		social_graph[from_id] = {"connections": {}}
	var connections: Dictionary = social_graph[from_id]["connections"]
	var key: String = str(to_id)
	if not connections.has(key):
		connections[key] = {"strength": 0.0, "type": relation_type}
	var current: float = float(connections[key].get("strength", 0.0))
	connections[key]["strength"] = clampf(current + delta, 0.0, 1.0)
	connections[key]["type"] = relation_type
	social_graph[from_id]["connections"] = connections


# ─── Persistence ──────────────────────────────────────────────────────────────

func save_analytics() -> void:
	var data: Dictionary = {
		"interactions": _serialize_interactions(),
		"notes": _serialize_notes(),
		"social": _serialize_social(),
	}
	var file: FileAccess = FileAccess.open(ANALYTICS_SAVE_PATH, FileAccess.WRITE)
	if file == null:
		push_warning("[AnalystModule] Cannot write to '%s': %d" % [
			ANALYTICS_SAVE_PATH, FileAccess.get_open_error()])
		return
	file.store_string(JSON.stringify(data))
	file.close()


func load_analytics() -> void:
	if not FileAccess.file_exists(ANALYTICS_SAVE_PATH):
		return
	var file: FileAccess = FileAccess.open(ANALYTICS_SAVE_PATH, FileAccess.READ)
	if file == null:
		push_warning("[AnalystModule] Cannot read '%s': %d" % [
			ANALYTICS_SAVE_PATH, FileAccess.get_open_error()])
		return
	var content: String = file.get_as_text()
	file.close()
	var parsed: Variant = JSON.parse_string(content)
	if parsed == null or parsed is not Dictionary:
		push_warning("[AnalystModule] Failed to parse analytics JSON.")
		return
	_deserialize_interactions(parsed.get("interactions", {}))
	_deserialize_notes(parsed.get("notes", {}))
	_deserialize_social(parsed.get("social", {}))


func _serialize_interactions() -> Dictionary:
	var out: Dictionary = {}
	for raw_id in interactions_by_player.keys():
		out[str(raw_id)] = interactions_by_player[raw_id]
	return out


func _deserialize_interactions(data: Dictionary) -> void:
	interactions_by_player.clear()
	for raw_key in data.keys():
		interactions_by_player[int(str(raw_key))] = data[raw_key]


func _serialize_notes() -> Dictionary:
	var out: Dictionary = {}
	for raw_id in player_notes.keys():
		out[str(raw_id)] = player_notes[raw_id]
	return out


func _deserialize_notes(data: Dictionary) -> void:
	player_notes.clear()
	for raw_key in data.keys():
		player_notes[int(str(raw_key))] = data[raw_key]


func _serialize_social() -> Dictionary:
	var out: Dictionary = {}
	for raw_id in social_graph.keys():
		out[str(raw_id)] = social_graph[raw_id]
	return out


func _deserialize_social(data: Dictionary) -> void:
	social_graph.clear()
	for raw_key in data.keys():
		social_graph[int(str(raw_key))] = data[raw_key]


# ─── Private helpers ──────────────────────────────────────────────────────────

func _map_audio_label_to_topic(label: String) -> String:
	var normalized: String = label.to_lower().strip_edges()
	if DEFAULT_AUDIO_TOPIC_MAP.has(normalized):
		return str(DEFAULT_AUDIO_TOPIC_MAP[normalized])
	return "general"


func _extract_top_topics(topics: Dictionary, limit: int) -> Array:
	var working: Dictionary = topics.duplicate()
	var result: Array = []
	for _i in range(limit):
		var best_topic: String = ""
		var best_count: int = -1
		for raw_key in working.keys():
			var topic: String = str(raw_key)
			var count: int = int(working.get(raw_key, 0))
			if count > best_count:
				best_count = count
				best_topic = topic
		if best_topic.is_empty():
			break
		result.append(best_topic)
		working.erase(best_topic)
	return result


func _normalize_topic_weights(topics: Dictionary) -> Dictionary:
	var total: float = 0.0
	for raw_key in topics.keys():
		total += float(topics.get(raw_key, 0))
	if total <= 0.0:
		return {}
	var out: Dictionary = {}
	for raw_key in topics.keys():
		var topic: String = str(raw_key)
		var weight: float = float(topics.get(raw_key, 0)) / total
		out[topic] = weight
	return out


func _tone_from_sentiment(avg_sentiment: float) -> String:
	if avg_sentiment >= 0.25:
		return "friendly"
	if avg_sentiment <= -0.2:
		return "careful"
	return "neutral"


func _response_length_from_activity(count: int) -> String:
	if count >= 20:
		return "long"
	if count >= 8:
		return "medium"
	return "short"
