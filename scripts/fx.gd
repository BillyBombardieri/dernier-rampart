class_name Fx
extends RefCounted
## Petits utilitaires visuels : formes simples, traînées de tir, textes flottants.

const LAYER_WORLD := 1
const LAYER_ZOMBIES := 2
const LAYER_STRUCTURES := 4
const LAYER_PLAYER := 8
const LAYER_PICKUPS := 16
const LAYER_INTERACT := 32  # Objets qu'on peut viser pour interagir sans qu'ils bloquent le passage.

static var _textures := {}


static func material(color: Color, emission := 0.0) -> StandardMaterial3D:
	var mat := StandardMaterial3D.new()
	mat.albedo_color = color
	if emission > 0.0:
		mat.emission_enabled = true
		mat.emission = color
		mat.emission_energy_multiplier = emission
	return mat


static func box(size: Vector3, color: Color, emission := 0.0) -> MeshInstance3D:
	var mesh := BoxMesh.new()
	mesh.size = size
	mesh.material = material(color, emission)
	var mi := MeshInstance3D.new()
	mi.mesh = mesh
	return mi


static func cylinder(radius: float, height: float, color: Color, emission := 0.0) -> MeshInstance3D:
	var mesh := CylinderMesh.new()
	mesh.top_radius = radius
	mesh.bottom_radius = radius
	mesh.height = height
	mesh.material = material(color, emission)
	var mi := MeshInstance3D.new()
	mi.mesh = mesh
	return mi


static func capsule(radius: float, height: float, color: Color) -> MeshInstance3D:
	var mesh := CapsuleMesh.new()
	mesh.radius = radius
	mesh.height = height
	mesh.material = material(color)
	var mi := MeshInstance3D.new()
	mi.mesh = mesh
	return mi


static func box_body(size: Vector3, color: Color, layer: int) -> StaticBody3D:
	var body := StaticBody3D.new()
	body.collision_layer = layer
	body.collision_mask = 0
	var shape := CollisionShape3D.new()
	var bs := BoxShape3D.new()
	bs.size = size
	shape.shape = bs
	body.add_child(shape)
	body.add_child(box(size, color))
	return body


static func tracer(parent: Node, from: Vector3, to: Vector3, color: Color) -> void:
	var length := from.distance_to(to)
	if length < 0.05:
		return
	var mi := box(Vector3(0.05, 0.05, length), color, 3.0)
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	parent.add_child(mi)
	mi.global_position = (from + to) * 0.5
	mi.look_at(to, Vector3.UP if absf((to - from).normalized().y) < 0.99 else Vector3.FORWARD)
	parent.get_tree().create_timer(0.06).timeout.connect(mi.queue_free)


static func popup(parent: Node, pos: Vector3, text: String, color: Color, size := 48) -> void:
	var label := Label3D.new()
	label.text = text
	label.modulate = color
	label.font_size = size
	label.outline_size = 8
	label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	label.no_depth_test = true
	parent.add_child(label)
	label.global_position = pos
	var tween := label.create_tween()
	tween.set_parallel(true)
	tween.tween_property(label, "global_position", pos + Vector3(0, 1.5, 0), 0.9)
	tween.tween_property(label, "modulate:a", 0.0, 0.9)
	tween.chain().tween_callback(label.queue_free)


## Matériau réaliste (PBR) à partir des textures de assets/textures.
## Les textures sont projetées selon la position dans le monde : pas d'étirement sur les grandes boîtes.
static func textured(name: String, uv_scale := 0.25, tint := Color.WHITE, world := true) -> StandardMaterial3D:
	var mat := StandardMaterial3D.new()
	mat.albedo_texture = _tex(name + "_albedo")
	mat.albedo_color = tint
	mat.normal_enabled = true
	mat.normal_texture = _tex(name + "_normal")
	var rough := _tex(name + "_roughness")
	if rough:
		mat.roughness_texture = rough
		mat.roughness_texture_channel = BaseMaterial3D.TEXTURE_CHANNEL_RED
	mat.uv1_triplanar = true
	mat.uv1_world_triplanar = world
	mat.uv1_scale = Vector3.ONE * uv_scale
	mat.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC
	return mat


