class_name Scrap
extends Area3D
## Ferraille laissée par un zombie. Il faut aller la ramasser avant qu'elle disparaisse.

const LIFETIME := 30.0

var value := 1
var _time := 0.0
var _mesh: MeshInstance3D
var _base_y := 0.0


func setup(p_value: int, pos: Vector3) -> void:
	value = p_value
	global_position = Vector3(pos.x, 0.0, pos.z)
	_base_y = 0.15
	collision_layer = Fx.LAYER_PICKUPS
	collision_mask = Fx.LAYER_PLAYER
	monitoring = true
	var shape := CollisionShape3D.new()
	var ss := SphereShape3D.new()
	ss.radius = 0.8
	shape.shape = ss
	shape.position.y = 0.5
	add_child(shape)
	# Petit tas de pièces métalliques, avec un léger reflet doré pour le repérer la nuit.
	_mesh = MeshInstance3D.new()
	_mesh.position.y = _base_y
	add_child(_mesh)
	var metal := Fx.textured("metal", 3.0, Color(0.9, 0.85, 0.8), false)
	var glint := Fx.material(Color(1.0, 0.7, 0.3), 0.8)
	glint.metallic = 1.0
	glint.roughness = 0.25
	for i in 3:
		var piece := Fx.box_mat(Vector3(randf_range(0.12, 0.25), 0.05, randf_range(0.1, 0.3)), glint if i == 0 else metal)
		piece.position = Vector3(randf_range(-0.1, 0.1), i * 0.04, randf_range(-0.1, 0.1))
		piece.rotation = Vector3(randf_range(-0.4, 0.4), randf() * TAU, randf_range(-0.4, 0.4))
		_mesh.add_child(piece)
	add_to_group("scrap")
	body_entered.connect(_on_body_entered)


func _physics_process(delta: float) -> void:
	_time += delta
	_mesh.position.y = _base_y + sin(_time * 3.0) * 0.03
	_mesh.rotation.y += delta * 0.8
	if _time > LIFETIME:
		queue_free()
		return
	var player := Game.player as Player
	if player == null or not player.alive:
		return
	var magnet := 12.0 if Game.has_implant("charognard") else 2.5
	var target := player.global_position
	var d := Vector2(global_position.x - target.x, global_position.z - target.z).length()
	if d < magnet:
		var dir := (target - global_position)
		dir.y = 0.0
		global_position += dir.normalized() * min(d, 14.0 * delta)


func _on_body_entered(body: Node) -> void:
	if body is Player:
		Game.add_scrap(value)
		Game.scrap_picked.emit(value)
		Sfx.play(Game.main, "pickup", -8.0, 0.1)
		Fx.popup(Game.main, global_position + Vector3(0, 1.2, 0), "+%d" % value, Color(1.0, 0.8, 0.3), 32)
		queue_free()
