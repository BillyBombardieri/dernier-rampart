class_name Tutorial
extends Node
## Tutoriel guidé de la première partie : une étape à la fois, validée par ce que fait le joueur.
## Il met en pause le compte à rebours de la première préparation. P pour le passer.
## Une fois terminé ou passé, il ne se relance plus (sauvegardé dans les réglages).
## Le bouton TUTORIEL du menu principal permet de le revoir.

var hud: Hud
var _step := 0
var _steps: Array[Dictionary] = []
var _start_pos := Vector3.ZERO
var _turned := 0.0
var _last_yaw := 0.0
var _timer := 0.0
var _player_hits := 0
var _picked := 0


func _ready() -> void:
	var k := func(action: String) -> String: return Settings.key(action)
	_steps = [
		{"text": "Déplace-toi avec %s %s %s %s et cours avec %s." % [k.call("move_forward"), k.call("move_left"), k.call("move_back"), k.call("move_right"), k.call("sprint")], "check": _moved},
		{"text": "Regarde autour de toi avec la [b]souris[/b].", "check": _looked},
		{"text": "Les zombies arrivent du [color=#ff5a3c][b]PORTAIL ROUGE[/b][/color] : repère la colonne de lumière rouge (visible seulement avant la première vague) ou le [b]![/b] sur la minimap, toujours affiché. Ils suivent le chemin de boue jusqu'au [color=#5aa5ff][b]Cœur[/b][/color], qu'il faut protéger.", "check": _wait.bind(7.0)},
		{"text": "Le chemin est coupé par la [b]barrière de l'Avant-poste[/b] (grillage, barbelés et blocs de béton). Les zombies doivent la casser pour passer, pendant que tes tours tirent. Si elle tombe, maintiens %s dessus pour la relever. Une deuxième barrière protège la Muraille." % k.call("interact"), "check": _wait.bind(9.0)},
		{"text": "Approche d'un [b]ancrage[/b] (dalle de béton avec des repères orange) près de la barrière. La tour choisie s'affiche en bas : appuie sur %s pour poser une [b]Mitrailleuse[/b]." % k.call("interact"), "check": _built.bind("gun")},
		{"text": "Sur un autre ancrage, choisis la [color=#7fd8ff][b]Cryo[/b][/color] avec la [b]molette[/b] ou %s, puis %s. Elle gèle les zombies. Il existe 6 tours : Mitrailleuse, Cryo, Lance-flammes, Arc électrique, Mortier et Phare." % [k.call("cycle_tower"), k.call("interact")], "check": _built.bind("cryo")},
		{"text": "Chaque tour allumée consomme de l'[b]énergie[/b] (en haut à gauche). Vise une tour et appuie sur %s pour l'éteindre et récupérer son énergie, puis rallume-la." % k.call("toggle_power"), "check": _toggled},
		{"text": "Près du Cœur, l'[color=#8cffa0][b]ÉTABLI[/b][/color] améliore tes armes (canon, chargeur, munitions spéciales), choisit tes gadgets et renforce le générateur. Il s'utilise entre les vagues avec %s." % k.call("interact"), "check": _bench},
		{"text": "Prêt ? Appuie sur %s pour lancer la première vague." % k.call("skip_phase"), "check": _assault},
		{"text": "Vise avec %s et tire avec %s. Un tir à la tête fait x2 dégâts." % [k.call("aim"), k.call("fire")], "check": _shot.bind(3)},
		{"text": "Vise un zombie et appuie sur %s pour le [b]marquer[/b] : tes tours le ciblent en priorité (+25 %% de dégâts)." % k.call("mark"), "check": _marked},
		{"text": "Gadgets : %s pose une [b]barricade[/b] en travers du chemin, %s lance un [b]leurre sonore[/b] qui attire les zombies. Leur recharge s'affiche en bas à gauche." % [k.call("gadget_1"), k.call("gadget_2")], "check": _gadget},
		{"text": "Combos : un tir de [b]pistolet lourd[/b] (%s) sur un zombie gelé le [color=#7fd8ff][b]BRISE[/b][/color] (x3). Un zombie chargé par l'Arc électrique touché par une balle déclenche une [color=#b8a8ff][b]SURCHARGE[/b][/color]. Un obus de Mortier sur un zombie en feu provoque un [color=#ffa060][b]EMBRASEMENT[/b][/color]." % k.call("weapon_1"), "check": _wait.bind(12.0)},
		{"text": "Les zombies laissent de la [color=#ffb840][b]ferraille[/b][/color]. Va la ramasser : elle sert à construire, réparer et améliorer.", "check": _collected},
		{"text": "Tutoriel terminé ! Si un [b]relais[/b] est détruit, les tours de son anneau s'éteignent : maintiens %s dessus pour le réparer. %s affiche l'aide, Échap met en pause. Bonne chance !" % [k.call("interact"), k.call("help")], "check": _wait.bind(9.0)},
	]
	Game.tutorial_hold = true
	Game.hit_marker.connect(func(_kill: bool): _player_hits += 1)
	Game.scrap_picked.connect(func(amount: int): _picked += amount)
	_start_step()


func _process(delta: float) -> void:
	if Input.is_action_just_pressed("skip_tutorial"):
		Game.say("Tutoriel passé. %s pour afficher l'aide." % Settings.key_label("help"))
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
	hud.set_objective("TUTORIEL  %d / %d      [%s] passer" % [_step + 1, _steps.size(), Settings.key_label("skip_tutorial")], _steps[_step]["text"])


func _finish() -> void:
	Game.tutorial_hold = false
	hud.set_objective("", "")
	Settings.set_tutorial_done(true)
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


func _bench() -> bool:
	return Game.main.bench_panel.opened > 0 or _timer > 14.0


func _gadget() -> bool:
	for id in Game.gadget_cd:
		if Game.gadget_cd[id] > 0.0:
			return true
	return _timer > 20.0
