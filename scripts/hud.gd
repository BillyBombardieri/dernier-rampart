class_name Hud
extends CanvasLayer
## Interface moderne : minimap, vague et Cœur, vie, munitions, ressources, viseur dynamique,
## indicateurs du portail et du Cœur, notifications, objectif du tutoriel, aide, implants et fin.

const PHASE_NAMES := {
	"prep": "PRÉPARATION",
	"assault": "ASSAUT",
	"implant": "CHOIX D'IMPLANT",
	"harvest": "RÉCOLTE",
}
const ACCENT := Color(1.0, 0.72, 0.25)
const DANGER := Color(1.0, 0.28, 0.22)
const CORE_BLUE := Color(0.35, 0.65, 1.0)
const PANEL_BG := Color(0.04, 0.05, 0.07, 0.72)
const HELP_TEXT := """[b]DÉPLACEMENT[/b]
ZQSD  bouger      Espace  sauter      Maj  courir

[b]COMBAT[/b]
Clic gauche  tirer      Clic droit  viser
& / é  changer d'arme      R  recharger
F  marquer un zombie (les tours le ciblent, +25 %)
L  lampe torche

[b]CONSTRUCTION[/b]
E sur un ancrage  Mitrailleuse      C  Cryo
E sur une tour  améliorer      X  allumer / éteindre
Maintenir E sur un relais ou le Cœur  réparer

[b]PARTIE[/b]
Entrée  lancer la vague      H  afficher / masquer l'aide
P  passer le tutoriel      Échap  libérer la souris

[color=#7fd8ff]COMBO : la Cryo gèle, un tir de pistolet lourd BRISE (x3)[/color]"""

var wave_manager: WaveManager

var _wave_label: Label
var _phase_label: Label
var _phase_bar: ProgressBar
var _core_bar: ProgressBar
var _core_label: Label
var _scrap_label: Label
var _energy_bar: ProgressBar
var _energy_label: Label
var _hp_bar: ProgressBar
var _hp_label: Label
var _implants_label: Label
var _weapon_label: Label
var _ammo_label: Label
var _mag_label: Label
var _reload_bar: ProgressBar
var _slots_label: RichTextLabel
var _prompt_panel: PanelContainer
var _prompt_label: RichTextLabel
var _toasts: VBoxContainer
var _objective_panel: PanelContainer
var _objective_title: Label
var _objective_text: RichTextLabel
var _help_panel: PanelContainer
var _implant_panel: PanelContainer
var _implant_box: VBoxContainer
var _end_panel: PanelContainer
var _end_label: Label
var _vignette: ColorRect
var _crosshair: Crosshair
var _indicators: Indicators
var _hurt := 0.0


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	_build_vignette()
	_indicators = Indicators.new()
	_indicators.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(_indicators)
	_crosshair = Crosshair.new()
	_crosshair.set_anchors_preset(Control.PRESET_CENTER)
	add_child(_crosshair)
	_build_top_left()
	_build_top_center()
	_build_objective()
	_build_bottom_left()
	_build_bottom_right()
	_build_prompt()
	_build_help()
	_build_implant_panel()
	_build_end_panel()
	Game.message.connect(_toast)
	Game.ended.connect(_on_ended)
	Game.hit_marker.connect(_crosshair.hit)
	Game.player_hurt.connect(func(amount: float): _hurt = min(1.0, _hurt + amount / 25.0))


# ------------------------------------------------------------------ construction

func _panel(parent: Node = null) -> PanelContainer:
	var p := PanelContainer.new()
	var sb := StyleBoxFlat.new()
	sb.bg_color = PANEL_BG
	sb.set_corner_radius_all(8)
	sb.content_margin_left = 14
	sb.content_margin_right = 14
	sb.content_margin_top = 8
	sb.content_margin_bottom = 8
	sb.border_color = Color(1, 1, 1, 0.08)
	sb.set_border_width_all(1)
	p.add_theme_stylebox_override("panel", sb)
	p.mouse_filter = Control.MOUSE_FILTER_IGNORE
	(parent if parent else self).add_child(p)
	return p


