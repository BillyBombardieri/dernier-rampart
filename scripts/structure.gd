class_name Structure
extends StaticBody3D
## Bâtiment de la base qui a des points de vie : le Cœur ou le relais d'énergie d'un anneau.

signal destroyed
signal restored

var kind := "relay"  # "core" ou "relay"
var ring := ""
var display_name := ""
var max_hp := 400.0
var hp := 400.0
var alive := true

var _label: Label3D
var _mesh: MeshInstance3D
var _color := Color.WHITE


func setup(p_kind: String, p_name: String, p_ring: String, p_hp: float, size: Vector3, color: Color) -> void:
	kind = p_kind
	display_name = p_name
	ring = p_ring
	max_hp = p_hp
	hp = p_hp
	_color = color
	collision_layer = Fx.LAYER_STRUCTURES | Fx.LAYER_WORLD
	collision_mask = 0
	var shape := CollisionShape3D.new()
	var bs := BoxShape3D.new()
	bs.size = size
	shape.shape = bs
	shape.position.y = size.y * 0.5
	add_child(shape)
	_mesh = Fx.box(size, color, 0.6)
	_mesh.position.y = size.y * 0.5
	add_child(_mesh)
	_label = Label3D.new()
	_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	_label.font_size = 40
	_label.outline_size = 8
	_label.position.y = size.y + 1.0
	add_child(_label)
	add_to_group("structures")
	if kind == "relay":
		add_to_group("attackable")
	_refresh()


func take_damage(amount: float) -> void:
	if not alive or Game.is_over:
		return
	hp = max(0.0, hp - amount)
	if hp <= 0.0:
		alive = false
		_mesh.material_override = Fx.material(Color(0.15, 0.15, 0.15))
		destroyed.emit()
		if kind == "core":
			Game.end_game(false)
		else:
			Game.say("%s détruit ! Les tours de l'anneau sont coupées." % display_name)
	_refresh()


## Répare et renvoie les PV réellement rendus.
func repair(amount: float) -> float:
	var before := hp
	hp = min(max_hp, hp + amount)
	if not alive and hp >= max_hp:
		alive = true
		_mesh.material_override = null
		restored.emit()
		Game.say("%s réparé, l'anneau est de nouveau alimenté." % display_name)
	_refresh()
	return hp - before


func _refresh() -> void:
	var state := "" if alive else " (HORS SERVICE)"
	_label.text = "%s%s\n%d / %d" % [display_name, state, int(hp), int(max_hp)]
	_label.modulate = Color(1, 1, 1) if alive else Color(1, 0.3, 0.3)
	Game.changed.emit()
