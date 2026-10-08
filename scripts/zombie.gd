class_name Zombie
extends CharacterBody3D
## Zombie qui suit le couloir jusqu'au Cœur. Certains types chassent le joueur ou cassent les bâtiments.

const TYPES := {
	"rodeur": {"hp": 60.0, "speed": 2.4, "damage": 8.0, "size": 1.0, "color": Color(0.35, 0.6, 0.3), "scrap": 3},
	"coureur": {"hp": 35.0, "speed": 5.2, "damage": 6.0, "size": 0.85, "color": Color(0.8, 0.75, 0.25), "scrap": 3},
	"brute": {"hp": 280.0, "speed": 1.5, "damage": 25.0, "size": 1.5, "color": Color(0.55, 0.15, 0.15), "scrap": 8},
	"boss": {"hp": 1600.0, "speed": 1.3, "damage": 45.0, "size": 2.4, "color": Color(0.45, 0.15, 0.6), "scrap": 50},
}
const GRAVITY := 20.0

signal died(zombie: Zombie)

var type := "rodeur"
var max_hp := 60.0
var hp := 60.0
var speed := 2.4
var damage := 8.0
var size := 1.0
var dead := false
var marked := false
var frozen_time := 0.0
var path_index := 1

var _marked_time := 0.0
var _attack_cd := 0.0
var _body_mesh: MeshInstance3D
var _mark_label: Label3D
var _base_color := Color.WHITE


func setup(p_type: String, hp_scale: float) -> void:
	type = p_type
	var s: Dictionary = TYPES[type]
	max_hp = s["hp"] * hp_scale
	hp = max_hp
	speed = s["speed"]
	damage = s["damage"]
	size = s["size"]
	_base_color = s["color"]
	collision_layer = Fx.LAYER_ZOMBIES
	collision_mask = Fx.LAYER_WORLD | Fx.LAYER_PLAYER
	var shape := CollisionShape3D.new()
	var cs := CapsuleShape3D.new()
	cs.radius = 0.4 * size
	cs.height = 1.8 * size
	shape.shape = cs
	shape.position.y = 0.9 * size
	add_child(shape)
	_body_mesh = Fx.capsule(0.4 * size, 1.8 * size, _base_color)
	_body_mesh.position.y = 0.9 * size
	add_child(_body_mesh)
	var eyes := Fx.box(Vector3(0.5, 0.1, 0.1) * size, Color(1, 0.2, 0.1), 4.0)
	eyes.position = Vector3(0, 1.5, -0.35) * size
	add_child(eyes)
	_mark_label = Label3D.new()
	_mark_label.text = "▼ MARQUÉ"
	_mark_label.modulate = Color(1, 0.25, 0.25)
	_mark_label.font_size = 36
	_mark_label.outline_size = 6
	_mark_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	_mark_label.no_depth_test = true
	_mark_label.position.y = 2.2 * size
	_mark_label.visible = false
	add_child(_mark_label)
	add_to_group("zombies")


## Score d'avancée sur le couloir (plus c'est haut, plus il est proche du Cœur).
func progress() -> float:
	var pts: Array[Vector3] = Game.main.path_points
	if path_index >= pts.size():
		return 1000.0 * pts.size()
	return 1000.0 * path_index - _flat_dist(pts[path_index])


func mark() -> void:
	marked = true
	_marked_time = 8.0
	_mark_label.visible = true


func freeze(duration: float) -> void:
	frozen_time = max(frozen_time, duration)


func take_damage(amount: float, from_player: bool, heavy: bool) -> void:
	if dead:
		return
	if from_player and heavy and frozen_time > 0.0:
		# Combo : un tir lourd sur un zombie gelé le brise.
		amount *= 5.0 if Game.has_implant("sang_froid") else 3.0
		frozen_time = 0.0
		Fx.popup(Game.main, global_position + Vector3(0, 2.2 * size, 0), "BRISÉ !", Color(0.6, 0.9, 1.0), 64)
	hp -= amount
	if hp <= 0.0:
		_die()


func _die() -> void:
	dead = true
	var value: int = TYPES[type]["scrap"]
	var pieces := clampi(value / 3, 1, 8)
	for i in pieces:
		var s := Scrap.new()
		Game.main.add_child(s)
		var offset := Vector3(randf_range(-1, 1), 0.5, randf_range(-1, 1))
		s.setup(int(ceil(float(value) / pieces)), global_position + offset)
	died.emit(self)
	queue_free()


func _physics_process(delta: float) -> void:
	if dead:
		return
	_update_status(delta)
	_attack_cd -= delta
	if not is_on_floor():
		velocity.y -= GRAVITY * delta
	else:
		velocity.y = 0.0

	var target_pos := global_position
	var target: Node3D = null
	var reach := 1.4 + 0.4 * size
	var player: Player = Game.player as Player
	var to_player := INF
	if player and player.alive:
		to_player = _flat_dist(player.global_position)
	var chase_radius := 14.0 if type == "coureur" else 4.0

	var structure: Node3D = null
	if type == "brute" or type == "boss":
		structure = _find_structure()

	if to_player < chase_radius:
		target = player
		target_pos = player.global_position
	elif structure:
		target = structure
		target_pos = structure.global_position
		reach = 2.2 + 0.4 * size
	else:
		var pts: Array[Vector3] = Game.main.path_points
		if path_index < pts.size():
			target_pos = pts[path_index]
			if _flat_dist(target_pos) < 1.2:
				path_index += 1
		else:
			target = Game.core
			target_pos = Game.core.global_position
			reach = 4.0 + 0.4 * size

	var dir := target_pos - global_position
	dir.y = 0.0
	if target and _flat_dist(target_pos) <= reach:
		velocity.x = 0.0
		velocity.z = 0.0
		if _attack_cd <= 0.0:
			_attack_cd = 1.0
			target.take_damage(damage)
	elif dir.length() > 0.05:
		dir = dir.normalized()
		var spd := speed * (0.35 if frozen_time > 0.0 else 1.0)
		velocity.x = dir.x * spd
		velocity.z = dir.z * spd
	if dir.length() > 0.05:
		rotation.y = atan2(-dir.x, -dir.z)
	move_and_slide()


func _update_status(delta: float) -> void:
	if frozen_time > 0.0:
		frozen_time -= delta
	if marked:
		_marked_time -= delta
		if _marked_time <= 0.0:
			marked = false
			_mark_label.visible = false
	var color := _base_color
	if frozen_time > 0.0:
		color = color.lerp(Color(0.6, 0.9, 1.0), 0.7)
	(_body_mesh.mesh.material as StandardMaterial3D).albedo_color = color


func _find_structure() -> Node3D:
	var best: Node3D = null
	var best_d := 7.0
	for node in get_tree().get_nodes_in_group("attackable"):
		var n := node as Node3D
		if n is Structure and not (n as Structure).alive:
			continue
		var d := _flat_dist(n.global_position)
		if d < best_d:
			best_d = d
			best = n
	return best


func _flat_dist(p: Vector3) -> float:
	return Vector2(global_position.x - p.x, global_position.z - p.z).length()
