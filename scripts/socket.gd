class_name Socket
extends StaticBody3D
## Ancrage : le seul endroit où l'on peut poser une tour.

var ring := ""
var tower: Tower = null

var _preview: MeshInstance3D
var _preview_type := ""
var _preview_time := 0.0


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
	tower.position = Vector3(0, 0.3, 0)
	tower.setup(type, self)
	if not tower.powered:
		Game.say("Plus assez d'énergie : la tour est posée mais éteinte (%s pour couper une autre tour)." % Settings.key_label("toggle_power"))


func clear_tower() -> void:
	tower = null


## Montre au sol la portée de la tour choisie, tant que le joueur vise l'ancrage.
func preview(type: String) -> void:
	_preview_time = 0.15
	if _preview_type != type:
		if _preview:
			_preview.queue_free()
		_preview_type = type
		_preview = Tower.range_ring(Tower.STATS[type]["range"], Tower.STATS[type]["color"])
		add_child(_preview)
	_preview.visible = true


func _process(delta: float) -> void:
	if _preview_time > 0.0:
		_preview_time -= delta
		if _preview_time <= 0.0 and _preview:
			_preview.visible = false
