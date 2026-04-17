extends Node

# SERVER-SIDE: evaluates FPS reports, applies penalties/rewards to Kim,
# triggers Kim's automatic dispute, and manages player optimization votes.

signal penalty_applied(penalty_id: String, player_id: int, fps: float)
signal penalty_cancelled(penalty_id: String, reason: String)
signal reward_applied(player_id: int, fps: float)
signal dispute_started(penalty_id: String)
signal vote_completed(penalty_id: String, agreed: bool, vote_count: int)
signal kim_notified(message: String)

# Configuration
# FPS threshold is now dynamic per player — no fixed constant here.
# Tolerance: allowed drop = 1/6 of effective target (same ratio as PerformanceMonitor).
const TOLERANCE_RATIO: float = 1.0 / 6.0
# Reward when FPS is ≥ effective_target (player is getting exactly what they asked for).
const REWARD_SURPLUS_RATIO: float = 1.0       # at or above target → eligible for reward
const PENALTY_AMOUNT: float = 0.08
const REWARD_AMOUNT: float = 0.04
const KIM_DISPUTE_QUALITY_THRESHOLD: float = 0.65
const VOTE_TIMEOUT_SECONDS: float = 30.0
const MIN_VOTERS_FOR_CANCELLATION: int = 1
const AGREE_RATIO_TO_CANCEL: float = 0.6

# FPS tracking per player
# Stores: avg, target_fps, monitor_hz, effective_target, penalty_threshold, last_update
var _player_fps: Dictionary = {}
var _player_reward_streak: Dictionary = {}    # player_id -> consecutive good reports

# Active penalties awaiting potential dispute
var _pending_penalties: Dictionary = {}       # penalty_id -> PenaltyData

# Active vote sessions
var _vote_sessions: Dictionary = {}           # penalty_id -> VoteSession
var _vote_timers: Dictionary = {}             # penalty_id -> float (countdown)


func _process(delta: float) -> void:
	if multiplayer.multiplayer_peer == null:
		return
	if not multiplayer.is_server():
		return
	_tick_vote_timers(delta)


# ─── FPS Report ingestion ─────────────────────────────────────────────────────

func receive_fps_report(player_id: int, avg_fps: float, target_fps: float = 60.0, monitor_hz: float = 60.0) -> void:
	if multiplayer.multiplayer_peer == null:
		return
	if not multiplayer.is_server():
		return

	# Effective target = min(player's FPS cap, their monitor refresh rate)
	var effective_target: float = minf(maxf(target_fps, 1.0), maxf(monitor_hz, 1.0))
	# Penalty threshold = effective_target * (1 - tolerance)
	var penalty_threshold: float = effective_target * (1.0 - TOLERANCE_RATIO)

	_player_fps[player_id] = {
		"avg":              avg_fps,
		"target_fps":       target_fps,
		"monitor_hz":       monitor_hz,
		"effective_target": effective_target,
		"penalty_threshold":penalty_threshold,
		"last_update":      Time.get_ticks_msec(),
	}

	print("[PerfManager] Player %d FPS report: avg=%.0f target=%.0f hz=%.0f → threshold=%.0f" % [
		player_id, avg_fps, target_fps, monitor_hz, penalty_threshold])

	if avg_fps < penalty_threshold:
		_handle_fps_penalty(player_id, avg_fps, effective_target, penalty_threshold)
	elif avg_fps >= effective_target * REWARD_SURPLUS_RATIO:
		_handle_fps_reward(player_id, avg_fps, effective_target)


# ─── Penalty handling ─────────────────────────────────────────────────────────

func _handle_fps_penalty(
		player_id: int,
		fps: float,
		effective_target: float,
		penalty_threshold: float) -> void:

	var penalty_id: String = "penalty_%d_%d" % [Time.get_ticks_msec(), player_id]

	var fl: Node = get_node_or_null("/root/FeedbackLearner")
	if fl != null and fl.has_method("apply_performance_penalty"):
		fl.call("apply_performance_penalty", penalty_id, PENALTY_AMOUNT, fps)

	var penalty_data: Dictionary = {
		"id":               penalty_id,
		"player_id":        player_id,
		"fps":              fps,
		"effective_target": effective_target,
		"threshold":        penalty_threshold,
		"timestamp":        Time.get_ticks_msec(),
		"cancelled":        false,
	}
	_pending_penalties[penalty_id] = penalty_data
	penalty_applied.emit(penalty_id, player_id, fps)

	var drop_pct: float = (1.0 - fps / effective_target) * 100.0
	var msg: String = (
		"[PERFORMANCE ALERT] Penalty applied for player %d. " +
		"Reported %.0f FPS against their target of %.0f FPS " +
		"(%.1f%% drop, threshold was %.0f FPS). " +
		"Penalty: -%.0f%% quality. " +
		"If this was an isolated spike or your scene was truly optimized, a player vote can cancel it."
	) % [player_id, fps, effective_target, drop_pct, penalty_threshold, PENALTY_AMOUNT * 100.0]
	_inject_kim_context(msg)
	kim_notified.emit(msg)
	print("[PerfManager] Penalty %s: %.0f FPS / target %.0f / threshold %.0f" % [
		penalty_id, fps, effective_target, penalty_threshold])

	var kim_quality: float = _get_kim_quality_estimate()
	if kim_quality >= KIM_DISPUTE_QUALITY_THRESHOLD:
		_start_dispute(penalty_id, player_id, fps, kim_quality)


