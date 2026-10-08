class_name Hud
extends CanvasLayer
## Interface moderne : minimap, vague, Cœur et barrières, vie, munitions, ressources, score,
## gadgets, choix des tours, viseur dynamique, indicateurs du portail et du Cœur, notifications,
## objectif du tutoriel, aide, implants et fin de partie.

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

var wave_manager: WaveManager

var _wave_label: Label
var _phase_label: Label
var _phase_bar: ProgressBar
var _core_bar: ProgressBar
var _core_label: Label
var _scrap_label: Label
var _score_label: Label
var _help_hint: Label
var _energy_bar: ProgressBar
var _energy_label: Label
var _hp_bar: ProgressBar
var _hp_label: Label
var _implants_label: Label
var _gadgets_label: RichTextLabel
var _weapon_label: Label
var _ammo_type_label: Label
var _ammo_label: Label
var _mag_label: Label
var _reload_bar: ProgressBar
var _slots_label: RichTextLabel
var _prompt_panel: PanelContainer
var _prompt_label: RichTextLabel
var _build_bar: HBoxContainer
var _build_cards: Array[PanelContainer] = []
var _gate_rows: Array = []  # [barrière, barre, texte]
var _toasts: VBoxContainer
var _objective_panel: PanelContainer
var _objective_title: Label
var _objective_text: RichTextLabel
var _help_panel: PanelContainer
var _help_text: RichTextLabel
var _implant_panel: PanelContainer
var _implant_box: VBoxContainer
var _end_panel: PanelContainer
var _end_label: Label
var _end_score: Label
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
	_build_build_bar()
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
	_score_label = _text(v, 14, Color(0.95, 0.95, 0.95))


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
	# Une ligne par barrière d'avant-poste : on voit tout de suite laquelle est attaquée.
	for node in get_tree().get_nodes_in_group("gates"):
		var b := node as Barrier
		var gate_row := HBoxContainer.new()
		gate_row.add_theme_constant_override("separation", 8)
		v.add_child(gate_row)
		var gate_tag := _text(gate_row, 12, Color(0.85, 0.85, 0.85))
		gate_tag.text = b.display_name.replace("Barrière de l'", "").replace("Barrière de la ", "").to_upper()
		gate_tag.custom_minimum_size = Vector2(96, 0)
		var bar := _bar(gate_row, Color(0.85, 0.85, 0.8), 6)
		bar.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		bar.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		var value := _text(gate_row, 12, Color(0.85, 0.85, 0.85))
		_gate_rows.append([b, bar, value])
	_toasts = VBoxContainer.new()
	_toasts.add_theme_constant_override("separation", 6)
	_toasts.custom_minimum_size = Vector2(560, 0)
	_toasts.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_anchor(_toasts, Control.PRESET_CENTER_TOP, Vector2(-280, 124 + 22 * _gate_rows.size()))
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
	_help_hint = _text(self, 13, Color(1, 1, 1, 0.55))
	_anchor(_help_hint, Control.PRESET_BOTTOM_RIGHT, Vector2(-72, -26))


func _build_bottom_left() -> void:
	var p := _panel()
	p.custom_minimum_size = Vector2(300, 0)
	_anchor(p, Control.PRESET_BOTTOM_LEFT, Vector2(20, -128))
	var v := VBoxContainer.new()
	p.add_child(v)
	var row := HBoxContainer.new()
	v.add_child(row)
	var plus := _text(row, 22, Color(0.95, 0.95, 0.95))
	plus.text = "✚ "
	_hp_label = _text(row, 22, Color.WHITE)
	_hp_bar = _bar(v, Color(0.35, 0.9, 0.45), 10)
	_implants_label = _text(v, 13, Color(0.8, 0.8, 0.8))
	_gadgets_label = _rich(v, 14)
	_gadgets_label.custom_minimum_size = Vector2(272, 0)


