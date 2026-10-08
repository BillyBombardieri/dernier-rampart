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
	var pad_mesh := CylinderMesh.new()
	pad_mesh.top_radius = 1.2
	pad_mesh.bottom_radius = 1.3
	pad_mesh.height = 0.3
	pad_mesh.radial_segments = 16
	var pad := Fx.mesh_with(pad_mesh, Fx.textured("concrete", 0.5))
	pad.position.y = 0.15
	add_child(pad)
	# Repères lumineux discrets pour voir où construire.
	for i in 4:
		var mark := Fx.box(Vector3(0.12, 0.04, 0.4), Color(1.0, 0.75, 0.2), 2.0)
		var a := i * PI * 0.5
		mark.position = Vector3(cos(a) * 0.9, 0.31, sin(a) * 0.9)
		mark.rotation.y = -a + PI * 0.5
		add_child(mark)


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
