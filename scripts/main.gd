extends Node3D
## Construit la carte : portail, couloir boueux, deux anneaux (Avant-poste, Muraille), le Cœur et le décor.

# Couloir suivi par les zombies, du portail jusqu'au Cœur.
const PATH := [
	Vector3(0, 0, -55), Vector3(0, 0, -35), Vector3(-18, 0, -25), Vector3(-18, 0, -2),
	Vector3(12, 0, 6), Vector3(12, 0, 22), Vector3(0, 0, 30),
]
const CORE_POS := Vector3(0, 0, 35)
const RINGS := {
	"avant": {
		"name": "Relais Avant-poste", "relay": Vector3(-24, 0, -30),
		"sockets": [Vector3(5, 0, -45), Vector3(-5, 0, -42), Vector3(-8, 0, -36), Vector3(-24, 0, -14)],
	},
	"muraille": {
		"name": "Relais Muraille", "relay": Vector3(17, 0, 8),
		"sockets": [Vector3(-12, 0, -8), Vector3(-4, 0, 6), Vector3(6, 0, 13), Vector3(18, 0, 14), Vector3(-6, 0, 24)],
	},
}

var path_points: Array[Vector3] = []
var _relays := {}
var _blocked: Array[Vector3] = []  # Endroits où ne pas poser de décor.
var _rng := RandomNumberGenerator.new()
var _beacon: MeshInstance3D
var _beacon_mat: StandardMaterial3D
var _beacon_time := 0.0
var _board: Label3D


func _ready() -> void:
	Game.reset()
	Game.main = self
	_rng.seed = 2026
	for p in PATH:
		path_points.append(p)
	_build_environment()
	_build_ground()
	_build_path()
	_build_portal()
	_build_core()
	_build_rings()
	_build_base_walls()
	_build_lights()
	_build_props()
	_spawn_player()
	_start_ambience()
	var wm := WaveManager.new()
	wm.name = "WaveManager"
	add_child(wm)
	var hud := Hud.new()
	hud.wave_manager = wm
	add_child(hud)
	wm.implant_choice.connect(hud.show_implants)
	if not Tutorial.already_done():
		var tuto := Tutorial.new()
		tuto.hud = hud
		add_child(tuto)


func relay_for_ring(ring: String) -> Structure:
	return _relays.get(ring)


func _build_environment() -> void:
	var env := Environment.new()
	var sky_mat := ProceduralSkyMaterial.new()
	sky_mat.sky_top_color = Color(0.03, 0.04, 0.08)
	sky_mat.sky_horizon_color = Color(0.3, 0.2, 0.17)
	sky_mat.sky_curve = 0.12
	sky_mat.ground_horizon_color = Color(0.12, 0.08, 0.06)
	sky_mat.ground_bottom_color = Color(0.02, 0.02, 0.02)
	sky_mat.sun_angle_max = 8.0
	var sky := Sky.new()
	sky.sky_material = sky_mat
	env.background_mode = Environment.BG_SKY
	env.sky = sky
	env.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	env.ambient_light_energy = 0.45
	env.tonemap_mode = Environment.TONE_MAPPER_AGX
	env.tonemap_exposure = 1.1
	env.ssao_enabled = true
	env.ssao_radius = 1.5
	env.ssao_intensity = 2.0
	env.ssil_enabled = true
	env.glow_enabled = true
	env.glow_intensity = 0.6
	env.glow_bloom = 0.05
	env.fog_enabled = true
	env.fog_light_color = Color(0.13, 0.14, 0.17)
	env.fog_density = 0.008
	env.fog_height = 2.0
	env.fog_height_density = 0.15
	env.volumetric_fog_enabled = true
	env.volumetric_fog_density = 0.01
	env.volumetric_fog_albedo = Color(0.6, 0.62, 0.68)
	env.volumetric_fog_length = 80.0
	env.adjustment_enabled = true
	env.adjustment_contrast = 1.08
	env.adjustment_saturation = 0.8
	var we := WorldEnvironment.new()
	we.environment = env
	add_child(we)
	# Soleil couchant, bas sur l'horizon.
	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-14, -150, 0)
	sun.light_color = Color(1.0, 0.68, 0.48)
	sun.light_energy = 0.9
	sun.light_volumetric_fog_energy = 0.4
	sun.shadow_enabled = true
	sun.directional_shadow_max_distance = 120.0
	add_child(sun)


