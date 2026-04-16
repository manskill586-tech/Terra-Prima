extends Node

const WEIGHTS_SAVE_PATH: String = "user://kim_weights.json"
const RATINGS_SAVE_PATH: String = "user://kim_ratings.json"
const LEARNING_RATE: float = 0.05
const KIM_STUBBORNNESS: float = 0.3
const MAX_RATINGS_HISTORY: int = 500

var preference_weights: Dictionary = {
	"organic_shapes":      0.5,
	"geometric_shapes":    0.5,
	"bright_colors":       0.5,
	"dark_tones":          0.5,
	"complex_scripts":     0.5,
	"simple_interactions": 0.5,
}

# Ratings from players: 0–100 scale
var ratings_log: Array = []              # {game_id, player_id, rating, timestamp}
var player_avg_rating: Dictionary = {}   # player_id -> running average (0.0–100.0)
var global_avg_rating: float = 50.0
var total_ratings_count: int = 0

# Performance penalty/reward tracking
var performance_log: Array = []                   # {id, type, amount, fps, timestamp}
var _applied_penalties: Dictionary = {}           # penalty_id -> {snapshot, amount}
var total_performance_score: float = 0.5          # 0..1, Kim's running perf quality


func _ready() -> void:
	load_weights()
	load_ratings()


# ─── Game rating (0–100 from players) ────────────────────────────────────────

func rate_game(game_id: String, player_id: int, rating: int) -> void:
	var clamped: int = clampi(rating, 0, 100)
	var entry: Dictionary = {
		"game_id":   game_id,
		"player_id": player_id,
		"rating":    clamped,
		"timestamp": Time.get_ticks_msec(),
	}
	ratings_log.append(entry)
	if ratings_log.size() > MAX_RATINGS_HISTORY:
		ratings_log = ratings_log.slice(ratings_log.size() - MAX_RATINGS_HISTORY, ratings_log.size())

	# Update per-player running average
	var prev_avg: float = float(player_avg_rating.get(player_id, 50.0))
	var prev_count: int = _player_rating_count(player_id)
	player_avg_rating[player_id] = (prev_avg * float(prev_count) + float(clamped)) / float(prev_count + 1)

	# Update global average
	global_avg_rating = (global_avg_rating * float(total_ratings_count) + float(clamped)) / float(total_ratings_count + 1)
	total_ratings_count += 1

	# Adjust preference_weights based on rating
	_adjust_weights_from_rating(game_id, clamped)

	save_ratings()
	save_weights()
	print("[FeedbackLearner] Rating received: game=%s player=%d rating=%d" % [game_id, player_id, clamped])


func get_player_avg_rating(player_id: int) -> float:
	return float(player_avg_rating.get(player_id, 50.0))


func get_global_avg_rating() -> float:
	return global_avg_rating


func get_rating_context() -> String:
	if total_ratings_count == 0:
		return "No ratings yet."
	return "Global average rating: %.1f/100 (%d total ratings)." % [global_avg_rating, total_ratings_count]


# ─── Object feedback (likes/dislikes on creations) ───────────────────────────

func process_feedback(
		creation_id: String,
		player_rating: float,
		creation_metadata: Dictionary) -> void:

	var clamped_rating: float = clampf(player_rating, 0.0, 1.0)
	var kim_rating: float = _evaluate_internally(creation_metadata)
	var blended: float = lerpf(clamped_rating, kim_rating, KIM_STUBBORNNESS)

	var tags: Array = creation_metadata.get("tags", [])
	for raw_tag in tags:
		var tag: String = str(raw_tag)
		if not preference_weights.has(tag):
			continue
		var current: float = float(preference_weights[tag])
		var delta: float = (blended - 0.5) * LEARNING_RATE
		preference_weights[tag] = clampf(current + delta, 0.1, 0.9)

	save_weights()
	print("[FeedbackLearner] Object feedback: id=%s tags=%s" % [creation_id, str(tags)])


func _evaluate_internally(metadata: Dictionary) -> float:
	var poly_score: float = 1.0 - clampf(float(metadata.get("poly_count", 1000)) / 10000.0, 0.0, 1.0)
	var concept_depth: int = int(metadata.get("concept_depth", 1))
	var concept_score: float = clampf(float(concept_depth) / 5.0, 0.0, 1.0)
	return (poly_score + concept_score) / 2.0


func _adjust_weights_from_rating(game_id: String, rating: int) -> void:
	# Convert 0–100 to -1..+1 learning signal
	var signal_val: float = (float(rating) - 50.0) / 50.0
	var delta: float = signal_val * LEARNING_RATE
	# Broadly adjust all weights based on overall satisfaction
	for key in preference_weights.keys():
		var current: float = float(preference_weights[key])
		preference_weights[key] = clampf(current + delta * 0.3, 0.1, 0.9)


# ─── Performance penalty / reward ─────────────────────────────────────────────

func apply_performance_penalty(penalty_id: String, amount: float, fps: float) -> void:
	# Snapshot current weights so we can roll back if disputed
	var snapshot: Dictionary = preference_weights.duplicate()
	_applied_penalties[penalty_id] = {
		"snapshot": snapshot,
		"amount":   amount,
		"fps":      fps,
	}

	# Reduce all preference weights uniformly
	for key in preference_weights.keys():
		var current: float = float(preference_weights[key])
		preference_weights[key] = clampf(current - amount, 0.1, 0.9)

	# Update running performance score
	total_performance_score = clampf(total_performance_score - amount * 0.5, 0.0, 1.0)

	performance_log.append({
		"id":        penalty_id,
		"type":      "penalty",
		"amount":    amount,
		"fps":       fps,
		"timestamp": Time.get_ticks_msec(),
	})
	save_weights()
	print("[FeedbackLearner] Performance penalty applied: id=%s fps=%.0f amount=%.2f" % [
		penalty_id, fps, amount])