func _text(parent: Node, size: int, color := Color.WHITE, align := HORIZONTAL_ALIGNMENT_LEFT) -> Label:
	var l := Label.new()
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_color", color)
	l.add_theme_constant_override("outline_size", 4)
	l.add_theme_color_override("font_outline_color", Color(0, 0, 0, 0.8))
	l.horizontal_alignment = align
	l.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(l)
	return l


func _rich(parent: Node, size: int) -> RichTextLabel:
	var r := RichTextLabel.new()
	r.bbcode_enabled = true
	r.fit_content = true
	r.scroll_active = false
	r.autowrap_mode = TextServer.AUTOWRAP_OFF
	r.add_theme_font_size_override("normal_font_size", size)
	r.add_theme_font_size_override("bold_font_size", size)
	r.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(r)
	return r


func _bar(parent: Node, color: Color, height := 8.0) -> ProgressBar:
	var b := ProgressBar.new()
	b.show_percentage = false
	b.custom_minimum_size = Vector2(0, height)
	var bg := StyleBoxFlat.new()
	bg.bg_color = Color(1, 1, 1, 0.1)
	bg.set_corner_radius_all(int(height / 2))
	var fill := StyleBoxFlat.new()
	fill.bg_color = color
	fill.set_corner_radius_all(int(height / 2))
	b.add_theme_stylebox_override("background", bg)
	b.add_theme_stylebox_override("fill", fill)
	b.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(b)
	return b


## Place un élément par rapport à un coin ou un bord de l'écran.
func _anchor(c: Control, preset: int, offset: Vector2) -> void:
	c.set_anchors_preset(preset)
	c.position = Vector2.ZERO
	c.offset_left = offset.x
	c.offset_top = offset.y
	c.offset_right = offset.x
	c.offset_bottom = offset.y


func _build_vignette() -> void:
	_vignette = ColorRect.new()
	_vignette.set_anchors_preset(Control.PRESET_FULL_RECT)
	_vignette.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var shader := Shader.new()
	shader.code = """
shader_type canvas_item;
uniform float hurt = 0.0;
void fragment() {
	vec2 d = UV - vec2(0.5);
	float edge = smoothstep(0.25, 0.75, length(d) * 1.2);
	vec3 col = mix(vec3(0.0), vec3(0.5, 0.0, 0.0), hurt);
	COLOR = vec4(col, clamp(edge * 0.5 + edge * hurt * 0.8, 0.0, 1.0));
}
"""
	var sm := ShaderMaterial.new()
	sm.shader = shader
	_vignette.material = sm
	add_child(_vignette)


func _build_top_left() -> void:
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 8)
	box.position = Vector2(20, 20)
	box.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(box)
	box.add_child(Minimap.new())
	var res := _panel(box)
	res.custom_minimum_size = Vector2(190, 0)
	var v := VBoxContainer.new()
	res.add_child(v)
	_scrap_label = _text(v, 18, ACCENT)
	_energy_label = _text(v, 14, Color(0.8, 0.9, 1.0))
	_energy_bar = _bar(v, Color(0.45, 0.8, 1.0), 6)


func _build_top_center() -> void:
	var p := _panel()
	p.custom_minimum_size = Vector2(380, 0)
	_anchor(p, Control.PRESET_CENTER_TOP, Vector2(-190, 16))
	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 4)
	p.add_child(v)
	var row := HBoxContainer.new()
	v.add_child(row)
	_wave_label = _text(row, 24, Color.WHITE)
	_wave_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_phase_label = _text(row, 16, ACCENT, HORIZONTAL_ALIGNMENT_RIGHT)
	_phase_label.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	_phase_bar = _bar(v, ACCENT, 5)
	var core_row := HBoxContainer.new()
	core_row.add_theme_constant_override("separation", 8)
	v.add_child(core_row)
	var tag := _text(core_row, 13, CORE_BLUE)
	tag.text = "CŒUR"
	_core_bar = _bar(core_row, CORE_BLUE, 8)
	_core_bar.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_core_bar.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	_core_label = _text(core_row, 13, Color(0.85, 0.9, 1.0))
	_toasts = VBoxContainer.new()
	_toasts.add_theme_constant_override("separation", 6)
	_toasts.custom_minimum_size = Vector2(560, 0)
	_toasts.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_anchor(_toasts, Control.PRESET_CENTER_TOP, Vector2(-280, 124))
	add_child(_toasts)