static func _tex(name: String) -> Texture2D:
	if not _textures.has(name):
		var path := "res://assets/textures/%s.png" % name
		_textures[name] = load(path) if ResourceLoader.exists(path) else null
	return _textures[name]


static func box_mat(size: Vector3, mat: Material) -> MeshInstance3D:
	var mesh := BoxMesh.new()
	mesh.size = size
	mesh.material = mat
	var mi := MeshInstance3D.new()
	mi.mesh = mesh
	return mi


static func mesh_with(mesh: PrimitiveMesh, mat: Material) -> MeshInstance3D:
	mesh.material = mat
	var mi := MeshInstance3D.new()
	mi.mesh = mesh
	return mi


## Gerbe de particules (sang, poussière, étincelles) qui se détruit toute seule.
static func burst(parent: Node, pos: Vector3, normal: Vector3, color: Color, amount := 14, speed := 4.0, size := 0.06) -> void:
	var p := CPUParticles3D.new()
	p.one_shot = true
	p.explosiveness = 0.95
	p.amount = amount
	p.lifetime = 0.7
	p.direction = normal if normal.length() > 0.1 else Vector3.UP
	p.spread = 45.0
	p.initial_velocity_min = speed * 0.4
	p.initial_velocity_max = speed
	p.gravity = Vector3(0, -12, 0)
	p.scale_amount_min = 0.5
	p.scale_amount_max = 1.2
	var mesh := BoxMesh.new()
	mesh.size = Vector3.ONE * size
	var mat := StandardMaterial3D.new()
	mat.albedo_color = color
	mat.roughness = 0.4
	mesh.material = mat
	p.mesh = mesh
	parent.add_child(p)
	p.global_position = pos
	p.emitting = true
	parent.get_tree().create_timer(1.2).timeout.connect(p.queue_free)


## Éclair lumineux bref (flash de tir, impact).
static func flash(parent: Node, pos: Vector3, color: Color, energy := 4.0, radius := 6.0, duration := 0.05) -> void:
	var light := OmniLight3D.new()
	light.light_color = color
	light.light_energy = energy
	light.omni_range = radius
	light.shadow_enabled = false
	parent.add_child(light)
	light.global_position = pos
	parent.get_tree().create_timer(duration).timeout.connect(light.queue_free)


## Onde circulaire qui s'élargit au sol (cri du Hurleur, Surcharge, explosion).
static func ring_wave(parent: Node, pos: Vector3, color: Color, radius: float, duration := 0.5) -> void:
	var torus := TorusMesh.new()
	torus.inner_radius = 0.9
	torus.outer_radius = 1.0
	torus.rings = 32
	torus.ring_segments = 6
	var mat := StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	mat.albedo_color = color
	torus.material = mat
	var mi := MeshInstance3D.new()
	mi.mesh = torus
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	parent.add_child(mi)
	mi.global_position = pos
	mi.scale = Vector3(0.3, 1.0, 0.3)
	var tween := mi.create_tween()
	tween.set_parallel(true)
	tween.tween_property(mi, "scale", Vector3(radius, 1.0, radius), duration).set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_CUBIC)
	tween.tween_property(mat, "albedo_color:a", 0.0, duration)
	tween.chain().tween_callback(mi.queue_free)


## Éclair en zigzag entre deux points (Arc électrique, Surcharge).
static func lightning(parent: Node, from: Vector3, to: Vector3, color: Color, segments := 7, jitter := 0.35) -> void:
	var prev := from
	for i in range(1, segments + 1):
		var t := float(i) / segments
		var p := from.lerp(to, t)
		if i < segments:
			p += Vector3(randf_range(-jitter, jitter), randf_range(-jitter, jitter), randf_range(-jitter, jitter))
		tracer(parent, prev, p, color)
		prev = p


