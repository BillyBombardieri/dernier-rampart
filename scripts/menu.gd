extends Control
## Menu principal : jouer (choix du niveau), améliorations permanentes, revoir le tutoriel,
## réglages, quitter, et le record.

const GAME_SCENE := "res://scenes/main.tscn"

var _main_box: VBoxContainer
var _settings: SettingsPanel
var _levels: LevelPanel
var _upgrades: UpgradePanel
var _insignes: Label


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	get_tree().paused = false
	_build_background()
	var center := Ui.center(self)
	_main_box = VBoxContainer.new()
	_main_box.add_theme_constant_override("separation", 12)
	_main_box.custom_minimum_size = Vector2(420, 0)
	center.add_child(_main_box)
	var title := Ui.label(_main_box, "DERNIER REMPART", 64, Color(0.95, 0.92, 0.88), HORIZONTAL_ALIGNMENT_CENTER)
	title.add_theme_constant_override("outline_size", 10)
	title.add_theme_color_override("font_outline_color", Color(0.25, 0.02, 0.0, 0.9))
	Ui.label(_main_box, "Tiens la base face aux zombies. 4 niveaux. Aucun renfort.", 17, Color(0.85, 0.75, 0.65), HORIZONTAL_ALIGNMENT_CENTER)
	var gap := Control.new()
	gap.custom_minimum_size = Vector2(0, 18)
	_main_box.add_child(gap)
	Ui.button(_main_box, "JOUER", func(): _open(_levels), 22, Vector2(0, 54))
	Ui.button(_main_box, "AMÉLIORATIONS", func(): _open(_upgrades), 20, Vector2(0, 48))
	Ui.button(_main_box, "TUTORIEL", func():
		Settings.set_tutorial_done(false)
		_play(0)
	, 20, Vector2(0, 48))
	Ui.button(_main_box, "RÉGLAGES", _open_settings, 20, Vector2(0, 48))
	Ui.button(_main_box, "QUITTER", func(): get_tree().quit(), 20, Vector2(0, 48))
	var record := "Record : %s points  ·  vague %d atteinte" % [_thousands(Settings.best_score), Settings.best_wave] if Settings.best_score > 0 else "Pas encore de record : à toi de jouer."
	Ui.label(_main_box, record, 16, Ui.ACCENT, HORIZONTAL_ALIGNMENT_CENTER)
	_insignes = Ui.label(_main_box, "", 15, Color(0.85, 0.85, 0.85), HORIZONTAL_ALIGNMENT_CENTER)
	_refresh_insignes()
	var footer := Ui.label(self, "Prototype · Godot 4.7 · clavier AZERTY", 13, Color(1, 1, 1, 0.4))
	footer.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_LEFT, Control.PRESET_MODE_MINSIZE, 16)
	_settings = SettingsPanel.new()
	center.add_child(_settings)
	_settings.visible = false
	_levels = LevelPanel.new()
	center.add_child(_levels)
	_levels.visible = false
	_levels.chosen.connect(_play)
	_upgrades = UpgradePanel.new()
	center.add_child(_upgrades)
	_upgrades.visible = false
	for panel in [_settings, _levels, _upgrades]:
		panel.closed.connect(func():
			_refresh_insignes()
			_main_box.visible = true
			Ui.pop_in(_main_box, center)
		)
	Ui.pop_in(_main_box, center, 0.45)


func _play(level: int) -> void:
	Game.level = level
	get_tree().change_scene_to_file(GAME_SCENE)


func _open_settings() -> void:
	_open(_settings)


func _open(panel: Control) -> void:
	_main_box.visible = false
	panel.open()


func _refresh_insignes() -> void:
	var level_text := "Niveaux débloqués : %d / %d" % [Settings.unlocked_level + 1, Levels.count()]
	_insignes.text = "%s  ·  ★ %d insignes à dépenser" % [level_text, Settings.insignes]


func _unhandled_input(event: InputEvent) -> void:
	if not (event is InputEventKey and event.pressed and event.keycode == KEY_ESCAPE):
		return
	for panel in [_settings, _levels, _upgrades]:
		if panel.visible:
			panel.close()
			get_viewport().set_input_as_handled()
			return


func _thousands(n: int) -> String:
	var s := str(n)
	var out := ""
	while s.length() > 3:
		out = " " + s.substr(s.length() - 3) + out
		s = s.substr(0, s.length() - 3)
	return s + out


## Fond animé : ciel de crépuscule, brume qui dérive, lueur rouge du portail et braises.
func _build_background() -> void:
	var bg := ColorRect.new()
	bg.set_anchors_preset(Control.PRESET_FULL_RECT)
	bg.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var shader := Shader.new()
	shader.code = """
shader_type canvas_item;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), u.x), mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y);
}
float fbm(vec2 p) {
	float v = 0.0;
	float a = 0.5;
	for (int i = 0; i < 5; i++) { v += a * noise(p); p *= 2.0; a *= 0.5; }
	return v;
}
void fragment() {
	vec2 uv = UV;
	vec3 top = vec3(0.02, 0.025, 0.05);
	vec3 horizon = vec3(0.32, 0.14, 0.09);
	vec3 col = mix(horizon, top, smoothstep(0.35, 1.0, 1.0 - uv.y));
	// Silhouette de la muraille et des barbelés à l'horizon.
	float ridge = 0.72 + 0.03 * sin(uv.x * 18.0) + 0.015 * sin(uv.x * 63.0);
	float wall = step(ridge, uv.y);
	float posts = step(0.985, fract(uv.x * 22.0)) * step(ridge - 0.06, uv.y);
	col = mix(col, vec3(0.015, 0.012, 0.012), max(wall, posts));
	// Lueur rouge du portail au loin.
	float glow = exp(-pow((uv.x - 0.62) * 6.0, 2.0)) * smoothstep(0.45, 0.75, uv.y);
	col += vec3(0.6, 0.08, 0.03) * glow * (0.75 + 0.25 * sin(TIME * 2.3));
	// Brume qui dérive.
	float fog = fbm(vec2(uv.x * 3.0 + TIME * 0.03, uv.y * 4.0 - TIME * 0.02));
	col = mix(col, vec3(0.35, 0.33, 0.36), fog * 0.22 * smoothstep(0.3, 0.9, uv.y));
	// Vignette.
	float v = smoothstep(0.95, 0.35, length(uv - vec2(0.5)));
	COLOR = vec4(col * v, 1.0);
}
"""
	var sm := ShaderMaterial.new()
	sm.shader = shader
	bg.material = sm
	add_child(bg)
	var embers := CPUParticles2D.new()
	embers.amount = 60
	embers.lifetime = 7.0
	embers.preprocess = 7.0
	embers.emission_shape = CPUParticles2D.EMISSION_SHAPE_RECTANGLE
	embers.direction = Vector2(0.2, -1)
	embers.spread = 20.0
	embers.gravity = Vector2(10, -6)
	embers.initial_velocity_min = 30.0
	embers.initial_velocity_max = 80.0
	embers.scale_amount_min = 1.5
	embers.scale_amount_max = 3.5
	var grad := Gradient.new()
	grad.set_color(0, Color(1.0, 0.6, 0.2, 0.9))
	grad.set_color(1, Color(1.0, 0.2, 0.05, 0.0))
	embers.color_ramp = grad
	add_child(embers)
	# Les braises montent du bas de l'écran, quelle que soit la taille de la fenêtre.
	var place := func():
		embers.position = Vector2(size.x * 0.5, size.y + 20.0)
		embers.emission_rect_extents = Vector2(size.x * 0.55, 10.0)
	resized.connect(place)
	place.call()