func _build_objective() -> void:
	_objective_panel = _panel()
	_objective_panel.custom_minimum_size = Vector2(340, 0)
	_anchor(_objective_panel, Control.PRESET_TOP_RIGHT, Vector2(-360, 20))
	var v := VBoxContainer.new()
	_objective_panel.add_child(v)
	_objective_title = _text(v, 14, ACCENT)
	_objective_text = _rich(v, 17)
	_objective_text.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_objective_text.custom_minimum_size = Vector2(312, 0)
	_objective_panel.visible = false
	var help_hint := _text(self, 13, Color(1, 1, 1, 0.55))
	help_hint.text = "[H] aide"
	_anchor(help_hint, Control.PRESET_BOTTOM_RIGHT, Vector2(-72, -26))


func _build_bottom_left() -> void:
	var p := _panel()
	p.custom_minimum_size = Vector2(300, 0)
	_anchor(p, Control.PRESET_BOTTOM_LEFT, Vector2(20, -100))
	var v := VBoxContainer.new()
	p.add_child(v)
	var row := HBoxContainer.new()
	v.add_child(row)
	var plus := _text(row, 22, Color(0.95, 0.95, 0.95))
	plus.text = "✚ "
	_hp_label = _text(row, 22, Color.WHITE)
	_hp_bar = _bar(v, Color(0.35, 0.9, 0.45), 10)
	_implants_label = _text(v, 13, Color(0.8, 0.8, 0.8))


func _build_bottom_right() -> void:
	var p := _panel()
	p.custom_minimum_size = Vector2(280, 0)
	_anchor(p, Control.PRESET_BOTTOM_RIGHT, Vector2(-300, -150))
	var v := VBoxContainer.new()
	p.add_child(v)
	_weapon_label = _text(v, 15, Color(0.85, 0.85, 0.85), HORIZONTAL_ALIGNMENT_RIGHT)
	var row := HBoxContainer.new()
	row.alignment = BoxContainer.ALIGNMENT_END
	v.add_child(row)
	_ammo_label = _text(row, 44, Color.WHITE, HORIZONTAL_ALIGNMENT_RIGHT)
	_mag_label = _text(row, 20, Color(0.7, 0.7, 0.7))
	_mag_label.size_flags_vertical = Control.SIZE_SHRINK_END
	_reload_bar = _bar(v, ACCENT, 4)
	_slots_label = _rich(v, 13)
	_slots_label.custom_minimum_size = Vector2(250, 0)


func _build_prompt() -> void:
	_prompt_panel = _panel()
	_prompt_panel.custom_minimum_size = Vector2(420, 0)
	_anchor(_prompt_panel, Control.PRESET_CENTER_BOTTOM, Vector2(-210, -200))
	_prompt_label = _rich(_prompt_panel, 17)
	_prompt_label.custom_minimum_size = Vector2(392, 0)
	_prompt_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_prompt_panel.visible = false


func _build_help() -> void:
	_help_panel = _panel()
	_anchor(_help_panel, Control.PRESET_CENTER, Vector2(-270, -230))
	var r := _rich(_help_panel, 16)
	r.custom_minimum_size = Vector2(510, 0)
	r.text = HELP_TEXT
	_help_panel.visible = false


func _build_implant_panel() -> void:
	_implant_panel = _panel()
	_anchor(_implant_panel, Control.PRESET_CENTER, Vector2(-250, -190))
	_implant_panel.mouse_filter = Control.MOUSE_FILTER_STOP
	_implant_panel.visible = false
	_implant_box = VBoxContainer.new()
	_implant_box.add_theme_constant_override("separation", 12)
	_implant_panel.add_child(_implant_box)
	var title := _text(_implant_box, 24, ACCENT, HORIZONTAL_ALIGNMENT_CENTER)
	title.text = "SIÈGE REPOUSSÉ"
	var sub := _text(_implant_box, 15, Color(0.85, 0.85, 0.85), HORIZONTAL_ALIGNMENT_CENTER)
	sub.text = "Choisis un implant : chacun a un bonus ET un malus"


