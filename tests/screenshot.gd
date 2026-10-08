extends Node
## Captures d'écran automatiques pour vérifier le rendu : menu principal, barrière de l'Avant-poste
## attaquée, nouveaux zombies de près, barre de construction, établi et réglages.
## Lancer : godot --path . --fixed-fps 60 res://tests/screenshot.tscn -- <dossier_de_sortie>

# Tours posées pour les captures : [ancrage le plus proche, type].
const LAYOUT := [
	[Vector3(-5.5, 0, -39.5), "flame"], [Vector3(5.5, 0, -39.5), "arc"],
	[Vector3(6.5, 0, -33), "mortar"], [Vector3(-12, 0, -36), "beacon"],
]

var _out := "user://"
var _main: Node3D


func _ready() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		_out = args[0]
	seed(7)
	Settings.use_test_file()
	Settings.best_score = 0  # Pas de record affiché sur la capture du menu.
	Settings.best_wave = 0
	Settings.tutorial_done = true  # Pas de tutoriel sur les captures de la partie.
	await _frames(5)
	# 1. Menu principal.
	var menu: Control = load("res://scenes/menu.tscn").instantiate()
	add_child(menu)
	await _frames(40)
	await _capture("menu-principal")
	menu.free()
	# Partie : les 4 nouvelles tours autour de la barrière de l'Avant-poste.
	_main = load("res://scenes/main.tscn").instantiate()
	add_child(_main)
	Game.tutorial_hold = true
	Game.scrap = 2000
	Game.generator = 2
	for entry in LAYOUT:
		_socket_near(entry[0]).build(entry[1])
	Game.scrap = 340
	var p := Game.player as Player
	var wm: WaveManager = _main.get_node("WaveManager")
	# 2. La horde attaque la barrière de l'Avant-poste, les nouvelles tours ripostent.
	wm._enter("assault", 0.0)
	_place(p, Vector3(-0.5, 0.2, -31.0), 2.0, -4.0, "rifle")
	for t in ["brute", "rodeur", "cracheur", "rodeur", "hurleur", "coureur", "rodeur", "rodeur", "cracheur"]:
		var z := Zombie.new()
		_main.add_child(z)
		z.global_position = Vector3(randf_range(-3.0, 3.0), 0.2, randf_range(-55.0, -50.0))
		z.setup(t, 6.0)
	await _frames(430)
	await _capture("barriere-avant-poste")
	await _frames(70)
	await _capture("barriere-avant-poste-2")
	# Fin de l'assaut pour la suite (l'établi ne s'ouvre qu'entre les vagues).
	wm._queue.clear()
	for z in get_tree().get_nodes_in_group("zombies"):
		z.queue_free()
	for n in _main.get_children():
		if n is Projectile:
			n.queue_free()
	wm._enter("prep", 25.0)
	# 3. Les trois nouveaux zombies de près, à la lampe torche (et une brute enragée par le Hurleur).
	_place(p, Vector3(-18, 0.2, -6), 0.0, -3.0, "pistol")
	p._flashlight.visible = true
	await _frames(5)
	var demo := [["cracheur", 3.6, -1.5], ["hurleur", 4.8, 0.1], ["fouisseur", 3.5, 1.6], ["brute", 7.0, -0.3]]
	for d in demo:
		var z := Zombie.new()
		_main.add_child(z)
		z.setup(d[0], 1.0)
		z.set_physics_process(false)
		z.global_position = p.global_position + Vector3(d[2], -0.2, -d[1])
		z.look_at(Vector3(p.global_position.x, z.global_position.y, p.global_position.z), Vector3.UP)
		z._walk_phase = randf() * TAU
		z.velocity = -z.global_transform.basis.z * 1.5
		if d[0] == "hurleur":
			z._scream_anim = 0.9
		elif d[0] == "cracheur":
			z._attack_anim = 0.55
		else:
			z.buff_time = 5.0
		z._update_status(0.0)
		z._animate(0.05)
	await _frames(20)
	await _capture("nouveaux-zombies")
	p._flashlight.visible = false
	for z in get_tree().get_nodes_in_group("zombies"):
		z.queue_free()
	# 4. Barre de construction : le joueur vise un ancrage vide de la Muraille.
	_place(p, Vector3(-4, 0.2, 10.6), 0.0, -21.0, "rifle")
	await _frames(30)
	p.cycle_build(2)
	await _frames(10)
	await _capture("construction")
	# 5. Établi.
	_place(p, Vector3(-5.5, 0.2, 30.5), 180.0, -10.0, "rifle")
	await _frames(10)
	_main.open_bench()
	await _frames(10)
	await _capture("etabli")
	_main.bench_panel.close()
	# 6. Réglages (menu pause).
	_main.pause_menu.open()
	_main.pause_menu._open_settings()
	await _frames(10)
	await _capture("reglages")
	get_tree().quit()


func _place(p: Player, pos: Vector3, rot_y: float, pitch: float, weapon: String) -> void:
	p.global_position = pos
	p.velocity = Vector3.ZERO
	p.rotation.y = deg_to_rad(rot_y)
	p._aim_pitch = deg_to_rad(pitch)
	if p.weapon != weapon:
		p._switch(weapon)


func _socket_near(pos: Vector3) -> Socket:
	var best: Socket = null
	for c in _main.get_children():
		if c is Socket and (best == null or c.global_position.distance_to(pos) < best.global_position.distance_to(pos)):
			best = c
	return best


func _frames(n: int) -> void:
	for i in n:
		await get_tree().process_frame


func _capture(file: String) -> void:
	await RenderingServer.frame_post_draw
	var img := get_viewport().get_texture().get_image()
	img.save_png("%s/%s.png" % [_out, file])
	print("Capture ", file)
