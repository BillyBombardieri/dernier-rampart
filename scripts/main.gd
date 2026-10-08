extends Node3D
## Construit la carte du prototype : un portail, un couloir, deux anneaux (Avant-poste, Muraille) et le Cœur.

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


func _ready() -> void:
	Game.reset()
	Game.main = self
	for p in PATH:
		path_points.append(p)
	_build_environment()
	_build_ground()
	_build_path()
	_build_portal()
	_build_core()
	_build_rings()
	_spawn_player()
	var wm := WaveManager.new()
	add_child(wm)
	var hud := Hud.new()
	hud.wave_manager = wm
	add_child(hud)
	wm.implant_choice.connect(hud.show_implants)


func relay_for_ring(ring: String) -> Structure:
	return _relays.get(ring)


func _build_environment() -> void:
	var env := Environment.new()
	var sky_mat := ProceduralSkyMaterial.new()
	sky_mat.sky_top_color = Color(0.12, 0.1, 0.2)
	sky_mat.sky_horizon_color = Color(0.55, 0.3, 0.25)
	sky_mat.ground_horizon_color = Color(0.3, 0.2, 0.18)
	sky_mat.ground_bottom_color = Color(0.05, 0.05, 0.05)
	var sky := Sky.new()
	sky.sky_material = sky_mat
	env.background_mode = Environment.BG_SKY
	env.sky = sky
	env.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	env.ambient_light_energy = 0.6
	env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	env.fog_enabled = true
	env.fog_light_color = Color(0.35, 0.25, 0.25)
	env.fog_density = 0.008
	env.glow_enabled = true
	var we := WorldEnvironment.new()
	we.environment = env
	add_child(we)
	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-50, -30, 0)
	sun.light_color = Color(1.0, 0.8, 0.65)
	sun.light_energy = 1.1
	sun.shadow_enabled = true
	add_child(sun)


func _build_ground() -> void:
	var ground := Fx.box_body(Vector3(160, 1, 160), Color(0.22, 0.25, 0.2), Fx.LAYER_WORLD)
	ground.position.y = -0.5
	add_child(ground)
	# Murets de la base autour du Cœur (décor + couverture).
	for wall in [
		[Vector3(-9, 1, 30), Vector3(8, 2, 1)], [Vector3(9, 1, 40), Vector3(1, 2, 12)],
		[Vector3(-9, 1, 40), Vector3(1, 2, 12)], [Vector3(0, 1, 46), Vector3(18, 2, 1)],
		[Vector3(-30, 1.5, 10), Vector3(2, 3, 14)], [Vector3(28, 1.5, -20), Vector3(2, 3, 18)],
	]:
		var w := Fx.box_body(wall[1], Color(0.4, 0.38, 0.35), Fx.LAYER_WORLD)
		w.position = wall[0]
		add_child(w)


func _build_path() -> void:
	for i in path_points.size() - 1:
		var a := path_points[i]
		var b := path_points[i + 1]
		var seg := Fx.box(Vector3(4.0, 0.05, a.distance_to(b) + 4.0), Color(0.35, 0.25, 0.18))
		add_child(seg)
		seg.global_position = (a + b) * 0.5 + Vector3(0, 0.02, 0)
		seg.look_at(Vector3(b.x, seg.global_position.y, b.z), Vector3.UP)


func _build_portal() -> void:
	var p := path_points[0]
	for x in [-3.5, 3.5]:
		var pillar := Fx.box(Vector3(1, 6, 1), Color(0.2, 0.05, 0.05))
		pillar.position = p + Vector3(x, 3, -2)
		add_child(pillar)
	var top := Fx.box(Vector3(8, 1, 1), Color(0.2, 0.05, 0.05))
	top.position = p + Vector3(0, 6.5, -2)
	add_child(top)
	var glow := Fx.box(Vector3(6, 5.5, 0.2), Color(0.9, 0.1, 0.1), 2.5)
	glow.position = p + Vector3(0, 2.75, -2)
	add_child(glow)


func _build_core() -> void:
	var core := Structure.new()
	add_child(core)
	core.position = CORE_POS
	core.setup("core", "Cœur", "", 1500.0, Vector3(4, 4, 4), Color(0.3, 0.6, 1.0))
	Game.core = core


func _build_rings() -> void:
	for ring in RINGS:
		var data: Dictionary = RINGS[ring]
		var relay := Structure.new()
		add_child(relay)
		relay.position = data["relay"]
		relay.setup("relay", data["name"], ring, 500.0, Vector3(1.6, 3, 1.6), Color(0.9, 0.8, 0.3))
		_relays[ring] = relay
		for pos in data["sockets"]:
			var s := Socket.new()
			add_child(s)
			s.position = pos
			s.setup(ring)


func _spawn_player() -> void:
	var player := Player.new()
	player.position = Vector3(0, 0.2, 27)
	add_child(player)
	Game.player = player
