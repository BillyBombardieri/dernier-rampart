class_name Player
extends CharacterBody3D
## Le joueur en vue FPS : déplacement, tir (recul, visée, flash, munitions spéciales), marquage,
## construction, réparation, établi et gadgets.

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
const REPAIR_RATE := 80.0  # PV par seconde
const REPAIR_COST := 10.0  # PV réparés par ferraille
const VIEWMODEL_LAYER := 2  # Couche visuelle de l'arme : la lampe torche ne l'éclaire pas.
# Modèles 3D des armes (tools/make_models.py). Pièces mobiles : Glissiere ou Culasse, Chargeur,
# Detente, Chien ; repères : Bouche (sortie du canon) et Visee (point aligné en visée).
const WEAPON_MODELS := {
	"pistol": "res://assets/models/arme_pistolet.gltf",
	"rifle": "res://assets/models/arme_fusil.gltf",
}
const ACTION_TRAVEL := {"pistol": 0.028, "rifle": 0.05}  # Recul de la glissière ou de la culasse (m).
const MAG_DROP := {"pistol": Vector3(0, -0.956, 0.292), "rifle": Vector3(0, -0.995, -0.105)}  # Sortie du chargeur.
const ADS_DEPTH := {"pistol": -0.22, "rifle": -0.13}  # Distance de l'arme à l'œil en visée.
# Geste de rechargement (décalage, rotation) : l'arme remonte vers le centre et pivote vers la main
# gauche, on voit le chargeur sortir puis rentrer.
const RELOAD_POSE := {
	"pistol": [Vector3(-0.07, 0.06, 0.04), Vector3(0.3, 0.35, 0.35)],
	"rifle": [Vector3(-0.06, 0.06, 0.02), Vector3(0.15, 0.3, 0.4)],
}
const HAMMER_COCKED := 0.75

var max_hp := 100.0
var hp := 100.0
var alive := true
var weapon := "pistol"
var ammo := {"pistol": 10, "rifle": 30}
var reloading := 0.0
var respawn_time := 0.0
var aiming := false
var socket_in_sight: Socket = null  # Ancrage vide visé (l'interface affiche alors le choix des tours).
var build_choice := 0  # Index dans Tower.BUILD_ORDER.

var _camera: Camera3D
var _gun_root: Node3D
var _models := {}
var _parts := {}  # Pour chaque arme : pièces mobiles, leurs positions de repos et les repères.
var _action_kick := 0.0  # 1 au moment du tir, la glissière ou la culasse revient ensuite en place.
var _trigger_pull := 0.0
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
	_camera.fov = Settings.fov
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
	fill.position = Vector3(-0.25, 0.35, 0.15)  # En haut à gauche : éclaire les faces de l'arme qu'on voit.
	_camera.add_child(fill)
	_spawn_point = global_position
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	for id in WEAPONS:
		ammo[id] = mag_size(id)
	apply_implants()


func _build_viewmodels() -> void:
	_gun_root = Node3D.new()
	_gun_root.scale = Vector3.ONE * 0.6
	_camera.add_child(_gun_root)
	for id in WEAPON_MODELS:
		var model: Node3D = load(WEAPON_MODELS[id]).instantiate()
		_gun_root.add_child(model)
		_models[id] = model
		for node in model.find_children("*", "MeshInstance3D", true, false):
			var mi := node as MeshInstance3D
			mi.layers = VIEWMODEL_LAYER
			mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
			for i in mi.get_surface_override_material_count():
				_finish_material(mi.get_active_material(i) as StandardMaterial3D)
		var parts := {}
		for key in ["Glissiere", "Culasse", "Chargeur", "Detente", "Chien", "Bouche", "Visee"]:
			var n := model.find_child(key, true, false) as Node3D
			parts[key] = n
			if n:
				parts[key + "_repos"] = n.transform
		_parts[id] = parts

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
	_muzzle_flash.layers = VIEWMODEL_LAYER
	_gun_root.add_child(_muzzle_flash)
	_show_model()


