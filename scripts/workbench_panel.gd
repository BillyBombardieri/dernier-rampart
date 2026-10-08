class_name WorkbenchPanel
extends CanvasLayer
## Fenêtre de l'établi : canon, chargeur et munitions de chaque arme, choix des 2 gadgets
## et amélioration du générateur. Le jeu est en pause tant qu'elle est ouverte.

const WEAPON_IDS := ["pistol", "rifle"]

var opened := 0  # Nombre d'ouvertures (le tutoriel s'en sert).
var _root: Control
var _content: VBoxContainer
var _scrap_label: Label


func _ready() -> void:
	layer = 6
	process_mode = Node.PROCESS_MODE_ALWAYS
	_root = Control.new()
	_root.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(_root)
	Ui.dimmer(_root, 0.55)
	var center := Ui.center(_root)
	var p := Ui.panel(center)
	p.custom_minimum_size = Vector2(980, 0)
	_content = VBoxContainer.new()
	_content.add_theme_constant_override("separation", 12)
	p.add_child(_content)
	visible = false
	Game.changed.connect(func():
		if visible and _scrap_label:
			_scrap_label.text = "⚙ %d ferraille" % Game.scrap
	)


func open() -> void:
	visible = true
	opened += 1
	_rebuild()
	get_tree().paused = true
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	Sfx.play(self, "ui_click", -6.0)


func close() -> void:
	if not visible:
		return
	visible = false
	get_tree().paused = false
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED


func _unhandled_input(event: InputEvent) -> void:
	if visible and event.is_action_pressed("interact"):
		close()
		get_viewport().set_input_as_handled()


func _rebuild() -> void:
	for c in _content.get_children():
		c.queue_free()
	var head := HBoxContainer.new()
	_content.add_child(head)
	var title := Ui.label(head, "ÉTABLI", 30, Ui.ACCENT)
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_scrap_label = Ui.label(head, "⚙ %d ferraille" % Game.scrap, 22, Ui.ACCENT, HORIZONTAL_ALIGNMENT_RIGHT)
	Ui.label(_content, "Tout se paie en ferraille. Les munitions achetées restent à toi : tu peux en changer gratuitement ensuite.", 14, Ui.MUTED)
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 12)
	_content.add_child(row)
	for w in WEAPON_IDS:
		_weapon_card(row, w)
	var row2 := HBoxContainer.new()
	row2.add_theme_constant_override("separation", 12)
	_content.add_child(row2)
	_gadget_card(row2)
	_generator_card(row2)
	var bottom := HBoxContainer.new()
	bottom.alignment = BoxContainer.ALIGNMENT_END
	_content.add_child(bottom)
	Ui.label(bottom, "%s ou Échap pour fermer   " % Settings.key_label("interact"), 13, Ui.MUTED)
	Ui.button(bottom, "FERMER", close, 17, Vector2(160, 40))


func _card(parent: Node, title: String, subtitle: String) -> VBoxContainer:
	var p := Ui.panel(parent, Color(1, 1, 1, 0.04), 8, 14)
	p.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 8)
	p.add_child(v)
	Ui.label(v, title, 19, Color.WHITE)
	if subtitle != "":
		Ui.label(v, subtitle, 13, Ui.MUTED)
	return v


