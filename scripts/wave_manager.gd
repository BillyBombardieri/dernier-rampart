class_name WaveManager
extends Node
## Enchaîne les phases : Préparation → Assaut → (choix d'implant après un siège) → Récolte → vague suivante.

const PREP_TIME := 25.0
const FIRST_PREP_TIME := 40.0
const HARVEST_TIME := 12.0
# Montée en difficulté par vague (vague 1 = valeurs de base).
const HP_PER_WAVE := 0.2
const SPEED_PER_WAVE := 0.02
const DAMAGE_PER_WAVE := 0.08
const SIEGE_EVERY := 5
# Message affiché la première fois qu'un nouveau type de zombie apparaît.
const INTRO := {
	"cracheur": "Nouveau zombie : le CRACHEUR (vert). Il reste en retrait et crache de l'acide sur la barrière et sur toi, par-dessus la horde.",
	"hurleur": "Nouveau zombie : le HURLEUR (orange). Son cri rend les zombies autour de lui plus rapides et plus forts. Abats-le en priorité.",
	"fouisseur": "Nouveau zombie : le FOUISSEUR (brun). Il creuse sous les barrières et ressort juste derrière. Un Phare le fait sortir de terre.",
}

signal implant_choice(options: Array)

var _queue: Array[String] = []
var _spawn_cd := 0.0
var _alive := 0
var _seen := {}
var _bonus := 0  # Prime de ferraille de la dernière vague repoussée.
var wave_total := 1


func _ready() -> void:
	Game.wave = 1
	_enter("prep", FIRST_PREP_TIME)


## Zombies de la vague encore à venir ou en vie.
func remaining() -> int:
	return _queue.size() + _alive


func phase_duration() -> float:
	match Game.phase:
		"prep":
			return FIRST_PREP_TIME if Game.wave == 1 else PREP_TIME
		"harvest":
			return HARVEST_TIME
	return 1.0


func is_siege(w: int) -> bool:
	return w % SIEGE_EVERY == 0


func _enter(phase: String, duration: float) -> void:
	Game.phase = phase
	Game.phase_time = duration
	match phase:
		"prep":
			var label := "NUIT DE SIÈGE" if is_siege(Game.wave) else "Vague %d" % Game.wave
			Game.say("%s dans %d s. Construis tes défenses (%s pour lancer)." % [label, int(duration), Settings.key_label("skip_phase")])
		"assault":
			Sfx.play(self, "siren", -8.0, 0.0)
			_queue = _build_queue(Game.wave)
			wave_total = _queue.size()
			_spawn_cd = 0.0
			Game.say("NUIT DE SIÈGE : un boss arrive !" if is_siege(Game.wave) else "Vague %d : ils arrivent !" % Game.wave)
		"harvest":
			Game.say("Vague repoussée : santé restaurée et +%d ferraille de prime. Ramasse le reste avant qu'il disparaisse." % _bonus)
	Game.changed.emit()


func _process(delta: float) -> void:
	if Game.is_over:
		return
	match Game.phase:
		"prep":
			if not Game.tutorial_hold:
				Game.phase_time -= delta
			if Game.phase_time <= 0.0 or Input.is_action_just_pressed("skip_phase"):
				_enter("assault", 0.0)
		"assault":
			_spawn_cd -= delta
			if not _queue.is_empty() and _spawn_cd <= 0.0:
				_spawn(_queue.pop_front())
				_spawn_cd = max(0.3, 1.3 - 0.11 * Game.wave)
				# À partir de la vague 3, les zombies arrivent parfois en meute serrée.
				if Game.wave >= 3 and randf() < 0.15 + 0.03 * Game.wave:
					_spawn_cd = 0.15
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
	Game.add_score(Game.WAVE_POINTS * Game.wave)
	if Game.wave >= Game.LAST_WAVE:
		Game.end_game(true)
		return
	_bonus = Game.wave_scrap(Game.wave)
	Game.add_scrap(_bonus)
	# Entre deux vagues, le joueur récupère toute sa santé.
	var player := Game.player as Player
	if player:
		player.heal_full()
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
	var count := 8 + w * 4
	var runner_chance := minf(0.15 + 0.03 * w, 0.4)
	var brute_chance := 0.0 if w < 2 else minf(0.06 + 0.02 * w, 0.22)
	var spitter_chance := 0.0 if w < 3 else minf(0.06 + 0.015 * (w - 3), 0.14)
	for i in count:
		var r := randf()
		var t := "rodeur"
		if r < runner_chance:
			t = "coureur"
		elif r < runner_chance + spitter_chance:
			t = "cracheur"
		elif r > 1.0 - brute_chance:
			t = "brute"
		q.append(t)
	# Hurleurs (dès la vague 4) au milieu de la vague, au cœur de la horde.
	if w >= 4:
		for i in 1 + (w - 4) / 3:
			q.insert(randi_range(count / 4, count * 3 / 4), "hurleur")
	# Fouisseurs (dès la vague 5) : ils creusent sous les barrières.
	if w >= 5:
		for i in 1 + (w - 5) / 2:
			q.insert(randi_range(count / 5, count * 4 / 5), "fouisseur")
	# Fin de vague : une ruée de coureurs pour mettre la pression.
	if w >= 4:
		for i in w - 2:
			q.append("coureur")
	if is_siege(w):
		q.insert(count / 2, "boss")
	return q


func _spawn(type: String) -> void:
	var z := Zombie.new()
	var portal: Vector3 = Game.main.path_points[0]
	Game.main.add_child(z)
	z.global_position = portal + Vector3(randf_range(-2.5, 2.5), 0.2, randf_range(-1.5, 1.5))
	var w := Game.wave - 1
	z.setup(type, 1.0 + HP_PER_WAVE * w, 1.0 + SPEED_PER_WAVE * w, 1.0 + DAMAGE_PER_WAVE * w)
	z.died.connect(_on_zombie_died)
	_alive += 1
	if INTRO.has(type) and not _seen.has(type):
		_seen[type] = true
		Game.say(INTRO[type])


func _on_zombie_died(_z: Zombie) -> void:
	_alive -= 1
