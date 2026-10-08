class_name Player
extends CharacterBody3D
## Le joueur en vue FPS : déplacement, tir (recul, visée, flash), marquage, construction et réparation.

const WEAPONS := {
	"pistol": {
		"name": "Pistolet lourd", "damage": 34.0, "rate": 0.28, "auto": false,
		"mag": 10, "reload": 1.2, "heavy": true, "sound": "pistol",
		"spread": 0.004,
		# Recul : montée de la vue (rad), dérive latérale, vitesse de retour, coup de l'arme.
		"kick_up": 0.035, "kick_side": 0.008, "recovery": 9.0, "punch": 1.0,
	},
	"rifle": {
		"name": "Fusil d'assaut", "damage": 13.0, "rate": 0.09, "auto": true,
		"mag": 30, "reload": 2.0, "heavy": false, "sound": "rifle",
		"spread": 0.02,
		"kick_up": 0.011, "kick_side": 0.006, "recovery": 5.0, "punch": 0.45,
	},
}
const WALK_SPEED := 5.5
const SPRINT_SPEED := 8.5
const AIM_SPEED := 3.2
const JUMP_VELOCITY := 5.5
const GRAVITY := 20.0
const MOUSE_SENS := 0.0025
const FOV := 78.0
const AIM_FOV := 55.0
const REPAIR_RATE := 40.0  # PV par seconde
const REPAIR_COST := 10.0  # PV réparés par ferraille
const VIEWMODEL_LAYER := 2  # Couche visuelle de l'arme : la lampe torche ne l'éclaire pas.

var max_hp := 100.0
var hp := 100.0
var alive := true
var weapon := "pistol"
var ammo := {"pistol": 10, "rifle": 30}
var reloading := 0.0
var respawn_time := 0.0
var aiming := false

var _camera: Camera3D
var _gun_root: Node3D
var _models := {}
var _muzzle_flash: MeshInstance3D
var _flashlight: SpotLight3D
var _fire_cd := 0.0
var _speed_mult := 1.0
var _repair_bank := 0.0
var _spawn_point := Vector3.ZERO
var _recoil := 0.0
var _bob := 0.0
var _step_time := 0.0
var _step_left := false
var _was_on_floor := true
var _aim_pitch := 0.0
var _kick := Vector2.ZERO  # Recul actuel de la vue (montée, côté).
var _kick_target := Vector2.ZERO
var _since_shot := 1.0
var _burst := 0
var _vm_kick := 0.0  # Ressort de l'arme : position et vitesse.
var _vm_kick_vel := 0.0
var _fov_punch := 0.0
var _gun_base_pos := Vector3.ZERO
var _gun_base_rot := Vector3.ZERO
var _shake := 0.0
var _aim_blend := 0.0
var _switch_anim := 0.0


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
	_camera.fov = FOV
	_camera.near = 0.03
	_camera.current = true
	add_child(_camera)
	_build_viewmodels()
	_flashlight = SpotLight3D.new()
	_flashlight.light_color = Color(1.0, 0.95, 0.85)
	_flashlight.light_energy = 3.0
	_flashlight.spot_range = 30.0
	_flashlight.spot_angle = 24.0
	_flashlight.shadow_enabled = true
	_flashlight.position = Vector3(-0.2, 0.05, 0.1)
	_flashlight.light_cull_mask = ~VIEWMODEL_LAYER
	_camera.add_child(_flashlight)
	# Petite lumière d'appoint qui n'éclaire que l'arme, pour qu'elle reste lisible la nuit.
	var fill := OmniLight3D.new()
	fill.light_color = Color(1.0, 0.9, 0.8)
	fill.light_energy = 1.6
	fill.omni_range = 1.5
	fill.light_cull_mask = VIEWMODEL_LAYER
	fill.position = Vector3(0.3, 0.3, 0.2)
	_camera.add_child(fill)
	_spawn_point = global_position
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	apply_implants()


