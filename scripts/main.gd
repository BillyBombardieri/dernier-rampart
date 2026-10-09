extends Node3D
## Construit la carte du niveau choisi (Levels) : ambiance, portail, couloir, deux avant-postes
## (Avant-poste, Muraille) qui sont chacun une barrière en travers du chemin entourée d'ancrages
## pour les tours, le Cœur, l'établi, le décor et la météo.

const GATE_NAMES := {"avant": "Barrière de l'Avant-poste", "muraille": "Barrière de la Muraille"}

# Le Cœur et la base qui l'entoure (murets, établi) sont au même endroit dans tous les niveaux.
# Le chemin, les barrières, les ancrages et le décor viennent du niveau choisi (Levels).
const CORE_POS := Vector3(0, 0, 35)
const GATE_WIDTH := 7.2
const WORKBENCH_POS := Vector3(-5.5, 0, 33)

var path_points: Array[Vector3] = []
var level: Dictionary
var bench_panel: WorkbenchPanel
var pause_menu: PauseMenu
var _blocked: Array[Vector3] = []  # Endroits où ne pas poser de décor.
var _rng := RandomNumberGenerator.new()
var _beacon: MeshInstance3D
var _beacon_mat: StandardMaterial3D
var _beacon_time := 0.0
var _board: Label3D
var _weather: CPUParticles3D


func _ready() -> void:
	Game.reset()
	Game.main = self
	_rng.seed = 2026 + Game.level
	level = Levels.get_level(Game.level)
	for p in level["path"]:
		path_points.append(p)
	_build_environment()
	_build_ground()
	_build_path()
	_build_portal()
	_build_core()
	_build_rings()
	_build_base_walls()
	_build_workbench()
	_build_lights()
	_build_props()
	_spawn_player()
	_build_weather()
	_start_ambience()
	var wm := WaveManager.new()
	wm.name = "WaveManager"
	add_child(wm)
	var hud := Hud.new()
	hud.wave_manager = wm
	add_child(hud)
	wm.implant_choice.connect(hud.show_implants)
	bench_panel = WorkbenchPanel.new()
	add_child(bench_panel)
	pause_menu = PauseMenu.new()
	pause_menu.bench = bench_panel
	add_child(pause_menu)
	if not Settings.tutorial_done:
		var tuto := Tutorial.new()
		tuto.hud = hud
		add_child(tuto)


## Ouvre l'établi, seulement entre les vagues.
func open_bench() -> void:
	if Game.phase == "assault":
		Game.say("Trop dangereux pendant l'assaut : reviens à l'établi entre deux vagues.")
		return
	bench_panel.open()


func _build_environment() -> void:
	var e: Dictionary = level["env"]
	var env := Environment.new()
	var sky_mat := ProceduralSkyMaterial.new()
	sky_mat.sky_top_color = e["sky_top"]
	sky_mat.sky_horizon_color = e["sky_horizon"]
	sky_mat.sky_curve = 0.12
	sky_mat.ground_horizon_color = e["ground_horizon"]
	sky_mat.ground_bottom_color = Color(0.02, 0.02, 0.02)
	sky_mat.sun_angle_max = 8.0
	var sky := Sky.new()
	sky.sky_material = sky_mat
	env.background_mode = Environment.BG_SKY
	env.sky = sky
	env.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	env.ambient_light_energy = e["ambient"]
	env.tonemap_mode = Environment.TONE_MAPPER_AGX
	env.tonemap_exposure = e["exposure"]
	env.ssao_enabled = true
	env.ssao_radius = 1.5
	env.ssao_intensity = 2.0
	env.ssil_enabled = true
	env.glow_enabled = true
	env.glow_intensity = 0.6
	env.glow_bloom = 0.05
	env.fog_enabled = true
	env.fog_light_color = e["fog_color"]
	env.fog_density = e["fog"]
	env.fog_height = 2.0
	env.fog_height_density = 0.15
	env.volumetric_fog_enabled = true
	env.volumetric_fog_density = e["vol_fog"]
	env.volumetric_fog_albedo = e["vol_albedo"]
	env.volumetric_fog_length = 80.0
	env.adjustment_enabled = true
	env.adjustment_contrast = 1.08
	env.adjustment_saturation = e["saturation"]
	var we := WorldEnvironment.new()
	we.environment = env
	add_child(we)
	# Soleil couchant ou lune, selon le niveau.
	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = e["sun_rot"]
	sun.light_color = e["sun_color"]
	sun.light_energy = e["sun_energy"]
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
	ground.add_child(Fx.mesh_with(plane, _level_material(level["ground"])))
	add_child(ground)