func _build_ground() -> void:
	var ground := StaticBody3D.new()
	ground.collision_layer = Fx.LAYER_WORLD
	var shape := CollisionShape3D.new()
	var bs := BoxShape3D.new()
	bs.size = Vector3(200, 1, 200)
	shape.shape = bs
	shape.position.y = -0.5
	ground.add_child(shape)
	var plane := PlaneMesh.new()
	plane.size = Vector2(200, 200)
	plane.subdivide_width = 20
	plane.subdivide_depth = 20
	ground.add_child(Fx.mesh_with(plane, Fx.textured("ground", 0.12)))
	add_child(ground)


func _build_path() -> void:
	var mud := Fx.textured("mud", 0.2)
	for i in path_points.size() - 1:
		var a := path_points[i]
		var b := path_points[i + 1]
		var plane := PlaneMesh.new()
		plane.size = Vector2(4.5, a.distance_to(b) + 4.5)
		var seg := Fx.mesh_with(plane, mud)
		add_child(seg)
		seg.global_position = (a + b) * 0.5 + Vector3(0, 0.02 + i * 0.004, 0)
		seg.look_at(Vector3(b.x, seg.global_position.y, b.z), Vector3.UP)
		_blocked.append(a)
		_blocked.append((a + b) * 0.5)


func _build_portal() -> void:
	# Brèche dans un vieux mur d'enceinte, éclairée par une lampe de secours.
	var p := path_points[0]
	var concrete := Fx.textured("concrete", 0.3, Color(0.75, 0.72, 0.7))
	for x in [-14.0, 14.0]:
		var wall := Fx.box_body(Vector3(22, 5, 1.2), Color.WHITE, Fx.LAYER_WORLD)
		(wall.get_child(1) as MeshInstance3D).material_override = concrete
		wall.position = p + Vector3(x, 2.5, -3)
		add_child(wall)
	for x in [-3.3, 3.3]:
		var post := Fx.box_body(Vector3(1.0, 6, 1.4), Color.WHITE, Fx.LAYER_WORLD)
		(post.get_child(1) as MeshInstance3D).material_override = concrete
		post.position = p + Vector3(x, 3, -3)
		add_child(post)
	var light := Flicker.new()
	light.light_color = Color(1.0, 0.15, 0.08)
	light.base_energy = 3.0
	light.speed = 4.0
	light.amount = 0.5
	light.omni_range = 14.0
	light.light_volumetric_fog_energy = 3.0
	add_child(light)
	light.position = p + Vector3(0, 5.5, -2)
	_build_portal_beacon(p)


