class_name Projectile
extends Node3D
## Projectile en cloche : crachat d'acide du Cracheur ou obus du Mortier.
## Il suit une trajectoire balistique jusqu'au point visé, puis applique ses dégâts.

const GRAVITY := Vector3(0, -14.0, 0)

var kind := "acid"  # "acid" ou "shell"
var damage := 10.0
var radius := 0.0  # Rayon de l'explosion (obus).
var stun := 0.0  # Durée d'étourdissement (obus).
var _target: WeakRef
var _velocity := Vector3.ZERO
var _time := 0.0
var _flight := 1.0
var _trail: CPUParticles3D


## Lance un projectile de from vers to en flight secondes.
static func launch(parent: Node, p_kind: String, from: Vector3, to: Vector3, flight: float, p_damage: float, target: Node3D = null, p_radius := 0.0, p_stun := 0.0) -> Projectile:
	var p := Projectile.new()
	p.kind = p_kind
	p.damage = p_damage
	p.radius = p_radius
	p.stun = p_stun
	p._target = weakref(target) if target else null
	p._flight = flight
	# Vitesse de départ pour arriver pile sur la cible : to = from + v*t + g*t²/2.
	p._velocity = (to - from - 0.5 * GRAVITY * flight * flight) / flight
	parent.add_child(p)
	p.global_position = from
	p._build()
	return p


func _build() -> void:
	var sphere := SphereMesh.new()
	if kind == "acid":
		sphere.radius = 0.16
		sphere.height = 0.3
		sphere.material = Fx.material(Color(0.45, 0.95, 0.2), 3.0)
		var light := OmniLight3D.new()
		light.light_color = Color(0.5, 1.0, 0.3)
		light.light_energy = 1.2
		light.omni_range = 3.0
		add_child(light)
	else:
		sphere.radius = 0.13
		sphere.height = 0.36
		sphere.material = Fx.material(Color(0.12, 0.12, 0.1))
	var mi := MeshInstance3D.new()
	mi.mesh = sphere
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(mi)
	_trail = CPUParticles3D.new()
	_trail.amount = 36 if kind == "acid" else 48
	_trail.lifetime = 0.5 if kind == "acid" else 0.9
	_trail.local_coords = false
	_trail.gravity = Vector3(0, -2.0, 0) if kind == "acid" else Vector3(0, 0.3, 0)
	_trail.initial_velocity_max = 0.3
	_trail.scale_amount_min = 0.5
	_trail.scale_amount_max = 1.0
	var q := QuadMesh.new()
	q.size = Vector2(0.2, 0.2) if kind == "acid" else Vector2(0.34, 0.34)
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.vertex_color_use_as_albedo = true
	m.albedo_texture = Fx.soft_texture()
	if kind == "acid":
		m.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	q.material = m
	_trail.mesh = q
	var grad := Gradient.new()
	if kind == "acid":
		grad.set_color(0, Color(0.5, 1.0, 0.3, 0.8))
		grad.set_color(1, Color(0.2, 0.5, 0.1, 0.0))
	else:
		grad.set_color(0, Color(0.55, 0.53, 0.5, 0.55))
		grad.set_color(1, Color(0.3, 0.3, 0.3, 0.0))
	_trail.color_ramp = grad
	add_child(_trail)


func _physics_process(delta: float) -> void:
	_time += delta
	var prev := global_position
	_velocity += GRAVITY * delta
	global_position += _velocity * delta
	if _velocity.length() > 0.1:
		look_at(global_position + _velocity, Vector3.UP if absf(_velocity.normalized().y) < 0.99 else Vector3.FORWARD)
	var arrived := _time >= _flight
	var hit_pos := global_position
	if kind == "acid" and not arrived:
		# L'acide s'écrase sur le premier obstacle rencontré (mur, tour, joueur...).
		var query := PhysicsRayQueryParameters3D.create(prev, global_position, Fx.LAYER_WORLD | Fx.LAYER_STRUCTURES | Fx.LAYER_PLAYER)
		var hit := get_world_3d().direct_space_state.intersect_ray(query)
		if not hit.is_empty():
			arrived = true
			hit_pos = hit["position"]
	if arrived or global_position.y < -0.5:
		if kind == "acid":
			_acid_impact(hit_pos)
		else:
			_explode(Vector3(hit_pos.x, maxf(hit_pos.y, 0.05), hit_pos.z))
		queue_free()


