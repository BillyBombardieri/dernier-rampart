class_name Zombie
extends CharacterBody3D
## Zombie à forme humaine qui suit le couloir jusqu'au Cœur. Le corps est animé par le code.
## Il s'arrête devant les barrières pour les casser. Il ne s'en prend jamais aux tours : seulement
## aux barrières, au joueur et au Cœur. Certains types chassent le joueur, crachent de l'acide
## (Cracheur), renforcent les autres (Hurleur) ou creusent sous les barrières (Fouisseur).
## États : gelé (Cryo), en feu, chargé (Arc), étourdi (Mortier), enragé (cri du Hurleur), marqué.

const TYPES := {
	"rodeur": {"hp": 60.0, "speed": 1.8, "damage": 8.0, "size": 1.0, "bulk": 1.0, "lean": 0.1, "scrap": 3, "points": 10},
	"coureur": {"hp": 35.0, "speed": 3.9, "damage": 6.0, "size": 0.95, "bulk": 0.85, "lean": 0.35, "scrap": 3, "points": 12},
	"brute": {"hp": 280.0, "speed": 1.15, "damage": 25.0, "size": 1.35, "bulk": 1.5, "lean": 0.15, "scrap": 8, "points": 40},
	"cracheur": {"hp": 55.0, "speed": 1.65, "damage": 14.0, "size": 1.0, "bulk": 0.95, "lean": 0.05, "scrap": 5, "points": 25},
	"hurleur": {"hp": 110.0, "speed": 1.5, "damage": 8.0, "size": 1.15, "bulk": 0.8, "lean": 0.0, "scrap": 8, "points": 35},
	"fouisseur": {"hp": 80.0, "speed": 2.1, "damage": 16.0, "size": 0.95, "bulk": 1.15, "lean": 0.5, "scrap": 6, "points": 30},
	"boss": {"hp": 2400.0, "speed": 1.0, "damage": 45.0, "size": 2.3, "bulk": 1.4, "lean": 0.2, "scrap": 50, "points": 300},
}
const SHIRT_COLORS := [
	Color(0.45, 0.42, 0.38), Color(0.3, 0.35, 0.45), Color(0.5, 0.3, 0.28),
	Color(0.35, 0.4, 0.3), Color(0.6, 0.58, 0.52), Color(0.25, 0.25, 0.27),
]
const GRAVITY := 20.0
const HEADSHOT_MULT := 2.0
const SPIT_RANGE := 14.0
const SCREAM_RADIUS := 10.0
const DECOY_RADIUS := 16.0
const DIG_SPEED := 3.4
const DIG_DISTANCE := 6.0  # Le Fouisseur creuse quand il arrive à cette distance d'une barrière.
const DIG_COOLDOWN := 4.0
const BUFF_SPEED := 1.3
const BUFF_DAMAGE := 1.25

signal died(zombie: Zombie)

var type := "rodeur"
var max_hp := 60.0
var hp := 60.0
var speed := 1.8
var damage := 8.0
var size := 1.0
var dead := false
var marked := false
var frozen_time := 0.0
var burn_time := 0.0
var burn_dps := 0.0
var charged_time := 0.0
var stun_time := 0.0
var buff_time := 0.0
var burrowed := false
var path_index := 1

var _marked_time := 0.0
var _attack_cd := 0.0
var _attack_anim := 0.0
var _hit_kick := 0.0
var _walk_phase := randf() * TAU
var _groan_cd := randf_range(2.0, 8.0)
var _lean := 0.1
var _lane := randf_range(-1.4, 1.4)  # Décalage sur la largeur du chemin : la horde ne marche pas en file.
var _think_cd := 0.0
var _blocker: Barrier = null
var _decoy: Node3D = null
var _spit_target: Node3D = null
var _spit_cd := randf_range(0.5, 1.5)
var _scream_cd := randf_range(2.0, 4.0)
var _scream_anim := 0.0
var _dig_state := ""  # Fouisseur : "walk" (en surface), "dig", "under" puis "rise".
var _dig_timer := 0.0
var _dig_dest := Vector3.ZERO
var _under_time := 0.0
var _rumble_cd := 0.0
var _burn_tick := 0.0
var _anim_time := 0.0
var _rig: Node3D
var _torso: Node3D
var _head: Node3D
var _arms: Array[Node3D] = []
var _legs: Array[Node3D] = []
var _materials: Array[StandardMaterial3D] = []
var _base_tints: Array[Color] = []
var _eye_mat: StandardMaterial3D
var _mark_label: Label3D
var _fire: CPUParticles3D
var _sparks: CPUParticles3D
var _stun_ring: MeshInstance3D
var _mound: Node3D
var _dirt: CPUParticles3D


