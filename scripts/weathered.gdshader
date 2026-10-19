// Matériau « vieilli » des bâtiments : texture projetée dans le monde (triplanaire, comme
// Fx.textured), plus les traces du temps calculées à l'endroit où se trouve le mur, sans motif
// qui se répète : pied des murs sali par la boue et l'humidité, coulures sombres qui descendent,
// grandes taches qui cassent la répétition de la texture, et sur les faces du dessus de la
// neige, du sable ou de la poussière. Mouillé sous la pluie : plus sombre et plus brillant.
shader_type spatial;

uniform sampler2D albedo_tex : source_color, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D normal_tex : hint_normal, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D rough_tex : hint_default_white, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D noise_tex : hint_default_white, filter_linear_mipmap, repeat_enable;
uniform vec4 tint : source_color = vec4(1.0);
uniform float uv_scale = 0.3;
uniform float normal_strength = 1.0;
uniform float grime_height = 1.1;  // Hauteur des salissures au pied des murs (m).
uniform float grime = 0.55;
uniform vec4 grime_color : source_color = vec4(0.16, 0.13, 0.1, 1.0);
uniform float streaks = 0.5;  // Coulures verticales.
uniform float variation = 0.3;  // Grandes taches plus claires ou plus sombres.
uniform float wet = 0.0;
uniform vec4 top_color : source_color = vec4(0.85, 0.87, 0.9, 1.0);  // Neige, sable...
uniform float top_amount = 0.0;

varying vec3 w_pos;
varying vec3 w_nrm;

// Lecture triplanaire (une macro : chaque texture a ses propres réglages d'échantillonnage).
#define TRI(tex, w, p) (texture(tex, (p).xy) * (w).z + texture(tex, (p).xz) * (w).y + texture(tex, (p).zy * vec2(-1.0, 1.0)) * (w).x)

void vertex() {
	w_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	w_nrm = normalize(mat3(MODEL_MATRIX) * NORMAL);
	// Repère des normales pour la projection triplanaire (le même que Godot).
	TANGENT = normalize(vec3(0.0, 0.0, -1.0) * abs(NORMAL.x) + vec3(1.0, 0.0, 0.0) * abs(NORMAL.y) + vec3(1.0, 0.0, 0.0) * abs(NORMAL.z));
	BINORMAL = normalize(vec3(0.0, 1.0, 0.0) * abs(NORMAL.x) + vec3(0.0, 0.0, -1.0) * abs(NORMAL.y) + vec3(0.0, 1.0, 0.0) * abs(NORMAL.z));
}

void fragment() {
	vec3 w = pow(abs(w_nrm), vec3(4.0));
	w /= dot(w, vec3(1.0));
	vec3 p = w_pos * uv_scale * vec3(1.0, -1.0, 1.0);
	vec3 col = TRI(albedo_tex, w, p).rgb * tint.rgb;
	float rough = TRI(rough_tex, w, p).r;
	vec3 q = w_pos * vec3(1.0, -1.0, 1.0);
	float big = TRI(noise_tex, w, q * 0.04).r;
	float mid = TRI(noise_tex, w, q * 0.23).r;
	col *= 1.0 + (big - 0.5) * variation * 2.0;
	float side = 1.0 - smoothstep(0.5, 0.8, abs(w_nrm.y));  // 1 sur les murs, 0 dessus et dessous.
	// Pied des murs : boue et remontées d'humidité, en bord irrégulier.
	float foot = (1.0 - smoothstep(0.0, grime_height * (0.6 + mid * 0.8), w_pos.y)) * grime * side;
	col = mix(col, col * grime_color.rgb * 2.2, foot);
	rough = mix(rough, 0.95, foot * 0.5);
	// Coulures : bruit étiré à la verticale, le long du mur.
	float along = w.x > w.z ? w_pos.z : w_pos.x;
	float drip = texture(noise_tex, vec2(along * 0.7, w_pos.y * 0.025)).r;
	drip = smoothstep(0.55, 0.78, drip) * smoothstep(0.25, 0.75, mid) * streaks * side;
	col *= 1.0 - drip * 0.45;
	// Faces du dessus : neige, sable ou poussière, par plaques.
	float top = smoothstep(0.55, 0.85, w_nrm.y) * top_amount * smoothstep(0.3, 0.6, mid + big * 0.4);
	col = mix(col, top_color.rgb, top);
	rough = mix(rough, 0.9, top);
	// Pluie : le matériau mouillé fonce et brille.
	col *= 1.0 - wet * 0.35 * (1.0 - top);
	rough = mix(rough, 0.22, wet * 0.75 * (1.0 - top));
	ALBEDO = col;
	ROUGHNESS = rough;
	NORMAL_MAP = TRI(normal_tex, w, p).rgb;
	NORMAL_MAP_DEPTH = normal_strength * (1.0 - top * 0.7);
}
