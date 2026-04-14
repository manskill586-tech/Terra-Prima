extends Node

const DEFAULT_THEMES := [
	"living architecture",
	"kinetic sculpture",
	"biomorphic landscape",
	"minimalist ritual object",
]


func generate_creation_seed(creativity: float = 0.5, _perfectionism: float = 0.5) -> Dictionary:
	var clamped_creativity: float = clampf(creativity, 0.0, 1.0)
	var idx: int = randi() % DEFAULT_THEMES.size()
	var chosen_theme: String = DEFAULT_THEMES[idx]
	var scale: float = lerpf(0.8, 2.0, clamped_creativity)

	return {
		"structural_theme": chosen_theme,
		"prompt_for_llm": "Create a %s composition with scale %.2f" % [chosen_theme, scale],
		"scale": scale,
	}