## Particules de feu (lance-flammes, zombies en feu, barils). Additives, sans ombre.
static func fire_particles(amount := 24, size := 0.3, lifetime := 0.7) -> CPUParticles3D:
	var fire := CPUParticles3D.new()
	fire.amount = amount
	fire.lifetime = lifetime
	fire.direction = Vector3.UP
	fire.spread = 18.0
	fire.initial_velocity_min = 0.6
	fire.initial_velocity_max = 1.6
	fire.gravity = Vector3(0, 1.5, 0)
	fire.emission_shape = CPUParticles3D.EMISSION_SHAPE_SPHERE
	fire.emission_sphere_radius = 0.25
	fire.scale_amount_min = 0.6
	fire.scale_amount_max = 1.4
	# Les flammes s'affinent en montant : chaque particule rétrécit au cours de sa vie.
	var taper := Curve.new()
	taper.add_point(Vector2(0.0, 0.7))
	taper.add_point(Vector2(0.25, 1.0))
	taper.add_point(Vector2(1.0, 0.25))
	fire.scale_amount_curve = taper
	var q := QuadMesh.new()
	q.size = Vector2(size, size)
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.vertex_color_use_as_albedo = true
	m.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	m.albedo_texture = soft_texture()
	q.material = m
	fire.mesh = q
	var grad := Gradient.new()
	grad.offsets = PackedFloat32Array([0.0, 0.4, 1.0])
	grad.colors = PackedColorArray([Color(1.0, 0.75, 0.25, 1.0), Color(1.0, 0.4, 0.08, 0.8), Color(0.3, 0.05, 0.0, 0.0)])
	fire.color_ramp = grad
	fire.local_coords = false
	return fire


static var _soft: GradientTexture2D


## Tache ronde et douce pour les particules (feu, fumée, poussière).
static func soft_texture() -> GradientTexture2D:
	if _soft == null:
		_soft = GradientTexture2D.new()
		_soft.fill = GradientTexture2D.FILL_RADIAL
		_soft.fill_from = Vector2(0.5, 0.5)
		_soft.fill_to = Vector2(1.0, 0.5)
		var g := Gradient.new()
		g.set_color(0, Color(1, 1, 1, 1))
		g.set_color(1, Color(1, 1, 1, 0))
		_soft.gradient = g
	return _soft


## Nuage de fumée ou de poussière qui se dissipe (explosion, zombie qui sort de terre).
static func puff(parent: Node, pos: Vector3, color: Color, amount := 16, radius := 1.0, lifetime := 1.6) -> void:
	var p := CPUParticles3D.new()
	p.one_shot = true
	p.explosiveness = 0.9
	p.amount = amount
	p.lifetime = lifetime
	p.direction = Vector3.UP
	p.spread = 70.0
	p.initial_velocity_min = 0.5 * radius
	p.initial_velocity_max = 2.5 * radius
	p.gravity = Vector3(0, 0.4, 0)
	p.damping_min = 1.5
	p.damping_max = 3.0
	p.emission_shape = CPUParticles3D.EMISSION_SHAPE_SPHERE
	p.emission_sphere_radius = 0.4 * radius
	p.scale_amount_min = 1.0 * radius
	p.scale_amount_max = 2.2 * radius
	var q := QuadMesh.new()
	q.size = Vector2(1, 1)
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.vertex_color_use_as_albedo = true
	m.albedo_texture = soft_texture()
	q.material = m
	p.mesh = q
	var grad := Gradient.new()
	grad.set_color(0, color)
	grad.set_color(1, Color(color.r, color.g, color.b, 0.0))
	p.color_ramp = grad
	parent.add_child(p)
	p.global_position = pos
	p.emitting = true
	parent.get_tree().create_timer(lifetime + 0.3).timeout.connect(p.queue_free)