## Matériaux des armes : grain de surface (pointillé des poignées, acier légèrement inégal),
## vitre teintée du viseur, réticule et points de visée qui brillent dans le noir.
## Les modèles n'ont pas de coordonnées de texture : les textures sont projetées (triplanaire).
func _finish_material(mat: StandardMaterial3D) -> void:
	if mat == null or mat.has_meta("fini"):
		return
	mat.set_meta("fini", true)
	match mat.resource_name:
		"polymere", "caoutchouc":
			mat.normal_enabled = true
			mat.normal_texture = _grain(0.18 if mat.resource_name == "caoutchouc" else 0.1, 6.0, FastNoiseLite.TYPE_CELLULAR)
			mat.normal_scale = 0.6
			mat.uv1_triplanar = true
			mat.uv1_scale = Vector3.ONE * 25.0
		"acier_noir", "acier_brut":
			mat.roughness = 1.0
			mat.roughness_texture = _grain(0.02, 0.0, FastNoiseLite.TYPE_SIMPLEX_SMOOTH, mat.resource_name == "acier_brut")
			mat.uv1_triplanar = true
			mat.uv1_scale = Vector3.ONE * 8.0
		"verre":
			mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
			mat.albedo_color = Color(0.25, 0.35, 0.4, 0.18)
			mat.metallic_specular = 1.0
			mat.roughness = 0.05
		"reticule", "tritium":
			mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
			mat.emission_energy_multiplier = 3.0


## Texture de bruit : relief en pointillé (normal_strength > 0) ou rugosité qui varie.
func _grain(frequency: float, normal_strength: float, kind: FastNoiseLite.NoiseType, bright := false) -> NoiseTexture2D:
	var noise := FastNoiseLite.new()
	noise.noise_type = kind
	noise.frequency = frequency
	noise.seed = 7
	var tex := NoiseTexture2D.new()
	tex.width = 256
	tex.height = 256
	tex.seamless = true
	tex.noise = noise
	if normal_strength > 0.0:
		tex.as_normal_map = true
		tex.bump_strength = normal_strength
	else:
		var ramp := Gradient.new()
		ramp.set_color(0, Color.from_hsv(0, 0, 0.22 if bright else 0.32))
		ramp.set_color(1, Color.from_hsv(0, 0, 0.36 if bright else 0.5))
		tex.color_ramp = ramp
	return tex


func _show_model() -> void:
	for id in _models:
		_models[id].visible = id == weapon
	var muzzle: Node3D = _parts[weapon]["Bouche"]
	_muzzle_flash.position = muzzle.position if muzzle else Vector3(0, 0.03, -0.2)


func apply_implants() -> void:
	_speed_mult = 0.85 if Game.has_implant("batterie") else 1.0
	var new_max := 80.0 if Game.has_implant("charognard") else 100.0
	hp = min(hp, new_max)
	max_hp = new_max
	Game.changed.emit()


## Réglages changés dans le menu (champ de vision) : appliqués au retour en jeu.
func apply_settings() -> void:
	_camera.fov = Settings.fov


func weapon_damage(id: String) -> float:
	var dmg: float = WEAPONS[id]["damage"]
	if id == "pistol" and Game.has_implant("main_lourde"):
		dmg *= 1.5
	if id == "rifle" and Game.has_implant("sang_froid"):
		dmg *= 0.8
	return dmg * (1.0 + 0.2 * Game.mods[id]["barrel"])


## Taille du chargeur, avec les améliorations de l'établi.
func mag_size(id: String) -> int:
	return int(round(WEAPONS[id]["mag"] * (1.0 + 0.3 * Game.mods[id]["mag"])))


## Recharge gratuitement une arme (après l'achat d'un plus grand chargeur).
func refill(id: String) -> void:
	ammo[id] = mag_size(id)
	if weapon == id:
		reloading = 0.0
	Game.changed.emit()


func look_direction() -> Vector3:
	return -_camera.global_transform.basis.z


func eye_position() -> Vector3:
	return _camera.global_position


func add_shake(amount: float) -> void:
	_shake = minf(1.0, _shake + amount)


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


