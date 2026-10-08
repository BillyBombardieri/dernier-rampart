class_name Player
extends CharacterBody3D
## Le joueur en vue FPS : déplacement, tir, marquage, construction et réparation.

const WEAPONS := {
	"pistol": {
		"name": "Pistolet lourd", "damage": 34.0, "rate": 0.28, "auto": false,
		"mag": 10, "reload": 1.2, "heavy": true, "color": Color(1.0, 0.9, 0.6),
	},
	"rifle": {
		"name": "Fusil d'assaut", "damage": 13.0, "rate": 0.09, "auto": true,
		"mag": 30, "reload": 2.0, "heavy": false, "color": Color(1.0, 0.7, 0.4),
	},
}
const WALK_SPEED := 6.0
const SPRINT_SPEED := 9.5
const JUMP_VELOCITY := 6.0
const GRAVITY := 20.0
const MOUSE_SENS := 0.0025
const REPAIR_RATE := 40.0  # PV par seconde
const REPAIR_COST := 10.0  # PV réparés par ferraille

var max_hp := 100.0
var hp := 100.0
var alive := true
var weapon := "pistol"
var ammo := {"pistol": 10, "rifle": 30}
var reloading := 0.0
var respawn_time := 0.0

var _camera: Camera3D
var _fire_cd := 0.0
var _speed_mult := 1.0
var _repair_bank := 0.0
var _spawn_point := Vector3.ZERO


func _ready() -> void:
	collision_layer = Fx.LAYER_PLAYER
	collision_mask = Fx.LAYER_WORLD | Fx.LAYER_ZOMBIES | Fx.LAYER_STRUCTURES
	var shape := CollisionShape3D.new()
	var cs := CapsuleShape3D.new()
	cs.radius = 0.4
	cs.height = 1.8
	shape.shape = cs
	shape.position.y = 0.9
	add_child(shape)
	_camera = Camera3D.new()
	_camera.position.y = 1.6
	_camera.fov = 80.0
	_camera.current = true
	add_child(_camera)
	var gun := Fx.box(Vector3(0.12, 0.15, 0.6), Color(0.2, 0.2, 0.22))
	gun.position = Vector3(0.3, -0.25, -0.5)
	_camera.add_child(gun)
	_spawn_point = global_position
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	apply_implants()


func apply_implants() -> void:
	_speed_mult = 0.85 if Game.has_implant("batterie") else 1.0
	var new_max := 80.0 if Game.has_implant("charognard") else 100.0
	hp = min(hp, new_max)
	max_hp = new_max
	Game.changed.emit()


func weapon_damage(id: String) -> float:
	var dmg: float = WEAPONS[id]["damage"]
	if id == "pistol" and Game.has_implant("main_lourde"):
		dmg *= 1.5
	if id == "rifle" and Game.has_implant("sang_froid"):
		dmg *= 0.8
	return dmg


func reload_time(id: String) -> float:
	var t: float = WEAPONS[id]["reload"]
	return t * (1.5 if Game.has_implant("main_lourde") else 1.0)


func take_damage(amount: float) -> void:
	if not alive or Game.is_over:
		return
	hp -= amount
	Game.changed.emit()
	if hp <= 0.0:
		_die()


func _die() -> void:
	alive = false
	Game.deaths += 1
	respawn_time = 5.0 + 3.0 * (Game.deaths - 1)
	visible = false
	collision_layer = 0
	Game.say("Tu es tombé ! Retour au Cœur dans %d s. Les tours défendent seules." % int(respawn_time))


func _respawn() -> void:
	alive = true
	hp = max_hp
	visible = true
	collision_layer = Fx.LAYER_PLAYER
	global_position = _spawn_point
	velocity = Vector3.ZERO
	Game.changed.emit()


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		rotate_y(-event.relative.x * MOUSE_SENS)
		_camera.rotate_x(-event.relative.y * MOUSE_SENS)
		_camera.rotation.x = clamp(_camera.rotation.x, -1.45, 1.45)
	elif event is InputEventKey and event.pressed and event.keycode == KEY_ESCAPE:
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	elif event is InputEventMouseButton and event.pressed and Input.mouse_mode != Input.MOUSE_MODE_CAPTURED:
		if not get_tree().paused:
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
			get_viewport().set_input_as_handled()


func _physics_process(delta: float) -> void:
	if Game.is_over:
		return
	if not alive:
		respawn_time -= delta
		if respawn_time <= 0.0:
			_respawn()
		Game.hint = "Retour au Cœur dans %d s" % int(ceil(respawn_time))
		return
	_move(delta)
	_handle_weapons(delta)
	_handle_interaction(delta)


func _move(delta: float) -> void:
	if not is_on_floor():
		velocity.y -= GRAVITY * delta
	elif Input.is_action_just_pressed("jump"):
		velocity.y = JUMP_VELOCITY
	var input := Input.get_vector("move_left", "move_right", "move_forward", "move_back")
	var dir := (transform.basis * Vector3(input.x, 0, input.y)).normalized()
	var spd := (SPRINT_SPEED if Input.is_action_pressed("sprint") else WALK_SPEED) * _speed_mult
	velocity.x = dir.x * spd
	velocity.z = dir.z * spd
	move_and_slide()