func setup(p_type: String, hp_scale: float, speed_scale := 1.0, damage_scale := 1.0) -> void:
	type = p_type
	var s: Dictionary = TYPES[type]
	max_hp = s["hp"] * hp_scale
	hp = max_hp
	speed = s["speed"] * speed_scale * randf_range(0.9, 1.1)
	damage = s["damage"] * damage_scale
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
	if type == "fouisseur":
		_dig_state = "walk"
		_build_mound()
	add_to_group("zombies")


func _build_body(bulk: float) -> void:
	var skin_tint := Color(0.85, 0.9, 0.8).lerp(Color(0.7, 0.62, 0.6), randf())
	var shirt_tint: Color = SHIRT_COLORS[randi() % SHIRT_COLORS.size()]
	match type:
		"boss":
			skin_tint = Color(0.6, 0.5, 0.55)
		"cracheur":
			skin_tint = Color(0.62, 0.78, 0.45)
		"hurleur":
			skin_tint = Color(0.82, 0.78, 0.76)
			shirt_tint = Color(0.32, 0.18, 0.15)
		"fouisseur":
			skin_tint = Color(0.55, 0.47, 0.38)
			shirt_tint = Color(0.3, 0.25, 0.2)
	var skin := _mat("skin", skin_tint)
	var shirt := _mat("cloth", shirt_tint)
	var pants := _mat("cloth", Color(0.22, 0.24, 0.3).lerp(Color(0.35, 0.3, 0.25), randf()))
	var dark := Fx.material(Color(0.05, 0.02, 0.02))
	_eye_mat = Fx.material(Color(0.05, 0.02, 0.02))

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
		var eye := Fx.box_mat(Vector3(0.05, 0.03, 0.02), _eye_mat)
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

	# Détails propres à chaque type.
	match type:
		"cracheur":
			# Poche de gorge gonflée d'acide et bouche qui luit en vert.
			var sac := SphereMesh.new()
			sac.radius = 0.11
			sac.height = 0.2
			var glow := Fx.material(Color(0.45, 0.85, 0.2), 0.6)
			var sac_mesh := Fx.mesh_with(sac, glow)
			sac_mesh.position = Vector3(0, -0.06, -0.07)
			_head.add_child(sac_mesh)
			jaw.material_override = Fx.material(Color(0.4, 0.95, 0.2), 2.5)
		"hurleur":
			# Mâchoire démesurée et longs bras.
			jaw.scale = Vector3(1.3, 3.0, 1.2)
			jaw.position.y = -0.03
			jaw.material_override = Fx.material(Color(0.5, 0.05, 0.03), 1.5)
			for arm in _arms:
				arm.scale = Vector3(1.0, 1.25, 1.0)
		"fouisseur":
			# Griffes pour creuser.
			var claw_mat := Fx.material(Color(0.12, 0.1, 0.08))
			for arm in _arms:
				for k in 3:
					var claw := Fx.box_mat(Vector3(0.02, 0.16, 0.02), claw_mat)
					claw.position = Vector3(-0.04 + k * 0.04, -0.82, -0.02)
					claw.rotation.x = -0.4
					arm.add_child(claw)