func _weapon_card(parent: Node, w: String) -> void:
	var p := Game.player as Player
	var mods: Dictionary = Game.mods[w]
	var data: Dictionary = Player.WEAPONS[w]
	var dmg := p.weapon_damage(w) if p else float(data["damage"])
	var mag := p.mag_size(w) if p else int(data["mag"])
	var v := _card(parent, String(data["name"]).to_upper(), "%d dégâts par balle · %d balles · munitions %s" % [int(round(dmg)), mag, String(Game.AMMO[mods["ammo"]]["name"]).to_lower()])
	_upgrade_row(v, "Canon", mods["barrel"], Game.BARREL_COSTS.size(), "+20 % de dégâts par niveau", Game.barrel_cost(w), func(): _buy(Game.buy_barrel.bind(w)))
	_upgrade_row(v, "Chargeur", mods["mag"], Game.MAG_COSTS.size(), "+30 % de balles par niveau", Game.mag_cost(w), func(): _buy(Game.buy_mag.bind(w)))
	Ui.label(v, "Munitions", 15, Ui.ACCENT)
	var grid := GridContainer.new()
	grid.columns = 2
	grid.add_theme_constant_override("h_separation", 8)
	grid.add_theme_constant_override("v_separation", 6)
	v.add_child(grid)
	for ammo in Game.AMMO:
		var info: Dictionary = Game.AMMO[ammo]
		var owned: bool = mods["owned"].has(ammo)
		var text: String = info["name"] if owned else "%s · %d ⚙" % [info["name"], info["cost"]]
		var b := Ui.button(grid, text, func(): _buy(Game.choose_ammo.bind(w, ammo)), 14, Vector2(215, 34))
		b.tooltip_text = info["desc"]
		b.disabled = not owned and Game.scrap < info["cost"]
		Ui.set_selected(b, mods["ammo"] == ammo, _ammo_color(ammo))
	Ui.label(v, Game.AMMO[mods["ammo"]]["desc"], 13, _ammo_color(mods["ammo"]).lerp(Color.WHITE, 0.4))


func _upgrade_row(parent: Node, title: String, lvl: int, max_lvl: int, desc: String, cost: int, on_buy: Callable) -> void:
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 8)
	parent.add_child(row)
	var dots := "●".repeat(lvl) + "○".repeat(max_lvl - lvl)
	var info := VBoxContainer.new()
	info.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	info.add_theme_constant_override("separation", 0)
	row.add_child(info)
	Ui.label(info, "%s  %s" % [title, dots], 16)
	Ui.label(info, desc, 12, Ui.MUTED)
	if cost < 0:
		Ui.label(row, "MAX", 15, Ui.ACCENT)
	else:
		var b := Ui.button(row, "Améliorer · %d ⚙" % cost, on_buy, 14, Vector2(170, 34))
		b.disabled = Game.scrap < cost


func _gadget_card(parent: Node) -> void:
	var v := _card(parent, "GADGETS", "Deux emplacements. Chaque gadget se recharge après usage.")
	for slot in 2:
		var action := "gadget_1" if slot == 0 else "gadget_2"
		var row := HBoxContainer.new()
		row.add_theme_constant_override("separation", 6)
		v.add_child(row)
		Ui.label(row, "[%s]" % Settings.key_label(action), 16, Ui.ACCENT).custom_minimum_size = Vector2(44, 0)
		for id in Game.GADGETS:
			var info: Dictionary = Game.GADGETS[id]
			var b := Ui.button(row, info["name"], func():
				Game.set_gadget(slot, id)
				_rebuild()
			, 14, Vector2(138, 34))
			b.tooltip_text = info["desc"]
			Ui.set_selected(b, Game.gadget_slots[slot] == id)
	for id in Game.GADGETS:
		var info: Dictionary = Game.GADGETS[id]
		Ui.label(v, "%s : %s (recharge %d s)" % [info["name"], info["desc"], int(info["cooldown"])], 12, Ui.MUTED)


func _generator_card(parent: Node) -> void:
	var v := _card(parent, "GÉNÉRATEUR", "Plus d'énergie = plus de tours allumées en même temps.")
	var cost := Game.generator_cost()
	var dots := "●".repeat(Game.generator) + "○".repeat(Game.GENERATOR_COSTS.size() - Game.generator)
	Ui.label(v, "Niveau %s   énergie max %d" % [dots, Game.energy_cap()], 16)
	if cost < 0:
		Ui.label(v, "Générateur au maximum.", 14, Ui.ACCENT)
	else:
		var b := Ui.button(v, "+2 énergie · %d ⚙" % cost, func(): _buy(Game.buy_generator), 15, Vector2(0, 38))
		b.disabled = Game.scrap < cost


func _buy(action: Callable) -> void:
	if action.call():
		Sfx.play(self, "upgrade", -4.0, 0.05)
	_rebuild()


func _ammo_color(ammo: String) -> Color:
	match ammo:
		"incendiaire":
			return Color(1.0, 0.5, 0.2)
		"perforante":
			return Color(0.85, 0.85, 0.9)
		"electrique":
			return Color(0.6, 0.55, 1.0)
	return Ui.ACCENT
