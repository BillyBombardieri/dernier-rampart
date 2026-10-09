extends Node
## Réglages du joueur (autoload "Settings") : sensibilité, volume, champ de vision, axe inversé,
## touches personnalisées, tutoriel déjà vu et record. Tout est sauvegardé dans user://reglages.cfg.

const PATH := "user://reglages.cfg"
const TEST_PATH := "user://reglages_test.cfg"

# Actions modifiables, dans l'ordre d'affichage : [action, libellé, type, code].
# Type "k" : touche lue d'après ce qui est écrit dessus (clavier AZERTY).
# Type "p" : touche lue par sa position (rangée des chiffres : & et é).
# Type "m" : bouton de souris.
const BINDABLE := [
	["move_forward", "Avancer", "k", KEY_Z],
	["move_back", "Reculer", "k", KEY_S],
	["move_left", "Aller à gauche", "k", KEY_Q],
	["move_right", "Aller à droite", "k", KEY_D],
	["jump", "Sauter", "k", KEY_SPACE],
	["sprint", "Courir", "k", KEY_SHIFT],
	["fire", "Tirer", "m", MOUSE_BUTTON_LEFT],
	["aim", "Viser", "m", MOUSE_BUTTON_RIGHT],
	["reload", "Recharger", "k", KEY_R],
	["weapon_1", "Pistolet lourd", "p", KEY_1],
	["weapon_2", "Fusil d'assaut", "p", KEY_2],
	["mark", "Marquer un zombie", "k", KEY_F],
	["interact", "Construire, améliorer, réparer, établi", "k", KEY_E],
	["cycle_tower", "Changer de tour à construire", "k", KEY_C],
	["toggle_power", "Allumer / éteindre une tour", "k", KEY_X],
	["gadget_1", "Gadget 1", "k", KEY_A],
	["gadget_2", "Gadget 2", "k", KEY_G],
	["skip_phase", "Lancer la vague", "k", KEY_ENTER],
	["flashlight", "Lampe torche", "k", KEY_L],
	["help", "Aide", "k", KEY_H],
	["skip_tutorial", "Passer le tutoriel", "k", KEY_P],
]
# Touches de secours qui restent toujours actives (pavé numérique).
const EXTRA := {
	"weapon_1": [["k", KEY_KP_1]],
	"weapon_2": [["k", KEY_KP_2]],
	"skip_phase": [["k", KEY_KP_ENTER]],
}
const KEY_NAMES := {
	KEY_SPACE: "Espace", KEY_SHIFT: "Maj", KEY_ENTER: "Entrée", KEY_KP_ENTER: "Entrée",
	KEY_CTRL: "Ctrl", KEY_ALT: "Alt", KEY_TAB: "Tab", KEY_BACKSPACE: "Retour arrière",
	KEY_CAPSLOCK: "Verr. Maj", KEY_UP: "Flèche haut", KEY_DOWN: "Flèche bas",
	KEY_LEFT: "Flèche gauche", KEY_RIGHT: "Flèche droite", KEY_ESCAPE: "Échap",
	KEY_DELETE: "Suppr", KEY_INSERT: "Inser", KEY_HOME: "Début", KEY_END: "Fin",
	KEY_PAGEUP: "Page haut", KEY_PAGEDOWN: "Page bas",
}
const MOUSE_NAMES := {
	MOUSE_BUTTON_LEFT: "Clic gauche", MOUSE_BUTTON_RIGHT: "Clic droit",
	MOUSE_BUTTON_MIDDLE: "Clic molette", MOUSE_BUTTON_XBUTTON1: "Souris 4",
	MOUSE_BUTTON_XBUTTON2: "Souris 5",
}