func _build_mound() -> void:
	# Butte de terre qui avance quand le Fouisseur creuse sous le sol.
	_mound = Node3D.new()
	var dome := SphereMesh.new()
	dome.radius = 0.9
	dome.height = 0.7
	var dirt_mat := Fx.textured("mud", 1.2, Color(0.75, 0.68, 0.6), false)
	var mesh := Fx.mesh_with(dome, dirt_mat)
	mesh.scale = Vector3(1.0, 0.6, 1.4)
	_mound.add_child(mesh)
	_mound.visible = false
	add_child(_mound)
	_dirt = CPUParticles3D.new()
	_dirt.amount = 18
	_dirt.lifetime = 0.8
	_dirt.direction = Vector3.UP
	_dirt.spread = 35.0
	_dirt.initial_velocity_min = 1.5
	_dirt.initial_velocity_max = 3.0
	_dirt.gravity = Vector3(0, -9.8, 0)
	_dirt.local_coords = false
	var box := BoxMesh.new()
	box.size = Vector3.ONE * 0.08
	box.material = Fx.material(Color(0.3, 0.24, 0.17))
	_dirt.mesh = box
	_dirt.emitting = false
	_dirt.position.y = 0.2
	add_child(_dirt)


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
	if burrowed:
		return
	marked = true
	_marked_time = 8.0
	_mark_label.visible = true


func freeze(duration: float) -> void:
	if burrowed:
		return
	frozen_time = max(frozen_time, duration)


## Met le feu au zombie : il perd dps PV par seconde pendant duration secondes.
func ignite(duration: float, dps: float) -> void:
	if dead or burrowed:
		return
	burn_time = maxf(burn_time, duration)
	burn_dps = maxf(burn_dps, dps)
	if _fire == null:
		_fire = Fx.fire_particles(26, 0.35 * size, 0.6)
		_fire.emission_sphere_radius = 0.3 * size
		_fire.position.y = 1.1 * size
		add_child(_fire)


## Charge électrique (Arc électrique, munitions électriques) : prépare la SURCHARGE.
func charge(duration: float) -> void:
	if dead or burrowed:
		return
	charged_time = maxf(charged_time, duration)
	if _sparks == null:
		_sparks = CPUParticles3D.new()
		_sparks.amount = 12
		_sparks.lifetime = 0.25
		_sparks.explosiveness = 0.3
		_sparks.direction = Vector3.UP
		_sparks.spread = 180.0
		_sparks.initial_velocity_min = 1.0
		_sparks.initial_velocity_max = 3.0
		_sparks.gravity = Vector3.ZERO
		_sparks.emission_shape = CPUParticles3D.EMISSION_SHAPE_SPHERE
		_sparks.emission_sphere_radius = 0.45 * size
		var spark := BoxMesh.new()
		spark.size = Vector3(0.03, 0.03, 0.12)
		spark.material = Fx.material(Color(0.65, 0.6, 1.0), 6.0)
		_sparks.mesh = spark
		_sparks.position.y = 1.2 * size
		add_child(_sparks)


func stun(duration: float) -> void:
	if dead or burrowed or type == "boss":
		return
	stun_time = maxf(stun_time, duration)
	if _stun_ring == null:
		var torus := TorusMesh.new()
		torus.inner_radius = 0.22
		torus.outer_radius = 0.26
		torus.ring_segments = 6
		torus.material = Fx.material(Color(1.0, 0.9, 0.3), 3.0)
		_stun_ring = MeshInstance3D.new()
		_stun_ring.mesh = torus
		_stun_ring.position.y = 2.05 * size
		add_child(_stun_ring)


## Cri du Hurleur : plus rapide et plus fort pendant quelques secondes.
func buff(duration: float) -> void:
	if dead:
		return
	buff_time = maxf(buff_time, duration)


func current_speed() -> float:
	var spd := speed * (0.35 if frozen_time > 0.0 else 1.0)
	return spd * (BUFF_SPEED if buff_time > 0.0 else 1.0)


func current_damage() -> float:
	return damage * (BUFF_DAMAGE if buff_time > 0.0 else 1.0)