func _build_path() -> void:
	var mud := _level_material(level["path_mat"])
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
	# Le mur est perpendiculaire au premier tronçon du chemin, juste derrière le portail.
	var fwd := Vector3(path_points[1].x - p.x, 0, path_points[1].z - p.z).normalized()
	var side := Vector3(fwd.z, 0, -fwd.x)
	var yaw := atan2(fwd.x, fwd.z)
	var concrete := Fx.textured("concrete", 0.3, Color(0.75, 0.72, 0.7))
	for x in [-14.0, 14.0]:
		var wall := Fx.box_body(Vector3(22, 5, 1.2), Color.WHITE, Fx.LAYER_WORLD)
		(wall.get_child(1) as MeshInstance3D).material_override = concrete
		wall.position = p + side * x - fwd * 3.0 + Vector3(0, 2.5, 0)
		wall.rotation.y = yaw
		add_child(wall)
	for x in [-3.3, 3.3]:
		var post := Fx.box_body(Vector3(1.0, 6, 1.4), Color.WHITE, Fx.LAYER_WORLD)
		(post.get_child(1) as MeshInstance3D).material_override = concrete
		post.position = p + side * x - fwd * 3.0 + Vector3(0, 3, 0)
		post.rotation.y = yaw
		add_child(post)
	var light := Flicker.new()
	light.light_color = Color(1.0, 0.15, 0.08)
	light.base_energy = 3.0
	light.speed = 4.0
	light.amount = 0.5
	light.omni_range = 14.0
	light.light_volumetric_fog_energy = 3.0
	add_child(light)
	light.position = p - fwd * 2.0 + Vector3(0, 5.5, 0)
	_build_portal_beacon(p, fwd)


## Colonne de lumière rouge et fumée au-dessus du portail : on voit d'où viennent les zombies, de partout.
func _build_portal_beacon(p: Vector3, fwd: Vector3) -> void:
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
	board.position = p - fwd * 2.0 + Vector3(0, 7, 0)
	_board = board


func _process(delta: float) -> void:
	# La neige, la cendre ou les lucioles suivent le joueur.
	if _weather and Game.player:
		_weather.global_position = Game.player.global_position + Vector3(0, 1.5 if level["weather"] == "fireflies" else 8.0, 0)
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
	var rings: Dictionary = level["rings"]
	for ring in rings:
		var data: Dictionary = rings[ring]
		for pos in data["sockets"]:
			var s := Socket.new()
			add_child(s)
			s.position = pos
			s.setup(ring)
			_blocked.append(pos)
		var barrier := Barrier.new()
		add_child(barrier)
		barrier.position = data["gate"]
		barrier.setup(GATE_NAMES[ring], ring, data["hp"] * Upgrades.mult("maconnerie"), data["segment"], GATE_WIDTH)
		_blocked.append(data["gate"])


func _build_base_walls() -> void:
	# Murets en béton et sacs de sable autour du Cœur (couverture).
	var concrete := Fx.textured("concrete", 0.3)
	var walls := [
		[Vector3(-9, 1, 30), Vector3(8, 2, 0.8)], [Vector3(9, 1, 40), Vector3(0.8, 2, 12)],
		[Vector3(-9, 1, 40), Vector3(0.8, 2, 12)], [Vector3(0, 1, 46), Vector3(18, 2, 0.8)],
	]
	walls.append_array(level["walls"])
	for wall in walls:
		var w := Fx.box_body(wall[1], Color.WHITE, Fx.LAYER_WORLD)
		(w.get_child(1) as MeshInstance3D).material_override = concrete
		w.position = wall[0]
		add_child(w)
		_blocked.append(wall[0])
	var cloth := Fx.textured("cloth", 1.2, Color(0.55, 0.48, 0.34), false)
	_sandbags(Vector3(4, 0, 29), 6, cloth)
	for start in level["sandbags"]:
		_sandbags(start, 6, cloth)


