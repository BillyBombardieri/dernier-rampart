class_name Zombie
extends CharacterBody3D
## Zombie à forme humaine qui suit le couloir jusqu'au Cœur.
## Certains types chassent le joueur ou cassent les bâtiments. Le corps est animé par le code.

const TYPES := {
	"rodeur": {"hp": 60.0, "speed": 2.4, "damage": 8.0, "size": 1.0, "bulk": 1.0, "lean": 0.1, "scrap": 3},
	"coureur": {"hp": 35.0, "speed": 5.2, "damage": 6.0, "size": 0.95, "bulk": 0.85, "lean": 0.35, "scrap": 3},
	"brute": {"hp": 280.0, "speed": 1.5, "damage": 25.0, "size": 1.35, "bulk": 1.5, "lean": 0.15, "scrap": 8},
	"boss": {"hp": 1600.0, "speed": 1.3, "damage": 45.0, "size": 2.3, "bulk": 1.4, "lean": 0.2, "scrap": 50},
}
const SHIRT_COLORS := [
	Color(0.45, 0.42, 0.38), Color(0.3, 0.35, 0.45), Color(0.5, 0.3, 0.28),
	Color(0.35, 0.4, 0.3), Color(0.6, 0.58, 0.52), Color(0.25, 0.25, 0.27),
]
const GRAVITY := 20.0
const HEADSHOT_MULT := 2.0

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
var _attack_anim := 0.0
var _hit_kick := 0.0
var _walk_phase := randf() * TAU
var _groan_cd := randf_range(2.0, 8.0)
var _lean := 0.1
var _rig: Node3D
var _torso: Node3D
var _head: Node3D
var _arms: Array[Node3D] = []
var _legs: Array[Node3D] = []
var _materials: Array[StandardMaterial3D] = []
var _base_tints: Array[Color] = []
var _mark_label: Label3D


func setup(p_type: String, hp_scale: float) -> void:
	type = p_type
	var s: Dictionary = TYPES[type]
	max_hp = s["hp"] * hp_scale
	hp = max_hp
	speed = s["speed"] * randf_range(0.9, 1.1)
	damage = s["damage"]
	size = s["size"]
	_lean = s["lean"]
	collision_layer = Fx.LAYER_ZOMBIES
	collision_mask = Fx.LAYER_WORLD | Fx.LAYER_PLAYER
	var shape := CollisionShape3D.new()
	var cs := CapsuleShape3D.new()
	cs.radius = 0.35 * size * s["bulk"]
	cs.height = 1.9 * size
	shape.shape = cs
	shape.position.y = 0.95 * size
	add_child(shape)
	_build_body(s["bulk"])
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