## hit_pos permet de détecter un tir à la tête (x2) et de placer les éclaboussures.
## ammo = munitions du joueur ("incendiaire", "perforante", "electrique" ou "standard").
func take_damage(amount: float, from_player := false, heavy := false, hit_pos := Vector3.INF, hit_normal := Vector3.UP, ammo := "") -> void:
	if dead or burrowed:
		return
	var where := hit_pos if hit_pos != Vector3.INF else global_position + Vector3(0, 1.3 * size, 0)
	if from_player and hit_pos != Vector3.INF and hit_pos.y > global_position.y + 1.62 * size:
		amount *= HEADSHOT_MULT
		Fx.popup(Game.main, where + Vector3(0, 0.4, 0), "TÊTE", Color(1.0, 0.4, 0.3), 40)
	if from_player and heavy and frozen_time > 0.0:
		# Combo : un tir lourd sur un zombie gelé le brise.
		amount *= 5.0 if Game.has_implant("sang_froid") else 3.0
		frozen_time = 0.0
		Game.add_score(10)
		Fx.popup(Game.main, global_position + Vector3(0, 2.2 * size, 0), "BRISÉ !", Color(0.6, 0.9, 1.0), 64)
		Fx.burst(Game.main, where, Vector3.UP, Color(0.8, 0.95, 1.0), 30, 6.0, 0.1)
	if from_player and charged_time > 0.0 and ammo != "electrique":
		# Combo : un tir sur un zombie chargé libère une onde électrique autour de lui.
		amount += _surcharge()
	Fx.burst(Game.main, where, hit_normal, Color(0.35, 0.02, 0.02), 10 if from_player else 4, 3.5, 0.05)
	if from_player:
		Sfx.play_at(Game.main, "hit_flesh", where, -2.0, 0.15)
	_hit_kick = min(1.0, _hit_kick + amount / max_hp * 3.0 + 0.2)
	hp -= amount
	if hp <= 0.0:
		_die()
		return
	if from_player:
		match ammo:
			"incendiaire":
				ignite(3.0, 6.0 * combo_scale())
			"electrique":
				charge(3.0)


## Les combos suivent la montée en puissance des zombies au fil des vagues.
static func combo_scale() -> float:
	return 1.0 + 0.15 * maxf(0.0, Game.wave - 1)


## SURCHARGE : décharge la cible, blesse et étourdit les zombies à moins de 5 m.
## Renvoie les dégâts subis par la cible elle-même.
func _surcharge() -> float:
	charged_time = 0.0
	var dmg := 45.0 * combo_scale()
	var center := global_position + Vector3(0, 1.2 * size, 0)
	Game.add_score(10)
	Fx.popup(Game.main, global_position + Vector3(0, 2.4 * size, 0), "SURCHARGE !", Color(0.75, 0.65, 1.0), 64)
	Fx.ring_wave(Game.main, global_position + Vector3(0, 0.3, 0), Color(0.6, 0.55, 1.0, 0.9), 5.0, 0.4)
	Fx.flash(Game.main, center, Color(0.6, 0.6, 1.0), 6.0, 9.0, 0.08)
	Sfx.play_at(Game.main, "zap", center, 3.0, 0.1)
	for node in get_tree().get_nodes_in_group("zombies"):
		var z := node as Zombie
		if z == self or z.dead or z.burrowed:
			continue
		if _flat_dist(z.global_position) < 5.0:
			Fx.lightning(Game.main, center, z.global_position + Vector3(0, 1.2 * z.size, 0), Color(0.7, 0.65, 1.0))
			z.stun(0.8)
			z.take_damage(dmg)
	stun(0.8)
	return dmg


func _die() -> void:
	dead = true
	burrowed = false
	remove_from_group("zombies")
	collision_layer = 0
	collision_mask = Fx.LAYER_WORLD
	marked = false
	_mark_label.visible = false
	for fx in [_fire, _sparks, _dirt]:
		if fx:
			fx.emitting = false
	if _stun_ring:
		_stun_ring.visible = false
	if _mound:
		_mound.visible = false
	_rig.visible = true
	Game.kills += 1
	Game.add_score(TYPES[type]["points"])
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


