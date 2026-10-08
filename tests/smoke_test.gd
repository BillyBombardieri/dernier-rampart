extends Node
## Test automatique : lance une partie, pose des tours partout et laisse les vagues tourner.
## Lancer : godot --headless --fixed-fps 60 --path . res://tests/smoke_test.tscn

const MAX_FRAMES := 120000

var _main: Node
var _frames := 0
var _last_wave := 0
var _brise_checked := false


func _ready() -> void:
	_main = load("res://scenes/main.tscn").instantiate()
	add_child(_main)
	Game.ended.connect(_on_ended)
	for c in _main.get_children():
		if c is WaveManager:
			c.implant_choice.connect(_on_implant.bind(c))
	Game.scrap = 2000
	var i := 0
	for c in _main.get_children():
		if c is Socket:
			c.build("gun" if i % 2 == 0 else "cryo")
			i += 1
	print("Tours posées : ", get_tree().get_nodes_in_group("towers").size(), "  énergie ", Game.energy_used, "/", Game.energy_cap())


func _on_implant(options: Array, wm: WaveManager) -> void:
	print("Choix d'implant proposé : ", options)
	get_tree().paused = false
	wm.implant_chosen(options[0])


func _on_ended(victory: bool) -> void:
	print("FIN : victoire=", victory, " vague=", Game.wave, " cœur=", Game.core.hp, " morts joueur=", Game.deaths, " implants=", Game.implants)
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
		print("Vague ", Game.wave, " | frame ", _frames, " | ferraille ", Game.scrap, " | cœur ", Game.core.hp, " | tours ", get_tree().get_nodes_in_group("towers").size())
	if _frames >= MAX_FRAMES:
		print("Limite de frames atteinte, vague ", Game.wave, " phase ", Game.phase)
		get_tree().quit(1)
