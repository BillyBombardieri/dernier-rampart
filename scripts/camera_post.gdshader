// Post-traitement « caméra » (qualité Haute et Ultra) : ce qui fait passer une image de jeu pour
// une image filmée. Netteté qui compense le flou de l'anticrénelage temporel, vignettage
// discret, aberration chromatique à peine visible sur les bords, grain fin qui bouge.
shader_type canvas_item;

uniform sampler2D screen : hint_screen_texture, filter_linear_mipmap;
uniform float sharpen = 0.35;
uniform float vignette = 0.32;
uniform float aberration = 0.0012;
uniform float grain = 0.025;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void fragment() {
	vec2 uv = SCREEN_UV;
	vec2 px = SCREEN_PIXEL_SIZE;
	vec2 from_center = uv - 0.5;
	float edge = dot(from_center, from_center);
	// Aberration : le rouge et le bleu glissent un peu vers les bords de l'image.
	vec2 shift = from_center * aberration * edge * 4.0;
	vec3 col;
	col.r = textureLod(screen, uv + shift, 0.0).r;
	col.g = textureLod(screen, uv, 0.0).g;
	col.b = textureLod(screen, uv - shift, 0.0).b;
	// Netteté : on renforce l'écart avec les quatre voisins.
	vec3 around = textureLod(screen, uv + vec2(px.x, 0.0), 0.0).rgb + textureLod(screen, uv - vec2(px.x, 0.0), 0.0).rgb
		+ textureLod(screen, uv + vec2(0.0, px.y), 0.0).rgb + textureLod(screen, uv - vec2(0.0, px.y), 0.0).rgb;
	col = max(col + (col - around * 0.25) * sharpen, vec3(0.0));
	// Vignettage doux.
	col *= 1.0 - vignette * smoothstep(0.08, 0.55, edge);
	// Grain, plus visible dans les ombres que dans les lumières.
	float n = hash(uv * vec2(1920.0, 1080.0) + fract(TIME * 7.3) * 91.0) - 0.5;
	col += n * grain * (1.0 - dot(col, vec3(0.33)) * 0.7);
	COLOR = vec4(col, 1.0);
}
