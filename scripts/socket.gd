class_name Socket
extends StaticBody3D
## Ancrage : le seul endroit où l'on peut poser une tour.

var ring := ""
var tower: Tower = null


func setup(p_ring: String) -> void:
	ring = p_ring
	collision_layer = Fx.LAYER_STRUCTURES
	collision_mask = 0
	var shape := CollisionShape3D.new()
	var cs := CylinderShape3D.new()
	cs.radius = 1.2
	cs.height = 0.4
	shape.shape = cs
	shape.position.y = 0.2
	add_child(shape)
	var pad := Fx.cylinder(1.2, 0.3, Color(0.35, 0.4, 0.5))
	pad.position.y = 0.15
	add_child(pad)
	var ring_mark := Fx.cylinder(0.3, 0.32, Color(0.3, 0.8, 1.0), 1.5)
	ring_mark.position.y = 0.16
	add_child(ring_mark)


func build(type: String) -> void:
	if tower:
		return
	var cost: int = Tower.STATS[type]["cost"]
	if not Game.try_spend(cost):
		return
	tower = Tower.new()
	add_child(tower)
	tower.setup(type, self)
	tower.position = Vector3(0, 0.3, 0)
	if not tower.powered:
		Game.say("Plus assez d'énergie : la tour est posée mais éteinte (X pour couper une autre tour).")


func clear_tower() -> void:
	tower = null