var sensitivity := 1.0
var volume := 0.8
var fov := 78.0
var invert_y := false
var tutorial_done := false
var best_score := 0
var best_wave := 0
# Progression : niveaux débloqués, insignes à dépenser et améliorations permanentes achetées.
var unlocked_level := 0
var levels_won: Array = []
var insignes := 0
var upgrades := {}  # id -> rang
var bindings := {}  # action -> {"kind": "k" | "p" | "m", "code": int}
var _labels := {}  # Noms des touches déjà calculés (le HUD les demande à chaque image).
var _path := PATH


func _ready() -> void:
	reset_bindings(false)
	load_settings()
	apply()


func load_settings() -> void:
	var cfg := ConfigFile.new()
	if cfg.load(_path) != OK:
		return
	sensitivity = clampf(cfg.get_value("reglages", "sensibilite", sensitivity), 0.1, 4.0)
	volume = clampf(cfg.get_value("reglages", "volume", volume), 0.0, 1.0)
	fov = clampf(cfg.get_value("reglages", "champ_de_vision", fov), 60.0, 110.0)
	invert_y = cfg.get_value("reglages", "inverser_y", invert_y)
	tutorial_done = cfg.get_value("tutoriel", "termine", false)
	best_score = cfg.get_value("record", "score", 0)
	best_wave = cfg.get_value("record", "vague", 0)
	unlocked_level = int(cfg.get_value("progression", "niveau_debloque", 0))
	levels_won = Array(cfg.get_value("progression", "niveaux_gagnes", []))
	insignes = int(cfg.get_value("progression", "insignes", 0))
	upgrades = {}
	var saved_upgrades: Dictionary = cfg.get_value("progression", "ameliorations", {})
	for id in saved_upgrades:
		if Upgrades.LIST.has(id):
			upgrades[id] = clampi(int(saved_upgrades[id]), 0, Upgrades.max_rank(id))
	for entry in BINDABLE:
		var saved: String = cfg.get_value("touches", entry[0], "")
		var b := _parse(saved)
		if not b.is_empty():
			bindings[entry[0]] = b


func save_settings() -> void:
	var cfg := ConfigFile.new()
	cfg.load(_path)  # Garde les autres sections du fichier.
	cfg.set_value("reglages", "sensibilite", sensitivity)
	cfg.set_value("reglages", "volume", volume)
	cfg.set_value("reglages", "champ_de_vision", fov)
	cfg.set_value("reglages", "inverser_y", invert_y)
	cfg.set_value("tutoriel", "termine", tutorial_done)
	cfg.set_value("record", "score", best_score)
	cfg.set_value("record", "vague", best_wave)
	cfg.set_value("progression", "niveau_debloque", unlocked_level)
	cfg.set_value("progression", "niveaux_gagnes", levels_won)
	cfg.set_value("progression", "insignes", insignes)
	cfg.set_value("progression", "ameliorations", upgrades)
	for action in bindings:
		var b: Dictionary = bindings[action]
		cfg.set_value("touches", action, "%s:%d" % [b["kind"], b["code"]])
	cfg.save(_path)


## Pour les tests automatiques : ils sauvegardent dans un autre fichier, sans toucher
## aux réglages ni au record du joueur.
func use_test_file() -> void:
	_path = TEST_PATH
	# Progression vierge pendant les tests : les améliorations du joueur ne faussent rien.
	unlocked_level = 0
	levels_won = []
	insignes = 0
	upgrades = {}


func apply() -> void:
	AudioServer.set_bus_volume_db(0, linear_to_db(maxf(volume, 0.0001)))
	AudioServer.set_bus_mute(0, volume <= 0.001)
	apply_bindings()


func apply_bindings() -> void:
	_labels.clear()
	for entry in BINDABLE:
		var action: String = entry[0]
		if InputMap.has_action(action):
			InputMap.action_erase_events(action)
		else:
			InputMap.add_action(action)
		InputMap.action_add_event(action, make_event(bindings[action]))
		for extra in EXTRA.get(action, []):
			InputMap.action_add_event(action, make_event({"kind": extra[0], "code": extra[1]}))