func _apply_dot(amount: float) -> void:
	if dead or burrowed:
		return
	hp -= amount
	if hp <= 0.0:
		_die()


func _physics_process(delta: float) -> void:
	if dead:
		return
	_anim_time += delta
	_update_status(delta)
	if dead:
		return
	if _dig_state != "" and _process_dig(delta):
		return
	_attack_cd -= delta
	_spit_cd -= delta
	_groan_cd -= delta
	if _groan_cd <= 0.0:
		_groan_cd = randf_range(5.0, 12.0)
		Sfx.play_at(Game.main, "groan_%d" % (randi() % 3), global_position + Vector3(0, 1.6, 0), -4.0, 0.15, 40.0)
	if not is_on_floor():
		velocity.y -= GRAVITY * delta
	else:
		velocity.y = 0.0
	if stun_time > 0.0:
		velocity.x = 0.0
		velocity.z = 0.0
		move_and_slide()
		_animate(delta)
		return
	_think_cd -= delta
	if _think_cd <= 0.0:
		_think_cd = randf_range(0.2, 0.3)
		_think()
	if type == "hurleur":
		_scream_cd -= delta
		if _scream_cd <= 0.0:
			_scream_cd = 5.0
			_scream()

	var target: Node3D = null
	var target_pos := global_position
	var reach := 1.4 + 0.4 * size
	var hold := false
	var player: Player = Game.player as Player
	var to_player := INF
	if player and player.alive and _reachable(player.global_position):
		to_player = _flat_dist(player.global_position)
	var chase_radius := 18.0 if type == "coureur" else (4.0 if type == "cracheur" else 7.0)
	var barrier_reach := 0.9 + 0.35 * size

	if is_instance_valid(_decoy) and _decoy.alive:
		target = _decoy
		target_pos = _decoy.global_position
		reach = 1.2 + 0.3 * size
	elif to_player < chase_radius:
		target = player
		target_pos = player.global_position
	elif type == "cracheur" and is_instance_valid(_spit_target):
		# Reste à distance et crache.
		hold = true
		target_pos = _aim_point(_spit_target)
		if _spit_cd <= 0.0:
			_spit(_spit_target)
	elif _blocker and _blocker.alive and _blocker.front_distance(global_position) <= barrier_reach + 0.05:
		target = _blocker
		target_pos = _blocker.contact_point(global_position)
		reach = barrier_reach + 0.1
	else:
		var pts: Array[Vector3] = Game.main.path_points
		if path_index < pts.size():
			target_pos = _lane_point(path_index)
			if _flat_dist(target_pos) < 1.2:
				path_index += 1
		else:
			target = Game.core
			target_pos = Game.core.global_position
			reach = 4.0 + 0.4 * size

	var dir := target_pos - global_position
	dir.y = 0.0
	if hold:
		velocity.x = 0.0
		velocity.z = 0.0
	elif target and _flat_dist(target_pos) <= reach:
		velocity.x = 0.0
		velocity.z = 0.0
		if _attack_cd <= 0.0:
			_attack_cd = 1.0
			_attack_anim = 1.0
			target.take_damage(current_damage())
	elif dir.length() > 0.05:
		dir = dir.normalized()
		var spd := current_speed()
		velocity.x = dir.x * spd
		velocity.z = dir.z * spd
	if dir.length() > 0.05:
		rotation.y = lerp_angle(rotation.y, atan2(-dir.x, -dir.z), min(1.0, delta * 6.0))
	move_and_slide()
	_animate(delta)


## Cherche, quelques fois par seconde, ce qui peut retenir ou attirer le zombie.
func _think() -> void:
	_blocker = _find_blocker()
	_decoy = _find_decoy()
	_spit_target = _find_spit_target() if type == "cracheur" else null


## Vrai si rien ne sépare le zombie de ce point (pas de barrière debout entre les deux).
func _reachable(p: Vector3) -> bool:
	return _blocker == null or not _blocker.alive or not _blocker.is_behind(p)


