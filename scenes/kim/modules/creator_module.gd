extends Node

signal creation_requested(spec: Dictionary)
signal creation_completed(creation_id: String, metadata: Dictionary)


func request_creation(spec: Dictionary) -> void:
	creation_requested.emit(spec)


func complete_creation(creation_id: String, metadata: Dictionary = {}) -> void:
	creation_completed.emit(creation_id, metadata)