## Colonne de lumière rouge et fumée au-dessus du portail : on voit d'où viennent les zombies, de partout.
func _build_portal_beacon(p: Vector3) -> void:
	var beam_mesh := CylinderMesh.new()
	beam_mesh.top_radius = 0.5
	beam_mesh.bottom_radius = 0.8
	beam_mesh.height = 60.0
	beam_mesh.cap_top = false
	beam_mesh.cap_bottom = false
	var beam_mat := StandardMaterial3D.new()
	beam_mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	beam_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	beam_mat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	beam_mat.cull_mode = BaseMaterial3D.CULL_DISABLED
	beam_mat.albedo_color = Color(1.0, 0.12, 0.05, 0.35)
	beam_mat.disable_fog = true
	beam_mesh.material = beam_mat
	_beacon = MeshInstance3D.new()
	_beacon.mesh = beam_mesh
	_beacon.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_beacon_mat = beam_mat
	add_child(_beacon)
	_beacon.position = p + Vector3(0, 30, 0)
	# Lueur au sol et fumée rouge qui monte.
	var glow := Flicker.new()
	glow.light_color = Color(1.0, 0.1, 0.05)
	glow.base_energy = 2.5
	glow.speed = 3.0
	glow.amount = 0.3
	glow.omni_range = 10.0
	glow.light_volumetric_fog_energy = 4.0
	add_child(glow)
	glow.position = p + Vector3(0, 1.5, 0)
	var smoke := CPUParticles3D.new()
	smoke.amount = 14
	smoke.lifetime = 5.0
	smoke.direction = Vector3.UP
	smoke.spread = 12.0
	smoke.initial_velocity_min = 2.0
	smoke.initial_velocity_max = 4.0
	smoke.gravity = Vector3(0, 0.3, 0)
	smoke.emission_shape = CPUParticles3D.EMISSION_SHAPE_SPHERE
	smoke.emission_sphere_radius = 1.5
	smoke.scale_amount_min = 0.8
	smoke.scale_amount_max = 2.0
	var q := QuadMesh.new()
	q.size = Vector2(1, 1)
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.vertex_color_use_as_albedo = true
	m.albedo_texture = _soft_texture()
	q.material = m
	smoke.mesh = q
	var grad := Gradient.new()
	grad.set_color(0, Color(0.9, 0.15, 0.08, 0.45))
	grad.set_color(1, Color(0.3, 0.05, 0.05, 0.0))
	smoke.color_ramp = grad
	add_child(smoke)
	smoke.position = p + Vector3(0, 1, 0)
	# Panneau au-dessus de la brèche.
	var board := Label3D.new()
	board.text = "☠ PORTAIL ☠"
	board.font_size = 64
	board.outline_size = 12
	board.modulate = Color(1.0, 0.3, 0.2)
	board.billboard = BaseMaterial3D.BILLBOARD_FIXED_Y
	board.fixed_size = false
	add_child(board)
	board.position = p + Vector3(0, 7, -2)
	_board = board


func _process(delta: float) -> void:
	if _beacon_mat == null:
		return
	# La colonne et le panneau ne s'affichent qu'au début de la partie (préparation de la
	# vague 1), puis s'effacent en douceur. Ensuite, seuls la minimap et la lueur au sol restent.
	var target := 1.0 if Game.wave == 1 and Game.phase == "prep" else 0.0
	Game.portal_reveal = move_toward(Game.portal_reveal, target, delta / 3.0)
	_beacon_time += delta * 2.0
	_beacon_mat.albedo_color.a = (0.22 + 0.06 * sin(_beacon_time)) * Game.portal_reveal
	_beacon.visible = Game.portal_reveal > 0.0
	_board.modulate.a = Game.portal_reveal
	_board.outline_modulate.a = Game.portal_reveal
	_board.visible = Game.portal_reveal > 0.0


func _soft_texture() -> GradientTexture2D:
	var soft := GradientTexture2D.new()
	soft.fill = GradientTexture2D.FILL_RADIAL
	soft.fill_from = Vector2(0.5, 0.5)
	soft.fill_to = Vector2(1.0, 0.5)
	var g := Gradient.new()
	g.set_color(0, Color(1, 1, 1, 1))
	g.set_color(1, Color(1, 1, 1, 0))
	soft.gradient = g
	return soft


func _build_core() -> void:
	var core := Structure.new()
	add_child(core)
	core.position = CORE_POS
	core.setup("core", "Cœur", "", 1200.0, Vector3(4, 4, 4), Color(0.3, 0.6, 1.0))
	Game.core = core
	_blocked.append(CORE_POS)


func _build_rings() -> void:
	for ring in RINGS:
		var data: Dictionary = RINGS[ring]
		var relay := Structure.new()
		add_child(relay)
		relay.position = data["relay"]
		relay.setup("relay", data["name"], ring, 400.0, Vector3(1.6, 3, 1.6), Color(0.9, 0.8, 0.3))
		_relays[ring] = relay
		_blocked.append(data["relay"])
		for pos in data["sockets"]:
			var s := Socket.new()
			add_child(s)
			s.position = pos
			s.setup(ring)
			_blocked.append(pos)