func _build_body(bulk: float) -> void:
	var skin_tint := Color(0.85, 0.9, 0.8).lerp(Color(0.7, 0.62, 0.6), randf())
	if type == "boss":
		skin_tint = Color(0.6, 0.5, 0.55)
	var skin := _mat("skin", skin_tint)
	var shirt := _mat("cloth", SHIRT_COLORS[randi() % SHIRT_COLORS.size()])
	var pants := _mat("cloth", Color(0.22, 0.24, 0.3).lerp(Color(0.35, 0.3, 0.25), randf()))
	var dark := Fx.material(Color(0.05, 0.02, 0.02))

	_rig = Node3D.new()
	_rig.scale = Vector3(size * bulk, size, size * bulk)
	add_child(_rig)

	# Jambes (pivot à la hanche).
	for side in [-1.0, 1.0]:
		var hip := Node3D.new()
		hip.position = Vector3(0.12 * side, 0.95, 0)
		_rig.add_child(hip)
		var leg := Fx.mesh_with(_capsule(0.09, 0.95), pants)
		leg.position.y = -0.47
		hip.add_child(leg)
		var foot := Fx.box_mat(Vector3(0.12, 0.08, 0.26), dark)
		foot.position = Vector3(0, -0.92, -0.06)
		hip.add_child(foot)
		_legs.append(hip)

	# Torse (pivot au bassin pour pouvoir le pencher).
	_torso = Node3D.new()
	_torso.position.y = 0.95
	_rig.add_child(_torso)
	var chest := Fx.mesh_with(_capsule(0.2, 0.75), shirt)
	chest.position.y = 0.36
	chest.scale = Vector3(1.15, 1.0, 0.8)
	_torso.add_child(chest)

	_head = Node3D.new()
	_head.position.y = 0.8
	_torso.add_child(_head)
	var skull := SphereMesh.new()
	skull.radius = 0.13
	skull.height = 0.3
	var head_mesh := Fx.mesh_with(skull, skin)
	head_mesh.position.y = 0.06
	_head.add_child(head_mesh)
	for side in [-1.0, 1.0]:
		var eye := Fx.box_mat(Vector3(0.05, 0.03, 0.02), dark)
		eye.position = Vector3(0.045 * side, 0.09, -0.12)
		_head.add_child(eye)
	var jaw := Fx.box_mat(Vector3(0.12, 0.04, 0.03), dark)
	jaw.position = Vector3(0, 0.0, -0.12)
	_head.add_child(jaw)

	# Bras tendus vers l'avant (pivot à l'épaule).
	for side in [-1.0, 1.0]:
		var shoulder := Node3D.new()
		shoulder.position = Vector3(0.27 * side, 0.65, 0)
		_torso.add_child(shoulder)
		var sleeve := Fx.mesh_with(_capsule(0.07, 0.4), shirt)
		sleeve.position.y = -0.18
		shoulder.add_child(sleeve)
		var forearm := Fx.mesh_with(_capsule(0.055, 0.45), skin)
		forearm.position.y = -0.52
		shoulder.add_child(forearm)
		_arms.append(shoulder)


func _mat(tex: String, tint: Color) -> StandardMaterial3D:
	var m := Fx.textured(tex, 3.0, tint, false)
	_materials.append(m)
	_base_tints.append(tint)
	return m


func _capsule(radius: float, height: float) -> CapsuleMesh:
	var c := CapsuleMesh.new()
	c.radius = radius
	c.height = height
	c.radial_segments = 10
	c.rings = 4
	return c


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


## hit_pos permet de détecter un tir à la tête (x2) et de placer les éclaboussures.
func take_damage(amount: float, from_player: bool, heavy: bool, hit_pos := Vector3.INF, hit_normal := Vector3.UP) -> void:
	if dead:
		return
	var where := hit_pos if hit_pos != Vector3.INF else global_position + Vector3(0, 1.3 * size, 0)
	if from_player and hit_pos != Vector3.INF and hit_pos.y > global_position.y + 1.62 * size:
		amount *= HEADSHOT_MULT
		Fx.popup(Game.main, where + Vector3(0, 0.4, 0), "TÊTE", Color(1.0, 0.4, 0.3), 40)
	if from_player and heavy and frozen_time > 0.0:
		# Combo : un tir lourd sur un zombie gelé le brise.
		amount *= 5.0 if Game.has_implant("sang_froid") else 3.0
		frozen_time = 0.0
		Fx.popup(Game.main, global_position + Vector3(0, 2.2 * size, 0), "BRISÉ !", Color(0.6, 0.9, 1.0), 64)
		Fx.burst(Game.main, where, Vector3.UP, Color(0.8, 0.95, 1.0), 30, 6.0, 0.1)
	Fx.burst(Game.main, where, hit_normal, Color(0.35, 0.02, 0.02), 10 if from_player else 4, 3.5, 0.05)
	if from_player:
		Sfx.play_at(Game.main, "hit_flesh", where, -2.0, 0.15)
	_hit_kick = min(1.0, _hit_kick + amount / max_hp * 3.0 + 0.2)
	hp -= amount
	if hp <= 0.0:
		_die()