func _build_workbench() -> void:
	var bench := Workbench.new()
	add_child(bench)
	bench.position = WORKBENCH_POS
	bench.rotation.y = PI  # Le plan de travail fait face au Cœur.
	_blocked.append(WORKBENCH_POS)


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
	for data in level["spots"]:
		var base: Vector3 = data[0]
		var pole := Fx.mesh_with(_cyl(0.12, 7.0), metal)
		pole.position = base + Vector3(0, 3.5, 0)
		add_child(pole)
		var spot := SpotLight3D.new()
		spot.light_color = level["spot_color"]
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
	for pos in level["barrels"]:
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
	var snowy: bool = level["weather"] == "snow"
	var rock_mat := Fx.textured("concrete", 0.6, Color(0.62, 0.64, 0.68) if snowy else Color(0.5, 0.48, 0.45))
	var bark := Fx.textured("mud", 0.8, Color(0.3, 0.32, 0.24) if level["weather"] == "fireflies" else Color(0.45, 0.4, 0.35), false)
	var rust := Fx.textured("metal", 0.5, Color(0.8, 0.6, 0.5))
	# Types de décor du niveau et leur poids : {"count": 70, "rock": 5, "tree": 3, ...}.
	var props: Dictionary = level["props"]
	var kinds: Array = props.keys().filter(func(k): return k != "count")
	var total := 0
	for k in kinds:
		total += int(props[k])
	var placed := 0
	var tries := 0
	while placed < int(props["count"]) and tries < 3000:
		tries += 1
		var pos := Vector3(_rng.randf_range(-75, 75), 0, _rng.randf_range(-75, 75))
		if not _free_spot(pos, 6.0):
			continue
		placed += 1
		_blocked.append(pos)
		var roll := _rng.randi() % total
		var kind: String = kinds[0]
		for k in kinds:
			if roll < int(props[k]):
				kind = k
				break
			roll -= int(props[k])
		match kind:
			"rock":
				_rock(pos, rock_mat)
			"tree":
				_dead_tree(pos, bark)
			"wreck":
				_wreck(pos, rust)
			"pool":
				_pool(pos)
			"reeds":
				_reeds(pos)
			"container":
				_container(pos)
			"tank":
				_tank(pos)
			"pine":
				_pine(pos)
			"ruin":
				_ruin(pos)


## Matériau du sol ou du chemin d'un niveau : {"tex", "scale", "tint", "plain"}.
## "plain" garde le relief de la texture mais pas sa couleur (neige).
func _level_material(d: Dictionary) -> StandardMaterial3D:
	var mat := Fx.textured(d["tex"], d["scale"], d["tint"])
	if d.get("plain", false):
		mat.albedo_texture = null
		mat.roughness = 0.85
	return mat


# ---------------------------------------------------------------- décor propre à chaque niveau

## Mare d'eau noire et immobile (Marais) : un miroir sombre qui reflète les lumières.
func _pool(pos: Vector3) -> void:
	var r := _rng.randf_range(1.8, 3.2)
	var disc := CylinderMesh.new()
	disc.top_radius = r
	disc.bottom_radius = r
	disc.height = 0.02
	disc.radial_segments = 20
	var water := StandardMaterial3D.new()
	water.albedo_color = Color(0.03, 0.045, 0.04)
	water.metallic = 0.6
	water.roughness = 0.04
	var mi := Fx.mesh_with(disc, water)
	mi.scale = Vector3(_rng.randf_range(1.0, 1.7), 1, _rng.randf_range(0.8, 1.2))
	mi.rotation.y = _rng.randf() * TAU
	mi.position = pos + Vector3(0, 0.03, 0)
	add_child(mi)
	_reeds(pos + Vector3(r * 0.9, 0, 0))


## Touffe de roseaux secs.
func _reeds(pos: Vector3) -> void:
	var mat := Fx.material(Color(0.32, 0.3, 0.2))
	var tuft := Node3D.new()
	for i in _rng.randi_range(7, 13):
		var h := _rng.randf_range(0.8, 1.9)
		var blade := Fx.mesh_with(_cyl(0.025, h, 0.005), mat)
		blade.position = Vector3(_rng.randf_range(-0.5, 0.5), h * 0.5, _rng.randf_range(-0.5, 0.5))
		blade.rotation = Vector3(_rng.randf_range(-0.25, 0.25), 0, _rng.randf_range(-0.25, 0.25))
		tuft.add_child(blade)
	add_child(tuft)
	tuft.position = pos