func _build_base_walls() -> void:
	# Murets en béton et sacs de sable autour du Cœur (couverture).
	var concrete := Fx.textured("concrete", 0.3)
	for wall in [
		[Vector3(-9, 1, 30), Vector3(8, 2, 0.8)], [Vector3(9, 1, 40), Vector3(0.8, 2, 12)],
		[Vector3(-9, 1, 40), Vector3(0.8, 2, 12)], [Vector3(0, 1, 46), Vector3(18, 2, 0.8)],
		[Vector3(-30, 1.5, 10), Vector3(1.2, 3, 14)], [Vector3(28, 1.5, -20), Vector3(1.2, 3, 18)],
	]:
		var w := Fx.box_body(wall[1], Color.WHITE, Fx.LAYER_WORLD)
		(w.get_child(1) as MeshInstance3D).material_override = concrete
		w.position = wall[0]
		add_child(w)
		_blocked.append(wall[0])
	var cloth := Fx.textured("cloth", 1.2, Color(0.55, 0.48, 0.34), false)
	for start in [Vector3(4, 0, 29), Vector3(-26, 0, -6), Vector3(22, 0, 18)]:
		_sandbags(start, 6, cloth)


func _sandbags(start: Vector3, count: int, mat: Material) -> void:
	var body := StaticBody3D.new()
	body.collision_layer = Fx.LAYER_WORLD
	var shape := CollisionShape3D.new()
	var bs := BoxShape3D.new()
	bs.size = Vector3(count * 0.75, 1.0, 0.7)
	shape.shape = bs
	shape.position = Vector3(count * 0.75 * 0.5 - 0.375, 0.5, 0)
	body.add_child(shape)
	for row in 3:
		for i in count - (row % 2):
			var bag := CapsuleMesh.new()
			bag.radius = 0.32
			bag.height = 0.8
			var mi := Fx.mesh_with(bag, mat)
			mi.rotation_degrees = Vector3(0, 0, 90)
			mi.scale = Vector3(0.55, 1.0, 1.0)
			mi.position = Vector3(i * 0.75 + (row % 2) * 0.375, 0.18 + row * 0.33, 0)
			body.add_child(mi)
	add_child(body)
	body.position = start
	_blocked.append(start)


func _build_lights() -> void:
	# Projecteurs de chantier qui éclairent les couloirs.
	var metal := Fx.textured("metal", 1.0, Color(0.7, 0.7, 0.7), false)
	for data in [
		[Vector3(-12, 0, 28), Vector3(-25, 0, 0)], [Vector3(14, 0, 30), Vector3(12, 0, 8)],
		[Vector3(-28, 0, -20), Vector3(-10, 0, -32)], [Vector3(10, 0, -30), Vector3(0, 0, -50)],
	]:
		var base: Vector3 = data[0]
		var pole := Fx.mesh_with(_cyl(0.12, 7.0), metal)
		pole.position = base + Vector3(0, 3.5, 0)
		add_child(pole)
		var spot := SpotLight3D.new()
		spot.light_color = Color(1.0, 0.92, 0.78)
		spot.light_energy = 6.0
		spot.spot_range = 45.0
		spot.spot_angle = 32.0
		spot.shadow_enabled = true
		spot.light_volumetric_fog_energy = 2.0
		add_child(spot)
		spot.position = base + Vector3(0, 7.2, 0)
		spot.look_at(data[1], Vector3.UP)
		var lamp := Fx.box(Vector3(0.6, 0.4, 0.3), Color(1, 0.95, 0.8), 6.0)
		spot.add_child(lamp)
		_blocked.append(base)
	# Barils en feu.
	for pos in [Vector3(-4, 0, 31), Vector3(16, 0, 2), Vector3(-22, 0, -26), Vector3(7, 0, -38)]:
		_burning_barrel(pos, metal)