func _find_blocker() -> Barrier:
	var prog := progress()
	var best: Barrier = null
	for node in get_tree().get_nodes_in_group("barriers"):
		var b := node as Barrier
		if b.alive and prog < b.progress_value and (best == null or b.progress_value < best.progress_value):
			best = b
	return best


func _find_decoy() -> Node3D:
	for node in get_tree().get_nodes_in_group("decoys"):
		var d := node as Node3D
		if d.alive and _flat_dist(d.global_position) < DECOY_RADIUS and _reachable(d.global_position):
			return d
	return null


## Le Cracheur crache de loin sur la barrière qui le bloque, ou sur le joueur s'il est assez près.
## L'acide passe par-dessus la horde. Il ne vise jamais les tours.
func _find_spit_target() -> Node3D:
	var best: Node3D = null
	var best_score := INF
	if _blocker and _blocker.alive:
		var d := _blocker.front_distance(global_position)
		if d >= 0.0 and d <= SPIT_RANGE * 0.6 and d < best_score:
			best_score = d
			best = _blocker
	var player := Game.player as Player
	if player and player.alive:
		var d := _flat_dist(player.global_position)
		if d <= 11.0 and d + 2.0 < best_score:
			best = player
	return best


func _aim_point(t: Node3D) -> Vector3:
	if t is Barrier:
		return (t as Barrier).contact_point(global_position) + Vector3(0, 1.0, 0)
	return t.global_position + Vector3(0, 1.0, 0)


func _spit(t: Node3D) -> void:
	_spit_cd = 2.6
	_attack_anim = 1.0
	var from := global_position + Vector3(0, 1.65 * size, 0) - global_transform.basis.z * 0.3
	var aim := _aim_point(t)
	var flight := clampf(from.distance_to(aim) / 11.0, 0.5, 1.4)
	Projectile.launch(Game.main, "acid", from, aim, flight, current_damage(), t)
	Sfx.play_at(Game.main, "spit", from, -1.0, 0.15)


func _scream() -> void:
	_scream_anim = 1.0
	for node in get_tree().get_nodes_in_group("zombies"):
		var z := node as Zombie
		if z != self and not z.dead and _flat_dist(z.global_position) < SCREAM_RADIUS:
			z.buff(6.0)
	Fx.ring_wave(Game.main, global_position + Vector3(0, 0.25, 0), Color(1.0, 0.35, 0.15, 0.8), SCREAM_RADIUS, 0.6)
	Sfx.play_at(Game.main, "howl", global_position + Vector3(0, 2.0, 0), 3.0, 0.1, 80.0)


func _lane_point(i: int) -> Vector3:
	var pts: Array[Vector3] = Game.main.path_points
	var p: Vector3 = pts[i]
	var prev: Vector3 = pts[i - 1] if i > 0 else p
	var d := Vector3(p.x - prev.x, 0, p.z - prev.z)
	if d.length() < 0.01:
		return p
	d = d.normalized()
	return p + Vector3(-d.z, 0, d.x) * _lane


# ---------------------------------------------------------------- Fouisseur

