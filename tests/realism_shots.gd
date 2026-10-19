extends Node
## Captures pour juger le réalisme des armes et des bâtiments : pistolet et fusil en main (nuit du
## chapitre 1 et coucher de soleil du chapitre 6), visée au fusil, le Cœur et ses murets, un
## immeuble de la Ville Morte et une ruine du Col Gelé, à la lampe torche.
## Lancer : xvfb-run -a -s "-screen 0 1280x720x24" godot --path . --rendering-driver vulkan
##          --fixed-fps 60 res://tests/realism_shots.tscn -- <dossier> [préfixe] [prises, ex. armes,base]

const CORE_POS := Vector3(0, 0, 35)  # Position du Cœur (main.gd).

var _out := "user://"
var _prefix := "realisme"
var _only: Array = []
var _main: Node3D


func _ready() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		_out = args[0]
	if args.size() > 1:
		_prefix = args[1]
	if args.size() > 2:
		_only = Array(args[2].split(","))
	Settings.use_test_file()
	Settings.tutorial_done = true
	if _wanted("armes"):
		await _open(1)
		var p := _player(Vector3(5.5, 0.2, 24.0), 200.0, -8.0, "pistol")
		await _frames(40)
		await _capture("pistolet-nuit")
		_place(p, p.global_position, 200.0, -8.0, "rifle")
		await _frames(40)
		await _capture("fusil-nuit")
		Input.action_press("aim")
		await _frames(40)
		await _capture("fusil-visee-nuit")
		Input.action_release("aim")
		await _close()
		await _open(6)
		p = _player(Vector3(5.5, 0.2, 24.0), 140.0, -6.0, "pistol")
		await _frames(40)
		await _capture("pistolet-soir")
		_place(p, p.global_position, 140.0, -6.0, "rifle")
		await _frames(40)
		await _capture("fusil-soir")
		await _close()
	if _wanted("base"):
		await _open(1)
		_player(CORE_POS + Vector3(7.0, 0.2, -11.0), 150.0, -4.0, "rifle")
		await _frames(60)
		await _capture("base-nuit")
		await _close()
		await _open(6)
		_player(CORE_POS + Vector3(7.0, 0.2, -11.0), 150.0, -4.0, "rifle")
		await _frames(60)
		await _capture("base-soir")
		await _close()
	if _wanted("ville"):
		await _open(5)
		var at := Vector3(-40.0, 0.2, 60.0)
		var p := _player(at, 0.0, 6.0, "pistol")
		p._flashlight.visible = true
		_main._rng.seed = 3
		_main._building(at + Vector3(-1.0, -0.2, -17.0))
		(_main.get_child(-1) as Node3D).rotation.y = 0.12
		_lit_lamp(at + Vector3(4.5, -0.2, -9.0))
		await _frames(60)
		await _capture("immeuble")
		await _close()
	if _wanted("ruine"):
		await _open(4)
		var at := Vector3(-40.0, 0.2, 60.0)
		var p := _player(at, 0.0, -4.0, "pistol")
		p._flashlight.visible = true
		_main._rng.seed = 4
		_main._ruin(at + Vector3(0.0, -0.2, -9.0))
		(_main.get_child(-1) as Node3D).rotation.y = 0.3
		await _frames(60)
		await _capture("ruine")
		await _close()
	get_tree().quit()


func _wanted(shot: String) -> bool:
	return _only.is_empty() or _only.has(shot)


func _open(chapter: int) -> void:
	seed(11)
	Game.level = (chapter - 1) * Levels.PER_CHAPTER
	_main = load("res://scenes/main.tscn").instantiate()
	add_child(_main)
	Game.tutorial_hold = true
	await _frames(5)


func _close() -> void:
	_main.queue_free()
	await _frames(3)


func _player(pos: Vector3, yaw: float, pitch: float, weapon: String) -> Player:
	var p := Game.player as Player
	_place(p, pos, yaw, pitch, weapon)
	return p


func _place(p: Player, pos: Vector3, yaw: float, pitch: float, weapon: String) -> void:
	p.global_position = pos
	p.velocity = Vector3.ZERO
	p.rotation.y = deg_to_rad(yaw)
	p._aim_pitch = deg_to_rad(pitch)
	if p.weapon != weapon:
		p._switch(weapon)


## Réverbère qui éclaire encore : on cherche une graine qui le tire « vivant ».
func _lit_lamp(pos: Vector3) -> void:
	for s in 50:
		_main._rng.seed = s
		if _main._rng.randf() < 0.34:
			_main._rng.seed = s
			_main._lamp(pos)
			return


func _capture(name: String) -> void:
	await RenderingServer.frame_post_draw
	get_viewport().get_texture().get_image().save_png("%s/%s-%s.png" % [_out, _prefix, name])
	print("Capture ", _prefix, "-", name)


func _frames(n: int) -> void:
	for i in n:
		await get_tree().process_frame