func _burning_barrel(pos: Vector3, metal: Material) -> void:
	var barrel := Fx.mesh_with(_cyl(0.35, 1.0), metal)
	barrel.position = pos + Vector3(0, 0.5, 0)
	add_child(barrel)
	var fire := CPUParticles3D.new()
	fire.amount = 24
	fire.lifetime = 0.8
	fire.direction = Vector3.UP
	fire.spread = 15.0
	fire.initial_velocity_min = 0.8
	fire.initial_velocity_max = 1.8
	fire.gravity = Vector3(0, 1.0, 0)
	fire.emission_shape = CPUParticles3D.EMISSION_SHAPE_SPHERE
	fire.emission_sphere_radius = 0.25
	fire.scale_amount_min = 0.6
	fire.scale_amount_max = 1.4
	var q := QuadMesh.new()
	q.size = Vector2(0.3, 0.3)
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.vertex_color_use_as_albedo = true
	m.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	var soft := GradientTexture2D.new()
	soft.fill = GradientTexture2D.FILL_RADIAL
	soft.fill_from = Vector2(0.5, 0.5)
	soft.fill_to = Vector2(1.0, 0.5)
	var g := Gradient.new()
	g.set_color(0, Color(1, 1, 1, 1))
	g.set_color(1, Color(1, 1, 1, 0))
	soft.gradient = g
	m.albedo_texture = soft
	q.material = m
	fire.mesh = q
	var grad := Gradient.new()
	grad.set_color(0, Color(1.0, 0.7, 0.2, 1.0))
	grad.set_color(1, Color(0.6, 0.1, 0.0, 0.0))
	fire.color_ramp = grad
	add_child(fire)
	fire.position = pos + Vector3(0, 1.0, 0)
	var light := Flicker.new()
	light.light_color = Color(1.0, 0.55, 0.2)
	light.base_energy = 2.5
	light.omni_range = 9.0
	light.shadow_enabled = true
	add_child(light)
	light.position = pos + Vector3(0, 1.6, 0)
	_blocked.append(pos)


func _build_props() -> void:
	var rock_mat := Fx.textured("concrete", 0.6, Color(0.5, 0.48, 0.45))
	var bark := Fx.textured("mud", 0.8, Color(0.45, 0.4, 0.35), false)
	var rust := Fx.textured("metal", 0.5, Color(0.8, 0.6, 0.5))
	var placed := 0
	var tries := 0
	while placed < 70 and tries < 2000:
		tries += 1
		var pos := Vector3(_rng.randf_range(-75, 75), 0, _rng.randf_range(-75, 75))
		if not _free_spot(pos, 6.0):
			continue
		placed += 1
		_blocked.append(pos)
		var kind := _rng.randi() % 10
		if kind < 5:
			_rock(pos, rock_mat)
		elif kind < 8:
			_dead_tree(pos, bark)
		else:
			_wreck(pos, rust)


func _free_spot(pos: Vector3, margin: float) -> bool:
	for i in path_points.size() - 1:
		if _dist_to_segment(pos, path_points[i], path_points[i + 1]) < margin:
			return false
	for b in _blocked:
		if Vector2(pos.x - b.x, pos.z - b.z).length() < margin * 0.7:
			return false
	return Vector2(pos.x - CORE_POS.x, pos.z - CORE_POS.z).length() > 14.0


func _dist_to_segment(p: Vector3, a: Vector3, b: Vector3) -> float:
	var p2 := Vector2(p.x, p.z)
	var a2 := Vector2(a.x, a.z)
	var b2 := Vector2(b.x, b.z)
	var ab := b2 - a2
	var t := clampf((p2 - a2).dot(ab) / ab.length_squared(), 0.0, 1.0)
	return p2.distance_to(a2 + ab * t)