func _build_end_panel() -> void:
	_end_panel = _panel()
	_end_panel.custom_minimum_size = Vector2(480, 0)
	_anchor(_end_panel, Control.PRESET_CENTER, Vector2(-240, -110))
	_end_panel.visible = false
	_end_label = _text(_end_panel, 30, Color.WHITE, HORIZONTAL_ALIGNMENT_CENTER)


# ------------------------------------------------------------------ mise à jour

func _process(delta: float) -> void:
	var p := Game.player as Player
	var core := Game.core as Structure
	if Input.is_action_just_pressed("help"):
		_help_panel.visible = not _help_panel.visible

	var siege := Game.wave % WaveManager.SIEGE_EVERY == 0
	_wave_label.text = "VAGUE %d / %d%s" % [Game.wave, Game.LAST_WAVE, "  ☠ SIÈGE" if siege else ""]
	_wave_label.add_theme_color_override("font_color", DANGER if siege else Color.WHITE)
	var phase_name: String = PHASE_NAMES.get(Game.phase, Game.phase)
	if wave_manager and Game.phase in ["prep", "harvest"]:
		var hold := " ⏸" if Game.tutorial_hold else ""
		_phase_label.text = "%s  %d s%s" % [phase_name, int(ceil(Game.phase_time)), hold]
		_phase_bar.max_value = wave_manager.phase_duration()
		_phase_bar.value = Game.phase_time
	elif wave_manager and Game.phase == "assault":
		var left := wave_manager.remaining()
		_phase_label.text = "%s  ☠ %d" % [phase_name, left]
		_phase_bar.max_value = max(1, wave_manager.wave_total)
		_phase_bar.value = left
	else:
		_phase_label.text = phase_name
	_phase_label.add_theme_color_override("font_color", DANGER if Game.phase == "assault" else ACCENT)
	if core:
		_core_bar.max_value = core.max_hp
		_core_bar.value = core.hp
		_core_label.text = "%d" % int(core.hp)

	_scrap_label.text = "⚙ %d  ferraille" % Game.scrap
	_energy_label.text = "⚡ Énergie  %d / %d" % [Game.energy_used, Game.energy_cap()]
	_energy_bar.max_value = Game.energy_cap()
	_energy_bar.value = Game.energy_used

	if p:
		_hp_label.text = "%d" % int(max(p.hp, 0))
		_hp_bar.max_value = p.max_hp
		_hp_bar.value = max(p.hp, 0)
		var ratio := p.hp / p.max_hp
		var hp_col := Color(0.35, 0.9, 0.45) if ratio > 0.5 else (ACCENT if ratio > 0.25 else DANGER)
		(_hp_bar.get_theme_stylebox("fill") as StyleBoxFlat).bg_color = hp_col
		var names: Array[String] = []
		for id in Game.implants:
			names.append(Game.IMPLANTS[id]["name"])
		_implants_label.text = "Implants : " + (", ".join(names) if not names.is_empty() else "aucun")
		var w: Dictionary = Player.WEAPONS[p.weapon]
		_weapon_label.text = String(w["name"]).to_upper()
		if p.reloading > 0.0:
			_ammo_label.text = "--"
			_reload_bar.visible = true
			_reload_bar.max_value = p.reload_time(p.weapon)
			_reload_bar.value = _reload_bar.max_value - p.reloading
		else:
			_ammo_label.text = "%d" % p.ammo[p.weapon]
			_reload_bar.visible = false
		var low: bool = p.ammo[p.weapon] <= int(w["mag"]) / 4
		_ammo_label.add_theme_color_override("font_color", DANGER if low and p.reloading <= 0.0 else Color.WHITE)
		_mag_label.text = " / %d" % w["mag"]
		_slots_label.text = "[right]%s   %s[/right]" % [_slot("&", "Pistolet", p.weapon == "pistol"), _slot("é", "Fusil", p.weapon == "rifle")]
		_crosshair.spread = p.spread_amount()
		_crosshair.aiming = p.aiming
		_crosshair.visible = p.alive

	if Game.hint != "":
		_prompt_label.text = "[center]%s[/center]" % _format_keys(Game.hint)
		_prompt_panel.visible = true
	else:
		_prompt_panel.visible = false

	_hurt = move_toward(_hurt, 0.0, delta * 0.8)
	(_vignette.material as ShaderMaterial).set_shader_parameter("hurt", _hurt)


