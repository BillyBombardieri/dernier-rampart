class_name PauseMenu
extends CanvasLayer
## Menu de pause (Échap) : reprendre, réglages, recommencer, menu principal, quitter.

const MENU_SCENE := "res://scenes/menu.tscn"

var bench: WorkbenchPanel
var _root: Control
var _panel: PanelContainer
var _settings: SettingsPanel


func _ready() -> void:
	layer = 10
	process_mode = Node.PROCESS_MODE_ALWAYS
	_root = Control.new()
	_root.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(_root)
	Ui.dimmer(_root, 0.6)
	var center := Ui.center(_root)
	_panel = Ui.panel(center)
	_panel.custom_minimum_size = Vector2(380, 0)
	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 10)
	_panel.add_child(v)
	Ui.label(v, "PAUSE", 34, Ui.ACCENT, HORIZONTAL_ALIGNMENT_CENTER)
	Ui.button(v, "REPRENDRE", close, 18, Vector2(0, 46))
	Ui.button(v, "RÉGLAGES", _open_settings, 18, Vector2(0, 46))
	Ui.button(v, "RECOMMENCER LA PARTIE", func():
		get_tree().paused = false
		get_tree().reload_current_scene()
	, 18, Vector2(0, 46))
	Ui.button(v, "MENU PRINCIPAL", func():
		get_tree().paused = false
		get_tree().change_scene_to_file(MENU_SCENE)
	, 18, Vector2(0, 46))
	Ui.button(v, "QUITTER LE JEU", func(): get_tree().quit(), 18, Vector2(0, 46))
	_settings = SettingsPanel.new()
	center.add_child(_settings)
	_settings.visible = false
	_settings.closed.connect(func(): _panel.visible = true)
	visible = false


func is_open() -> bool:
	return visible


func open() -> void:
	visible = true
	_panel.visible = true
	_settings.visible = false
	get_tree().paused = true
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func close() -> void:
	visible = false
	get_tree().paused = false
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	var p := Game.player as Player
	if p:
		p.apply_settings()


func _open_settings() -> void:
	_panel.visible = false
	_settings.open()


func _unhandled_input(event: InputEvent) -> void:
	if not (event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_ESCAPE):
		return
	get_viewport().set_input_as_handled()
	if visible:
		if _settings.visible:
			_settings.close()
		else:
			close()
	elif bench and bench.visible:
		bench.close()
	elif not get_tree().paused and not Game.is_over:
		open()