func _handle_weapons(delta: float) -> void:
	_fire_cd -= delta
	if Input.is_action_just_pressed("weapon_1"):
		_switch("pistol")
	elif Input.is_action_just_pressed("weapon_2"):
		_switch("rifle")
	if reloading > 0.0:
		reloading -= delta
		if reloading <= 0.0:
			ammo[weapon] = WEAPONS[weapon]["mag"]
			Game.changed.emit()
		return
	if Input.is_action_just_pressed("reload") and ammo[weapon] < WEAPONS[weapon]["mag"]:
		_start_reload()
		return
	if Input.mouse_mode != Input.MOUSE_MODE_CAPTURED:
		return
	var wants_fire := Input.is_action_pressed("fire") if WEAPONS[weapon]["auto"] else Input.is_action_just_pressed("fire")
	if wants_fire and _fire_cd <= 0.0:
		_shoot()
	if Input.is_action_just_pressed("mark"):
		var hit := _ray(Fx.LAYER_WORLD | Fx.LAYER_ZOMBIES, 120.0)
		if not hit.is_empty() and hit["collider"] is Zombie:
			(hit["collider"] as Zombie).mark()


func _switch(id: String) -> void:
	if weapon == id:
		return
	weapon = id
	reloading = 0.0
	_fire_cd = 0.25
	Game.changed.emit()


func _start_reload() -> void:
	reloading = reload_time(weapon)
	Game.changed.emit()


func _shoot() -> void:
	if ammo[weapon] <= 0:
		_start_reload()
		return
	_fire_cd = WEAPONS[weapon]["rate"]
	ammo[weapon] -= 1
	var muzzle := _camera.global_position + _camera.global_transform.basis * Vector3(0.3, -0.25, -0.8)
	var hit := _ray(Fx.LAYER_WORLD | Fx.LAYER_ZOMBIES, 150.0)
	var end_point: Vector3 = _camera.global_position - _camera.global_transform.basis.z * 150.0
	if not hit.is_empty():
		end_point = hit["position"]
		if hit["collider"] is Zombie:
			(hit["collider"] as Zombie).take_damage(weapon_damage(weapon), true, WEAPONS[weapon]["heavy"])
	Fx.tracer(Game.main, muzzle, end_point, WEAPONS[weapon]["color"])
	if ammo[weapon] <= 0:
		_start_reload()
	Game.changed.emit()


func _handle_interaction(delta: float) -> void:
	var hit := _ray(Fx.LAYER_STRUCTURES, 7.0)
	var target: Object = null if hit.is_empty() else hit["collider"]
	if target is Tower:
		target = (target as Tower).socket
	Game.hint = ""
	if target is Socket:
		var socket := target as Socket
		if socket.tower == null:
			Game.hint = "E : Mitrailleuse (%d)   C : Cryo (%d)" % [Tower.STATS["gun"]["cost"], Tower.STATS["cryo"]["cost"]]
			if Input.is_action_just_pressed("build_gun"):
				socket.build("gun")
			elif Input.is_action_just_pressed("build_cryo"):
				socket.build("cryo")
		else:
			var t := socket.tower
			var up := "niveau max" if t.level >= Tower.MAX_LEVEL else "E : améliorer (%d)" % t.upgrade_cost()
			var power := "X : éteindre" if t.powered else "X : allumer"
			Game.hint = "%s niv.%d   %s   %s   (énergie %d)" % [t.display_name(), t.level, up, power, t.energy_cost()]
			if Input.is_action_just_pressed("build_gun"):
				t.upgrade()
			elif Input.is_action_just_pressed("toggle_power"):
				t.toggle_power()
	elif target is Structure:
		var s := target as Structure
		if s.hp < s.max_hp:
			Game.hint = "Maintenir E : réparer %s (1 ferraille pour %d PV)" % [s.display_name, int(REPAIR_COST)]
			if Input.is_action_pressed("build_gun"):
				_repair(s, delta)
		else:
			Game.hint = "%s en bon état" % s.display_name


func _repair(s: Structure, delta: float) -> void:
	_repair_bank += REPAIR_RATE * delta
	while _repair_bank >= REPAIR_COST:
		if Game.scrap <= 0:
			Game.hint = "Plus de ferraille pour réparer"
			_repair_bank = 0.0
			return
		Game.scrap -= 1
		_repair_bank -= REPAIR_COST
		s.repair(REPAIR_COST)
		Game.changed.emit()


func _ray(mask: int, length: float) -> Dictionary:
	var from := _camera.global_position
	var to := from - _camera.global_transform.basis.z * length
	var query := PhysicsRayQueryParameters3D.create(from, to, mask, [get_rid()])
	return get_world_3d().direct_space_state.intersect_ray(query)