func _build_bottom_right() -> void:
	var p := _panel()
	p.custom_minimum_size = Vector2(280, 0)
	_anchor(p, Control.PRESET_BOTTOM_RIGHT, Vector2(-300, -168))
	var v := VBoxContainer.new()
	p.add_child(v)
	_weapon_label = _text(v, 15, Color(0.85, 0.85, 0.85), HORIZONTAL_ALIGNMENT_RIGHT)
	_ammo_type_label = _text(v, 12, ACCENT, HORIZONTAL_ALIGNMENT_RIGHT)
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
	_prompt_panel.custom_minimum_size = Vector2(520, 0)
	_anchor(_prompt_panel, Control.PRESET_CENTER_BOTTOM, Vector2(-260, -210))
	_prompt_label = _rich(_prompt_panel, 17)
	_prompt_label.custom_minimum_size = Vector2(492, 0)
	_prompt_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_prompt_panel.visible = false


func _build_help() -> void:
	_help_panel = _panel()
	_anchor(_help_panel, Control.PRESET_CENTER, Vector2(-295, -270))
	_help_text = _rich(_help_panel, 15)
	_help_text.custom_minimum_size = Vector2(560, 0)
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
	_end_panel.custom_minimum_size = Vector2(520, 0)
	_anchor(_end_panel, Control.PRESET_CENTER, Vector2(-260, -150))
	_end_panel.mouse_filter = Control.MOUSE_FILTER_STOP
	_end_panel.visible = false
	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 10)
	_end_panel.add_child(v)
	_end_label = _text(v, 30, Color.WHITE, HORIZONTAL_ALIGNMENT_CENTER)
	_end_score = _text(v, 18, Color(0.9, 0.9, 0.9), HORIZONTAL_ALIGNMENT_CENTER)
	var row := HBoxContainer.new()
	row.alignment = BoxContainer.ALIGNMENT_CENTER
	row.add_theme_constant_override("separation", 12)
	v.add_child(row)
	Ui.button(row, "RECOMMENCER", _restart, 17, Vector2(200, 44))
	Ui.button(row, "MENU PRINCIPAL", func():
		get_tree().paused = false
		get_tree().change_scene_to_file(PauseMenu.MENU_SCENE)
	, 17, Vector2(200, 44))


func _build_build_bar() -> void:
	# Cartes des tours, au-dessus du message d'interaction quand on vise un ancrage vide.
	_build_bar = HBoxContainer.new()
	_build_bar.add_theme_constant_override("separation", 6)
	_build_bar.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_anchor(_build_bar, Control.PRESET_CENTER_BOTTOM, Vector2(-393, -338))
	add_child(_build_bar)
	for type in Tower.BUILD_ORDER:
		var data: Dictionary = Tower.STATS[type]
		var card := _panel(_build_bar)
		card.custom_minimum_size = Vector2(126, 0)
		var v := VBoxContainer.new()
		v.add_theme_constant_override("separation", 1)
		card.add_child(v)
		var name_label := _text(v, 14, data["color"])
		name_label.text = data["name"]
		var cost := _text(v, 13, Color(0.9, 0.9, 0.9))
		cost.text = "⚙ %d   ⚡ %d" % [data["cost"], data["energy"]]
		var info := _text(v, 11, Color(0.75, 0.75, 0.75))
		info.text = data["short"]
		info.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		info.custom_minimum_size = Vector2(100, 0)
		_build_cards.append(card)
	_build_bar.visible = false


# ------------------------------------------------------------------ mise à jour

