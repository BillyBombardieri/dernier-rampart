class_name WaveManager
extends Node
## Enchaîne les phases : Préparation → Assaut → (choix d'implant après un siège) → Récolte → vague suivante.

const PREP_TIME := 30.0
const FIRST_PREP_TIME := 45.0
const HARVEST_TIME := 15.0
const SIEGE_EVERY := 5

signal implant_choice(options: Array)

var _queue: Array[String] = []
var _spawn_cd := 0.0
var _alive := 0


func _ready() -> void:
	Game.wave = 1
	_enter("prep", FIRST_PREP_TIME)


func is_siege(w: int) -> bool:
	return w % SIEGE_EVERY == 0


func _enter(phase: String, duration: float) -> void:
	Game.phase = phase
	Game.phase_time = duration
	match phase:
		"prep":
			var label := "NUIT DE SIÈGE" if is_siege(Game.wave) else "Vague %d" % Game.wave
			Game.say("%s dans %d s. Construis tes défenses (Entrée pour lancer)." % [label, int(duration)])
		"assault":
			_queue = _build_queue(Game.wave)
			_spawn_cd = 0.0
			Game.say("NUIT DE SIÈGE : un boss arrive !" if is_siege(Game.wave) else "Vague %d : ils arrivent !" % Game.wave)
		"harvest":
			Game.say("Vague repoussée ! Ramasse la ferraille avant qu'elle disparaisse.")
	Game.changed.emit()


func _process(delta: float) -> void:
	if Game.is_over:
		return
	match Game.phase:
		"prep":
			Game.phase_time -= delta
			if Game.phase_time <= 0.0 or Input.is_action_just_pressed("skip_phase"):
				_enter("assault", 0.0)
		"assault":
			_spawn_cd -= delta
			if not _queue.is_empty() and _spawn_cd <= 0.0:
				_spawn(_queue.pop_front())
				_spawn_cd = max(0.35, 1.6 - 0.12 * Game.wave)
			if _queue.is_empty() and _alive <= 0:
				_wave_cleared()
		"harvest":
			Game.phase_time -= delta
			if Game.phase_time <= 0.0 or Input.is_action_just_pressed("skip_phase"):
				for s in get_tree().get_nodes_in_group("scrap"):
					s.queue_free()
				Game.wave += 1
				_enter("prep", PREP_TIME)
	Game.changed.emit()


func _wave_cleared() -> void:
	if Game.wave >= Game.LAST_WAVE:
		Game.end_game(true)
		return
	if is_siege(Game.wave):
		var options := Game.implant_choices(3)
		if not options.is_empty():
			Game.phase = "implant"
			implant_choice.emit(options)
			return
	_enter("harvest", HARVEST_TIME)


## Appelé par l'interface une fois l'implant choisi.
func implant_chosen(id: String) -> void:
	Game.add_implant(id)
	_enter("harvest", HARVEST_TIME)


func _build_queue(w: int) -> Array[String]:
	var q: Array[String] = []
	var count := 6 + w * 3
	for i in count:
		var r := randf()
		var t := "rodeur"
		if w >= 2 and r < 0.3:
			t = "coureur"
		elif w >= 3 and r > 0.88:
			t = "brute"
		q.append(t)
	if is_siege(w):
		q.insert(count / 2, "boss")
	return q


func _spawn(type: String) -> void:
	var z := Zombie.new()
	var portal: Vector3 = Game.main.path_points[0]
	Game.main.add_child(z)
	z.global_position = portal + Vector3(randf_range(-2.5, 2.5), 0.2, randf_range(-1.5, 1.5))
	z.setup(type, 1.0 + 0.15 * (Game.wave - 1))
	z.tree_exited.connect(_on_zombie_gone.bind(z))
	_alive += 1


func _on_zombie_gone(_z: Zombie) -> void:
	_alive -= 1