func _slot(key: String, label: String, active: bool) -> String:
	if active:
		return "[color=#ffb840][b][%s] %s[/b][/color]" % [key, label]
	return "[color=#8a8a8a][%s] %s[/color]" % [key, label]


## Met en forme les touches ("E : ...") façon touche de clavier.
func _format_keys(text: String) -> String:
	var re := RegEx.new()
	re.compile("(^|\\s)(Maintenir E|[A-ZÉ]) : ")
	return re.sub(text, "$1[color=#ffb840][b][$2][/b][/color] ", true)


## Objectif affiché en haut à droite (utilisé par le tutoriel). Texte vide = masqué.
func set_objective(title: String, text: String) -> void:
	_objective_panel.visible = text != ""
	_objective_title.text = title
	_objective_text.text = text


func _toast(text: String) -> void:
	var p := _panel(_toasts)
	var l := _text(p, 17, Color(1.0, 0.92, 0.75), HORIZONTAL_ALIGNMENT_CENTER)
	l.text = text
	l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	l.custom_minimum_size = Vector2(530, 0)
	while _toasts.get_child_count() > 3:
		_toasts.get_child(0).free()
	p.modulate.a = 0.0
	var tween := p.create_tween()
	tween.tween_property(p, "modulate:a", 1.0, 0.2)
	tween.tween_interval(3.5)
	tween.tween_property(p, "modulate:a", 0.0, 0.5)
	tween.tween_callback(p.queue_free)


func show_implants(options: Array) -> void:
	for c in _implant_box.get_children():
		if c is Button:
			c.queue_free()
	for id in options:
		var data: Dictionary = Game.IMPLANTS[id]
		var b := Button.new()
		b.text = "%s\n+ %s\n- %s" % [data["name"], data["plus"], data["minus"]]
		b.custom_minimum_size = Vector2(470, 86)
		b.add_theme_font_size_override("font_size", 16)
		b.pressed.connect(_pick_implant.bind(id))
		_implant_box.add_child(b)
	_implant_panel.visible = true
	get_tree().paused = true
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func _pick_implant(id: String) -> void:
	_implant_panel.visible = false
	get_tree().paused = false
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	wave_manager.implant_chosen(id)


func _on_ended(victory: bool) -> void:
	_end_label.text = ("VICTOIRE !\nTu as tenu les %d vagues." % Game.LAST_WAVE) if victory else ("LE CŒUR EST TOMBÉ\nTu as tenu jusqu'à la vague %d." % Game.wave)
	_end_label.text += "\n\n[Entrée] recommencer"
	_end_label.add_theme_color_override("font_color", ACCENT if victory else DANGER)
	_end_panel.visible = true
	get_tree().paused = true
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func _unhandled_input(event: InputEvent) -> void:
	if _end_panel.visible and event.is_action_pressed("skip_phase"):
		get_tree().paused = false
		get_tree().reload_current_scene()


# ------------------------------------------------------------------ viseur