## Santé au maximum (quand une vague est repoussée).
func heal_full() -> void:
	if not alive:
		return
	hp = max_hp
	Game.changed.emit()


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
		var sens := MOUSE_SENS * Settings.sensitivity * (0.6 if aiming else 1.0)
		rotate_y(-event.relative.x * sens)
		var pitch_delta: float = -event.relative.y * sens * (-1.0 if Settings.invert_y else 1.0)
		# Tirer la souris vers le bas compense d'abord le recul, puis bouge la visée.
		if pitch_delta < 0.0 and _kick_target.x > 0.0:
			var used: float = min(_kick_target.x, -pitch_delta)
			_kick_target.x -= used
			_kick.x -= used
			pitch_delta += used
		_aim_pitch = clamp(_aim_pitch + pitch_delta, -1.45, 1.45)
	elif event is InputEventMouseButton and event.pressed and Input.mouse_mode != Input.MOUSE_MODE_CAPTURED:
		if not get_tree().paused:
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
			get_viewport().set_input_as_handled()
	elif event is InputEventMouseButton and event.pressed and alive and not Game.is_over:
		# Molette : choisit la tour à construire si on vise un ancrage vide, sinon change d'arme.
		var mb := event as InputEventMouseButton
		if mb.button_index in [MOUSE_BUTTON_WHEEL_UP, MOUSE_BUTTON_WHEEL_DOWN]:
			var step := 1 if mb.button_index == MOUSE_BUTTON_WHEEL_DOWN else -1
			if socket_in_sight:
				cycle_build(step)
			else:
				_switch("rifle" if weapon == "pistol" else "pistol")


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
	if Input.is_action_just_pressed("gadget_1"):
		Gadgets.use(0, self)
	elif Input.is_action_just_pressed("gadget_2"):
		Gadgets.use(1, self)
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
			ammo[weapon] = mag_size(weapon)
			Game.changed.emit()
		return
	if Input.is_action_just_pressed("reload") and ammo[weapon] < mag_size(weapon):
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
	var ammo_type: String = Game.mods[weapon]["ammo"]
	var dmg := weapon_damage(weapon) * (0.9 if ammo_type == "incendiaire" else 1.0)
	# Les munitions perforantes traversent jusqu'à 3 zombies.
	var max_hits := 3 if ammo_type == "perforante" else 1
	var exclude: Array[RID] = [get_rid()]
	var muzzle := _muzzle_flash.global_position
	var end_point: Vector3 = _camera.global_position - _camera.global_transform.basis.z * 150.0
	var hits := 0
	while hits < max_hits:
		var hit := _ray(Fx.LAYER_WORLD | Fx.LAYER_ZOMBIES, 150.0, offset, exclude)
		if hit.is_empty():
			break
		end_point = hit["position"]
		if hit["collider"] is Zombie:
			var z := hit["collider"] as Zombie
			var d := dmg * pow(0.7, hits)
			if ammo_type == "perforante" and z.type in ["brute", "boss"]:
				d *= 1.25
			z.take_damage(d, true, w["heavy"], hit["position"], hit["normal"], ammo_type)
			Game.hit_marker.emit(z.dead)
			exclude.append(z.get_rid())
			hits += 1
		else:
			Fx.burst(Game.main, end_point, hit["normal"], Color(0.4, 0.36, 0.3), 8, 3.0, 0.04)
			Fx.burst(Game.main, end_point, hit["normal"], Color(1.0, 0.8, 0.4), 3, 6.0, 0.02)
			break
	Fx.tracer(Game.main, muzzle, end_point, _tracer_color(ammo_type))
	Fx.flash(Game.main, muzzle, Color(1.0, 0.7, 0.35), 5.0, 7.0, 0.05)
	Sfx.play(self, w["sound"], -2.0, 0.06)
	_muzzle_flash.visible = true
	_muzzle_flash.rotation.z = randf() * TAU
	get_tree().create_timer(0.04).timeout.connect(func(): _muzzle_flash.visible = false)
	_action_kick = 1.0
	_trigger_pull = 1.0
	_apply_recoil(w)
	if ammo[weapon] <= 0:
		_start_reload()
	Game.changed.emit()