func _build_viewmodels() -> void:
	_gun_root = Node3D.new()
	_gun_root.scale = Vector3.ONE * 0.6
	_camera.add_child(_gun_root)
	var steel := Fx.material(Color(0.16, 0.16, 0.17))
	steel.metallic = 0.85
	steel.roughness = 0.35
	var polymer := Fx.material(Color(0.05, 0.05, 0.05))
	polymer.roughness = 0.75

	var pistol := Node3D.new()
	_part(pistol, Vector3(0.05, 0.05, 0.24), Vector3(0, 0.03, -0.05), steel)  # glissière
	_part(pistol, Vector3(0.045, 0.035, 0.2), Vector3(0, -0.01, -0.04), polymer)  # carcasse
	var grip := _part(pistol, Vector3(0.042, 0.13, 0.06), Vector3(0, -0.08, 0.04), polymer)
	grip.rotation.x = -0.25
	_part(pistol, Vector3(0.012, 0.012, 0.012), Vector3(0, 0.06, -0.15), steel)  # guidon
	_models["pistol"] = pistol
	_gun_root.add_child(pistol)

	var rifle := Node3D.new()
	_part(rifle, Vector3(0.06, 0.08, 0.36), Vector3(0, 0, 0), steel)  # boîte de culasse
	_part(rifle, Vector3(0.065, 0.07, 0.25), Vector3(0, 0, -0.3), polymer)  # garde-main
	var barrel := Fx.mesh_with(_cyl(0.012, 0.25), steel)
	barrel.rotation.x = PI * 0.5
	barrel.position = Vector3(0, 0.01, -0.53)
	barrel.layers = VIEWMODEL_LAYER
	rifle.add_child(barrel)
	var mag := _part(rifle, Vector3(0.035, 0.16, 0.07), Vector3(0, -0.11, -0.06), steel)
	mag.rotation.x = 0.2
	var rgrip := _part(rifle, Vector3(0.04, 0.11, 0.05), Vector3(0, -0.08, 0.1), polymer)
	rgrip.rotation.x = -0.3
	_part(rifle, Vector3(0.05, 0.08, 0.22), Vector3(0, -0.02, 0.28), polymer)  # crosse
	_part(rifle, Vector3(0.03, 0.03, 0.08), Vector3(0, 0.06, -0.02), polymer)  # viseur
	_models["rifle"] = rifle
	_gun_root.add_child(rifle)

	var flash_mesh := QuadMesh.new()
	flash_mesh.size = Vector2(0.18, 0.18)
	var fm := StandardMaterial3D.new()
	fm.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	fm.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	fm.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	fm.albedo_color = Color(1.0, 0.75, 0.35)
	fm.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
	flash_mesh.material = fm
	_muzzle_flash = MeshInstance3D.new()
	_muzzle_flash.mesh = flash_mesh
	_muzzle_flash.visible = false
	_gun_root.add_child(_muzzle_flash)
	_show_model()


func _part(parent: Node3D, size: Vector3, pos: Vector3, mat: Material) -> MeshInstance3D:
	var mi := Fx.box_mat(size, mat)
	mi.position = pos
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	mi.layers = VIEWMODEL_LAYER
	parent.add_child(mi)
	return mi


func _cyl(radius: float, height: float) -> CylinderMesh:
	var c := CylinderMesh.new()
	c.top_radius = radius
	c.bottom_radius = radius
	c.height = height
	c.radial_segments = 10
	return c


func _show_model() -> void:
	for id in _models:
		_models[id].visible = id == weapon
	_muzzle_flash.position = Vector3(0, 0.03, -0.2) if weapon == "pistol" else Vector3(0, 0.01, -0.7)


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
	_shake = min(1.0, _shake + amount / 30.0)
	Sfx.play(self, "hurt", -4.0, 0.15)
	Game.player_hurt.emit(amount)
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
		var sens := MOUSE_SENS * (0.6 if aiming else 1.0)
		rotate_y(-event.relative.x * sens)
		var pitch_delta: float = -event.relative.y * sens
		# Tirer la souris vers le bas compense d'abord le recul, puis bouge la visée.
		if pitch_delta < 0.0 and _kick_target.x > 0.0:
			var used: float = min(_kick_target.x, -pitch_delta)
			_kick_target.x -= used
			_kick.x -= used
			pitch_delta += used
		_aim_pitch = clamp(_aim_pitch + pitch_delta, -1.45, 1.45)
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
	if Input.is_action_just_pressed("flashlight"):
		_flashlight.visible = not _flashlight.visible
	_move(delta)
	_handle_weapons(delta)
	_handle_interaction(delta)
	_animate_view(delta)


func _move(delta: float) -> void:
	if not is_on_floor():
		velocity.y -= GRAVITY * delta
	elif Input.is_action_just_pressed("jump"):
		velocity.y = JUMP_VELOCITY
	var input := Input.get_vector("move_left", "move_right", "move_forward", "move_back")
	var dir := (transform.basis * Vector3(input.x, 0, input.y)).normalized()
	var sprinting := Input.is_action_pressed("sprint") and not aiming and input.y < 0.0
	var spd := (AIM_SPEED if aiming else (SPRINT_SPEED if sprinting else WALK_SPEED)) * _speed_mult
	# Accélération progressive : on ne s'arrête pas net.
	var accel := 12.0 if is_on_floor() else 3.0
	velocity.x = move_toward(velocity.x, dir.x * spd, accel * spd * delta)
	velocity.z = move_toward(velocity.z, dir.z * spd, accel * spd * delta)
	move_and_slide()


