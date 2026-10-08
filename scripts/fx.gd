class_name Fx
extends RefCounted
## Petits utilitaires visuels : formes simples, traînées de tir, textes flottants.

const LAYER_WORLD := 1
const LAYER_ZOMBIES := 2
const LAYER_STRUCTURES := 4
const LAYER_PLAYER := 8
const LAYER_PICKUPS := 16

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