func _tracer_color(ammo_type: String) -> Color:
	match ammo_type:
		"incendiaire":
			return Color(1.0, 0.5, 0.15)
		"electrique":
			return Color(0.65, 0.6, 1.0)
		"perforante":
			return Color(0.9, 0.95, 1.0)
	return Color(1.0, 0.85, 0.6)


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
	_camera.fov = lerpf(Settings.fov, Settings.fov * AIM_FOV / FOV, _aim_blend) + _fov_punch

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

	# Position de l'arme : à la hanche, en visée (repère Visee au centre de l'écran), en rechargement, au changement.
	var hip := Vector3(0.15, -0.13, -0.28)
	var sight: Node3D = _parts[weapon]["Visee"]
	var sight_pos := sight.position * _gun_root.scale.x if sight else Vector3(0, 0.06, 0)
	var ads := Vector3(-sight_pos.x, -sight_pos.y, ADS_DEPTH[weapon])
	var pos := hip.lerp(ads, _aim_blend)
	pos += Vector3(cos(_bob * 0.5) * bob_amount * 0.5, -absf(sin(_bob)) * bob_amount * 0.5, 0)
	var rot := Vector3(0, _kick.y * 2.0, sin(_bob) * bob_amount * 0.6)
	if reloading > 0.0:
		var t := clampf(1.0 - reloading / reload_time(weapon), 0.0, 1.0)
		var k := smoothstep(0.0, 0.14, t) * (1.0 - smoothstep(0.86, 1.0, t))
		var seat := maxf(0.0, 1.0 - absf(t - 0.8) / 0.04)  # Le chargeur claque en place.
		pos += RELOAD_POSE[weapon][0] * k + Vector3(0, 0.012 * seat, 0)
		rot += RELOAD_POSE[weapon][1] * k + Vector3(0.06 * seat, 0, 0)
	if _switch_anim > 0.0:
		pos.y -= _switch_anim * 0.6
	_gun_base_pos = _gun_base_pos.lerp(pos, min(1.0, delta * 18.0))
	_gun_base_rot = _gun_base_rot.lerp(rot, min(1.0, delta * 18.0))
	# Le coup de recul s'ajoute sans lissage pour rester sec.
	_gun_root.position = _gun_base_pos + Vector3(0, _vm_kick * 0.012, _vm_kick * 0.06)
	_gun_root.rotation = _gun_base_rot + Vector3(_vm_kick * 0.35, 0, 0)
	_animate_parts(delta)


## Pièces mobiles : la glissière (ou la culasse) recule à chaque tir et reste ouverte quand le
## pistolet est vide, la détente et le chien bougent, le chargeur sort puis rentre au rechargement.
func _animate_parts(delta: float) -> void:
	_action_kick = move_toward(_action_kick, 0.0, delta * 14.0)
	_trigger_pull = move_toward(_trigger_pull, 0.0, delta * 7.0)
	var p: Dictionary = _parts[weapon]
	var k := 1.0 - reloading / reload_time(weapon) if reloading > 0.0 else 0.0
	var back := _action_kick
	if weapon == "pistol" and ammo["pistol"] <= 0 and k < 0.82:
		back = 1.0
	elif weapon == "rifle" and k > 0.8:
		back = sin(clampf((k - 0.8) / 0.18, 0.0, 1.0) * PI)  # On réarme la culasse.
	var action: Node3D = p["Glissiere"] if p["Glissiere"] else p["Culasse"]
	if action:
		action.transform = p[action.name + "_repos"].translated_local(Vector3(0, 0, ACTION_TRAVEL[weapon] * back))
	if p["Detente"]:
		p["Detente"].transform = p["Detente_repos"].rotated_local(Vector3.RIGHT, -0.3 * _trigger_pull)
	if p["Chien"]:
		# Armé au repos ; il tombe au tir puis la glissière le réarme en reculant.
		var fall := _action_kick * _action_kick
		p["Chien"].transform = p["Chien_repos"].rotated_local(Vector3.RIGHT, HAMMER_COCKED * (1.0 - fall))
	if p["Chargeur"]:
		var out := 0.0
		if reloading > 0.0:
			out = smoothstep(0.08, 0.3, k) if k < 0.45 else 1.0 - smoothstep(0.5, 0.78, k)
		p["Chargeur"].transform = p["Chargeur_repos"].translated(MAG_DROP[weapon] * 0.16 * out)
		p["Chargeur"].visible = not (k > 0.3 and k < 0.5)


## Choisit la tour à construire (molette ou touche C).
func cycle_build(step: int) -> void:
	build_choice = wrapi(build_choice + step, 0, Tower.BUILD_ORDER.size())
	Sfx.play(self, "ui_click", -12.0, 0.05)


