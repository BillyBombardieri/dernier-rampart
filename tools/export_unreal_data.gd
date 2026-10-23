extends Node
## Exporte les données du jeu en JSON pour la version Unreal Engine 5 (dossier unreal/), pour que
## le portage garde exactement les mêmes chiffres : unreal/DernierRempart/Content/Data/jeu.json
## (joueur, armes, zombies, tours, vagues, niveaux, améliorations...) et chapitres.json (cartes).
## Les positions des cartes sont converties dans le repère d'Unreal : centimètres, X vers l'avant
## (-Z de Godot), Y à droite (X de Godot), Z vers le haut (Y de Godot). Les autres valeurs gardent
## les unités de Godot (mètres, m/s, secondes, radians pour le recul).
## Lancer après chaque changement de chiffres : godot --headless --path . res://tools/export_unreal_data.tscn

const OUT_DIR := "res://unreal/DernierRempart/Content/Data"
const PlayerScript := preload("res://scripts/player.gd")
const ZombieScript := preload("res://scripts/zombie.gd")
const TowerScript := preload("res://scripts/tower.gd")
const GameScript := preload("res://scripts/game.gd")
const WaveScript := preload("res://scripts/wave_manager.gd")
const BarrierScript := preload("res://scripts/barrier.gd")

var _errors := 0


func _ready() -> void:
	var dir := ProjectSettings.globalize_path(OUT_DIR)
	DirAccess.make_dir_recursive_absolute(dir)
	_write(dir.path_join("jeu.json"), _game_data())
	_write(dir.path_join("chapitres.json"), _chapter_data())
	print("Export Unreal terminé : ", _errors, " erreur(s)")
	get_tree().quit(1 if _errors > 0 else 0)


func _write(path: String, data: Dictionary) -> void:
	var f := FileAccess.open(path, FileAccess.WRITE)
	if f == null:
		print("ERREUR : impossible d'écrire ", path)
		_errors += 1
		return
	f.store_string(JSON.stringify(data, "\t", false) + "\n")
	print("Écrit : ", path)


func _game_data() -> Dictionary:
	var levels := []
	for i in Levels.count():
		levels.append({"numero": i, "chapitre": Levels.chapter_index(i), "titre": Levels.title(i),
			"court": Levels.short(i), "difficulte": Levels.difficulty(i)})
	return _plain({
		"_source": "Exporté de la version Godot par tools/export_unreal_data.tscn. Ne pas modifier à la main.",
		"_unites": "Comme dans Godot : distances en mètres, vitesses en m/s, temps en secondes, recul en radians, couleurs RVBA sRGB de 0 à 1.",
		"joueur": {
			"walk_speed": PlayerScript.WALK_SPEED, "sprint_speed": PlayerScript.SPRINT_SPEED,
			"aim_speed": PlayerScript.AIM_SPEED, "jump_velocity": PlayerScript.JUMP_VELOCITY,
			"gravity": PlayerScript.GRAVITY, "mouse_sens": PlayerScript.MOUSE_SENS, "fov": PlayerScript.FOV,
			"aim_fov": PlayerScript.AIM_FOV, "repair_rate": PlayerScript.REPAIR_RATE,
			"repair_cost": PlayerScript.REPAIR_COST,
		},
		"armes": PlayerScript.WEAPONS,
		"zombies": ZombieScript.TYPES,
		"zombies_regles": {
			"gravity": ZombieScript.GRAVITY, "headshot_mult": ZombieScript.HEADSHOT_MULT,
			"spit_range": ZombieScript.SPIT_RANGE, "scream_radius": ZombieScript.SCREAM_RADIUS,
			"decoy_radius": ZombieScript.DECOY_RADIUS, "dig_speed": ZombieScript.DIG_SPEED,
			"dig_distance": ZombieScript.DIG_DISTANCE, "dig_cooldown": ZombieScript.DIG_COOLDOWN,
			"buff_speed": ZombieScript.BUFF_SPEED, "buff_damage": ZombieScript.BUFF_DAMAGE,
			"attack_time": ZombieScript.ATTACK_TIME, "spit_time": ZombieScript.SPIT_TIME,
			"scream_time": ZombieScript.SCREAM_TIME, "strike_margin": ZombieScript.STRIKE_MARGIN,
		},
		"tours": TowerScript.STATS,
		"tours_regles": {
			"build_order": TowerScript.BUILD_ORDER, "max_level": TowerScript.MAX_LEVEL,
			"mark_bonus": TowerScript.MARK_BONUS, "beacon_reveal": TowerScript.BEACON_REVEAL,
			"model_scale": TowerScript.MODEL_SCALE,
		},
		"barrieres": {"rebuild_ratio": BarrierScript.REBUILD_RATIO},
		"partie": {
			"energy_base": GameScript.ENERGY_BASE, "start_scrap": GameScript.START_SCRAP,
			"last_wave": GameScript.LAST_WAVE, "wave_points": GameScript.WAVE_POINTS,
			"victory_points": GameScript.VICTORY_POINTS, "wave_scrap_base": GameScript.WAVE_SCRAP_BASE,
			"wave_scrap_per_wave": GameScript.WAVE_SCRAP_PER_WAVE,
		},
		"vagues": {
			"prep_time": WaveScript.PREP_TIME, "first_prep_time": WaveScript.FIRST_PREP_TIME,
			"harvest_time": WaveScript.HARVEST_TIME, "hp_per_wave": WaveScript.HP_PER_WAVE,
			"speed_per_wave": WaveScript.SPEED_PER_WAVE, "damage_per_wave": WaveScript.DAMAGE_PER_WAVE,
			"siege_every": WaveScript.SIEGE_EVERY, "intro": WaveScript.INTRO,
		},
		"implants": GameScript.IMPLANTS,
		"etabli": {"canon": GameScript.BARREL_COSTS, "chargeur": GameScript.MAG_COSTS,
			"generateur": GameScript.GENERATOR_COSTS},
		"munitions": GameScript.AMMO,
		"gadgets": GameScript.GADGETS,
		"ameliorations": Upgrades.LIST,
		"insignes_victoire": Upgrades.VICTORY_INSIGNES,
		"niveaux": levels,
	})