func _handle_fps_reward(player_id: int, fps: float, effective_target: float) -> void:
	var streak: int = int(_player_reward_streak.get(player_id, 0)) + 1
	_player_reward_streak[player_id] = streak

	if streak < 2:
		return  # Need 2 consecutive good reports to earn a reward

	_player_reward_streak[player_id] = 0

	var fl: Node = get_node_or_null("/root/FeedbackLearner")
	if fl != null and fl.has_method("apply_performance_reward"):
		fl.call("apply_performance_reward", REWARD_AMOUNT, fps)

	var msg: String = (
		"[PERFORMANCE REWARD] Player %d is hitting their target of %.0f FPS. " +
		"Current: %.0f FPS. Optimization reward applied (+%.0f%%)."
	) % [player_id, effective_target, fps, REWARD_AMOUNT * 100.0]
	_inject_kim_context(msg)
	kim_notified.emit(msg)
	reward_applied.emit(player_id, fps)
	print("[PerfManager] Reward: player %d at %.0f / target %.0f" % [player_id, fps, effective_target])


# ─── Dispute (Kim challenges the penalty) ─────────────────────────────────────

func _start_dispute(penalty_id: String, player_id: int, fps: float, kim_quality: float) -> void:
	print("[PerfManager] Kim disputes penalty %s (quality=%.2f)" % [penalty_id, kim_quality])
	dispute_started.emit(penalty_id)

	var context_msg: String = (
		"[DISPUTE INITIATED] Kim's internal quality assessment (%.0f%%) exceeds the dispute threshold. " +
		"Asking player %d to evaluate optimization quality. " +
		"If the player confirms good optimization, the penalty will be cancelled."
	) % [kim_quality * 100.0, player_id]
	_inject_kim_context(context_msg)
	kim_notified.emit(context_msg)

	# Start vote session
	var penalty_data: Dictionary = _pending_penalties.get(penalty_id, {})
	var effective_target: float = float(penalty_data.get("effective_target", fps))
	var threshold: float = float(penalty_data.get("threshold", fps * (1.0 - TOLERANCE_RATIO)))
	var vote_question: String = (
		"Kim disputes a performance penalty. " +
		"Your FPS dropped to %.0f (your target: %.0f, penalty threshold: %.0f). " +
		"Kim believes the scene was well-optimized. Do you agree? (yes/no)"
	) % [fps, effective_target, threshold]

	_vote_sessions[penalty_id] = {
		"penalty_id":    penalty_id,
		"players_asked": [player_id],
		"votes":         {},   # player_id -> bool
		"question":      vote_question,
	}
	_vote_timers[penalty_id] = VOTE_TIMEOUT_SECONDS

	# Ask the affected player for their vote
	_request_player_vote(player_id, penalty_id, vote_question)


func _request_player_vote(player_id: int, penalty_id: String, question: String) -> void:
	var server: Node = _get_game_server()
	if server == null:
		return
	if not server.has_method("server_request_optimization_vote"):
		return
	server.call("server_request_optimization_vote", player_id, penalty_id, question)


# ─── Vote collection ──────────────────────────────────────────────────────────

func receive_vote(player_id: int, penalty_id: String, agrees: bool) -> void:
	if not _vote_sessions.has(penalty_id):
		push_warning("[PerfManager] Vote for unknown penalty: %s" % penalty_id)
		return

	var session: Dictionary = _vote_sessions[penalty_id]
	var votes: Dictionary = session["votes"]
	votes[player_id] = agrees
	session["votes"] = votes
	_vote_sessions[penalty_id] = session

	print("[PerfManager] Vote received: player=%d penalty=%s agrees=%s" % [
		player_id, penalty_id, str(agrees)])

	_evaluate_votes(penalty_id)


