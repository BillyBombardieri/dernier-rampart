class_name Levels
extends RefCounted
## Les niveaux : chacun a sa carte (chemin, barrières, ancrages, décor), son ambiance et sa
## difficulté. Ils se débloquent l'un après l'autre : gagner un niveau ouvre le suivant.
## Le Cœur est toujours au même endroit (0, 0, 35), avec la même base autour (établi, murets) :
## le dernier point du chemin est donc toujours (0, 0, 30).
##
## Difficulté (multiplie les valeurs de la vague) : PV, dégâts, nombre de zombies, temps entre
## deux apparitions ; "early" = les zombies spéciaux arrivent autant de vagues plus tôt ;
## "bosses" = nombre de boss à la dernière Nuit de siège ; "points" = multiplicateur des insignes.
## La vitesse des zombies ne change jamais d'un niveau à l'autre : ils restent lents.

const LEVELS := [
	{
		"name": "La Brèche",
		"place": "Terrain vague, au crépuscule",
		"desc": "Une brèche dans le vieux mur d'enceinte. Le premier rempart.",
		"path": [
			Vector3(0, 0, -55), Vector3(0, 0, -35), Vector3(-18, 0, -25), Vector3(-18, 0, -2),
			Vector3(12, 0, 6), Vector3(12, 0, 22), Vector3(0, 0, 30),
		],
		"rings": {
			"avant": {"gate": Vector3(0, 0, -43), "segment": 0, "hp": 600.0,
				"sockets": [Vector3(-5.5, 0, -39.5), Vector3(5.5, 0, -39.5), Vector3(6.5, 0, -33), Vector3(-12, 0, -36)]},
			"muraille": {"gate": Vector3(12, 0, 10.5), "segment": 4, "hp": 900.0,
				"sockets": [Vector3(-12, 0, -8), Vector3(-4, 0, 6), Vector3(6, 0, 13), Vector3(18, 0, 13), Vector3(-6, 0, 24)]},
		},
		"env": {
			"sky_top": Color(0.03, 0.04, 0.08), "sky_horizon": Color(0.3, 0.2, 0.17), "ground_horizon": Color(0.12, 0.08, 0.06),
			"sun_rot": Vector3(-14, -150, 0), "sun_color": Color(1.0, 0.68, 0.48), "sun_energy": 0.9,
			"ambient": 0.45, "exposure": 1.1, "saturation": 0.8,
			"fog_color": Color(0.13, 0.14, 0.17), "fog": 0.008, "vol_albedo": Color(0.6, 0.62, 0.68), "vol_fog": 0.01,
		},
		"ground": {"tex": "ground", "scale": 0.12, "tint": Color.WHITE},
		"path_mat": {"tex": "mud", "scale": 0.2, "tint": Color.WHITE},
		"walls": [[Vector3(-30, 1.5, 10), Vector3(1.2, 3, 14)], [Vector3(28, 1.5, -20), Vector3(1.2, 3, 18)]],
		"sandbags": [Vector3(-26, 0, -6), Vector3(22, 0, 18)],
		"spots": [
			[Vector3(-12, 0, 28), Vector3(-25, 0, 0)], [Vector3(14, 0, 30), Vector3(12, 0, 8)],
			[Vector3(-28, 0, -20), Vector3(-10, 0, -32)], [Vector3(10, 0, -30), Vector3(0, 0, -50)],
		],
		"spot_color": Color(1.0, 0.92, 0.78),
		"barrels": [Vector3(-4, 0, 31), Vector3(16, 0, 2), Vector3(-22, 0, -26), Vector3(9.5, 0, -44.5)],
		"props": {"count": 70, "rock": 5, "tree": 3, "wreck": 2},
		"weather": "",
		"difficulty": {"hp": 1.0, "damage": 1.0, "count": 1.0, "spawn": 1.0, "early": 0, "bosses": 1, "points": 1.0},
	},
	{
		"name": "Le Marais",
		"place": "Marécage noyé de brume, la nuit",
		"desc": "Une passerelle de boue entre les eaux mortes. La brume cache la horde.",
		"path": [
			Vector3(-40, 0, -55), Vector3(-40, 0, -30), Vector3(-15, 0, -30), Vector3(-15, 0, -10),
			Vector3(-30, 0, 5), Vector3(-25, 0, 18), Vector3(0, 0, 18), Vector3(0, 0, 30),
		],
		"rings": {
			"avant": {"gate": Vector3(-31, 0, -30), "segment": 1, "hp": 600.0,
				"sockets": [Vector3(-28, 0, -35.5), Vector3(-27, 0, -24.5), Vector3(-35.5, 0, -24), Vector3(-20, 0, -36)]},
			"muraille": {"gate": Vector3(-12, 0, 18), "segment": 5, "hp": 900.0,
				"sockets": [Vector3(-17, 0, 23.5), Vector3(-7, 0, 23.5), Vector3(-6, 0, 12.5), Vector3(-19, 0, 12.5), Vector3(5.5, 0, 22)]},
		},
		"env": {
			"sky_top": Color(0.015, 0.03, 0.03), "sky_horizon": Color(0.1, 0.14, 0.11), "ground_horizon": Color(0.05, 0.07, 0.05),
			"sun_rot": Vector3(-28, 40, 0), "sun_color": Color(0.6, 0.78, 0.72), "sun_energy": 0.5,
			"ambient": 0.7, "exposure": 1.2, "saturation": 0.72,
			"fog_color": Color(0.1, 0.14, 0.12), "fog": 0.013, "vol_albedo": Color(0.55, 0.66, 0.56), "vol_fog": 0.022,
		},
		"ground": {"tex": "mud", "scale": 0.1, "tint": Color(0.55, 0.62, 0.45)},
		"path_mat": {"tex": "mud", "scale": 0.2, "tint": Color(0.62, 0.55, 0.42)},
		"walls": [[Vector3(-47, 1.5, -18), Vector3(1.2, 3, 16)]],
		"sandbags": [Vector3(-36, 0, 12), Vector3(-9, 0, -20)],
		"spots": [
			[Vector3(-12, 0, 28), Vector3(-22, 0, 16)], [Vector3(10, 0, 24), Vector3(0, 0, 14)],
			[Vector3(-24, 0, -18), Vector3(-30, 0, -30)], [Vector3(-34, 0, 0), Vector3(-25, 0, 12)],
		],
		"spot_color": Color(0.85, 0.95, 0.9),
		"barrels": [Vector3(-4, 0, 31), Vector3(-22, 0, -22), Vector3(-34, 0, -38), Vector3(-8, 0, 8)],
		"props": {"count": 95, "tree": 4, "pool": 3, "reeds": 4, "rock": 1},
		"weather": "fireflies",
		"difficulty": {"hp": 1.15, "damage": 1.1, "count": 1.15, "spawn": 0.95, "early": 1, "bosses": 1, "points": 1.5},
	},
	{
		"name": "La Raffinerie",
		"place": "Zone industrielle sous un ciel de fumée",
		"desc": "Conteneurs, cuves et projecteurs au sodium. Plus de zombies, plus durs.",
		"path": [
			Vector3(30, 0, -55), Vector3(30, 0, -38), Vector3(5, 0, -38), Vector3(5, 0, -20),
			Vector3(28, 0, -12), Vector3(28, 0, 6), Vector3(8, 0, 10), Vector3(0, 0, 20), Vector3(0, 0, 30),
		],
		"rings": {
			"avant": {"gate": Vector3(21, 0, -38), "segment": 1, "hp": 600.0,
				"sockets": [Vector3(24, 0, -44), Vector3(18, 0, -32.5), Vector3(12, 0, -43.5), Vector3(35.5, 0, -33)]},
			"muraille": {"gate": Vector3(19, 0, 7.8), "segment": 5, "hp": 900.0,
				"sockets": [Vector3(16, 0, 14), Vector3(22, 0, 0.5), Vector3(12, 0, 3), Vector3(30, 0, 12.5), Vector3(-5.5, 0, 18)]},
		},
		"env": {
			"sky_top": Color(0.05, 0.035, 0.03), "sky_horizon": Color(0.42, 0.21, 0.09), "ground_horizon": Color(0.14, 0.08, 0.05),
			"sun_rot": Vector3(-9, 115, 0), "sun_color": Color(1.0, 0.52, 0.28), "sun_energy": 0.6,
			"ambient": 0.45, "exposure": 1.1, "saturation": 0.85,
			"fog_color": Color(0.2, 0.13, 0.08), "fog": 0.01, "vol_albedo": Color(0.7, 0.56, 0.42), "vol_fog": 0.016,
		},
		"ground": {"tex": "concrete", "scale": 0.1, "tint": Color(0.58, 0.55, 0.52)},
		"path_mat": {"tex": "mud", "scale": 0.2, "tint": Color(0.5, 0.47, 0.45)},
		"walls": [[Vector3(-12, 1.5, -8), Vector3(1.2, 3, 18)], [Vector3(42, 1.5, -22), Vector3(1.2, 3, 22)]],
		"sandbags": [Vector3(15, 0, -26), Vector3(34, 0, -4)],
		"spots": [
			[Vector3(-10, 0, 26), Vector3(4, 0, 14)], [Vector3(14, 0, 22), Vector3(22, 0, 6)],
			[Vector3(-3, 0, -28), Vector3(6, 0, -36)], [Vector3(38, 0, -46), Vector3(26, 0, -38)],
			[Vector3(36, 0, 0), Vector3(28, 0, -10)], [Vector3(31, 0, -26), Vector3(20, 0, -36)],
		],
		"spot_color": Color(1.0, 0.62, 0.3),
		"barrels": [Vector3(-4, 0, 31), Vector3(34, 0, -20), Vector3(0, 0, -30), Vector3(36, 0, -50), Vector3(20, 0, 16)],
		"props": {"count": 55, "container": 5, "tank": 2, "wreck": 2, "rock": 1},
		"weather": "ash",
		"difficulty": {"hp": 1.3, "damage": 1.2, "count": 1.3, "spawn": 0.9, "early": 1, "bosses": 2, "points": 2.0},
	},
	{
		"name": "Le Col Gelé",
		"place": "Village de montagne sous la neige",
		"desc": "Blizzard, ruines et sapins. Le dernier rempart avant la vallée.",
		"path": [
			Vector3(-10, 0, -50), Vector3(-10, 0, -36), Vector3(18, 0, -36), Vector3(18, 0, -20),
			Vector3(-16, 0, -14), Vector3(-16, 0, 2), Vector3(10, 0, 8), Vector3(10, 0, 20), Vector3(0, 0, 30),
		],
		"rings": {
			"avant": {"gate": Vector3(5, 0, -36), "segment": 1, "hp": 600.0,
				"sockets": [Vector3(1, 0, -41.5), Vector3(9, 0, -30.5), Vector3(-5, 0, -30.5), Vector3(14, 0, -41.5)]},
			"muraille": {"gate": Vector3(10, 0, 13), "segment": 6, "hp": 900.0,
				"sockets": [Vector3(4.5, 0, 15), Vector3(15.5, 0, 11), Vector3(16, 0, 22), Vector3(3, 0, 0), Vector3(-5.5, 0, 21)]},
		},
		"env": {
			"sky_top": Color(0.01, 0.02, 0.05), "sky_horizon": Color(0.13, 0.16, 0.23), "ground_horizon": Color(0.2, 0.22, 0.27),
			"sun_rot": Vector3(-34, -60, 0), "sun_color": Color(0.66, 0.76, 1.0), "sun_energy": 0.55,
			"ambient": 0.55, "exposure": 1.0, "saturation": 0.7,
			"fog_color": Color(0.17, 0.19, 0.25), "fog": 0.012, "vol_albedo": Color(0.85, 0.88, 0.95), "vol_fog": 0.02,
		},
		"ground": {"tex": "ground", "scale": 0.12, "tint": Color(0.78, 0.81, 0.87), "plain": true},
		"path_mat": {"tex": "mud", "scale": 0.2, "tint": Color(0.6, 0.6, 0.64)},
		"walls": [[Vector3(-30, 1.5, -26), Vector3(1.2, 3, 16)], [Vector3(30, 1.5, -6), Vector3(1.2, 3, 16)]],
		"sandbags": [Vector3(-24, 0, 8), Vector3(24, 0, -30)],
		"spots": [
			[Vector3(-12, 0, 28), Vector3(-4, 0, 16)], [Vector3(20, 0, 26), Vector3(10, 0, 12)],
			[Vector3(-24, 0, -6), Vector3(-16, 0, -12)], [Vector3(26, 0, -44), Vector3(10, 0, -36)],
		],
		"spot_color": Color(0.9, 0.95, 1.0),
		"barrels": [Vector3(-4, 0, 31), Vector3(-22, 0, -20), Vector3(24, 0, -14), Vector3(-4, 0, -46)],
		"props": {"count": 85, "pine": 6, "ruin": 2, "rock": 2, "wreck": 1},
		"weather": "snow",
		"difficulty": {"hp": 1.5, "damage": 1.3, "count": 1.45, "spawn": 0.85, "early": 2, "bosses": 2, "points": 3.0},
	},
]


static func count() -> int:
	return LEVELS.size()


static func get_level(index: int) -> Dictionary:
	return LEVELS[clampi(index, 0, LEVELS.size() - 1)]


static func difficulty(index: int) -> Dictionary:
	return get_level(index)["difficulty"]