## Conteneur maritime rouillé (Raffinerie), parfois deux empilés.
func _container(pos: Vector3) -> void:
	var colors := [Color(0.45, 0.16, 0.1), Color(0.15, 0.25, 0.35), Color(0.4, 0.35, 0.12), Color(0.2, 0.3, 0.2)]
	var stack := 2 if _rng.randf() < 0.3 else 1
	var yaw := _rng.randf() * TAU
	for i in stack:
		var mat := Fx.textured("metal", 0.6, colors[_rng.randi() % colors.size()])
		var box := Fx.box_body(Vector3(2.4, 2.6, 6.0), Color.WHITE, Fx.LAYER_WORLD)
		(box.get_child(1) as MeshInstance3D).material_override = mat
		# Nervures de la tôle.
		for k in 7:
			var rib := Fx.box_mat(Vector3(2.46, 2.5, 0.08), mat)
			rib.position = Vector3(0, 0, -2.7 + k * 0.9)
			box.add_child(rib)
		add_child(box)
		box.position = pos + Vector3(0, 1.3 + i * 2.6, 0)
		box.rotation.y = yaw + (_rng.randf_range(-0.2, 0.2) if i > 0 else 0.0)


## Cuve de stockage avec une échelle (Raffinerie).
func _tank(pos: Vector3) -> void:
	var r := _rng.randf_range(2.0, 3.0)
	var h := _rng.randf_range(4.0, 7.0)
	var mat := Fx.textured("metal", 0.4, Color(0.55, 0.53, 0.5))
	var body := StaticBody3D.new()
	body.collision_layer = Fx.LAYER_WORLD
	var shape := CollisionShape3D.new()
	var cs := CylinderShape3D.new()
	cs.radius = r
	cs.height = h
	shape.shape = cs
	shape.position.y = h * 0.5
	body.add_child(shape)
	var c := _cyl(r, h)
	c.radial_segments = 24
	var mi := Fx.mesh_with(c, mat)
	mi.position.y = h * 0.5
	body.add_child(mi)
	var dark := Fx.material(Color(0.12, 0.12, 0.12))
	for side in [-0.25, 0.25]:
		var rail := Fx.mesh_with(_cyl(0.04, h + 0.8), dark)
		rail.position = Vector3(side, (h + 0.8) * 0.5, r + 0.25)
		body.add_child(rail)
	var lamp := Fx.box(Vector3(0.2, 0.2, 0.2), Color(1.0, 0.25, 0.15), 4.0)
	lamp.position = Vector3(0, h + 0.1, 0)
	body.add_child(lamp)
	add_child(body)
	body.position = pos
	body.rotation.y = _rng.randf() * TAU


## Sapin enneigé (Col Gelé) : tronc et étages de branches coniques, la neige sur le dessus.
func _pine(pos: Vector3) -> void:
	var tree := Node3D.new()
	var h := _rng.randf_range(6.0, 11.0)
	var trunk := Fx.mesh_with(_cyl(0.22, h * 0.35, 0.18), Fx.textured("mud", 0.8, Color(0.3, 0.24, 0.2), false))
	trunk.position.y = h * 0.175
	tree.add_child(trunk)
	var needles := Fx.material(Color(0.06, 0.11, 0.08))
	var snow := Fx.material(Color(0.82, 0.85, 0.9))
	var tiers := 4
	for i in tiers:
		var t := float(i) / tiers
		var cone := CylinderMesh.new()
		cone.top_radius = 0.05
		cone.bottom_radius = lerpf(2.3, 0.8, t) * h / 9.0
		cone.height = h * 0.32
		cone.radial_segments = 10
		var y := h * (0.22 + 0.2 * i)
		var mi := Fx.mesh_with(cone, needles)
		mi.position.y = y
		tree.add_child(mi)
		var cap := CylinderMesh.new()
		cap.top_radius = 0.04
		cap.bottom_radius = cone.bottom_radius * 0.75
		cap.height = cone.height * 0.55
		cap.radial_segments = 10
		var sm := Fx.mesh_with(cap, snow)
		sm.position.y = y + cone.height * 0.25
		tree.add_child(sm)
	var body := StaticBody3D.new()
	body.collision_layer = Fx.LAYER_WORLD
	var shape := CollisionShape3D.new()
	var cs := CylinderShape3D.new()
	cs.radius = 0.4
	cs.height = h
	shape.shape = cs
	shape.position.y = h * 0.5
	body.add_child(shape)
	tree.add_child(body)
	tree.rotation.y = _rng.randf() * TAU
	add_child(tree)
	tree.position = pos