func _evaluate_votes(penalty_id: String) -> void:
	if not _vote_sessions.has(penalty_id):
		return

	var session: Dictionary = _vote_sessions[penalty_id]
	var votes: Dictionary = session["votes"]

	if votes.size() < MIN_VOTERS_FOR_CANCELLATION:
		return  # Not enough votes yet

	var agree_count: int = 0
	for v in votes.values():
		if bool(v):
			agree_count += 1

	var agree_ratio: float = float(agree_count) / float(votes.size())
	var cancelled: bool = agree_ratio >= AGREE_RATIO_TO_CANCEL

	_finalize_vote(penalty_id, cancelled, votes.size(), agree_ratio)


func _tick_vote_timers(delta: float) -> void:
	for penalty_id in _vote_timers.keys():
		_vote_timers[penalty_id] = float(_vote_timers[penalty_id]) - delta
		if float(_vote_timers[penalty_id]) <= 0.0:
			_on_vote_timeout(penalty_id)


func _on_vote_timeout(penalty_id: String) -> void:
	_vote_timers.erase(penalty_id)
	if not _vote_sessions.has(penalty_id):
		return

	var session: Dictionary = _vote_sessions[penalty_id]
	var votes: Dictionary = session["votes"]

	if votes.is_empty():
		# No votes received → penalty stands
		_finalize_vote(penalty_id, false, 0, 0.0)
		return

	var agree_count: int = 0
	for v in votes.values():
		if bool(v):
			agree_count += 1
	var agree_ratio: float = float(agree_count) / float(votes.size())
	_finalize_vote(penalty_id, agree_ratio >= AGREE_RATIO_TO_CANCEL, votes.size(), agree_ratio)


func _finalize_vote(penalty_id: String, cancelled: bool, vote_count: int, agree_ratio: float) -> void:
	_vote_sessions.erase(penalty_id)
	_vote_timers.erase(penalty_id)

	if not _pending_penalties.has(penalty_id):
		return

	var penalty_data: Dictionary = _pending_penalties[penalty_id]
	var fps: float = float(penalty_data.get("fps", 0.0))

	vote_completed.emit(penalty_id, cancelled, vote_count)

	if cancelled:
		# Cancel the penalty
		var fl: Node = get_node_or_null("/root/FeedbackLearner")
		if fl != null and fl.has_method("cancel_penalty"):
			fl.call("cancel_penalty", penalty_id)

		var cancel_msg: String = (
			"[DISPUTE WON] Players confirmed good optimization (%.0f%% agreed, %d voters). " +
			"The performance penalty for %.0f FPS has been cancelled. " +
			"Kim's quality assessment was correct."
		) % [agree_ratio * 100.0, vote_count, fps]
		_inject_kim_context(cancel_msg)
		kim_notified.emit(cancel_msg)
		penalty_cancelled.emit(penalty_id, "player_vote")
		print("[PerfManager] Penalty %s CANCELLED by player vote (%.0f%% agree)" % [
			penalty_id, agree_ratio * 100.0])
	else:
		var stand_msg: String
		if vote_count == 0:
			stand_msg = (
				"[DISPUTE UNRESOLVED] No players voted within the timeout. " +
				"The performance penalty for %.0f FPS stands."
			) % fps
		else:
			stand_msg = (
				"[DISPUTE LOST] Players did not confirm good optimization " +
				"(%.0f%% agreed, %d voters, needed %.0f%%). " +
				"The performance penalty for %.0f FPS stands. " +
				"Consider optimizing the creation or the scene."
			) % [agree_ratio * 100.0, vote_count, AGREE_RATIO_TO_CANCEL * 100.0, fps]
		_inject_kim_context(stand_msg)
		kim_notified.emit(stand_msg)
		print("[PerfManager] Penalty %s STANDS (%.0f%% agree)" % [
			penalty_id, agree_ratio * 100.0])

	_pending_penalties.erase(penalty_id)


# ─── Helpers ──────────────────────────────────────────────────────────────────

func _get_kim_quality_estimate() -> float:
	var fl: Node = get_node_or_null("/root/FeedbackLearner")
	if fl != null and fl.has_method("get_kim_quality_estimate"):
		return float(fl.call("get_kim_quality_estimate"))
	return 0.5


func _inject_kim_context(message: String) -> void:
	var kim_core: Node = get_node_or_null("/root/KimCore")
	if kim_core == null:
		return
	var personality: Node = kim_core.get_module("personality")
	if personality == null:
		personality = kim_core.activate_module("personality")
	if personality != null and personality.has_method("inject_context"):
		personality.call("inject_context", message)


func _get_game_server() -> Node:
	return get_node_or_null("/root/GameServer")


func get_player_fps(player_id: int) -> float:
	var entry: Dictionary = _player_fps.get(player_id, {})
	return float(entry.get("avg", 60.0))


func get_all_player_fps() -> Dictionary:
	var out: Dictionary = {}
	for raw_id in _player_fps.keys():
		out[int(str(raw_id))] = float(_player_fps[raw_id].get("avg", 60.0))
	return out