## Machine à états du Fouisseur. Renvoie vrai quand elle gère seule le déplacement.
func _process_dig(delta: float) -> bool:
	match _dig_state:
		"walk":
			# En surface : creuse dès qu'il arrive devant une barrière debout.
			_dig_timer -= delta
			if _dig_timer <= 0.0 and is_on_floor() and stun_time <= 0.0 and _wants_to_dig():
				_start_dig()
				return true
			return false
		"dig":
			_dig_timer -= delta
			_rig.position.y = lerpf(-2.2 * size, 0.0, clampf(_dig_timer, 0.0, 1.0))
			if _dig_timer <= 0.0:
				_dig_state = "under"
				burrowed = true
				marked = false
				_mark_label.visible = false
				collision_layer = 0
				_rig.visible = false
				_mound.visible = true
				_dirt.emitting = true
			return true
		"under":
			_under_time += delta
			var to := _dig_dest - global_position
			to.y = 0.0
			if to.length() < 0.6 or _under_time > 25.0:
				_start_rise()
				return true
			global_position += to.normalized() * minf(to.length(), DIG_SPEED * delta)
			rotation.y = atan2(-to.x, -to.z)
			_mound.scale = Vector3.ONE * (1.0 + sin(_anim_time * 9.0) * 0.06)
			_rumble_cd -= delta
			if _rumble_cd <= 0.0:
				_rumble_cd = 1.3
				Sfx.play_at(Game.main, "dig", global_position, -8.0, 0.2, 35.0)
			return true
		"rise":
			_dig_timer -= delta
			_rig.position.y = lerpf(0.0, -2.2 * size, clampf(_dig_timer / 0.9, 0.0, 1.0))
			if _dig_timer <= 0.0:
				_dig_state = "walk"
				_dig_timer = DIG_COOLDOWN
				_rig.position.y = 0.0
				_snap_to_path()
			return true
	return false


func _wants_to_dig() -> bool:
	if _blocker == null or not _blocker.alive:
		return false
	var d := _blocker.front_distance(global_position)
	return d >= -0.5 and d <= DIG_DISTANCE


func _start_dig() -> void:
	# Passe sous la barrière en ligne droite et ressort juste derrière, côté Cœur.
	_dig_dest = _blocker.contact_point(global_position) + _blocker.dir * 3.5
	_dig_dest.y = global_position.y
	_dig_state = "dig"
	_dig_timer = 1.0
	_under_time = 0.0
	velocity = Vector3.ZERO
	Fx.burst(Game.main, global_position + Vector3(0, 0.3, 0), Vector3.UP, Color(0.3, 0.24, 0.17), 24, 4.0, 0.1)
	Fx.puff(Game.main, global_position + Vector3(0, 0.4, 0), Color(0.4, 0.33, 0.25, 0.6), 10, 1.0)
	Sfx.play_at(Game.main, "dig", global_position, 0.0, 0.1)


func _start_rise() -> void:
	_dig_state = "rise"
	_dig_timer = 0.9
	burrowed = false
	collision_layer = Fx.LAYER_ZOMBIES
	_rig.visible = true
	_mound.visible = false
	_dirt.emitting = false
	Fx.burst(Game.main, global_position + Vector3(0, 0.3, 0), Vector3.UP, Color(0.3, 0.24, 0.17), 30, 5.0, 0.1)
	Fx.puff(Game.main, global_position + Vector3(0, 0.5, 0), Color(0.4, 0.33, 0.25, 0.7), 14, 1.3)
	Sfx.play_at(Game.main, "dig", global_position, 4.0, 0.1)


## Le Phare force un Fouisseur à sortir de terre.
func force_surface() -> void:
	if _dig_state == "under":
		_start_rise()
		Fx.popup(Game.main, global_position + Vector3(0, 2.5, 0), "DÉBUSQUÉ !", Color(1.0, 0.9, 0.5), 48)


## Après être sorti de terre, reprend le chemin depuis le tronçon le plus proche.
func _snap_to_path() -> void:
	var pts: Array[Vector3] = Game.main.path_points
	var best := INF
	for i in pts.size() - 1:
		var a := Vector2(pts[i].x, pts[i].z)
		var b := Vector2(pts[i + 1].x, pts[i + 1].z)
		var p := Vector2(global_position.x, global_position.z)
		var ab := b - a
		var t := clampf((p - a).dot(ab) / ab.length_squared(), 0.0, 1.0)
		var d := p.distance_to(a + ab * t)
		if d < best:
			best = d
			path_index = i + 1


# ---------------------------------------------------------------- animation et états

