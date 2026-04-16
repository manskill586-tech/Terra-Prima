extends Node

const DOMAINS: Dictionary = {
	"space":    ["звезда", "туман", "антигравитация", "кристалл", "пульсар"],
	"biology":  ["мицелий", "хитин", "биолюминесценция", "споры", "симбиоз"],
	"geometry": ["тессеракт", "фрактал", "торсион", "спираль", "инверсия"],
	"emotion":  ["меланхолия", "экстаз", "тревога", "ностальгия", "удивление"],
}

const TOPIC_TO_DOMAIN: Dictionary = {
	"creation": "geometry",
	"ideas":    "geometry",
	"nature":   "biology",
	"animals":  "biology",
	"social":   "emotion",
	"danger":   "emotion",
	"movement": "space",
	"general":  "space",
	"mechanics":"geometry",
}


func generate_creation_seed(creativity: float = 0.5, perfectionism: float = 0.5) -> Dictionary:
	var clamped_creativity: float = clampf(creativity, 0.0, 1.0)
	var clamped_perf: float = clampf(perfectionism, 0.0, 1.0)

	var domain_keys: Array = DOMAINS.keys()
	domain_keys.shuffle()
	var domain_count: int = maxi(1, roundi(clamped_creativity * domain_keys.size()))
	var selected_domains: Array = domain_keys.slice(0, domain_count)

	var chain_length: int = roundi(2.0 + clamped_creativity * 4.0)
	var chain: Array = _build_chain(selected_domains, chain_length)
	var palette: Array = _generate_palette(chain)
	var structural_theme: String = selected_domains[0]
	var scale: float = lerpf(0.8, 2.0, clamped_creativity)
	var style: String = "изящно и лаконично" if clamped_perf > 0.7 else "дерзко и экспериментально"

	return {
		"structural_theme": structural_theme,
		"concept": chain,
		"scale": scale,
		"color_palette": palette,
		"target_polygon_budget": roundi(lerpf(500.0, 5000.0, 1.0 - clamped_perf)),
		"prompt_for_llm": "Create a %s composition with scale %.2f. Style: %s. Concepts: %s" % [
			structural_theme, scale, style, ", ".join(chain)
		],
	}


func generate_creation_seed_from_profile(
		profile: Dictionary,
		creativity: float = 0.5,
		perfectionism: float = 0.5) -> Dictionary:
	var topic_weights: Dictionary = profile.get("topic_weights", {})

	var weighted_pool: Array = []
	for raw_topic in topic_weights.keys():
		var topic: String = str(raw_topic)
		var weight: float = float(topic_weights[raw_topic])
		var domain: String = TOPIC_TO_DOMAIN.get(topic, "geometry")
		var slots: int = maxi(1, roundi(weight * 5.0))
		for _i in range(slots):
			weighted_pool.append(domain)

	if weighted_pool.is_empty():
		return generate_creation_seed(creativity, perfectionism)

	weighted_pool.shuffle()

	var clamped_creativity: float = clampf(creativity, 0.0, 1.0)
	var clamped_perf: float = clampf(perfectionism, 0.0, 1.0)
	var max_domains: int = maxi(1, roundi(clamped_creativity * DOMAINS.size()))

	var selected_domains: Array = []
	var seen: Dictionary = {}
	for d in weighted_pool:
		if seen.has(d):
			continue
		if not DOMAINS.has(d):
			continue
		seen[d] = true
		selected_domains.append(d)
		if selected_domains.size() >= max_domains:
			break

	if selected_domains.is_empty():
		selected_domains = [DOMAINS.keys()[0]]

	var chain_length: int = roundi(2.0 + clamped_creativity * 4.0)
	var chain: Array = _build_chain(selected_domains, chain_length)
	var palette: Array = _generate_palette(chain)
	var structural_theme: String = selected_domains[0]
	var scale: float = lerpf(0.8, 2.0, clamped_creativity)
	var style: String = "изящно и лаконично" if clamped_perf > 0.7 else "дерзко и экспериментально"

	return {
		"structural_theme": structural_theme,
		"concept": chain,
		"scale": scale,
		"color_palette": palette,
		"target_polygon_budget": roundi(lerpf(500.0, 5000.0, 1.0 - clamped_perf)),
		"prompt_for_llm": "Create a %s composition with scale %.2f. Style: %s. Concepts: %s" % [
			structural_theme, scale, style, ", ".join(chain)
		],
	}


func _build_chain(domains: Array, length: int) -> Array:
	var chain: Array = []
	for domain in domains:
		if not DOMAINS.has(domain):
			continue
		var words: Array = (DOMAINS[domain] as Array).duplicate()
		words.shuffle()
		var per_domain: int = ceili(float(length) / float(domains.size()))
		chain.append_array(words.slice(0, per_domain))
	chain.shuffle()
	return chain.slice(0, length)


func _generate_palette(chain: Array) -> Array:
	var h: int = hash(str(chain)) % 360
	if h < 0:
		h = -h
	return [
		Color.from_hsv(float(h) / 360.0, 0.7, 0.9),
		Color.from_hsv(float((h + 120) % 360) / 360.0, 0.5, 0.8),
		Color.from_hsv(float((h + 240) % 360) / 360.0, 0.3, 0.7),
	]