func _handle_weapons(delta: float) -> void:
	_fire_cd -= delta
	aiming = Input.is_action_pressed("aim") and reloading <= 0.0 and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED
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
	if wants_fire and _fire_cd <= 0.0 and _switch_anim <= 0.0:
		_shoot()
	if Input.is_action_just_pressed("mark"):
		var hit := _ray(Fx.LAYER_WORLD | Fx.LAYER_ZOMBIES, 120.0, Vector2.ZERO)
		if not hit.is_empty() and hit["collider"] is Zombie:
			(hit["collider"] as Zombie).mark()


func _switch(id: String) -> void:
	if weapon == id:
		return
	weapon = id
	reloading = 0.0
	_fire_cd = 0.25
	_switch_anim = 0.3
	_show_model()
	Game.changed.emit()


func _start_reload() -> void:
	reloading = reload_time(weapon)
	Sfx.play(self, "reload", -6.0)
	Game.changed.emit()


func _shoot() -> void:
	if ammo[weapon] <= 0:
		_start_reload()
		return
	var w: Dictionary = WEAPONS[weapon]
	_fire_cd = w["rate"]
	ammo[weapon] -= 1
	# Dispersion : précis en visée, moins en mouvement ou au jugé.
	var moving := Vector2(velocity.x, velocity.z).length() / WALK_SPEED
	var spread: float = w["spread"] * (0.25 if aiming else 1.0 + moving) + _recoil * 0.02
	var offset := Vector2(randf_range(-spread, spread), randf_range(-spread, spread))
	var hit := _ray(Fx.LAYER_WORLD | Fx.LAYER_ZOMBIES, 150.0, offset)
	var muzzle := _muzzle_flash.global_position
	var end_point: Vector3 = _camera.global_position - _camera.global_transform.basis.z * 150.0
	if not hit.is_empty():
		end_point = hit["position"]
		if hit["collider"] is Zombie:
			var z := hit["collider"] as Zombie
			z.take_damage(weapon_damage(weapon), true, w["heavy"], hit["position"], hit["normal"])
			Game.hit_marker.emit(z.dead)
		else:
			Fx.burst(Game.main, end_point, hit["normal"], Color(0.4, 0.36, 0.3), 8, 3.0, 0.04)
			Fx.burst(Game.main, end_point, hit["normal"], Color(1.0, 0.8, 0.4), 3, 6.0, 0.02)
	Fx.tracer(Game.main, muzzle, end_point, Color(1.0, 0.85, 0.6))
	Fx.flash(Game.main, muzzle, Color(1.0, 0.7, 0.35), 5.0, 7.0, 0.05)
	Sfx.play(self, w["sound"], -2.0, 0.06)
	_muzzle_flash.visible = true
	_muzzle_flash.rotation.z = randf() * TAU
	get_tree().create_timer(0.04).timeout.connect(func(): _muzzle_flash.visible = false)
	_apply_recoil(w)
	if ammo[weapon] <= 0:
		_start_reload()
	Game.changed.emit()


## Recul : la vue monte vite puis revient doucement à sa place.
## Au fusil, la montée grandit pendant une rafale et dérive d'un côté puis de l'autre :
## il faut tirer par courtes rafales ou compenser à la souris.
func _apply_recoil(w: Dictionary) -> void:
	_burst = _burst + 1 if _since_shot < 0.3 else 1
	_since_shot = 0.0
	var control := 0.65 if aiming else 1.0
	var up: float = w["kick_up"] * control
	var side: float = w["kick_side"] * control
	if weapon == "rifle":
		up *= 1.0 + min(_burst, 12) * 0.06
		side *= sin(_burst * 0.7) * 1.5 + randf_range(-0.5, 0.5)
	else:
		side *= randf_range(-1.0, 1.0)
	_kick_target += Vector2(up, side)
	_kick_target.x = min(_kick_target.x, 0.35)
	_recoil = min(_recoil + w["punch"] * 0.5, 1.0)
	_vm_kick_vel += w["punch"] * 2.2
	_fov_punch = w["punch"] * 1.6
	_shake = min(1.0, _shake + w["punch"] * 0.08)


## Écart du viseur (pour l'interface) : grandit avec le recul et le mouvement.
func spread_amount() -> float:
	var moving := Vector2(velocity.x, velocity.z).length() / WALK_SPEED
	var base: float = WEAPONS[weapon]["spread"] * (0.25 if aiming else 1.0 + moving)
	return base + _recoil * 0.02 + _kick_target.x * 0.05


