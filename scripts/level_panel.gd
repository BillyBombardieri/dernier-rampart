class_name LevelPanel
extends PanelContainer
## Choix du niveau dans le menu principal. Chaque niveau a sa carte et son ambiance, et il est
## plus dur que le précédent. Gagner un niveau débloque le suivant.

signal closed
signal chosen(level: int)

var _list: VBoxContainer


func _ready() -> void:
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
	custom_minimum_size = Vector2(640, 0)
	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 10)
	add_child(v)
	Ui.label(v, "CHOISIS TON NIVEAU", 28, Ui.ACCENT)
	Ui.label(v, "Chaque niveau est plus dur que le précédent. Gagne-le pour ouvrir le suivant.", 15, Ui.MUTED)
	_list = VBoxContainer.new()
	_list.add_theme_constant_override("separation", 8)
	v.add_child(_list)
	var bottom := HBoxContainer.new()
	bottom.alignment = BoxContainer.ALIGNMENT_END
	v.add_child(bottom)
	Ui.button(bottom, "RETOUR", close, 17, Vector2(160, 40))


func open() -> void:
	visible = true
	_refresh()
	Ui.pop_in(self, get_parent() as Control)


func close() -> void:
	visible = false
	closed.emit()


func _refresh() -> void:
	for c in _list.get_children():
		c.queue_free()
	for i in Levels.count():
		var data := Levels.get_level(i)
		var unlocked := i <= Settings.unlocked_level
		var won := Settings.levels_won.has(i)
		var b := Button.new()
		b.alignment = HORIZONTAL_ALIGNMENT_LEFT
		b.custom_minimum_size = Vector2(0, 74)
		b.add_theme_font_size_override("font_size", 16)
		b.focus_mode = Control.FOCUS_NONE
		Ui.style_button(b)
		var head := "NIVEAU %d · %s%s" % [i + 1, (data["name"] as String).to_upper(), "   ✔ gagné" if won else ""]
		if unlocked:
			b.text = "%s\n%s. %s" % [head, data["place"], data["desc"]]
			b.pressed.connect(func():
				Sfx.play(b, "ui_click", -8.0, 0.05)
				chosen.emit(i)
			)
		else:
			b.text = "🔒 NIVEAU %d · %s\nGagne « %s » pour le débloquer." % [i + 1, (data["name"] as String).to_upper(), Levels.get_level(i - 1)["name"]]
			b.disabled = true
		_list.add_child(b)
