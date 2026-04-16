extends Node

enum QuestState { PENDING, ACTIVE, COMPLETED, FAILED, ABANDONED }

signal quest_created(quest_id: String, theme: String)
signal quest_started(quest_id: String)
signal quest_updated(quest_id: String, event: String)
signal quest_ended(quest_id: String, state: QuestState, score: float)

const QUESTS_SAVE_PATH: String = "user://kim_quests.json"
const MAX_HISTORY: int = 100

# Active quests: quest_id -> QuestData dictionary
var active_quests: Dictionary = {}

# Completed/failed history
var quest_history: Array = []


func _ready() -> void:
	load_quests()


# ─── Quest lifecycle ──────────────────────────────────────────────────────────

func create_quest(
		theme: String,
		description: String,
		player_ids: Array,
		objectives: Array) -> String:

	var quest_id: String = "quest_%d_%d" % [Time.get_ticks_msec(), randi() % 9999]
	var quest_data: Dictionary = {
		"id":          quest_id,
		"theme":       theme,
		"description": description,
		"players":     player_ids,
		"objectives":  objectives,
		"state":       QuestState.PENDING,
		"score":       0.0,
		"created_at":  Time.get_ticks_msec(),
		"started_at":  0,
		"ended_at":    0,
		"events":      [],
	}
	active_quests[quest_id] = quest_data
	quest_created.emit(quest_id, theme)
	print("[QuestManager] Created quest '%s': %s" % [quest_id, theme])
	save_quests()
	return quest_id


func start_quest(quest_id: String) -> void:
	if not active_quests.has(quest_id):
		push_warning("[QuestManager] Quest not found: %s" % quest_id)
		return
	var quest: Dictionary = active_quests[quest_id]
	quest["state"] = QuestState.ACTIVE
	quest["started_at"] = Time.get_ticks_msec()
	active_quests[quest_id] = quest
	quest_started.emit(quest_id)
	print("[QuestManager] Started quest: %s" % quest_id)
	save_quests()


func update_quest(quest_id: String, event: String, data: Dictionary = {}) -> void:
	if not active_quests.has(quest_id):
		return
	var quest: Dictionary = active_quests[quest_id]
	var event_entry: Dictionary = {
		"event":    event,
		"data":     data,
		"timestamp": Time.get_ticks_msec(),
	}
	var events: Array = quest.get("events", [])
	events.append(event_entry)
	quest["events"] = events
	active_quests[quest_id] = quest
	quest_updated.emit(quest_id, event)
	save_quests()


func complete_quest(quest_id: String, score: float = 100.0) -> void:
	_end_quest(quest_id, QuestState.COMPLETED, clampf(score, 0.0, 100.0))


func fail_quest(quest_id: String) -> void:
	_end_quest(quest_id, QuestState.FAILED, 0.0)


func abandon_quest(quest_id: String) -> void:
	_end_quest(quest_id, QuestState.ABANDONED, 0.0)


func _end_quest(quest_id: String, state: QuestState, score: float) -> void:
	if not active_quests.has(quest_id):
		return
	var quest: Dictionary = active_quests[quest_id]
	quest["state"]    = state
	quest["score"]    = score
	quest["ended_at"] = Time.get_ticks_msec()

	quest_history.append(quest)
	if quest_history.size() > MAX_HISTORY:
		quest_history = quest_history.slice(quest_history.size() - MAX_HISTORY, quest_history.size())

	active_quests.erase(quest_id)
	quest_ended.emit(quest_id, state, score)
	print("[QuestManager] Quest ended '%s': state=%d score=%.0f" % [quest_id, state, score])
	save_quests()


# ─── Context for LLM ─────────────────────────────────────────────────────────

func get_active_quest_context() -> String:
	if active_quests.is_empty():
		return "No active quests."
	var lines: Array = []
	for quest_id in active_quests.keys():
		var q: Dictionary = active_quests[quest_id]
		var state_name: String = QuestState.keys()[int(q.get("state", 0))]
		lines.append("[%s] theme=%s state=%s objectives=%d" % [
			quest_id,
			str(q.get("theme", "?")),
			state_name,
			(q.get("objectives", []) as Array).size(),
		])
	return "Active quests:\n" + "\n".join(lines)


func get_quest_history_context(limit: int = 5) -> String:
	if quest_history.is_empty():
		return "No past quests."
	var recent: Array = quest_history.slice(
		max(0, quest_history.size() - limit),
		quest_history.size()
	)
	var lines: Array = []
	for q in recent:
		var state_name: String = QuestState.keys()[int(q.get("state", 0))]
		lines.append("theme=%s state=%s score=%.0f" % [
			str(q.get("theme", "?")),
			state_name,
			float(q.get("score", 0.0)),
		])
	return "Recent quests:\n" + "\n".join(lines)


func get_quest_data(quest_id: String) -> Dictionary:
	if active_quests.has(quest_id):
		return active_quests[quest_id]
	for q in quest_history:
		if str(q.get("id", "")) == quest_id:
			return q
	return {}


# ─── Persistence ──────────────────────────────────────────────────────────────

func save_quests() -> void:
	var data: Dictionary = {
		"active":  _serialize_active(),
		"history": quest_history,
	}
	var file: FileAccess = FileAccess.open(QUESTS_SAVE_PATH, FileAccess.WRITE)
	if file == null:
		push_warning("[QuestManager] Cannot write '%s': %d" % [QUESTS_SAVE_PATH, FileAccess.get_open_error()])
		return
	file.store_string(JSON.stringify(data))
	file.close()


func load_quests() -> void:
	if not FileAccess.file_exists(QUESTS_SAVE_PATH):
		return
	var file: FileAccess = FileAccess.open(QUESTS_SAVE_PATH, FileAccess.READ)
	if file == null:
		return
	var content: String = file.get_as_text()
	file.close()
	var parsed: Variant = JSON.parse_string(content)
	if parsed == null or parsed is not Dictionary:
		return
	_deserialize_active(parsed.get("active", {}))
	var hist: Variant = parsed.get("history", [])
	if hist is Array:
		quest_history = hist as Array


func _serialize_active() -> Dictionary:
	var out: Dictionary = {}
	for quest_id in active_quests.keys():
		var q: Dictionary = active_quests[quest_id].duplicate(true)
		# QuestState enum -> int (already stored as int)
		out[str(quest_id)] = q
	return out


func _deserialize_active(data: Dictionary) -> void:
	active_quests.clear()
	for raw_key in data.keys():
		active_quests[str(raw_key)] = data[raw_key]