func apply_performance_reward(amount: float, fps: float) -> void:
	for key in preference_weights.keys():
		var current: float = float(preference_weights[key])
		preference_weights[key] = clampf(current + amount, 0.1, 0.9)

	total_performance_score = clampf(total_performance_score + amount * 0.5, 0.0, 1.0)

	performance_log.append({
		"id":        "reward_%d" % Time.get_ticks_msec(),
		"type":      "reward",
		"amount":    amount,
		"fps":       fps,
		"timestamp": Time.get_ticks_msec(),
	})
	save_weights()
	print("[FeedbackLearner] Performance reward applied: fps=%.0f amount=%.2f" % [fps, amount])


func cancel_penalty(penalty_id: String) -> void:
	if not _applied_penalties.has(penalty_id):
		push_warning("[FeedbackLearner] Cannot cancel unknown penalty: %s" % penalty_id)
		return

	var penalty: Dictionary = _applied_penalties[penalty_id]
	var snapshot: Dictionary = penalty["snapshot"]

	# Restore weights from snapshot
	for key in snapshot.keys():
		if preference_weights.has(key):
			preference_weights[key] = float(snapshot[key])

	_applied_penalties.erase(penalty_id)

	# Restore performance score
	var amount: float = float(penalty.get("amount", 0.0))
	total_performance_score = clampf(total_performance_score + amount * 0.5, 0.0, 1.0)

	performance_log.append({
		"id":        penalty_id,
		"type":      "cancel",
		"amount":    amount,
		"timestamp": Time.get_ticks_msec(),
	})
	save_weights()
	print("[FeedbackLearner] Penalty cancelled: %s — weights restored." % penalty_id)


func get_kim_quality_estimate() -> float:
	# Returns 0..1: weighted average of current preference_weights and performance score.
	# High value = Kim has been doing quality work consistently.
	if preference_weights.is_empty():
		return total_performance_score
	var weight_avg: float = 0.0
	for key in preference_weights.keys():
		weight_avg += float(preference_weights[key])
	weight_avg /= float(preference_weights.size())
	return (weight_avg * 0.6 + total_performance_score * 0.4)


func get_performance_context() -> String:
	var recent: Array = performance_log.slice(
		max(0, performance_log.size() - 5),
		performance_log.size()
	)
	if recent.is_empty():
		return "No performance history."
	var lines: Array = []
	for entry in recent:
		lines.append("%s: type=%s fps=%.0f amount=%.2f" % [
			str(entry.get("id", "?")),
			str(entry.get("type", "?")),
			float(entry.get("fps", 0.0)),
			float(entry.get("amount", 0.0)),
		])
	return "Recent performance events:\n" + "\n".join(lines)


func _player_rating_count(player_id: int) -> int:
	var count: int = 0
	for entry in ratings_log:
		if int(entry.get("player_id", -1)) == player_id:
			count += 1
	return count


# ─── Persistence ──────────────────────────────────────────────────────────────

func save_weights() -> void:
	var file: FileAccess = FileAccess.open(WEIGHTS_SAVE_PATH, FileAccess.WRITE)
	if file == null:
		push_warning("[FeedbackLearner] Cannot write weights: %d" % FileAccess.get_open_error())
		return
	file.store_string(JSON.stringify(preference_weights))
	file.close()


func load_weights() -> void:
	if not FileAccess.file_exists(WEIGHTS_SAVE_PATH):
		return
	var file: FileAccess = FileAccess.open(WEIGHTS_SAVE_PATH, FileAccess.READ)
	if file == null:
		return
	var content: String = file.get_as_text()
	file.close()
	var parsed: Variant = JSON.parse_string(content)
	if parsed == null or parsed is not Dictionary:
		return
	for raw_key in parsed.keys():
		var key: String = str(raw_key)
		if preference_weights.has(key):
			preference_weights[key] = clampf(float(parsed[raw_key]), 0.1, 0.9)


func save_ratings() -> void:
	var data: Dictionary = {
		"log":           ratings_log,
		"player_avg":    _serialize_player_avg(),
		"global_avg":    global_avg_rating,
		"total_count":   total_ratings_count,
	}
	var file: FileAccess = FileAccess.open(RATINGS_SAVE_PATH, FileAccess.WRITE)
	if file == null:
		push_warning("[FeedbackLearner] Cannot write ratings: %d" % FileAccess.get_open_error())
		return
	file.store_string(JSON.stringify(data))
	file.close()


func load_ratings() -> void:
	if not FileAccess.file_exists(RATINGS_SAVE_PATH):
		return
	var file: FileAccess = FileAccess.open(RATINGS_SAVE_PATH, FileAccess.READ)
	if file == null:
		return
	var content: String = file.get_as_text()
	file.close()
	var parsed: Variant = JSON.parse_string(content)
	if parsed == null or parsed is not Dictionary:
		return
	if parsed.has("log") and parsed["log"] is Array:
		ratings_log = parsed["log"]
	global_avg_rating = float(parsed.get("global_avg", 50.0))
	total_ratings_count = int(parsed.get("total_count", 0))
	_deserialize_player_avg(parsed.get("player_avg", {}))


func _serialize_player_avg() -> Dictionary:
	var out: Dictionary = {}
	for raw_id in player_avg_rating.keys():
		out[str(raw_id)] = player_avg_rating[raw_id]
	return out


func _deserialize_player_avg(data: Dictionary) -> void:
	player_avg_rating.clear()
	for raw_key in data.keys():
		player_avg_rating[int(str(raw_key))] = float(data[raw_key])