func reset_bindings(apply_now := true) -> void:
	bindings.clear()
	for entry in BINDABLE:
		bindings[entry[0]] = {"kind": entry[2], "code": entry[3]}
	if apply_now:
		apply_bindings()


## Remplace la touche d'une action. Si une autre action l'utilisait déjà, elle reçoit
## l'ancienne touche (échange), pour ne jamais avoir deux actions sur la même touche.
func rebind(action: String, b: Dictionary) -> void:
	var old: Dictionary = bindings[action]
	for other in bindings:
		if other != action and same_binding(bindings[other], b):
			bindings[other] = old
	bindings[action] = b
	apply_bindings()
	save_settings()


func same_binding(a: Dictionary, b: Dictionary) -> bool:
	if a["kind"] == "m" or b["kind"] == "m":
		return a["kind"] == b["kind"] and a["code"] == b["code"]
	return _key_of(a) == _key_of(b)


## Nom de la touche d'une action, tel qu'il est écrit sur le clavier ("Z", "Espace", "&").
func key_label(action: String) -> String:
	if not bindings.has(action):
		return "?"
	if not _labels.has(action):
		_labels[action] = binding_label(bindings[action])
	return _labels[action]


## Touche mise en forme pour un texte riche (BBCode) : [E] en orange.
func key(action: String) -> String:
	return "[color=#ffb840][b][%s][/b][/color]" % key_label(action)


func binding_label(b: Dictionary) -> String:
	if b["kind"] == "m":
		return MOUSE_NAMES.get(b["code"], "Souris %d" % b["code"])
	return _key_name(_key_of(b))


func submit_score(score: int, wave: int) -> bool:
	var record := score > best_score
	if record:
		best_score = score
	best_wave = maxi(best_wave, wave)
	save_settings()
	return record


## Fin d'une partie sur un niveau : ajoute les insignes, et une victoire débloque le niveau
## suivant. Renvoie true si un nouveau niveau vient d'être débloqué.
func finish_level(level: int, victory: bool, earned: int) -> bool:
	insignes += earned
	var unlocked := false
	if victory:
		if not levels_won.has(level):
			levels_won.append(level)
		if level + 1 < Levels.count() and unlocked_level < level + 1:
			unlocked_level = level + 1
			unlocked = true
	save_settings()
	return unlocked


func set_tutorial_done(done: bool) -> void:
	tutorial_done = done
	save_settings()


static func make_event(b: Dictionary) -> InputEvent:
	if b["kind"] == "m":
		var mb := InputEventMouseButton.new()
		mb.button_index = b["code"]
		return mb
	var ev := InputEventKey.new()
	if b["kind"] == "p":
		ev.physical_keycode = b["code"]
	else:
		ev.keycode = b["code"]
	return ev


func _key_of(b: Dictionary) -> int:
	# Touche lue par position : on demande au système ce qui est écrit dessus (« & » en AZERTY).
	if b["kind"] == "p" and DisplayServer.get_name() != "headless":
		var code := DisplayServer.keyboard_get_keycode_from_physical(b["code"])
		return code if code != KEY_NONE else b["code"]
	return b["code"]


func _key_name(code: int) -> String:
	if KEY_NAMES.has(code):
		return KEY_NAMES[code]
	if code >= KEY_A and code <= KEY_Z:
		return String.chr(code)
	if code >= KEY_KP_0 and code <= KEY_KP_9:
		return "Pavé %d" % (code - KEY_KP_0)
	if code > 32 and code < 0x10000:  # Symbole imprimable : &, é, ", ( ...
		return String.chr(code)
	var s := OS.get_keycode_string(code)
	return s if s != "" else "?"


func _parse(s: String) -> Dictionary:
	var parts := s.split(":")
	if parts.size() != 2 or not parts[0] in ["k", "p", "m"] or not parts[1].is_valid_int():
		return {}
	return {"kind": parts[0], "code": int(parts[1])}