func _process(delta: float) -> void:
	var p := Game.player as Player
	var core := Game.core as Structure
	if Input.is_action_just_pressed("help"):
		_help_panel.visible = not _help_panel.visible
		if _help_panel.visible:
			_help_text.text = _help()
			Ui.pop_in(_help_panel, _help_panel, 0.15)

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
	for row in _gate_rows:
		var b: Barrier = row[0]
		var bar: ProgressBar = row[1]
		var value: Label = row[2]
		if not is_instance_valid(b):
			continue
		# Détruite : la barre montre où en est la remise en place (elle se relève une fois pleine).
		bar.max_value = b.max_hp if b.alive else b.rebuild_hp()
		bar.value = b.hp
		if b.alive:
			value.text = "%d" % int(b.hp)
		else:
			value.text = "DÉTRUITE" if b.hp < 1.0 else "%d / %d" % [int(b.hp), int(b.rebuild_hp())]
		var col := Color(0.85, 0.85, 0.8) if b.alive and b.hp > b.max_hp * 0.35 else DANGER
		(bar.get_theme_stylebox("fill") as StyleBoxFlat).bg_color = col
		value.add_theme_color_override("font_color", Color(0.85, 0.85, 0.85) if b.alive else DANGER)

	_scrap_label.text = "⚙ %d  ferraille" % Game.scrap
	_energy_label.text = "⚡ Énergie  %d / %d" % [Game.energy_used, Game.energy_cap()]
	_energy_bar.max_value = Game.energy_cap()
	_energy_bar.value = Game.energy_used
	_score_label.text = "★ %d points" % Game.score
	_help_hint.text = "[%s] aide" % Settings.key_label("help")

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
		_gadgets_label.text = "%s   %s" % [_gadget_slot(0), _gadget_slot(1)]
		var w: Dictionary = Player.WEAPONS[p.weapon]
		_weapon_label.text = String(w["name"]).to_upper()
		var ammo_type: String = Game.mods[p.weapon]["ammo"]
		_ammo_type_label.text = "" if ammo_type == "standard" else "munitions %s" % String(Game.AMMO[ammo_type]["name"]).to_lower()
		var mag := p.mag_size(p.weapon)
		if p.reloading > 0.0:
			_ammo_label.text = "--"
			_reload_bar.visible = true
			_reload_bar.max_value = p.reload_time(p.weapon)
			_reload_bar.value = _reload_bar.max_value - p.reloading
		else:
			_ammo_label.text = "%d" % p.ammo[p.weapon]
			_reload_bar.visible = false
		var low: bool = p.ammo[p.weapon] <= mag / 4
		_ammo_label.add_theme_color_override("font_color", DANGER if low and p.reloading <= 0.0 else Color.WHITE)
		_mag_label.text = " / %d" % mag
		_slots_label.text = "[right]%s   %s[/right]" % [_slot(Settings.key_label("weapon_1"), "Pistolet", p.weapon == "pistol"), _slot(Settings.key_label("weapon_2"), "Fusil", p.weapon == "rifle")]
		_crosshair.spread = p.spread_amount()
		_crosshair.aiming = p.aiming
		_crosshair.visible = p.alive
		_update_build_bar(p)

	if Game.hint != "":
		_prompt_label.text = "[center]%s[/center]" % Game.hint
		_prompt_panel.visible = true
	else:
		_prompt_panel.visible = false

	_hurt = move_toward(_hurt, 0.0, delta * 0.8)
	(_vignette.material as ShaderMaterial).set_shader_parameter("hurt", _hurt)


func _update_build_bar(p: Player) -> void:
	_build_bar.visible = p.socket_in_sight != null and p.alive
	if not _build_bar.visible:
		return
	for i in _build_cards.size():
		var card := _build_cards[i]
		var data: Dictionary = Tower.STATS[Tower.BUILD_ORDER[i]]
		var sb := card.get_theme_stylebox("panel") as StyleBoxFlat
		var selected := i == p.build_choice
		sb.border_color = ACCENT if selected else Color(1, 1, 1, 0.08)
		sb.set_border_width_all(2 if selected else 1)
		var affordable := Game.scrap >= int(data["cost"])
		card.modulate = Color(1, 1, 1, 1.0 if affordable else 0.45)


func _gadget_slot(slot: int) -> String:
	var id: String = Game.gadget_slots[slot]
	var key := Settings.key_label("gadget_1" if slot == 0 else "gadget_2")
	var left: float = Game.gadget_cd.get(id, 0.0)
	var gname: String = Game.GADGETS[id]["name"]
	if left > 0.0:
		return "[color=#8a8a8a][%s] %s %d s[/color]" % [key, gname, int(ceil(left))]
	return "[color=#ffb840][b][%s][/b][/color] %s" % [key, gname]


func _slot(key: String, label: String, active: bool) -> String:
	if active:
		return "[color=#ffb840][b][%s] %s[/b][/color]" % [key, label]
	return "[color=#8a8a8a][%s] %s[/color]" % [key, label]