func _acid_impact(pos: Vector3) -> void:
	var t: Node3D = _target.get_ref() if _target else null
	var hit := false
	if is_instance_valid(t) and not t.is_queued_for_deletion():
		var tp := t.global_position
		if t is Barrier:
			tp = (t as Barrier).contact_point(pos)
		var alive: bool = t.get("alive") if t.get("alive") != null else true
		if alive and Vector2(tp.x - pos.x, tp.z - pos.z).length() < 2.0:
			t.take_damage(damage)
			hit = true
	var player := Game.player as Player
	if not hit and player and player.alive and Vector2(player.global_position.x - pos.x, player.global_position.z - pos.z).length() < 1.5:
		player.take_damage(damage)
	Fx.burst(Game.main, pos, Vector3.UP, Color(0.45, 0.95, 0.2), 16, 3.0, 0.06)
	Sfx.play_at(Game.main, "acid_hit", pos, -3.0, 0.15)
	# Flaque d'acide qui fume puis s'évapore.
	var disc := CylinderMesh.new()
	disc.top_radius = 0.7
	disc.bottom_radius = 0.7
	disc.height = 0.02
	var mat := Fx.material(Color(0.35, 0.8, 0.15, 0.85), 1.5)
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	disc.material = mat
	var puddle := MeshInstance3D.new()
	puddle.mesh = disc
	puddle.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	Game.main.add_child(puddle)
	puddle.global_position = Vector3(pos.x, 0.03, pos.z)
	var tween := puddle.create_tween()
	tween.tween_property(mat, "albedo_color:a", 0.0, 3.5)
	tween.tween_callback(puddle.queue_free)


## Explosion d'obus : dégâts de zone qui diminuent avec la distance et étourdissent.
## EMBRASEMENT : si un zombie touché est en feu, le feu se propage à tous ceux autour.
func _explode(pos: Vector3) -> void:
	var victims: Array = []
	var burning := false
	for node in get_tree().get_nodes_in_group("zombies"):
		var z := node as Zombie
		if z.dead or z.burrowed:
			continue
		var d := Vector2(z.global_position.x - pos.x, z.global_position.z - pos.z).length()
		if d <= radius:
			victims.append([z, d, z.burn_time > 0.0])
			if z.burn_time > 0.0:
				burning = true
	if burning:
		Game.add_score(10)
		Fx.popup(Game.main, pos + Vector3(0, 2.5, 0), "EMBRASEMENT !", Color(1.0, 0.55, 0.15), 64)
		var fire := Fx.fire_particles(60, 0.9, 0.9)
		fire.one_shot = true
		fire.explosiveness = 0.85
		fire.emission_sphere_radius = radius
		fire.initial_velocity_max = 4.0
		Game.main.add_child(fire)
		fire.global_position = pos + Vector3(0, 0.5, 0)
		fire.emitting = true
		get_tree().create_timer(1.5).timeout.connect(fire.queue_free)
		for node in get_tree().get_nodes_in_group("zombies"):
			var z := node as Zombie
			if not z.dead and Vector2(z.global_position.x - pos.x, z.global_position.z - pos.z).length() <= radius + 2.5:
				z.ignite(4.0, 10.0 * Zombie.combo_scale())
	for v in victims:
		var z: Zombie = v[0]
		if z.dead:
			continue
		var falloff := lerpf(1.0, 0.4, float(v[1]) / maxf(radius, 0.1))
		var dmg := damage * falloff * (1.5 if burning and v[2] else 1.0)
		z.stun(stun)
		z.take_damage(dmg)
	Fx.burst(Game.main, pos + Vector3(0, 0.2, 0), Vector3.UP, Color(0.3, 0.25, 0.2), 30, 7.0, 0.1)
	Fx.burst(Game.main, pos + Vector3(0, 0.4, 0), Vector3.UP, Color(1.0, 0.7, 0.3), 14, 9.0, 0.05)
	Fx.puff(Game.main, pos + Vector3(0, 0.8, 0), Color(0.3, 0.28, 0.26, 0.75), 16, 1.6, 2.2)
	Fx.flash(Game.main, pos + Vector3(0, 1.0, 0), Color(1.0, 0.6, 0.3), 10.0, 14.0, 0.12)
	Fx.ring_wave(Game.main, pos + Vector3(0, 0.15, 0), Color(1.0, 0.6, 0.3, 0.6), radius, 0.35)
	Sfx.play_at(Game.main, "explosion", pos, 4.0, 0.1, 120.0)
	var player := Game.player as Player
	if player and player.alive:
		var d := player.global_position.distance_to(pos)
		if d < 14.0:
			player.add_shake(0.5 * (1.0 - d / 14.0))
