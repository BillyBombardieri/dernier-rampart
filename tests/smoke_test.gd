extends Node
## Test automatique : lance une partie, pose des tours partout et laisse les vagues tourner.
## Lancer : godot --headless --fixed-fps 60 --path . res://tests/smoke_test.tscn [-- <numéro du niveau, 1 à 20>]

const MAX_FRAMES := 120000

var _main: Node
var _frames := 0
var _last_wave := 0
var _brise_checked := false
var _seen_types := {}


func _ready() -> void:
	Settings.use_test_file()
	var args := OS.get_cmdline_user_args()
	Game.level = clampi(int(args[0]) - 1, 0, Levels.count() - 1) if args.size() > 0 else 0
	print("Niveau ", Game.level + 1, " : ", Levels.title(Game.level))
	_main = load("res://scenes/main.tscn").instantiate()
	add_child(_main)
	Game.ended.connect(_on_ended)
	for c in _main.get_children():
		if c is WaveManager:
			c.implant_choice.connect(_on_implant.bind(c))
	# Pas de tutoriel pendant le test automatique.
	for c in _main.get_children():
		if c is Tutorial:
			c.free()
	Game.tutorial_hold = false
	Game.scrap = 2000
	Game.generator = 3
	# Cœur quasi indestructible pour parcourir les 10 vagues et tester les implants.
	Game.core.max_hp = 1000000.0
	Game.core.hp = 1000000.0
	# Les 6 types de tours, à tour de rôle sur les 9 ancrages.
	var i := 0
	for c in _main.get_children():
		if c is Socket:
			c.build(Tower.BUILD_ORDER[i % Tower.BUILD_ORDER.size()])
			i += 1
	var types := []
	for t in get_tree().get_nodes_in_group("towers"):
		types.append("%s%s" % [t.type, "" if t.powered else "(éteinte)"])
	print("Tours posées : ", types, "  énergie ", Game.energy_used, "/", Game.energy_cap())


func _on_implant(options: Array, wm: WaveManager) -> void:
	print("Choix d'implant proposé : ", options)
	get_tree().paused = false
	wm.implant_chosen(options[0])


func _on_ended(victory: bool) -> void:
	var gates := []
	for b in get_tree().get_nodes_in_group("gates"):
		gates.append("%s %d/%d" % [b.display_name, int(b.hp), int(b.max_hp)])
	print("FIN : victoire=", victory, " vague=", Game.wave, " cœur=", Game.core.hp, " morts joueur=", Game.deaths, " implants=", Game.implants, " score=", Game.score, " zombies abattus=", Game.kills)
	print("Barrières : ", gates, "  tours restantes : ", get_tree().get_nodes_in_group("towers").size())
	get_tree().quit(0)


func _physics_process(_delta: float) -> void:
	_frames += 1
	if Game.phase == "prep" or Game.phase == "harvest":
		Game.phase_time = min(Game.phase_time, 0.05)
	if not _brise_checked:
		for z in get_tree().get_nodes_in_group("zombies"):
			var hp_before: float = z.hp
			z.freeze(1.0)
			z.take_damage(10.0, true, true)
			print("Test BRISÉ : PV ", hp_before, " -> ", z.hp, " (attendu -30)")
			_brise_checked = true
			break
	if Game.wave != _last_wave:
		_last_wave = Game.wave
		var gates := []
		for b in get_tree().get_nodes_in_group("gates"):
			gates.append(int(b.hp))
		print("Vague ", Game.wave, " | frame ", _frames, " | ferraille ", Game.scrap, " | cœur ", Game.core.hp, " | tours ", get_tree().get_nodes_in_group("towers").size(), " | barrières ", gates, " | score ", Game.score)
	# Compte les types de zombies vus, pour vérifier que les nouveaux apparaissent.
	for z in get_tree().get_nodes_in_group("zombies"):
		if not _seen_types.has(z.type):
			_seen_types[z.type] = Game.wave
			print("  Premier ", z.type, " à la vague ", Game.wave)
	# Joue le rôle du joueur : achève les zombies qui traînent (ex. le boss au pied du Cœur).
	if _frames % 1500 == 0 and Game.phase == "assault":
		var left := get_tree().get_nodes_in_group("zombies")
		if not left.is_empty():
			var desc := []
			for z in left:
				desc.append("%s(%d PV)" % [z.type, int(z.hp)])
			print("  Zombies restants achevés : ", desc)
			for z in left:
				z.take_damage(100000.0, true, false)
	if _frames >= MAX_FRAMES:
		print("Limite de frames atteinte, vague ", Game.wave, " phase ", Game.phase)
		get_tree().quit(1)