class Crosshair extends Control:
	var spread := 0.0
	var aiming := false
	var _hit := 0.0
	var _kill := false

	func _ready() -> void:
		mouse_filter = Control.MOUSE_FILTER_IGNORE

	func hit(kill: bool) -> void:
		_hit = 0.15
		_kill = kill

	func _process(delta: float) -> void:
		_hit = max(0.0, _hit - delta)
		queue_redraw()

	func _draw() -> void:
		var col := Color(1, 1, 1, 0.9)
		var shadow := Color(0, 0, 0, 0.5)
		if aiming:
			draw_circle(Vector2.ZERO, 2.5, shadow)
			draw_circle(Vector2.ZERO, 1.5, col)
		else:
			var gap := 6.0 + spread * 420.0
			for d in [Vector2.UP, Vector2.DOWN, Vector2.LEFT, Vector2.RIGHT]:
				draw_line(d * gap, d * (gap + 9.0), shadow, 4.0)
				draw_line(d * gap, d * (gap + 9.0), col, 2.0)
			draw_circle(Vector2.ZERO, 1.2, col)
		if _hit > 0.0:
			var hc := Color(1.0, 0.25, 0.2) if _kill else Color.WHITE
			for d in [Vector2(1, 1), Vector2(-1, 1), Vector2(1, -1), Vector2(-1, -1)]:
				draw_line(d.normalized() * 8.0, d.normalized() * 16.0, hc, 2.5)


# ------------------------------------------------------------------ indicateurs portail / Cœur

class Indicators extends Control:
	var _t := 0.0

	func _ready() -> void:
		mouse_filter = Control.MOUSE_FILTER_IGNORE

	func _process(delta: float) -> void:
		_t += delta
		queue_redraw()

	func _draw() -> void:
		var cam := get_viewport().get_camera_3d()
		if cam == null or Game.main == null or Game.player == null:
			return
		var portal: Vector3 = Game.main.path_points[0] + Vector3(0, 4, 0)
		var pulse := 0.65 + 0.35 * sin(_t * (7.0 if Game.phase == "assault" else 3.0))
		_marker(cam, portal, "PORTAIL", Color(1.0, 0.3, 0.2, pulse), true)
		if Game.core:
			_marker(cam, Game.core.global_position + Vector3(0, 5, 0), "CŒUR", Color(0.4, 0.7, 1.0, 0.85), false)

	func _marker(cam: Camera3D, world: Vector3, label: String, col: Color, skull: bool) -> void:
		var vp := get_viewport_rect().size
		var dist := Game.player.global_position.distance_to(world)
		var margin := 40.0
		if vp.x < margin * 4.0 or vp.y < margin * 4.0:
			return
		var behind := cam.is_position_behind(world)
		var pos := cam.unproject_position(world)
		var on_screen := not behind and Rect2(Vector2(margin, margin), vp - Vector2(margin, margin) * 2).has_point(pos)
		var font := get_theme_default_font()
		var text := "%s  %d m" % [label, int(dist)]
		if dist < 12.0:
			return
		if on_screen:
			_icon(pos, col, skull)
			draw_string(font, pos + Vector2(-60, -18), text, HORIZONTAL_ALIGNMENT_CENTER, 120, 13, col)
			return
		# Hors de l'écran : flèche collée au bord, dans la direction de la cible.
		var center := vp * 0.5
		var dir := pos - center
		if behind:
			# Derrière toi : la flèche part vers le bas de l'écran.
			dir = -dir
			dir.y = absf(dir.y) + 0.3
		if dir.length() < 1.0:
			dir = Vector2.DOWN
		dir = dir.normalized()
		var half := center - Vector2(margin + 20, margin + 20)
		var k: float = min(half.x / max(absf(dir.x), 0.001), half.y / max(absf(dir.y), 0.001))
		var edge := center + dir * k
		var tip := edge + dir * 18.0
		var side := Vector2(-dir.y, dir.x) * 9.0
		draw_colored_polygon(PackedVector2Array([tip, edge + side, edge - side]), col)
		var icon_pos := edge - dir * 16.0
		_icon(icon_pos, col, skull)
		var label_pos := icon_pos - dir * Vector2(62.0, 30.0) + Vector2(-60, 5)
		draw_string(font, label_pos, text, HORIZONTAL_ALIGNMENT_CENTER, 120, 12, col)

	func _icon(p: Vector2, col: Color, skull: bool) -> void:
		draw_circle(p, 11.0, Color(0, 0, 0, 0.55))
		draw_arc(p, 11.0, 0, TAU, 24, col, 2.0, true)
		var font := get_theme_default_font()
		draw_string(font, p + Vector2(-6, 6), "☠" if skull else "◆", HORIZONTAL_ALIGNMENT_LEFT, -1, 14, col)
