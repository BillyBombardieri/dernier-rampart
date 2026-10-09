class_name LevelPanel
extends PanelContainer
## Choix du niveau dans le menu principal : 4 chapitres, chacun sur sa carte et dans son
## environnement, avec 5 niveaux de plus en plus durs. Gagner un niveau débloque le suivant,
## et gagner le 5e niveau d'un chapitre ouvre le chapitre suivant.

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
	custom_minimum_size = Vector2(760, 0)
	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 10)
	add_child(v)
	Ui.label(v, "CHOISIS TON NIVEAU", 28, Ui.ACCENT)
	Ui.label(v, "5 niveaux par chapitre, chacun plus dur que le précédent. Gagne le 5e pour ouvrir le chapitre suivant.", 15, Ui.MUTED)
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
	for ch in Levels.CHAPTERS.size():
		var data: Dictionary = Levels.CHAPTERS[ch]
		var first := ch * Levels.PER_CHAPTER
		var open_chapter := first <= Settings.unlocked_level
		var card := Ui.panel(_list, Color(1, 1, 1, 0.04 if open_chapter else 0.015), 8, 12)
		var row := HBoxContainer.new()
		row.add_theme_constant_override("separation", 12)
		card.add_child(row)
		var text := VBoxContainer.new()
		text.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		row.add_child(text)
		var dim := Color(1, 1, 1, 0.35)
		Ui.label(text, "%sCHAPITRE %d · %s" % ["" if open_chapter else "🔒 ", ch + 1, (data["name"] as String).to_upper()], 17, Color.WHITE if open_chapter else dim)
		var sub := "%s. %s" % [data["place"], data["desc"]] if open_chapter else "Gagne le niveau 5 de « %s » pour l'ouvrir." % Levels.CHAPTERS[ch - 1]["name"]
		var l := Ui.label(text, sub, 14, Ui.MUTED if open_chapter else dim)
		l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		l.custom_minimum_size = Vector2(380, 0)
		var buttons := HBoxContainer.new()
		buttons.add_theme_constant_override("separation", 6)
		buttons.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		row.add_child(buttons)
		for s in Levels.PER_CHAPTER:
			var level := first + s
			var won := Settings.levels_won.has(level)
			var b := Ui.button(buttons, ("✔ %d" if won else "%d") % (s + 1), func(): chosen.emit(level), 16, Vector2(52, 44))
			b.tooltip_text = Levels.title(level)
			b.disabled = level > Settings.unlocked_level
			if level == Settings.unlocked_level and not won:
				Ui.set_selected(b, true)  # Le prochain niveau à gagner.