func _handle_interaction(delta: float) -> void:
	var hit := _ray(Fx.LAYER_STRUCTURES | Fx.LAYER_INTERACT, 7.0, Vector2.ZERO)
	var target: Object = null if hit.is_empty() else hit["collider"]
	if target is Tower:
		target = (target as Tower).socket
	if target == null:
		target = _nearby_barrier()
	if target is Barrier and (target as Barrier).temporary and not (target as Barrier).alive:
		target = null  # Barricade qui a cédé : elle disparaît, on ne la relève pas.
	Game.hint = ""
	socket_in_sight = null
	var e := Settings.key("interact")
	if target is Socket:
		var socket := target as Socket
		if socket.tower == null:
			socket_in_sight = socket
			if Input.is_action_just_pressed("cycle_tower"):
				cycle_build(1)
			var type: String = Tower.BUILD_ORDER[build_choice]
			var data: Dictionary = Tower.STATS[type]
			socket.preview(type)
			Game.hint = "%s construire : [b]%s[/b] (%d ferraille, %d énergie)\n[color=#9a9a9a]Molette ou %s : choisir une autre tour[/color]" % [e, data["name"], data["cost"], data["energy"], Settings.key("cycle_tower")]
			if Input.is_action_just_pressed("interact"):
				socket.build(type)
		else:
			var t := socket.tower
			t.hover()
			var power := "%s %s" % [Settings.key("toggle_power"), "éteindre" if t.powered else "allumer"]
			var up := "niveau max" if t.level >= Tower.MAX_LEVEL else "%s améliorer (%d)" % [e, t.upgrade_cost()]
			Game.hint = "%s niv.%d   %s   %s   (énergie %d)" % [t.display_name(), t.level, up, power, t.energy_cost()]
			if Input.is_action_just_pressed("interact"):
				t.upgrade()
			if Input.is_action_just_pressed("toggle_power"):
				t.toggle_power()
	elif target is Workbench:
		Game.hint = "%s ouvrir l'établi (armes, munitions, gadgets, générateur)" % e
		if Game.phase == "assault":
			Game.hint = "Établi : utilisable seulement entre les vagues"
		elif Input.is_action_just_pressed("interact"):
			Game.main.open_bench()
	elif target is Structure or target is Barrier:
		var s := target as Node3D
		var s_name: String = s.display_name
		if s.hp >= s.max_hp:
			Game.hint = "%s en bon état" % s_name
		elif Game.scrap <= 0:
			Game.hint = "[color=#ff5a46]Plus de ferraille pour réparer[/color] : ramasse celle des zombies abattus"
		else:
			# Une barrière détruite se relève dès qu'elle a retrouvé assez de PV.
			var rebuilding: bool = target is Barrier and not s.alive
			var goal: float = (target as Barrier).rebuild_hp() if rebuilding else s.max_hp
			Game.hint = "[color=#ffb840][b][Maintenir %s][/b][/color] %s : %s   %d / %d PV\n[color=#9a9a9a]1 ferraille pour %d PV[/color]" % [Settings.key_label("interact"), "relever" if rebuilding else "réparer", s_name, int(s.hp), int(goal), int(REPAIR_COST)]
			if Input.is_action_pressed("interact"):
				_repair(s, delta)


## Barrière à portée de main, vers laquelle on se tourne. Une barrière détruite est au ras du sol :
## avec le seul viseur, on la ratait en regardant les débris (ou en marchant dessus).
func _nearby_barrier() -> Barrier:
	var look := look_direction()
	look.y = 0.0
	look = look.normalized()
	var best: Barrier = null
	var best_d := 3.2
	for node in get_tree().get_nodes_in_group("barriers"):
		var b := node as Barrier
		var to := b.contact_point(global_position) - global_position
		to.y = 0.0
		var d := to.length()
		if d >= best_d or (d > 1.2 and look.dot(to / d) < 0.35):
			continue
		best_d = d
		best = b
	return best


func _repair(s: Node3D, delta: float) -> void:
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


func _ray(mask: int, length: float, offset: Vector2, exclude: Array[RID] = []) -> Dictionary:
	var from := _camera.global_position
	var b := _camera.global_transform.basis
	var dir := (-b.z + b.x * offset.x + b.y * offset.y).normalized()
	if exclude.is_empty():
		exclude = [get_rid()]
	var query := PhysicsRayQueryParameters3D.create(from, from + dir * length, mask, exclude)
	return get_world_3d().direct_space_state.intersect_ray(query)
