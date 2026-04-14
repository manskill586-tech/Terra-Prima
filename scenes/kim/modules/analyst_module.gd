extends Node

var interactions_by_player: Dictionary = {}


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
