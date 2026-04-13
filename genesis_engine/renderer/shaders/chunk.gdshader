shader_type spatial;
render_mode blend_mix, cull_back;

varying vec3 world_pos;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float voronoi(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	float min_d = 8.0;
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec2 g = vec2(x, y);
			vec2 o = vec2(hash(i + g), hash(i + g + 19.19));
			vec2 r = g + o - f;
			float d = dot(r, r);
			min_d = min(min_d, d);
		}
	}
	return sqrt(min_d);
}

void vertex() {
	world_pos = (WORLD_MATRIX * vec4(VERTEX, 1.0)).xyz;
	float pressure = INSTANCE_CUSTOM.a;
	float noise = hash(world_pos.xz * 0.25);
	float deform = (pressure - 0.5) * 0.15 + (noise - 0.5) * 0.05;
	VERTEX += NORMAL * deform;
	world_pos = (WORLD_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	vec4 base = COLOR;
	vec4 custom = INSTANCE_CUSTOM;
	float opacity = custom.r;
	float roughness = custom.g;
	float metallic = custom.b;
	float emissive = base.a;

	float v = voronoi(world_pos.xz * 0.2);
	float edge = smoothstep(0.02, 0.08, v);
	vec3 albedo = base.rgb * edge;

	ALBEDO = albedo;
	ALPHA = clamp(opacity, 0.0, 1.0);
	ROUGHNESS = clamp(roughness, 0.0, 1.0);
	METALLIC = clamp(metallic, 0.0, 1.0);
	EMISSION = albedo * emissive;
}