func _die() -> void:
	dead = true
	remove_from_group("zombies")
	collision_layer = 0
	collision_mask = Fx.LAYER_WORLD
	marked = false
	_mark_label.visible = false
	var value: int = TYPES[type]["scrap"]
	var pieces := clampi(value / 3, 1, 8)
	for i in pieces:
		var s := Scrap.new()
		Game.main.add_child(s)
		var offset := Vector3(randf_range(-1, 1), 0.5, randf_range(-1, 1))
		s.setup(int(ceil(float(value) / pieces)), global_position + offset)
	Sfx.play_at(Game.main, "zombie_death", global_position, 0.0, 0.2)
	died.emit(self)
	# Le corps tombe en arrière, reste au sol un moment puis s'enfonce.
	var tween := create_tween()
	tween.tween_property(_rig, "rotation:x", PI * 0.5 * (1 if randf() < 0.5 else -1), 0.55).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_IN)
	tween.parallel().tween_property(_rig, "position:y", 0.2 * size, 0.55)
	tween.tween_interval(4.0)
	tween.tween_property(_rig, "position:y", -1.0 * size, 1.5)
	tween.tween_callback(queue_free)


func _physics_process(delta: float) -> void:
	if dead:
		return
	_update_status(delta)
	_attack_cd -= delta
	_groan_cd -= delta
	if _groan_cd <= 0.0:
		_groan_cd = randf_range(5.0, 12.0)
		Sfx.play_at(Game.main, "groan_%d" % (randi() % 3), global_position + Vector3(0, 1.6, 0), -4.0, 0.15, 40.0)
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
			_attack_anim = 1.0
			target.take_damage(damage)
	elif dir.length() > 0.05:
		dir = dir.normalized()
		var spd := speed * (0.35 if frozen_time > 0.0 else 1.0)
		velocity.x = dir.x * spd
		velocity.z = dir.z * spd
	if dir.length() > 0.05:
		rotation.y = lerp_angle(rotation.y, atan2(-dir.x, -dir.z), min(1.0, delta * 6.0))
	move_and_slide()
	_animate(delta)


func _animate(delta: float) -> void:
	var moving := Vector2(velocity.x, velocity.z).length()
	_walk_phase += delta * moving * 2.4 / size
	var stride := clampf(moving / 2.0, 0.0, 1.0) * (0.75 if type == "coureur" else 0.5)
	_legs[0].rotation.x = sin(_walk_phase) * stride
	_legs[1].rotation.x = -sin(_walk_phase) * stride
	_hit_kick = move_toward(_hit_kick, 0.0, delta * 3.0)
	_attack_anim = move_toward(_attack_anim, 0.0, delta * 2.5)
	# Penché vers l'avant, recule quand il est touché.
	_torso.rotation.x = -_lean - sin(_walk_phase * 2.0) * 0.04 + _hit_kick * 0.5
	_torso.rotation.z = sin(_walk_phase) * 0.06
	_torso.position.y = 0.95 + absf(sin(_walk_phase)) * 0.04
	_head.rotation.z = sin(_walk_phase * 0.5) * 0.15
	_head.rotation.x = 0.1 + _hit_kick * 0.6
	# Bras tendus vers l'avant qui ballottent ; ils frappent vers le bas pendant une attaque.
	var swing := sin(_attack_anim * PI) * 1.2
	_arms[0].rotation.x = 1.35 + sin(_walk_phase + 0.5) * 0.15 - swing
	_arms[1].rotation.x = 1.25 - sin(_walk_phase + 0.5) * 0.15 - swing
	_arms[0].rotation.z = 0.08
	_arms[1].rotation.z = -0.08


func _update_status(delta: float) -> void:
	if frozen_time > 0.0:
		frozen_time -= delta
	if marked:
		_marked_time -= delta
		if _marked_time <= 0.0:
			marked = false
			_mark_label.visible = false
	var ice := 0.65 if frozen_time > 0.0 else 0.0
	for i in _materials.size():
		_materials[i].albedo_color = _base_tints[i].lerp(Color(0.7, 0.9, 1.0), ice)


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
