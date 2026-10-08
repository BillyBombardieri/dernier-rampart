class_name Structure
extends StaticBody3D
## Bâtiment de la base qui a des points de vie : le Cœur. S'il tombe, la partie est perdue.

signal destroyed

var kind := "core"
var ring := ""
var display_name := ""
var max_hp := 400.0
var hp := 400.0
var alive := true

var _label: Label3D
var _mesh: MeshInstance3D
var _material: StandardMaterial3D
var _glow: MeshInstance3D
var _light: OmniLight3D
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
	_material = Fx.textured("concrete", 0.5)
	_mesh = Fx.box_mat(size, _material)
	_mesh.position.y = size.y * 0.5
	add_child(_mesh)
	# Bande lumineuse et lumière qui montrent que le bâtiment est alimenté.
	_glow = Fx.box(Vector3(size.x + 0.05, 0.25, size.z + 0.05), color, 4.0)
	_glow.position.y = size.y * 0.75
	add_child(_glow)
	_light = OmniLight3D.new()
	_light.light_color = color
	_light.light_energy = 1.5
	_light.omni_range = 8.0
	_light.position.y = size.y + 0.5
	add_child(_light)
	_label = Label3D.new()
	_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	_label.font_size = 40
	_label.outline_size = 8
	_label.position.y = size.y + 1.0
	add_child(_label)
	add_to_group("structures")
	_refresh()


func take_damage(amount: float) -> void:
	if not alive or Game.is_over:
		return
	hp = max(0.0, hp - amount)
	if randf() < 0.3:
		Sfx.play_at(Game.main, "structure_hit", global_position, -6.0, 0.2)
	if hp <= 0.0:
		alive = false
		_set_powered(false)
		Fx.burst(Game.main, global_position + Vector3(0, 2, 0), Vector3.UP, Color(1.0, 0.6, 0.2), 40, 8.0, 0.12)
		Sfx.play_at(Game.main, "structure_hit", global_position, 6.0)
		destroyed.emit()
		Game.end_game(false)
	_refresh()


## Répare et renvoie les PV réellement rendus.
func repair(amount: float) -> float:
	if not alive:
		return 0.0
	var before := hp
	hp = min(max_hp, hp + amount)
	_refresh()
	return hp - before


func _set_powered(on: bool) -> void:
	_material.albedo_color = Color.WHITE if on else Color(0.3, 0.28, 0.26)
	_glow.visible = on
	_light.visible = on


func _refresh() -> void:
	_label.text = "%s\n%d / %d" % [display_name, int(hp), int(max_hp)]
	_label.modulate = Color(1, 1, 1) if alive else Color(1, 0.3, 0.3)
	Game.changed.emit()
