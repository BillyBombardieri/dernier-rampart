class_name Upgrades
extends RefCounted
## Améliorations permanentes : achetées dans le menu principal avec des insignes, gagnées à chaque
## partie selon la progression. Elles restent d'une partie à l'autre (sauvegardées avec le record).
## Les bonus sont volontairement modestes : le jeu doit rester un défi.

# "per" = bonus par rang, "costs" = prix de chaque rang en insignes.
const LIST := {
	"vitalite": {"name": "Vitalité", "desc": "+6 % de points de vie", "per": 0.06, "costs": [3, 6, 10]},
	"armurier": {"name": "Armurier", "desc": "+5 % de dégâts aux armes", "per": 0.05, "costs": [4, 8, 12]},
	"ingenieur": {"name": "Ingénieur", "desc": "+5 % de dégâts des tours", "per": 0.05, "costs": [4, 8, 12]},
	"maconnerie": {"name": "Maçonnerie", "desc": "+8 % de PV aux barrières", "per": 0.08, "costs": [3, 6, 10]},
	"reserves": {"name": "Réserves", "desc": "+10 ferraille au départ", "per": 10.0, "costs": [2, 5, 8]},
	"recuperation": {"name": "Récupération", "desc": "+10 % de prime de vague", "per": 0.1, "costs": [3, 6, 9]},
	"dynamo": {"name": "Dynamo", "desc": "+1 énergie pour les tours", "per": 1.0, "costs": [6, 12]},
}
# Insignes gagnés : 1 par vague repoussée (x le multiplicateur du niveau, de 1 à 1,95), plus 5
# en cas de victoire.
const VICTORY_INSIGNES := 5


static func rank(id: String) -> int:
	return int(Settings.upgrades.get(id, 0))


static func max_rank(id: String) -> int:
	return (LIST[id]["costs"] as Array).size()


## Bonus total d'une amélioration (rang x bonus par rang).
static func bonus(id: String) -> float:
	return rank(id) * float(LIST[id]["per"])


## Multiplicateur (1 + bonus), pour les pourcentages.
static func mult(id: String) -> float:
	return 1.0 + bonus(id)


## Prix du prochain rang, -1 si l'amélioration est au maximum.
static func next_cost(id: String) -> int:
	var r := rank(id)
	var costs: Array = LIST[id]["costs"]
	return costs[r] if r < costs.size() else -1


static func buy(id: String) -> bool:
	var cost := next_cost(id)
	if cost < 0 or Settings.insignes < cost:
		return false
	Settings.insignes -= cost
	Settings.upgrades[id] = rank(id) + 1
	Settings.save_settings()
	return true


## Insignes dépensés au total (ce que rend le remboursement).
static func spent() -> int:
	var total := 0
	for id in LIST:
		var costs: Array = LIST[id]["costs"]
		for i in rank(id):
			total += int(costs[i])
	return total


static func refund_all() -> void:
	Settings.insignes += spent()
	Settings.upgrades.clear()
	Settings.save_settings()


## Insignes gagnés en fin de partie.
static func earned(level: int, waves_cleared: int, victory: bool) -> int:
	var mult: float = Levels.difficulty(level)["points"]
	var total := int(floor(waves_cleared * mult))
	if victory:
		total += VICTORY_INSIGNES
	return total