func _animate_view(delta: float) -> void:
	_recoil = move_toward(_recoil, 0.0, delta * 4.0)
	_since_shot += delta
	var w: Dictionary = WEAPONS[weapon]
	# La vue retourne vers la visée de départ une fois la rafale terminée.
	if _since_shot > 0.08:
		_kick_target = _kick_target.lerp(Vector2.ZERO, min(1.0, delta * w["recovery"]))
	_kick = _kick.lerp(_kick_target, min(1.0, delta * 30.0))
	_camera.rotation.x = clamp(_aim_pitch + _kick.x, -1.5, 1.5)
	_camera.rotation.y = _kick.y
	# Ressort de l'arme : recule d'un coup, rebondit légèrement puis se stabilise.
	_vm_kick_vel += (-_vm_kick * 260.0 - _vm_kick_vel * 22.0) * delta
	_vm_kick += _vm_kick_vel * delta
	_fov_punch = move_toward(_fov_punch, 0.0, delta * 12.0)
	_shake = move_toward(_shake, 0.0, delta * 2.5)
	_switch_anim = move_toward(_switch_anim, 0.0, delta)
	_aim_blend = move_toward(_aim_blend, 1.0 if aiming else 0.0, delta * 7.0)
	_camera.fov = lerpf(FOV, AIM_FOV, _aim_blend) + _fov_punch

	# Balancement de la tête et pas : un pas par demi-oscillation, cadence réaliste.
	var speed := Vector2(velocity.x, velocity.z).length()
	var on_floor := is_on_floor()
	if on_floor and not _was_on_floor:
		Sfx.play(self, "land", -6.0, 0.08)
		_shake = min(1.0, _shake + 0.15)
	_was_on_floor = on_floor
	if on_floor and speed > 0.8:
		var running := speed > WALK_SPEED + 0.5
		# Marche : ~2 pas par seconde. Course : ~2,9 pas par seconde.
		var interval := 0.34 if running else (0.62 if aiming else 0.5)
		_bob += delta * PI / interval
		_step_time += delta
		if _step_time >= interval:
			_step_time = 0.0
			_step_left = not _step_left
			var variant := randi() % 4
			var sound := ("step_run_%d" if running else "step_walk_%d") % variant
			var volume := -9.0 if running else (-17.0 if aiming else -14.0)
			Sfx.play(self, sound, volume, 0.06)
	else:
		_step_time = 0.3
	var bob_amount := (0.03 if not aiming else 0.006) * clampf(speed / WALK_SPEED, 0.0, 1.4)
	var shake := Vector3(randf_range(-1, 1), randf_range(-1, 1), 0) * _shake * 0.05
	_camera.position = Vector3(cos(_bob * 0.5) * bob_amount * 0.6, 1.6 + absf(sin(_bob)) * bob_amount, 0) + shake

	# Position de l'arme : à la hanche, en visée, en rechargement, au changement.
	var hip := Vector3(0.15, -0.13, -0.28)
	var ads := Vector3(0.0, -0.064 if weapon == "rifle" else -0.055, -0.22)
	var pos := hip.lerp(ads, _aim_blend)
	pos += Vector3(cos(_bob * 0.5) * bob_amount * 0.5, -absf(sin(_bob)) * bob_amount * 0.5, 0)
	var rot := Vector3(0, _kick.y * 2.0, sin(_bob) * bob_amount * 0.6)
	if reloading > 0.0:
		var total := reload_time(weapon)
		var k := sin(clampf(1.0 - reloading / total, 0.0, 1.0) * PI)
		pos += Vector3(0, -0.12 * k, 0)
		rot += Vector3(-0.5 * k, 0.4 * k, 0.3 * k)
	if _switch_anim > 0.0:
		pos.y -= _switch_anim * 0.6
	_gun_base_pos = _gun_base_pos.lerp(pos, min(1.0, delta * 18.0))
	_gun_base_rot = _gun_base_rot.lerp(rot, min(1.0, delta * 18.0))
	# Le coup de recul s'ajoute sans lissage pour rester sec.
	_gun_root.position = _gun_base_pos + Vector3(0, _vm_kick * 0.012, _vm_kick * 0.06)
	_gun_root.rotation = _gun_base_rot + Vector3(_vm_kick * 0.35, 0, 0)


func _handle_interaction(delta: float) -> void:
	var hit := _ray(Fx.LAYER_STRUCTURES, 7.0, Vector2.ZERO)
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


func _ray(mask: int, length: float, offset: Vector2) -> Dictionary:
	var from := _camera.global_position
	var b := _camera.global_transform.basis
	var dir := (-b.z + b.x * offset.x + b.y * offset.y).normalized()
	var query := PhysicsRayQueryParameters3D.create(from, from + dir * length, mask, [get_rid()])
	return get_world_3d().direct_space_state.intersect_ray(query)
