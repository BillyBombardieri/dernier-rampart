extends Node
## État global de la partie (autoload "Game") : ressources, implants, références aux nœuds clés.

signal changed
signal message(text: String)
signal ended(victory: bool)

const ENERGY_BASE := 6
const START_SCRAP := 60
const LAST_WAVE := 10

const IMPLANTS := {
	"batterie": {
		"name": "Cœur de batterie",
		"plus": "+2 énergie pour les tours",
		"minus": "Tu cours 15 % moins vite",
	},
	"sang_froid": {
		"name": "Sang froid",
		"plus": "BRISÉ fait x5 dégâts au lieu de x3",
		"minus": "Fusil d'assaut -20 % de dégâts",
	},
	"charognard": {
		"name": "Charognard",
		"plus": "La ferraille vient à toi de loin",
		"minus": "-20 % de points de vie",
	},
	"main_lourde": {
		"name": "Main lourde",
		"plus": "Pistolet +50 % de dégâts",
		"minus": "Rechargements 50 % plus lents",
	},
}

var scrap := START_SCRAP
var energy_used := 0
var implants: Array[String] = []
var wave := 0
var phase := "prep"
var phase_time := 0.0
var is_over := false
var deaths := 0
var hint := ""

var main: Node3D
var player: Node3D
var core: Node3D


func _ready() -> void:
	_setup_inputs()


func reset() -> void:
	scrap = START_SCRAP
	energy_used = 0
	implants.clear()
	wave = 0
	phase = "prep"
	phase_time = 0.0
	is_over = false
	deaths = 0
	hint = ""


func energy_cap() -> int:
	return ENERGY_BASE + (2 if has_implant("batterie") else 0)


func request_energy(amount: int) -> bool:
	if energy_used + amount > energy_cap():
		return false
	energy_used += amount
	changed.emit()
	return true


func release_energy(amount: int) -> void:
	energy_used = max(0, energy_used - amount)
	changed.emit()


func add_scrap(amount: int) -> void:
	scrap += amount
	changed.emit()


func try_spend(amount: int) -> bool:
	if scrap < amount:
		say("Pas assez de ferraille (%d nécessaires)" % amount)
		return false
	scrap -= amount
	changed.emit()
	return true


func has_implant(id: String) -> bool:
	return implants.has(id)


func add_implant(id: String) -> void:
	if not implants.has(id):
		implants.append(id)
	if player and player.has_method("apply_implants"):
		player.apply_implants()
	say("Implant installé : %s" % IMPLANTS[id]["name"])
	changed.emit()


func implant_choices(count: int) -> Array:
	var pool: Array = []
	for id in IMPLANTS.keys():
		if not implants.has(id):
			pool.append(id)
	pool.shuffle()
	return pool.slice(0, count)


func say(text: String) -> void:
	message.emit(text)


func end_game(victory: bool) -> void:
	if is_over:
		return
	is_over = true
	ended.emit(victory)


func _setup_inputs() -> void:
	# Clavier AZERTY : les lettres sont lues d'après ce qui est écrit sur la touche.
	var keys := {
		"move_forward": [KEY_Z],
		"move_back": [KEY_S],
		"move_left": [KEY_Q],
		"move_right": [KEY_D],
		"jump": [KEY_SPACE],
		"sprint": [KEY_SHIFT],
		"reload": [KEY_R],
		"weapon_1": [KEY_KP_1],
		"weapon_2": [KEY_KP_2],
		"mark": [KEY_F],
		"build_gun": [KEY_E],
		"build_cryo": [KEY_C],
		"toggle_power": [KEY_X],
		"skip_phase": [KEY_ENTER, KEY_KP_ENTER],
	}
	for action in keys:
		if not InputMap.has_action(action):
			InputMap.add_action(action)
		for k in keys[action]:
			var ev := InputEventKey.new()
			ev.keycode = k
			InputMap.action_add_event(action, ev)
	# Rangée du haut (& et é en AZERTY) : lue par position pour marcher sans Maj.
	for pair in [["weapon_1", KEY_1], ["weapon_2", KEY_2]]:
		var ev := InputEventKey.new()
		ev.physical_keycode = pair[1]
		InputMap.action_add_event(pair[0], ev)
	if not InputMap.has_action("fire"):
		InputMap.add_action("fire")
		var mb := InputEventMouseButton.new()
		mb.button_index = MOUSE_BUTTON_LEFT
		InputMap.action_add_event("fire", mb)
