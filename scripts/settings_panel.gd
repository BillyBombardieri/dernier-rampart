class_name SettingsPanel
extends PanelContainer
## Fenêtre des réglages (menu principal et pause) : sensibilité, volume, champ de vision,
## axe vertical inversé et touches personnalisées. Tout est sauvegardé tout de suite.

signal closed

var _waiting_action := ""
var _key_buttons := {}
var _sens_value: Label
var _volume_value: Label
var _fov_value: Label
var _hint: Label


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	var sb := StyleBoxFlat.new()
	sb.bg_color = Ui.PANEL_BG
	sb.set_corner_radius_all(10)
	sb.content_margin_left = 22
	sb.content_margin_right = 22
	sb.content_margin_top = 16
	sb.content_margin_bottom = 16
	sb.border_color = Color(1, 1, 1, 0.1)
	sb.set_border_width_all(1)
	add_theme_stylebox_override("panel", sb)
	custom_minimum_size = Vector2(760, 0)
	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 10)
	add_child(v)
	Ui.label(v, "RÉGLAGES", 28, Ui.ACCENT)

	var grid := GridContainer.new()
	grid.columns = 3
	grid.add_theme_constant_override("h_separation", 16)
	grid.add_theme_constant_override("v_separation", 8)
	v.add_child(grid)
	Ui.label(grid, "Sensibilité de la souris", 16)
	Ui.slider(grid, 0.2, 3.0, 0.05, Settings.sensitivity, func(value: float):
		Settings.sensitivity = value
		_refresh_values()
	)
	_sens_value = Ui.label(grid, "", 16, Ui.ACCENT)
	Ui.label(grid, "Volume", 16)
	Ui.slider(grid, 0.0, 1.0, 0.01, Settings.volume, func(value: float):
		Settings.volume = value
		Settings.apply()
		_refresh_values()
	)
	_volume_value = Ui.label(grid, "", 16, Ui.ACCENT)
	Ui.label(grid, "Champ de vision", 16)
	Ui.slider(grid, 60.0, 100.0, 1.0, Settings.fov, func(value: float):
		Settings.fov = value
		_refresh_values()
	)
	_fov_value = Ui.label(grid, "", 16, Ui.ACCENT)
	Ui.label(grid, "Inverser l'axe vertical", 16)
	var invert := CheckBox.new()
	invert.button_pressed = Settings.invert_y
	invert.focus_mode = Control.FOCUS_NONE
	invert.toggled.connect(func(on: bool): Settings.invert_y = on)
	grid.add_child(invert)
	Ui.label(grid, "", 16)

	Ui.label(v, "TOUCHES  (clique sur une touche puis appuie sur la nouvelle)", 16, Ui.ACCENT)
	var scroll := ScrollContainer.new()
	scroll.custom_minimum_size = Vector2(0, 300)
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	v.add_child(scroll)
	var keys := GridContainer.new()
	keys.columns = 4
	keys.add_theme_constant_override("h_separation", 12)
	keys.add_theme_constant_override("v_separation", 6)
	keys.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(keys)
	for entry in Settings.BINDABLE:
		var action: String = entry[0]
		var l := Ui.label(keys, entry[1], 14)
		l.custom_minimum_size = Vector2(210, 0)
		l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		var b := Ui.button(keys, "", func(): _start_rebind(action), 14, Vector2(120, 32))
		_key_buttons[action] = b
	_hint = Ui.label(v, "", 14, Ui.MUTED)
	var bottom := HBoxContainer.new()
	bottom.add_theme_constant_override("separation", 10)
	v.add_child(bottom)
	Ui.button(bottom, "Touches par défaut", func():
		Settings.reset_bindings()
		Settings.save_settings()
		_refresh_keys()
	, 15, Vector2(200, 40))
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	bottom.add_child(spacer)
	Ui.button(bottom, "RETOUR", close, 17, Vector2(160, 40))
	_refresh_values()
	_refresh_keys()


func open() -> void:
	visible = true
	_waiting_action = ""
	_refresh_values()
	_refresh_keys()


func close() -> void:
	_waiting_action = ""
	Settings.save_settings()
	visible = false
	closed.emit()


func _refresh_values() -> void:
	_sens_value.text = "%.2f x" % Settings.sensitivity
	_volume_value.text = "%d %%" % int(round(Settings.volume * 100.0))
	_fov_value.text = "%d°" % int(Settings.fov)


func _refresh_keys() -> void:
	for action in _key_buttons:
		var b: Button = _key_buttons[action]
		b.text = "..." if action == _waiting_action else Settings.key_label(action)
		Ui.set_selected(b, action == _waiting_action)
	_hint.text = "Appuie sur une touche ou un bouton de souris (Échap pour annuler)." if _waiting_action != "" else "Si la touche est déjà prise, les deux actions échangent leurs touches."


func _start_rebind(action: String) -> void:
	_waiting_action = action
	_refresh_keys()


func _input(event: InputEvent) -> void:
	if _waiting_action == "" or not visible:
		return
	var b := {}
	if event is InputEventKey and event.pressed and not event.echo:
		var k := event as InputEventKey
		if k.keycode == KEY_ESCAPE:
			_waiting_action = ""
			_refresh_keys()
			get_viewport().set_input_as_handled()
			return
		# Touche enregistrée par sa position : elle reste la même quelle que soit la disposition.
		if k.physical_keycode != KEY_NONE:
			b = {"kind": "p", "code": k.physical_keycode}
		else:
			b = {"kind": "k", "code": k.keycode}
	elif event is InputEventMouseButton and event.pressed:
		var mb := event as InputEventMouseButton
		if mb.button_index in [MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_MIDDLE, MOUSE_BUTTON_XBUTTON1, MOUSE_BUTTON_XBUTTON2]:
			b = {"kind": "m", "code": mb.button_index}
	if b.is_empty():
		return
	get_viewport().set_input_as_handled()
	Settings.rebind(_waiting_action, b)
	_waiting_action = ""
	_refresh_keys()