func _animate(delta: float) -> void:
	var moving := Vector2(velocity.x, velocity.z).length()
	_walk_phase += delta * moving * 2.4 / size
	var stride := clampf(moving / 2.0, 0.0, 1.0) * (0.75 if type == "coureur" else 0.5)
	_legs[0].rotation.x = sin(_walk_phase) * stride
	_legs[1].rotation.x = -sin(_walk_phase) * stride
	_hit_kick = move_toward(_hit_kick, 0.0, delta * 3.0)
	_attack_anim = move_toward(_attack_anim, 0.0, delta * 2.5)
	_scream_anim = move_toward(_scream_anim, 0.0, delta * 1.1)
	var scream := sin(clampf(_scream_anim, 0.0, 1.0) * PI * 0.5)
	# Penché vers l'avant, recule quand il est touché.
	_torso.rotation.x = -_lean - sin(_walk_phase * 2.0) * 0.04 + _hit_kick * 0.5 + scream * 0.55
	_torso.rotation.z = sin(_walk_phase) * 0.06
	_torso.position.y = 0.95 + absf(sin(_walk_phase)) * 0.04
	_head.rotation.z = sin(_walk_phase * 0.5) * 0.15
	_head.rotation.x = 0.1 + _hit_kick * 0.6 - scream * 0.7
	if stun_time > 0.0:
		# Étourdi : tête et buste qui vacillent.
		_torso.rotation.z = sin(_anim_time * 11.0) * 0.18
		_head.rotation.z = sin(_anim_time * 7.0) * 0.35
	# Bras tendus vers l'avant qui ballottent ; ils frappent vers le bas pendant une attaque.
	var swing := sin(_attack_anim * PI) * 1.2
	if type == "cracheur":
		# Le Cracheur se cambre puis projette la tête en avant.
		_torso.rotation.x += sin(_attack_anim * PI) * 0.4 - sin(_attack_anim * PI * 2.0) * 0.25
		swing *= 0.2
	_arms[0].rotation.x = 1.35 + sin(_walk_phase + 0.5) * 0.15 - swing - scream * 1.0
	_arms[1].rotation.x = 1.25 - sin(_walk_phase + 0.5) * 0.15 - swing - scream * 1.0
	_arms[0].rotation.z = 0.08 + scream * 0.9
	_arms[1].rotation.z = -0.08 - scream * 0.9


func _update_status(delta: float) -> void:
	if frozen_time > 0.0:
		frozen_time -= delta
	if marked:
		_marked_time -= delta
		if _marked_time <= 0.0:
			marked = false
			_mark_label.visible = false
	buff_time = maxf(0.0, buff_time - delta)
	stun_time = maxf(0.0, stun_time - delta)
	charged_time = maxf(0.0, charged_time - delta)
	if burn_time > 0.0:
		burn_time -= delta
		_burn_tick += delta
		if _burn_tick >= 0.25:
			_apply_dot(burn_dps * _burn_tick)
			_burn_tick = 0.0
			if dead:
				return
	else:
		burn_dps = 0.0
	if _fire:
		_fire.emitting = burn_time > 0.0 and not burrowed
	if _sparks:
		_sparks.emitting = charged_time > 0.0 and not burrowed
	if _stun_ring:
		_stun_ring.visible = stun_time > 0.0
		_stun_ring.rotation.y += delta * 6.0
	# Yeux rouges et lumineux quand le Hurleur l'a enragé.
	var enraged := buff_time > 0.0
	if _eye_mat.emission_enabled != enraged:
		_eye_mat.emission_enabled = enraged
		_eye_mat.emission = Color(1.0, 0.15, 0.05)
		_eye_mat.emission_energy_multiplier = 5.0
	var ice := 0.65 if frozen_time > 0.0 else 0.0
	var char_amount := 0.45 if burn_time > 0.0 else 0.0
	var volt := (0.35 + 0.25 * sin(_anim_time * 25.0)) if charged_time > 0.0 else 0.0
	for i in _materials.size():
		var c := _base_tints[i].lerp(Color(0.7, 0.9, 1.0), ice)
		c = c.lerp(Color(0.15, 0.08, 0.05), char_amount)
		_materials[i].albedo_color = c.lerp(Color(0.65, 0.6, 1.0), volt)


func _flat_dist(p: Vector3) -> float:
	return Vector2(global_position.x - p.x, global_position.z - p.z).length()
