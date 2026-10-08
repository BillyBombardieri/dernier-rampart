class_name Tutorial
extends Node
## Tutoriel guidé de la première partie : une étape à la fois, validée par ce que fait le joueur.
## Il met en pause le compte à rebours de la première préparation. P pour le passer.
## Une fois terminé ou passé, il ne se relance plus (sauvegardé dans user://).

const SAVE_PATH := "user://reglages.cfg"

var hud: Hud
var _step := 0
var _steps: Array[Dictionary] = []
var _start_pos := Vector3.ZERO
var _turned := 0.0
var _last_yaw := 0.0
var _timer := 0.0
var _player_hits := 0
var _picked := 0


static func already_done() -> bool:
	var cfg := ConfigFile.new()
	if cfg.load(SAVE_PATH) != OK:
		return false
	return cfg.get_value("tutoriel", "termine", false)


func _ready() -> void:
	_steps = [
		{"text": "Déplace-toi avec [b]Z Q S D[/b] et cours avec [b]Maj[/b].", "check": _moved},
		{"text": "Regarde autour de toi avec la [b]souris[/b].", "check": _looked},
		{"text": "Les zombies arrivent du [color=#ff5a3c][b]PORTAIL ROUGE[/b][/color] : repère la colonne de lumière rouge (visible seulement avant la première vague) ou le [b]![/b] sur la minimap, toujours affiché. Ils suivent le chemin de boue jusqu'au [color=#5aa5ff][b]Cœur[/b][/color], qu'il faut protéger.", "check": _wait.bind(7.0)},
		{"text": "Approche d'un [b]ancrage[/b] (dalle de béton avec des repères orange) et appuie sur [b]E[/b] pour poser une [b]Mitrailleuse[/b].", "check": _built.bind("gun")},
		{"text": "Sur un autre ancrage, appuie sur [b]C[/b] pour poser une tour [color=#7fd8ff][b]Cryo[/b][/color]. Elle gèle les zombies.", "check": _built.bind("cryo")},
		{"text": "Chaque tour allumée consomme de l'[b]énergie[/b] (en haut à gauche). Vise une tour et appuie sur [b]X[/b] pour l'éteindre et récupérer son énergie, puis rallume-la.", "check": _toggled},
		{"text": "Prêt ? Appuie sur [b]Entrée[/b] pour lancer la première vague.", "check": _assault},
		{"text": "Vise avec le [b]clic droit[/b] et tire avec le [b]clic gauche[/b]. Un tir à la tête fait x2 dégâts.", "check": _shot.bind(3)},
		{"text": "Vise un zombie et appuie sur [b]F[/b] pour le [b]marquer[/b] : tes tours le ciblent en priorité (+25 % de dégâts).", "check": _marked},
		{"text": "Combo : un tir de [b]pistolet lourd[/b] ([b]&[/b]) sur un zombie gelé par la Cryo le [color=#7fd8ff][b]BRISE[/b][/color] (x3 dégâts).", "check": _wait.bind(8.0)},
		{"text": "Les zombies laissent de la [color=#ffb840][b]ferraille[/b][/color]. Va la ramasser : elle sert à construire et réparer.", "check": _collected},
		{"text": "Tutoriel terminé ! Si un [b]relais[/b] est détruit, les tours de son anneau s'éteignent : maintiens [b]E[/b] dessus pour le réparer. [b]H[/b] affiche l'aide. Bonne chance !", "check": _wait.bind(9.0)},
	]
	Game.tutorial_hold = true
	Game.hit_marker.connect(func(_kill: bool): _player_hits += 1)
	Game.scrap_picked.connect(func(amount: int): _picked += amount)
	_start_step()


func _process(delta: float) -> void:
	if Input.is_action_just_pressed("skip_tutorial"):
		Game.say("Tutoriel passé. H pour afficher l'aide.")
		_finish()
		return
	_timer += delta
	var p := Game.player as Player
	if p:
		var yaw := p.rotation.y
		_turned += absf(angle_difference(_last_yaw, yaw))
		_last_yaw = yaw
	var check: Callable = _steps[_step]["check"]
	if check.call():
		_step += 1
		if _step >= _steps.size():
			_finish()
			return
		_start_step()
	# La préparation reste en pause jusqu'à ce que le joueur lance lui-même la vague.
	Game.tutorial_hold = Game.phase == "prep" and Game.wave == 1


func _start_step() -> void:
	_timer = 0.0
	var p := Game.player as Player
	if p:
		_start_pos = p.global_position
		_last_yaw = p.rotation.y
	_turned = 0.0
	_player_hits = 0
	_picked = 0
	hud.set_objective("TUTORIEL  %d / %d      [P] passer" % [_step + 1, _steps.size()], _steps[_step]["text"])


func _finish() -> void:
	Game.tutorial_hold = false
	hud.set_objective("", "")
	var cfg := ConfigFile.new()
	cfg.load(SAVE_PATH)
	cfg.set_value("tutoriel", "termine", true)
	cfg.save(SAVE_PATH)
	queue_free()


# ---------------------------------------------------------------- conditions des étapes

func _moved() -> bool:
	var p := Game.player as Player
	return p != null and p.global_position.distance_to(_start_pos) > 4.0


func _looked() -> bool:
	return _turned > 1.5


func _wait(seconds: float) -> bool:
	return _timer > seconds


func _built(type: String) -> bool:
	for t in get_tree().get_nodes_in_group("towers"):
		if t.type == type:
			return true
	return false


func _toggled() -> bool:
	# Validé quand une tour a été éteinte puis rallumée (ou après un moment pour ne pas bloquer).
	for t in get_tree().get_nodes_in_group("towers"):
		if not t.powered:
			_steps[_step]["seen_off"] = true
	return (_steps[_step].get("seen_off", false) and _all_powered()) or _timer > 25.0


func _all_powered() -> bool:
	for t in get_tree().get_nodes_in_group("towers"):
		if not t.powered:
			return false
	return true


func _assault() -> bool:
	return Game.phase == "assault"


func _shot(hits: int) -> bool:
	return _player_hits >= hits


func _marked() -> bool:
	for z in get_tree().get_nodes_in_group("zombies"):
		if z.marked:
			return true
	return _timer > 30.0


func _collected() -> bool:
	return _picked > 0 or _timer > 40.0
