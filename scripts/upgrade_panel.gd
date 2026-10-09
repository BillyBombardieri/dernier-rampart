class_name UpgradePanel
extends PanelContainer
## Améliorations permanentes, dans le menu principal : on dépense les insignes gagnés en jouant.
## Les achats sont sauvegardés tout de suite et valent pour toutes les parties suivantes.

signal closed

var _points: Label
var _rows := {}  # id -> {"rank": Label, "button": Button}


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
	custom_minimum_size = Vector2(700, 0)
	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 10)
	add_child(v)
	var top := HBoxContainer.new()
	v.add_child(top)
	var title := Ui.label(top, "AMÉLIORATIONS PERMANENTES", 28, Ui.ACCENT)
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_points = Ui.label(top, "", 22, Ui.ACCENT)
	var info := Ui.label(v, "Chaque partie rapporte des insignes ★ : 1 par vague repoussée (plus dans les niveaux durs), et une prime en cas de victoire.", 15, Ui.MUTED)
	info.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	info.custom_minimum_size = Vector2(650, 0)
	var grid := GridContainer.new()
	grid.columns = 4
	grid.add_theme_constant_override("h_separation", 16)
	grid.add_theme_constant_override("v_separation", 8)
	v.add_child(grid)
	for id in Upgrades.LIST:
		var data: Dictionary = Upgrades.LIST[id]
		var name := Ui.label(grid, data["name"], 17)
		name.custom_minimum_size = Vector2(150, 0)
		var desc := Ui.label(grid, "%s par rang" % data["desc"], 15, Color(0.85, 0.85, 0.85))
		desc.custom_minimum_size = Vector2(270, 0)
		var rank := Ui.label(grid, "", 17, Ui.ACCENT)
		rank.custom_minimum_size = Vector2(70, 0)
		var b := Ui.button(grid, "", func(): _buy(id), 15, Vector2(150, 36))
		_rows[id] = {"rank": rank, "button": b}
	var bottom := HBoxContainer.new()
	bottom.add_theme_constant_override("separation", 10)
	v.add_child(bottom)
	Ui.button(bottom, "Tout rembourser", func():
		Upgrades.refund_all()
		_refresh()
	, 15, Vector2(200, 40))
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	bottom.add_child(spacer)
	Ui.button(bottom, "RETOUR", close, 17, Vector2(160, 40))
	_refresh()


func open() -> void:
	visible = true
	_refresh()
	Ui.pop_in(self, get_parent() as Control)


func close() -> void:
	visible = false
	closed.emit()


func _buy(id: String) -> void:
	if Upgrades.buy(id):
		Sfx.play(self, "upgrade", -6.0, 0.05)
	_refresh()


func _refresh() -> void:
	_points.text = "★ %d" % Settings.insignes
	for id in _rows:
		var r := Upgrades.rank(id)
		var m := Upgrades.max_rank(id)
		(_rows[id]["rank"] as Label).text = "●".repeat(r) + "○".repeat(m - r)
		var b: Button = _rows[id]["button"]
		var cost := Upgrades.next_cost(id)
		if cost < 0:
			b.text = "MAXIMUM"
			b.disabled = true
		else:
			b.text = "ACHETER  ★ %d" % cost
			b.disabled = Settings.insignes < cost