func _rock(pos: Vector3, mat: Material) -> void:
	var size := _rng.randf_range(0.6, 2.2)
	var body := StaticBody3D.new()
	body.collision_layer = Fx.LAYER_WORLD
	var shape := CollisionShape3D.new()
	var ss := SphereShape3D.new()
	ss.radius = size * 0.8
	shape.shape = ss
	body.add_child(shape)
	var sphere := SphereMesh.new()
	sphere.radius = size
	sphere.height = size * 1.3
	sphere.radial_segments = 9
	sphere.rings = 5
	var mi := Fx.mesh_with(sphere, mat)
	mi.scale = Vector3(_rng.randf_range(0.8, 1.4), _rng.randf_range(0.5, 0.9), _rng.randf_range(0.8, 1.4))
	body.add_child(mi)
	add_child(body)
	body.position = pos + Vector3(0, size * 0.2, 0)
	body.rotation.y = _rng.randf() * TAU


func _dead_tree(pos: Vector3, mat: Material) -> void:
	var tree := Node3D.new()
	var h := _rng.randf_range(5.0, 9.0)
	var trunk := Fx.mesh_with(_cyl(0.25, h, 0.12), mat)
	trunk.position.y = h * 0.5
	tree.add_child(trunk)
	for i in _rng.randi_range(3, 6):
		var blen := _rng.randf_range(1.5, 3.5)
		var branch := Fx.mesh_with(_cyl(0.09, blen, 0.03), mat)
		var pivot := Node3D.new()
		pivot.position.y = _rng.randf_range(h * 0.45, h * 0.95)
		pivot.rotation = Vector3(_rng.randf_range(0.5, 1.1), _rng.randf() * TAU, 0)
		branch.position.y = blen * 0.5
		pivot.add_child(branch)
		tree.add_child(pivot)
	var body := StaticBody3D.new()
	body.collision_layer = Fx.LAYER_WORLD
	var shape := CollisionShape3D.new()
	var cs := CylinderShape3D.new()
	cs.radius = 0.3
	cs.height = h
	shape.shape = cs
	shape.position.y = h * 0.5
	body.add_child(shape)
	tree.add_child(body)
	tree.rotation = Vector3(_rng.randf_range(-0.08, 0.08), _rng.randf() * TAU, _rng.randf_range(-0.08, 0.08))
	add_child(tree)
	tree.position = pos


func _wreck(pos: Vector3, mat: Material) -> void:
	# Carcasse de voiture rouillée.
	var car := Fx.box_body(Vector3(1.9, 1.0, 4.2), Color.WHITE, Fx.LAYER_WORLD)
	(car.get_child(1) as MeshInstance3D).material_override = mat
	var cabin := Fx.box_mat(Vector3(1.7, 0.7, 2.0), mat)
	cabin.position = Vector3(0, 0.85, -0.2)
	car.add_child(cabin)
	for wx in [-0.95, 0.95]:
		for wz in [-1.3, 1.3]:
			var wheel := Fx.mesh_with(_cyl(0.38, 0.3), Fx.material(Color(0.06, 0.06, 0.06)))
			wheel.rotation_degrees = Vector3(0, 0, 90)
			wheel.position = Vector3(wx, -0.3, wz)
			car.add_child(wheel)
	add_child(car)
	car.position = pos + Vector3(0, 0.75, 0)
	car.rotation = Vector3(_rng.randf_range(-0.05, 0.05), _rng.randf() * TAU, _rng.randf_range(-0.12, 0.12))


func _cyl(radius: float, height: float, top := -1.0) -> CylinderMesh:
	var c := CylinderMesh.new()
	c.bottom_radius = radius
	c.top_radius = radius if top < 0.0 else top
	c.height = height
	c.radial_segments = 12
	return c


func _start_ambience() -> void:
	var wind := Sfx.stream("wind")
	if wind == null:
		return
	if wind is AudioStreamWAV:
		var w := wind as AudioStreamWAV
		w.loop_mode = AudioStreamWAV.LOOP_FORWARD
		w.loop_end = w.data.size() / 2
	var p := AudioStreamPlayer.new()
	p.stream = wind
	p.volume_db = -14.0
	p.autoplay = true
	add_child(p)


func _spawn_player() -> void:
	var player := Player.new()
	player.position = Vector3(0, 0.2, 27)
	add_child(player)
	Game.player = player
