class_name Ui
extends RefCounted
## Petits outils pour construire les menus (panneaux, textes, boutons) avec un style commun.

const ACCENT := Color(1.0, 0.72, 0.25)
const DANGER := Color(1.0, 0.28, 0.22)
const MUTED := Color(0.7, 0.7, 0.72)
const PANEL_BG := Color(0.04, 0.05, 0.07, 0.9)


static func panel(parent: Node, bg := PANEL_BG, radius := 10, margin := 18) -> PanelContainer:
	var p := PanelContainer.new()
	var sb := StyleBoxFlat.new()
	sb.bg_color = bg
	sb.set_corner_radius_all(radius)
	sb.content_margin_left = margin
	sb.content_margin_right = margin
	sb.content_margin_top = margin * 0.7
	sb.content_margin_bottom = margin * 0.7
	sb.border_color = Color(1, 1, 1, 0.1)
	sb.set_border_width_all(1)
	p.add_theme_stylebox_override("panel", sb)
	parent.add_child(p)
	return p


static func label(parent: Node, text: String, size := 16, color := Color.WHITE, align := HORIZONTAL_ALIGNMENT_LEFT) -> Label:
	var l := Label.new()
	l.text = text
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_color", color)
	l.add_theme_constant_override("outline_size", 3)
	l.add_theme_color_override("font_outline_color", Color(0, 0, 0, 0.7))
	l.horizontal_alignment = align
	l.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(l)
	return l


static func rich(parent: Node, text: String, size := 15) -> RichTextLabel:
	var r := RichTextLabel.new()
	r.bbcode_enabled = true
	r.fit_content = true
	r.scroll_active = false
	r.text = text
	r.add_theme_font_size_override("normal_font_size", size)
	r.add_theme_font_size_override("bold_font_size", size)
	r.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(r)
	return r


static func button(parent: Node, text: String, on_press: Callable, size := 17, min_size := Vector2(0, 40)) -> Button:
	var b := Button.new()
	b.text = text
	b.custom_minimum_size = min_size
	b.focus_mode = Control.FOCUS_NONE
	b.add_theme_font_size_override("font_size", size)
	style_button(b)
	b.pressed.connect(func():
		Sfx.play(b, "ui_click", -8.0, 0.05)
		on_press.call()
	)
	parent.add_child(b)
	return b


static func style_button(b: Button, accent := ACCENT) -> void:
	var states := {
		"normal": [Color(1, 1, 1, 0.07), Color(1, 1, 1, 0.14)],
		"hover": [Color(accent.r, accent.g, accent.b, 0.22), accent],
		"pressed": [Color(accent.r, accent.g, accent.b, 0.4), accent],
		"disabled": [Color(1, 1, 1, 0.03), Color(1, 1, 1, 0.06)],
	}
	for state in states:
		var sb := StyleBoxFlat.new()
		sb.bg_color = states[state][0]
		sb.border_color = states[state][1]
		sb.set_border_width_all(1)
		sb.set_corner_radius_all(6)
		sb.content_margin_left = 14
		sb.content_margin_right = 14
		sb.content_margin_top = 6
		sb.content_margin_bottom = 6
		b.add_theme_stylebox_override(state, sb)
	b.add_theme_stylebox_override("focus", StyleBoxEmpty.new())
	b.add_theme_color_override("font_color", Color(0.95, 0.95, 0.95))
	b.add_theme_color_override("font_hover_color", Color.WHITE)
	b.add_theme_color_override("font_disabled_color", Color(1, 1, 1, 0.3))


## Marque un bouton comme « choisi » (bordure accentuée en permanence).
static func set_selected(b: Button, on: bool, accent := ACCENT) -> void:
	var sb := (b.get_theme_stylebox("normal") as StyleBoxFlat).duplicate() as StyleBoxFlat
	sb.border_color = accent if on else Color(1, 1, 1, 0.14)
	sb.bg_color = Color(accent.r, accent.g, accent.b, 0.18) if on else Color(1, 1, 1, 0.07)
	sb.set_border_width_all(2 if on else 1)
	b.add_theme_stylebox_override("normal", sb)


static func slider(parent: Node, min_value: float, max_value: float, step: float, value: float, on_change: Callable) -> HSlider:
	var s := HSlider.new()
	s.min_value = min_value
	s.max_value = max_value
	s.step = step
	s.value = value
	s.custom_minimum_size = Vector2(240, 24)
	s.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	s.focus_mode = Control.FOCUS_NONE
	s.value_changed.connect(on_change)
	parent.add_child(s)
	return s


## Fond sombre plein écran qui bloque les clics derrière un menu.
static func dimmer(parent: Node, alpha := 0.6) -> ColorRect:
	var r := ColorRect.new()
	r.color = Color(0, 0, 0, alpha)
	r.set_anchors_preset(Control.PRESET_FULL_RECT)
	r.mouse_filter = Control.MOUSE_FILTER_STOP
	parent.add_child(r)
	return r


## Centre un élément à l'écran.
static func center(parent: Node) -> CenterContainer:
	var c := CenterContainer.new()
	c.set_anchors_preset(Control.PRESET_FULL_RECT)
	c.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(c)
	return c