func _chapter_data() -> Dictionary:
	var chapters := []
	for c: Dictionary in Levels.CHAPTERS:
		var out := {}
		for key: String in c:
			match key:
				"walls":
					# [centre, taille] -> boîte dans le repère d'Unreal.
					var walls := []
					for w: Array in c[key]:
						walls.append({"centre": ue_pos(w[0]), "taille": ue_size(w[1])})
					out[key] = walls
				"spots":
					var spots := []
					for s: Array in c[key]:
						spots.append({"position": ue_pos(s[0]), "cible": ue_pos(s[1])})
					out[key] = spots
				"env":
					out[key] = _env(c[key])
				_:
					out[key] = _map_value(c[key])
		chapters.append(out)
	return {
		"_source": "Exporté de la version Godot par tools/export_unreal_data.tscn. Ne pas modifier à la main.",
		"_unites": "Positions et tailles en centimètres dans le repère d'Unreal (X avant = -Z de Godot, Y droite = X de Godot, Z haut = Y de Godot). Couleurs RVBA sRGB de 0 à 1. Le Cœur est en (-3500, 0, 0).",
		"par_chapitre": Levels.PER_CHAPTER,
		"chapitres": chapters,
	}


## Ambiance d'un chapitre : l'angle du soleil devient une direction et un tangage/lacet Unreal.
func _env(env: Dictionary) -> Dictionary:
	var rest := env.duplicate()
	rest.erase("sun_rot")
	var out: Dictionary = _plain(rest)
	var rot: Vector3 = env["sun_rot"]
	# Même ordre d'angles que rotation_degrees dans Godot (YXZ) ; la lumière part vers -Z.
	var d := -Basis.from_euler(rot * PI / 180.0).z
	var ue := Vector3(-d.z, d.x, d.y)
	out["sun_rot_godot"] = [rot.x, rot.y, rot.z]
	out["sun_dir"] = [_snap(ue.x, 0.0001), _snap(ue.y, 0.0001), _snap(ue.z, 0.0001)]
	out["sun_pitch"] = _snap(rad_to_deg(asin(ue.z)), 0.01)
	out["sun_yaw"] = _snap(rad_to_deg(atan2(ue.y, ue.x)), 0.01)
	return out


## Position Godot (mètres) -> Unreal (centimètres, X avant, Y droite, Z haut).
static func ue_pos(v: Vector3) -> Array:
	return [_snap(-v.z * 100.0, 0.01), _snap(v.x * 100.0, 0.01), _snap(v.y * 100.0, 0.01)]


## Taille d'une boîte : mêmes axes que ue_pos, sans changement de signe.
static func ue_size(v: Vector3) -> Array:
	return [_snap(v.z * 100.0, 0.01), _snap(v.x * 100.0, 0.01), _snap(v.y * 100.0, 0.01)]


## Arrondi sans « -0 » dans le JSON.
static func _snap(x: float, step: float) -> float:
	return snappedf(x, step) + 0.0


## Valeur d'une carte : les Vector3 sont des positions à convertir.
func _map_value(value: Variant) -> Variant:
	if value is Vector3:
		return ue_pos(value)
	if value is Array:
		var out := []
		for v: Variant in value:
			out.append(_map_value(v))
		return out
	if value is Dictionary:
		var out := {}
		for k: Variant in value:
			out[k] = _map_value(value[k])
		return out
	return _plain(value)


## Rend une valeur compatible JSON. Un Vector3 n'a rien à faire hors des cartes : on le signale.
func _plain(value: Variant) -> Variant:
	if value is Dictionary:
		var out := {}
		for k: Variant in value:
			out[k] = _plain(value[k])
		return out
	if value is Array:
		var out := []
		for v: Variant in value:
			out.append(_plain(v))
		return out
	if value is Color:
		return [_snap(value.r, 0.0001), _snap(value.g, 0.0001), _snap(value.b, 0.0001), _snap(value.a, 0.0001)]
	if value is Vector2:
		return [_snap(value.x, 0.0001), _snap(value.y, 0.0001)]
	if value == null or value is bool or value is int or value is float or value is String:
		return value
	print("ERREUR : valeur non exportable : ", value)
	_errors += 1
	return null