## Maison en ruine (Col Gelé) : deux ou trois pans de mur effondrés, de la neige au pied.
func _ruin(pos: Vector3) -> void:
	var stone := Fx.textured("concrete", 0.5, Color(0.55, 0.53, 0.52))
	var ruin := Node3D.new()
	var w := _rng.randf_range(5.0, 7.0)
	var walls := [[Vector3(0, 0, -w * 0.5), 0.0], [Vector3(-w * 0.5, 0, 0), PI * 0.5], [Vector3(w * 0.5, 0, 0), PI * 0.5]]
	for i in _rng.randi_range(2, 3):
		var h := _rng.randf_range(1.2, 3.5)
		var len := w * _rng.randf_range(0.5, 1.0)
		var wall := Fx.box_body(Vector3(len, h, 0.5), Color.WHITE, Fx.LAYER_WORLD)
		(wall.get_child(1) as MeshInstance3D).material_override = stone
		wall.position = walls[i][0] + Vector3(0, h * 0.5, 0)
		wall.rotation.y = walls[i][1]
		ruin.add_child(wall)
		var cap := Fx.box_mat(Vector3(len, 0.08, 0.6), Fx.material(Color(0.82, 0.85, 0.9)))
		cap.position = Vector3(0, h * 0.5 + 0.04, 0)
		wall.add_child(cap)
	add_child(ruin)
	ruin.position = pos
	ruin.rotation.y = _rng.randf() * TAU


# ---------------------------------------------------------------- météo

## Neige (Col Gelé), cendres (Raffinerie) ou lucioles (Marais), autour du joueur.
func _build_weather() -> void:
	var kind: String = level["weather"]
	if kind == "":
		return
	var p := CPUParticles3D.new()
	p.local_coords = false
	p.emission_shape = CPUParticles3D.EMISSION_SHAPE_BOX
	p.emission_box_extents = Vector3(22, 2, 22)
	var q := QuadMesh.new()
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.vertex_color_use_as_albedo = true
	m.albedo_texture = _soft_texture()
	var grad := Gradient.new()
	match kind:
		"snow":
			p.amount = 1400
			p.lifetime = 5.0
			p.direction = Vector3(0.3, -1, 0.1)
			p.spread = 15.0
			p.initial_velocity_min = 2.0
			p.initial_velocity_max = 3.5
			p.gravity = Vector3(0.6, -0.4, 0.2)
			q.size = Vector2(0.06, 0.06)
			grad.set_color(0, Color(0.95, 0.97, 1.0, 0.9))
			grad.set_color(1, Color(0.95, 0.97, 1.0, 0.0))
		"ash":
			p.amount = 500
			p.lifetime = 7.0
			p.direction = Vector3(0.2, -1, 0)
			p.spread = 30.0
			p.initial_velocity_min = 0.5
			p.initial_velocity_max = 1.2
			p.gravity = Vector3(0.25, -0.15, 0.05)
			q.size = Vector2(0.05, 0.05)
			grad.set_color(0, Color(0.55, 0.52, 0.5, 0.8))
			grad.set_color(1, Color(0.4, 0.38, 0.36, 0.0))
		"fireflies":
			p.amount = 70
			p.lifetime = 6.0
			p.emission_box_extents = Vector3(25, 3, 25)
			p.direction = Vector3.UP
			p.spread = 180.0
			p.initial_velocity_min = 0.1
			p.initial_velocity_max = 0.4
			p.gravity = Vector3.ZERO
			q.size = Vector2(0.07, 0.07)
			m.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
			m.disable_fog = true
			grad.set_color(0, Color(0.8, 1.0, 0.4, 0.0))
			grad.add_point(0.5, Color(0.8, 1.0, 0.4, 0.9))
			grad.set_color(grad.get_point_count() - 1, Color(0.8, 1.0, 0.4, 0.0))
	q.material = m
	p.mesh = q
	p.color_ramp = grad
	add_child(p)
	_weather = p


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
