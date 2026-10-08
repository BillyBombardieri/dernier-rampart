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
	_base_y = 0.4
	collision_layer = Fx.LAYER_PICKUPS
	collision_mask = Fx.LAYER_PLAYER
	monitoring = true
	var shape := CollisionShape3D.new()
	var ss := SphereShape3D.new()
	ss.radius = 0.8
	shape.shape = ss
	shape.position.y = 0.5
	add_child(shape)
	_mesh = Fx.box(Vector3(0.35, 0.35, 0.35), Color(1.0, 0.75, 0.2), 2.0)
	_mesh.position.y = _base_y
	add_child(_mesh)
	add_to_group("scrap")
	body_entered.connect(_on_body_entered)


func _physics_process(delta: float) -> void:
	_time += delta
	_mesh.position.y = _base_y + sin(_time * 4.0) * 0.12
	_mesh.rotation.y += delta * 2.0
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
		Fx.popup(Game.main, global_position + Vector3(0, 1.2, 0), "+%d" % value, Color(1.0, 0.8, 0.3), 32)
		queue_free()
