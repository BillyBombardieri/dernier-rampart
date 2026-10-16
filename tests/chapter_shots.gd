extends Node
## Une capture par chapitre, toujours au même endroit (derrière la barrière de l'Avant-poste,
## face au portail, quelques zombies sur le chemin) : pour comparer le rendu avant et après
## un changement graphique.
## Lancer : xvfb-run -a -s "-screen 0 1280x720x24" godot --path . --rendering-driver vulkan
##          --fixed-fps 60 res://tests/chapter_shots.tscn -- <dossier> [préfixe] [chapitres, ex. 1,3]

var _out := "user://"
var _prefix := "chapitre"


func _ready() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		_out = args[0]
	if args.size() > 1:
		_prefix = args[1]
	var chapters: Array = range(Levels.CHAPTERS.size())
	if args.size() > 2:
		chapters = Array(args[2].split(",")).map(func(s): return int(s) - 1)
	Settings.use_test_file()
	Settings.tutorial_done = true
	for ch in chapters:
		seed(11)
		Game.level = ch * Levels.PER_CHAPTER
		var main: Node3D = load("res://scenes/main.tscn").instantiate()
		add_child(main)
		Game.tutorial_hold = true
		await _frames(5)
		var data: Dictionary = Levels.chapter(Game.level)
		var ring: Dictionary = data["rings"]["avant"]
		var path: Array = data["path"]
		var s: int = ring["segment"]
		var d: Vector3 = (path[s + 1] - path[s]).normalized()
		var gate: Vector3 = ring["gate"]
		var side := Vector3(-d.z, 0, d.x)
		var p := Game.player as Player
		p.global_position = gate + d * 11.0 + side * 2.5 + Vector3(0, 0.2, 0)
		p.velocity = Vector3.ZERO
		var look := (gate - d * 10.0) - p.global_position
		p.rotation.y = atan2(-look.x, -look.z)
		p._aim_pitch = deg_to_rad(-3.0)
		var wm: WaveManager = main.get_node("WaveManager")
		wm._enter("assault", 0.0)
		wm._queue.clear()
		for i in 7:
			var z := Zombie.new()
			main.add_child(z)
			z.global_position = gate - d * (4.0 + i * 1.6) + side * randf_range(-1.5, 1.5) + Vector3(0, 0.2, 0)
			z.setup(["rodeur", "rodeur", "brute", "rodeur", "cracheur", "rodeur", "coureur"][i], 6.0)
			z.path_index = s + 1  # Il reprend le chemin là où il est posé.
		await _frames(100)
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png("%s/%s-%d.png" % [_out, _prefix, ch + 1])
		print("Capture ", _prefix, "-", ch + 1)
		main.queue_free()
		await _frames(3)
	get_tree().quit()


func _frames(n: int) -> void:
	for i in n:
		await get_tree().process_frame