## Aide des commandes, construite d'après les touches choisies dans les réglages.
func _help() -> String:
	var k := func(action: String) -> String: return "[color=#ffb840][b]%s[/b][/color]" % Settings.key_label(action)
	var lines := [
		"[b]DÉPLACEMENT[/b]",
		"%s %s %s %s  bouger      %s  sauter      %s  courir" % [k.call("move_forward"), k.call("move_left"), k.call("move_back"), k.call("move_right"), k.call("jump"), k.call("sprint")],
		"",
		"[b]COMBAT[/b]",
		"%s  tirer      %s  viser      %s / %s ou molette  changer d'arme      %s  recharger" % [k.call("fire"), k.call("aim"), k.call("weapon_1"), k.call("weapon_2"), k.call("reload")],
		"%s  marquer un zombie (les tours le ciblent, +25 %%)      %s  lampe torche" % [k.call("mark"), k.call("flashlight")],
		"%s  %s      %s  %s" % [k.call("gadget_1"), Game.GADGETS[Game.gadget_slots[0]]["name"], k.call("gadget_2"), Game.GADGETS[Game.gadget_slots[1]]["name"]],
		"",
		"[b]CONSTRUCTION[/b]",
		"Sur un ancrage : molette ou %s  choisir la tour,  %s  construire" % [k.call("cycle_tower"), k.call("interact")],
		"Sur une tour : %s  améliorer,  %s  allumer / éteindre" % [k.call("interact"), k.call("toggle_power")],
		"Maintenir %s près d'une barrière ou sur le Cœur : réparer (une barrière détruite se relève)" % k.call("interact"),
		"%s sur l'établi (près du Cœur, entre les vagues) : armes, munitions, gadgets, générateur" % k.call("interact"),
		"",
		"[b]PARTIE[/b]",
		"%s  lancer la vague      %s  aide      %s  passer le tutoriel      Échap  pause et réglages" % [k.call("skip_phase"), k.call("help"), k.call("skip_tutorial")],
		"",
		"[color=#7fd8ff]BRISÉ : la Cryo gèle, un tir de pistolet lourd brise (x3)[/color]",
		"[color=#b8a8ff]SURCHARGE : un zombie chargé (Arc ou balles électriques) + un tir normal = onde électrique[/color]",
		"[color=#ffa060]EMBRASEMENT : un obus de Mortier sur un zombie en feu propage l'incendie[/color]",
	]
	return "\n".join(lines)


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
		Ui.style_button(b)
		b.pressed.connect(_pick_implant.bind(id))
		_implant_box.add_child(b)
	_implant_panel.visible = true
	Ui.pop_in(_implant_panel, _implant_panel)
	get_tree().paused = true
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func _pick_implant(id: String) -> void:
	_implant_panel.visible = false
	get_tree().paused = false
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	wave_manager.implant_chosen(id)


func _on_ended(victory: bool) -> void:
	_end_label.text = ("VICTOIRE !\nTu as tenu les %d vagues." % Game.LAST_WAVE) if victory else ("LE CŒUR EST TOMBÉ\nTu as tenu jusqu'à la vague %d." % Game.wave)
	_end_label.add_theme_color_override("font_color", ACCENT if victory else DANGER)
	var record := "NOUVEAU RECORD !" if Game.new_record else "Record : %d points" % Settings.best_score
	_end_score.text = "Score : %d points  ·  %d zombies abattus\n%s\n\n[%s] recommencer" % [Game.score, Game.kills, record, Settings.key_label("skip_phase")]
	_end_score.add_theme_color_override("font_color", ACCENT if Game.new_record else Color(0.9, 0.9, 0.9))
	_end_panel.visible = true
	Ui.pop_in(_end_panel, _end_panel, 0.35)
	_build_bar.visible = false
	_prompt_panel.visible = false
	get_tree().paused = true
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func _restart() -> void:
	get_tree().paused = false
	get_tree().reload_current_scene()


func _unhandled_input(event: InputEvent) -> void:
	if _end_panel.visible and event.is_action_pressed("skip_phase"):
		_restart()


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
		# La flèche du portail n'apparaît qu'au début, avec la colonne de lumière.
		if Game.portal_reveal > 0.01:
			_marker(cam, portal, "PORTAIL", Color(1.0, 0.3, 0.2, pulse * Game.portal_reveal), true)
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
