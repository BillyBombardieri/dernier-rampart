class_name Hud
extends CanvasLayer
## Interface : ressources, phase, arme, aide contextuelle, choix d'implant et écran de fin.

const PHASE_NAMES := {
	"prep": "Préparation",
	"assault": "Assaut",
	"implant": "Choix d'implant",
	"harvest": "Récolte",
}

var wave_manager: WaveManager

var _top: Label
var _bottom: Label
var _hint: Label
var _message: Label
var _help: Label
var _implant_panel: PanelContainer
var _implant_box: VBoxContainer
var _end_panel: PanelContainer
var _end_label: Label
var _message_time := 0.0


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	_top = _label(Vector2(20, 16), 22)
	_bottom = _label(Vector2(20, 0), 22)
	_bottom.anchor_top = 1.0
	_bottom.anchor_bottom = 1.0
	_bottom.offset_top = -70
	_hint = _centered_label(90, 22, Color(0.7, 0.95, 1.0))
	_message = _centered_label(-220, 26, Color(1.0, 0.85, 0.4))
	_message.anchor_top = 0.0
	_message.anchor_bottom = 0.0
	_message.offset_top = 70
	_help = _label(Vector2(0, 16), 15)
	_help.anchor_left = 1.0
	_help.anchor_right = 1.0
	_help.offset_left = -330
	_help.modulate = Color(1, 1, 1, 0.75)
	_help.text = "ZQSD : bouger   Espace : sauter   Maj : courir\nClic gauche : tirer   1 / 2 : changer d'arme   R : recharger\nF : marquer un zombie (les tours le ciblent, +25 %)\nE / C sur un ancrage : poser une tour\nE : améliorer   X : allumer/éteindre une tour\nMaintenir E sur un relais ou le Cœur : réparer\nEntrée : passer la préparation   Échap : libérer la souris\n\nCOMBO : Cryo gèle, pistolet lourd = BRISÉ (x3)"
	var cross := _centered_label(0, 28, Color.WHITE)
	cross.anchor_top = 0.5
	cross.anchor_bottom = 0.5
	cross.offset_top = -20
	cross.text = "+"
	_build_implant_panel()
	_build_end_panel()
	Game.message.connect(_on_message)
	Game.ended.connect(_on_ended)


func _process(delta: float) -> void:
	var p := Game.player as Player
	var core := Game.core as Structure
	var phase_name: String = PHASE_NAMES.get(Game.phase, Game.phase)
	var timer := ""
	if Game.phase in ["prep", "harvest"]:
		timer = "  %d s" % int(ceil(Game.phase_time))
	var siege := "  (NUIT DE SIÈGE)" if Game.wave % WaveManager.SIEGE_EVERY == 0 else ""
	_top.text = "Vague %d / %d%s   —   %s%s\nFerraille : %d     Énergie : %d / %d     Cœur : %d PV" % [
		Game.wave, Game.LAST_WAVE, siege, phase_name, timer,
		Game.scrap, Game.energy_used, Game.energy_cap(), int(core.hp) if core else 0,
	]
	if p:
		var w: Dictionary = Player.WEAPONS[p.weapon]
		var ammo_text := "RECHARGE..." if p.reloading > 0.0 else "%d / %d" % [p.ammo[p.weapon], w["mag"]]
		var implants := ""
		for id in Game.implants:
			implants += "  [%s]" % Game.IMPLANTS[id]["name"]
		_bottom.text = "PV : %d / %d     %s : %s\nImplants :%s" % [int(max(p.hp, 0)), int(p.max_hp), w["name"], ammo_text, implants if implants != "" else " aucun"]
	_hint.text = Game.hint
	if _message_time > 0.0:
		_message_time -= delta
		_message.modulate.a = clamp(_message_time, 0.0, 1.0)


func _on_message(text: String) -> void:
	_message.text = text
	_message_time = 4.0


func show_implants(options: Array) -> void:
	for c in _implant_box.get_children():
		if c is Button:
			c.queue_free()
	for id in options:
		var data: Dictionary = Game.IMPLANTS[id]
		var b := Button.new()
		b.text = "%s\n+ %s\n- %s" % [data["name"], data["plus"], data["minus"]]
		b.custom_minimum_size = Vector2(460, 90)
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
	_end_label.text += "\n\nEntrée : recommencer"
	_end_panel.visible = true
	get_tree().paused = true
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func _unhandled_input(event: InputEvent) -> void:
	if _end_panel.visible and event.is_action_pressed("skip_phase"):
		get_tree().paused = false
		get_tree().reload_current_scene()


func _label(pos: Vector2, size: int) -> Label:
	var l := Label.new()
	l.position = pos
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_constant_override("outline_size", 6)
	l.add_theme_color_override("font_outline_color", Color.BLACK)
	add_child(l)
	return l


func _centered_label(offset_from_bottom: float, size: int, color: Color) -> Label:
	var l := _label(Vector2.ZERO, size)
	l.anchor_left = 0.0
	l.anchor_right = 1.0
	l.anchor_top = 1.0
	l.anchor_bottom = 1.0
	l.offset_top = -offset_from_bottom - 40
	l.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	l.modulate = color
	return l


func _build_implant_panel() -> void:
	_implant_panel = PanelContainer.new()
	_implant_panel.set_anchors_preset(Control.PRESET_CENTER)
	_implant_panel.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_implant_panel.grow_vertical = Control.GROW_DIRECTION_BOTH
	_implant_panel.visible = false
	add_child(_implant_panel)
	_implant_box = VBoxContainer.new()
	_implant_box.add_theme_constant_override("separation", 12)
	_implant_panel.add_child(_implant_box)
	var title := Label.new()
	title.text = "Siège repoussé ! Choisis un implant (bonus ET malus)"
	title.add_theme_font_size_override("font_size", 24)
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_implant_box.add_child(title)


func _build_end_panel() -> void:
	_end_panel = PanelContainer.new()
	_end_panel.set_anchors_preset(Control.PRESET_CENTER)
	_end_panel.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_end_panel.grow_vertical = Control.GROW_DIRECTION_BOTH
	_end_panel.visible = false
	add_child(_end_panel)
	_end_label = Label.new()
	_end_label.add_theme_font_size_override("font_size", 36)
	_end_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_end_panel.add_child(_end_label)
