extends Node
## État global de la partie (autoload "Game") : ressources, score, implants, établi, gadgets,
## références aux nœuds clés. Les touches et réglages sont dans l'autoload "Settings".

signal changed
signal message(text: String)
signal ended(victory: bool)
signal hit_marker(kill: bool)
signal player_hurt(amount: float)
signal scrap_picked(amount: int)

const ENERGY_BASE := 6
const START_SCRAP := 55
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

# Établi : améliorations achetées avec de la ferraille.
const BARREL_COSTS := [40, 80]  # Canon : +20 % de dégâts par niveau.
const MAG_COSTS := [30, 60]  # Chargeur : +30 % de balles par niveau.
const GENERATOR_COSTS := [50, 90, 140]  # Générateur : +2 énergie par niveau.
const AMMO := {
	"standard": {"name": "Standard", "desc": "Aucun effet spécial.", "cost": 0},
	"incendiaire": {"name": "Incendiaires", "desc": "Mettent le feu à la cible (-10 % de dégâts).", "cost": 60},
	"perforante": {"name": "Perforantes", "desc": "Traversent jusqu'à 3 zombies, +25 % sur Brutes et Boss.", "cost": 60},
	"electrique": {"name": "Électriques", "desc": "Chargent la cible : un tir normal ensuite déclenche la SURCHARGE.", "cost": 60},
}
const GADGETS := {
	"barricade": {"name": "Barricade", "desc": "Barre le chemin 20 s (250 PV). À poser sur le chemin.", "cooldown": 35.0},
	"leurre": {"name": "Leurre sonore", "desc": "Attire les zombies proches pendant 8 s.", "cooldown": 30.0},
	"drone": {"name": "Drone de récolte", "desc": "Ramasse la ferraille autour de toi pendant 25 s.", "cooldown": 45.0},
}
# Points gagnés quand une vague est repoussée (x numéro de vague) et en cas de victoire.
const WAVE_POINTS := 50
const VICTORY_POINTS := 1000
# Prime de ferraille quand une vague est repoussée : de quoi financer tours, établi et réparations.
const WAVE_SCRAP_BASE := 10
const WAVE_SCRAP_PER_WAVE := 5

var scrap := START_SCRAP
var energy_used := 0
var implants: Array[String] = []
var wave := 0
var phase := "prep"
var phase_time := 0.0
var is_over := false
var deaths := 0
var hint := ""
var portal_reveal := 1.0  # 1 = colonne et flèche du portail visibles (début de partie), 0 = discret.
var tutorial_hold := false  # Le tutoriel bloque le compte à rebours de la première préparation.
var level := 0  # Niveau joué (numéro global, voir Levels), choisi dans le menu. Garde sa valeur d'une partie à l'autre.
var score := 0
var kills := 0
var new_record := false
var earned_insignes := 0  # Insignes gagnés à la fin de la partie.
var unlocked_next := false  # Cette victoire vient de débloquer le niveau suivant.
var generator := 0
var mods := {}  # arme -> {"barrel": int, "mag": int, "ammo": String, "owned": Array}
var gadget_slots: Array[String] = ["barricade", "leurre"]
var gadget_cd := {}  # gadget -> secondes avant de pouvoir le réutiliser

var main: Node3D
var player: Node3D
var core: Node3D


func reset() -> void:
	scrap = START_SCRAP + int(Upgrades.bonus("reserves"))
	energy_used = 0
	implants.clear()
	wave = 0
	phase = "prep"
	phase_time = 0.0
	is_over = false
	deaths = 0
	hint = ""
	tutorial_hold = false
	portal_reveal = 1.0
	score = 0
	kills = 0
	new_record = false
	earned_insignes = 0
	unlocked_next = false
	generator = 0
	mods = {
		"pistol": {"barrel": 0, "mag": 0, "ammo": "standard", "owned": ["standard"]},
		"rifle": {"barrel": 0, "mag": 0, "ammo": "standard", "owned": ["standard"]},
	}
	gadget_slots = ["barricade", "leurre"]
	gadget_cd = {}


func _process(delta: float) -> void:
	for id in gadget_cd.keys():
		gadget_cd[id] = maxf(0.0, gadget_cd[id] - delta)


func energy_cap() -> int:
	return ENERGY_BASE + 2 * generator + (2 if has_implant("batterie") else 0) + int(Upgrades.bonus("dynamo"))


func request_energy(amount: int) -> bool:
	if energy_used + amount > energy_cap():
		return false
	energy_used += amount
	changed.emit()
	return true


func release_energy(amount: int) -> void:
	energy_used = max(0, energy_used - amount)
	changed.emit()


func wave_scrap(w: int) -> int:
	return int(round((WAVE_SCRAP_BASE + WAVE_SCRAP_PER_WAVE * w) * Upgrades.mult("recuperation")))


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


func add_score(points: int) -> void:
	if is_over:
		return
	score += points
	changed.emit()


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


# ---------------------------------------------------------------- établi

func barrel_cost(weapon: String) -> int:
	var lvl: int = mods[weapon]["barrel"]
	return BARREL_COSTS[lvl] if lvl < BARREL_COSTS.size() else -1


func mag_cost(weapon: String) -> int:
	var lvl: int = mods[weapon]["mag"]
	return MAG_COSTS[lvl] if lvl < MAG_COSTS.size() else -1


func generator_cost() -> int:
	return GENERATOR_COSTS[generator] if generator < GENERATOR_COSTS.size() else -1


func buy_barrel(weapon: String) -> bool:
	var cost := barrel_cost(weapon)
	if cost < 0 or not try_spend(cost):
		return false
	mods[weapon]["barrel"] += 1
	changed.emit()
	return true


func buy_mag(weapon: String) -> bool:
	var cost := mag_cost(weapon)
	if cost < 0 or not try_spend(cost):
		return false
	mods[weapon]["mag"] += 1
	if player and player.has_method("refill"):
		player.refill(weapon)
	changed.emit()
	return true


## Achète (si besoin) puis équipe un type de munitions sur une arme.
func choose_ammo(weapon: String, ammo: String) -> bool:
	var owned: Array = mods[weapon]["owned"]
	if not owned.has(ammo):
		if not try_spend(AMMO[ammo]["cost"]):
			return false
		owned.append(ammo)
	mods[weapon]["ammo"] = ammo
	changed.emit()
	return true


func buy_generator() -> bool:
	var cost := generator_cost()
	if cost < 0 or not try_spend(cost):
		return false
	generator += 1
	say("Générateur amélioré : %d énergie disponible." % energy_cap())
	changed.emit()
	return true


func set_gadget(slot: int, id: String) -> void:
	var other := 1 - slot
	if gadget_slots[other] == id:
		gadget_slots[other] = gadget_slots[slot]
	gadget_slots[slot] = id
	changed.emit()


# ---------------------------------------------------------------- messages et fin

func say(text: String) -> void:
	message.emit(text)


func end_game(victory: bool) -> void:
	if is_over:
		return
	if victory:
		score += VICTORY_POINTS + int(core.hp) if core else VICTORY_POINTS
	is_over = true
	new_record = Settings.submit_score(score, wave)
	var cleared := LAST_WAVE if victory else wave - 1
	earned_insignes = Upgrades.earned(level, cleared, victory)
	unlocked_next = Settings.finish_level(level, victory, earned_insignes)
	ended.emit(victory)
