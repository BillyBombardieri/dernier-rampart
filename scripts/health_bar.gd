class_name HealthBar
extends MeshInstance3D
## Fine barre de vie au-dessus d'un zombie, toujours tournée vers la caméra. Discrète : elle
## n'apparaît qu'une fois le zombie blessé, puis s'estompe s'il n'est plus touché pendant un moment.
## La couleur passe du vert au rouge. Un seul matériau partagé : le remplissage est un paramètre
## par instance, donc des centaines de barres ne coûtent presque rien.

const SHOW_TIME := 6.0  # Secondes d'affichage après le dernier coup reçu.
const FADE_TIME := 1.0

static var _material: ShaderMaterial

var fill := 1.0
var suppressed := false  # Cachée quoi qu'il arrive (Fouisseur sous terre).
var _shown := 0.0
var _alpha := 0.0


## width en mètres ; la barre fait toujours 6 cm de haut, avec un fin liseré sombre.
func setup(width: float) -> void:
	var q := QuadMesh.new()
	q.size = Vector2(width, 0.065)
	mesh = q
	material_override = _shared_material()
	cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	visible = false
	_apply()


## À appeler quand le zombie prend des dégâts.
func set_fill(value: float) -> void:
	fill = clampf(value, 0.0, 1.0)
	_shown = SHOW_TIME
	_apply()


func hide_now() -> void:
	_shown = 0.0
	_alpha = 0.0
	visible = false


func _process(delta: float) -> void:
	if _shown <= 0.0 and _alpha <= 0.0:
		return
	_shown = maxf(0.0, _shown - delta)
	var target := 1.0 if _shown > 0.0 else 0.0
	_alpha = move_toward(_alpha, target, delta / (0.15 if target > 0.0 else FADE_TIME))
	_apply()


func _apply() -> void:
	visible = _alpha > 0.0 and not suppressed
	set_instance_shader_parameter("fill", fill)
	set_instance_shader_parameter("alpha", _alpha)


static func _shared_material() -> ShaderMaterial:
	if _material:
		return _material
	var shader := Shader.new()
	shader.code = """
shader_type spatial;
render_mode unshaded, cull_disabled, depth_draw_never, shadows_disabled, fog_disabled;

instance uniform float fill = 1.0;
instance uniform float alpha = 1.0;

void vertex() {
	// Toujours face à la caméra, en gardant la position du zombie.
	MODELVIEW_MATRIX = VIEW_MATRIX * mat4(INV_VIEW_MATRIX[0], INV_VIEW_MATRIX[1], INV_VIEW_MATRIX[2], MODEL_MATRIX[3]);
}

void fragment() {
	vec2 px = fwidth(UV);
	float edge = step(UV.x, px.x * 1.5) + step(1.0 - px.x * 1.5, UV.x) + step(UV.y, px.y * 1.5) + step(1.0 - px.y * 1.5, UV.y);
	vec3 col = mix(vec3(0.85, 0.12, 0.08), vec3(0.95, 0.75, 0.15), smoothstep(0.15, 0.5, fill));
	col = mix(col, vec3(0.35, 0.8, 0.25), smoothstep(0.5, 0.85, fill));
	bool filled = UV.x <= fill;
	ALBEDO = edge > 0.0 ? vec3(0.02) : (filled ? col : vec3(0.08, 0.06, 0.06));
	ALPHA = alpha * (edge > 0.0 ? 0.75 : (filled ? 0.95 : 0.5));
}
"""
	_material = ShaderMaterial.new()
	_material.shader = shader
	return _material
