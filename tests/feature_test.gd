extends Node
## Test des fonctionnalités, une par une : barrières (et leur réparation en maintenant E), combos
## (SURCHARGE, EMBRASEMENT), zombies, tours, Phare, santé entre les vagues, établi, gadgets,
## réglages, score et menus.
## Lancer : godot --headless --fixed-fps 60 --path . res://tests/feature_test.tscn

var _main: Node3D
var _failed := 0
var _passed := 0


func _ready() -> void:
	Settings.use_test_file()
	_main = load("res://scenes/main.tscn").instantiate()
	add_child(_main)
	for c in _main.get_children():
		if c is Tutorial:
			c.free()
	# La préparation reste en pause : aucune vague ne part pendant le test.
	Game.tutorial_hold = true
	Game.scrap = 2000
	await _frames(2)
	await _test_health_bar()
	await _test_barrier()
	await _test_repair_hold()
	await _test_surcharge()
	await _test_embrasement()
	await _test_hurleur()
	await _test_cracheur()
	await _test_towers_ignored()
	await _test_fouisseur()
	await _test_heal_between_waves()
	await _test_workbench()
	await _test_gadgets()
	await _test_settings()
	await _test_score_and_menus()
	await _test_upgrades()
	await _test_levels()
	print("RÉSULTAT : %d réussis, %d échoués" % [_passed, _failed])
	get_tree().quit(1 if _failed > 0 else 0)


func _check(ok: bool, text: String) -> void:
	if ok:
		_passed += 1
		print("  OK      ", text)
	else:
		_failed += 1
		print("  ÉCHEC   ", text)


func _frames(n: int) -> void:
	for i in n:
		await get_tree().physics_frame


func _seconds(s: float) -> void:
	await _frames(int(s * 60.0))


func _spawn(type: String, pos: Vector3) -> Zombie:
	var z := Zombie.new()
	_main.add_child(z)
	z.global_position = pos
	z.setup(type, 1.0)
	z._snap_to_path()  # Reprend le chemin depuis le tronçon le plus proche, comme en partie.
	return z


func _clear_zombies() -> void:
	for z in get_tree().get_nodes_in_group("zombies"):
		z.burrowed = false
		z.take_damage(1e9)
	for p in get_tree().get_nodes_in_group("scrap"):
		p.queue_free()
	await _frames(2)


func _gate(ring: String) -> Barrier:
	for b in get_tree().get_nodes_in_group("gates"):
		if b.ring == ring:
			return b
	return null


## Place le joueur, oriente la vue, puis maintient la touche d'interaction (E) un moment.
func _hold_interact(pos: Vector3, yaw: float, pitch: float, seconds: float) -> void:
	var p := Game.player as Player
	p.global_position = pos
	p.velocity = Vector3.ZERO
	p.rotation.y = yaw
	p._aim_pitch = pitch
	await _frames(3)
	Input.action_press("interact")
	await _seconds(seconds)
	Input.action_release("interact")
	await _frames(2)


func _socket_near(pos: Vector3) -> Socket:
	var best: Socket = null
	for c in _main.get_children():
		if c is Socket and (best == null or c.global_position.distance_to(pos) < best.global_position.distance_to(pos)):
			best = c
	return best


func _test_health_bar() -> void:
	print("Barre de vie des zombies")
	var z := _spawn("rodeur", Vector3(-45, 0.2, 40))
	await _frames(2)
	_check(not z._bar.visible, "pas de barre tant que le zombie n'est pas blessé")
	z.take_damage(z.max_hp * 0.5)
	await _seconds(0.5)
	_check(z._bar.visible and absf(z._bar.fill - 0.5) < 0.01, "blessé de moitié : barre visible à moitié pleine")
	await _seconds(HealthBar.SHOW_TIME + HealthBar.FADE_TIME + 0.5)
	_check(not z._bar.visible, "la barre s'efface quand il n'est plus touché")
	z.take_damage(1e9)
	await _frames(2)
	_check(not z._bar.visible, "pas de barre sur un zombie mort")
	await _clear_zombies()


func _test_barrier() -> void:
	print("Barrière de l'Avant-poste")
	var gate := _gate("avant")
	_check(gate != null and gate.alive, "la barrière existe sur le chemin")
	var z := _spawn("rodeur", Vector3(0, 0.2, -52))
	await _seconds(9.0)
	_check(z.global_position.z < gate.global_position.z, "le rôdeur s'arrête devant (z=%.1f)" % z.global_position.z)
	_check(gate.hp < gate.max_hp, "il tape sur la barrière (%d PV)" % int(gate.hp))
	gate.hp = 5.0
	await _seconds(2.5)
	_check(not gate.alive, "la barrière tombe à 0 PV")
	await _seconds(5.0)
	_check(z.dead or z.global_position.z > gate.global_position.z + 1.0, "le zombie passe une fois la barrière tombée (z=%.1f)" % z.global_position.z)
	var restored := gate.repair(gate.max_hp)
	_check(gate.alive and restored > 0.0, "réparer relève la barrière")
	await _clear_zombies()


func _test_repair_hold() -> void:
	print("Réparer une barrière en maintenant E")
	var gate := _gate("avant")
	Game.scrap = 100
	gate.repair(gate.max_hp)
	gate.hp = 300.0
	# Derrière la barrière (côté Cœur), regard vers elle (nord).
	await _hold_interact(Vector3(0, 0, -40), 0.0, 0.0, 1.0)
	_check(gate.hp >= 370.0 and Game.scrap <= 93, "barrière abîmée : +%d PV en 1 s, ferraille %d" % [int(gate.hp - 300.0), Game.scrap])
	gate.take_damage(1e6)
	await _frames(45)  # Le temps qu'elle s'effondre.
	_check(not gate.alive, "la barrière est détruite")
	var scrap := Game.scrap
	# Les débris sont au ras du sol : en les regardant de près, le viseur passe dessous.
	await _hold_interact(Vector3(0, 0, -41), 0.0, -0.9, 0.5)
	_check(gate.hp > 0.0 and not gate.alive, "en regardant les débris, elle se relève petit à petit (%d / %d)" % [int(gate.hp), int(gate.rebuild_hp())])
	await _hold_interact(Vector3(1.0, 0, -43.1), 0.0, -0.8, 2.0)
	_check(gate.alive, "debout sur les débris, elle se relève (%d PV)" % int(gate.hp))
	_check(Game.scrap < scrap, "la réparation coûte de la ferraille (%d -> %d)" % [scrap, Game.scrap])
	# Sans ferraille, le joueur est prévenu au lieu de ne rien voir se passer.
	gate.hp = gate.max_hp - 100.0
	Game.scrap = 0
	var p := Game.player as Player
	p.global_position = Vector3(0, 0, -40)
	p.rotation.y = 0.0
	p._aim_pitch = 0.0
	await _frames(3)
	_check(Game.hint.contains("Plus de ferraille"), "sans ferraille, un message le dit")
	Game.scrap = 2000
	gate.repair(gate.max_hp)
	p.global_position = Vector3(40, 0, -40)  # Loin du chemin pour la suite.
	await _frames(2)


func _test_surcharge() -> void:
	print("Combo SURCHARGE")
	var a := _spawn("rodeur", Vector3(-45, 0.2, 40))
	var b := _spawn("rodeur", Vector3(-43, 0.2, 40))
	var c := _spawn("rodeur", Vector3(-45, 0.2, 42))
	await _frames(1)
	a.charge(3.0)
	a.take_damage(10.0, true, false, Vector3.INF, Vector3.UP, "standard")
	_check(is_equal_approx(a.hp, 5.0), "la cible prend 10 + 45 (PV %.0f, attendu 5)" % a.hp)
	_check(is_equal_approx(b.hp, 15.0) and is_equal_approx(c.hp, 15.0), "les voisins prennent 45 (PV %.0f et %.0f)" % [b.hp, c.hp])
	_check(b.stun_time > 0.0 and c.stun_time > 0.0, "les voisins sont étourdis")
	_check(a.charged_time <= 0.0, "la charge est consommée")
	var d := _spawn("rodeur", Vector3(-30, 0.2, 40))
	await _frames(1)
	d.take_damage(10.0, true, false, Vector3.INF, Vector3.UP, "electrique")
	_check(d.charged_time > 0.0 and is_equal_approx(d.hp, 50.0), "une balle électrique charge sans déclencher")
	d.take_damage(10.0, true, false, Vector3.INF, Vector3.UP, "electrique")
	_check(is_equal_approx(d.hp, 40.0), "une 2e balle électrique ne déclenche pas non plus")
	await _clear_zombies()


func _test_embrasement() -> void:
	print("Combo EMBRASEMENT (Mortier sur zombie en feu)")
	var a := _spawn("brute", Vector3(-45, 0.2, 40))
	var b := _spawn("brute", Vector3(-43.5, 0.2, 40))
	var c := _spawn("brute", Vector3(-45, 0.2, 43.5))
	await _frames(1)
	for z in [a, b, c]:
		z.set_physics_process(false)
	a.ignite(5.0, 1.0)
	var before := a.hp
	var p := a.global_position
	Projectile.launch(_main, "shell", p + Vector3(0, 6, 0), Vector3(p.x, 0.05, p.z), 0.3, 40.0, null, 3.5, 1.2)
	await _seconds(0.6)
	_check(b.burn_time > 0.0 and c.burn_time > 0.0, "le feu se propage aux voisins")
	_check(before - a.hp >= 59.0, "la cible en feu prend x1,5 (%.0f dégâts)" % (before - a.hp))
	_check(b.stun_time > 0.0, "l'obus étourdit")
	await _clear_zombies()


func _test_hurleur() -> void:
	print("Hurleur")
	var h := _spawn("hurleur", Vector3(-45, 0.2, 40))
	var r := _spawn("rodeur", Vector3(-41, 0.2, 40))
	var far := _spawn("rodeur", Vector3(-25, 0.2, 40))
	await _frames(1)
	h._scream_cd = 0.0
	await _frames(3)
	_check(r.buff_time > 0.0, "le cri enrage le rôdeur proche")
	_check(far.buff_time <= 0.0, "le rôdeur loin n'est pas touché")
	_check(is_equal_approx(r.current_damage(), r.damage * Zombie.BUFF_DAMAGE), "dégâts augmentés de 25 %")
	await _clear_zombies()


func _test_cracheur() -> void:
	print("Cracheur")
	var gate := _gate("avant")
	gate.repair(gate.max_hp)
	var socket := _socket_near(Vector3(5.5, 0, -39.5))
	socket.build("gun")
	var tower := socket.tower
	tower.toggle_power()  # Éteinte : elle ne tue pas le Cracheur pendant le test.
	var z := _spawn("cracheur", Vector3(0, 0.2, -55))
	var hp := gate.hp
	var aimed_tower := false
	for i in 8 * 60:
		await _frames(1)
		if z._spit_target is Tower:
			aimed_tower = true
	_check(gate.hp < hp, "l'acide abîme la barrière (%d / %d PV)" % [int(gate.hp), int(gate.max_hp)])
	_check(gate.front_distance(z.global_position) > 3.0, "il reste à distance pour cracher (%.1f m)" % gate.front_distance(z.global_position))
	_check(not aimed_tower, "il ne vise jamais la tour juste à côté")
	await _clear_zombies()
	gate.repair(gate.max_hp)


func _test_towers_ignored() -> void:
	print("Tours : plus petites, jamais attaquées")
	var socket := _socket_near(Vector3(18, 0, 13))
	socket.build("gun")
	var tower := socket.tower
	tower.toggle_power()
	_check(tower._model.scale.x < 1.0, "modèle réduit (x%.2f)" % tower._model.scale.x)
	_check(not tower.has_method("take_damage"), "une tour ne peut pas être abîmée")
	var brute := _spawn("brute", Vector3(12, 0.2, 14))
	var before := brute.progress()
	await _seconds(4.0)
	_check(brute.progress() > before + 3.0, "la Brute passe à côté de la tour sans s'arrêter (+%.1f m)" % (brute.progress() - before))
	await _clear_zombies()


func _test_fouisseur() -> void:
	print("Fouisseur et Phare")
	var gate := _gate("avant")
	gate.repair(gate.max_hp)
	var z := _spawn("fouisseur", Vector3(0, 0.2, -54))
	var t := 0.0
	while not z.burrowed and t < 12.0:
		await _frames(6)
		t += 0.1
	_check(z.burrowed and z.collision_layer == 0, "il s'enterre devant la barrière (après %.1f s)" % t)
	var hp := z.hp
	z.take_damage(50.0)
	_check(is_equal_approx(z.hp, hp), "les dégâts ne l'atteignent pas sous terre")
	t = 0.0
	while z._dig_state != "walk" and t < 30.0:
		await _seconds(0.25)
		t += 0.25
	_check(not z.burrowed, "il ressort de terre (après %.1f s)" % t)
	_check(gate.is_behind(z.global_position) and not gate.blocks(z), "il ressort derrière la barrière, qui ne le bloque plus")
	_check(is_equal_approx(gate.hp, gate.max_hp), "la barrière n'a pas été touchée")
	await _clear_zombies()
	# Un Phare allumé fait sortir de terre les Fouisseurs proches.
	for tw in get_tree().get_nodes_in_group("towers"):
		if tw.powered:
			tw.toggle_power()
	var socket := _socket_near(Vector3(-5.5, 0, -39.5))
	socket.build("beacon")
	_check(socket.tower != null and socket.tower.powered, "Phare posé et allumé")
	var f := _spawn("fouisseur", Vector3(0, 0.2, -54))
	var went_under := false
	var surfaced := false
	for i in 8 * 60:
		await _frames(1)
		if f._dig_state == "under":
			went_under = true
		if went_under and f._dig_state == "rise":
			surfaced = true
			break
	_check(surfaced, "le Phare le débusque dès qu'il passe sous terre")
	_check(not gate.is_behind(f.global_position), "il reste bloqué devant la barrière")
	await _clear_zombies()


func _test_heal_between_waves() -> void:
	print("Santé restaurée entre les vagues")
	var p := Game.player as Player
	var wm := _main.get_node("WaveManager") as WaveManager
	p.hp = 35.0
	var wave := Game.wave
	wm._wave_cleared()
	_check(is_equal_approx(p.hp, p.max_hp), "vague repoussée : santé %d / %d" % [int(p.hp), int(p.max_hp)])
	# Retour à la préparation pour la suite du test.
	Game.wave = wave
	wm._enter("prep", 999.0)


func _test_workbench() -> void:
	print("Établi")
	var p := Game.player as Player
	Game.scrap = 1000
	_check(Game.buy_barrel("pistol") and Game.scrap == 960, "canon du pistolet acheté (40)")
	_check(is_equal_approx(p.weapon_damage("pistol"), 34.0 * 1.2), "pistolet : %.1f dégâts (attendu 40,8)" % p.weapon_damage("pistol"))
	_check(Game.buy_mag("rifle") and p.mag_size("rifle") == 39 and p.ammo["rifle"] == 39, "chargeur du fusil : 39 balles")
	_check(Game.choose_ammo("rifle", "electrique") and Game.scrap == 870, "munitions électriques achetées (60)")
	_check(Game.choose_ammo("rifle", "standard") and Game.scrap == 870, "revenir aux munitions standard est gratuit")
	_check(Game.choose_ammo("rifle", "electrique") and Game.scrap == 870, "remettre les électriques est gratuit")
	var cap := Game.energy_cap()
	_check(Game.buy_generator() and Game.energy_cap() == cap + 2, "générateur : +2 énergie")
	Game.mods["pistol"]["barrel"] = 2
	_check(not Game.buy_barrel("pistol"), "pas plus de 2 niveaux de canon")
	(_main as Node).open_bench()
	await _frames(2)
	_check(_main.bench_panel.visible and get_tree().paused, "l'établi s'ouvre et met le jeu en pause")
	var alpha: float = _main.bench_panel._root.modulate.a
	_check(alpha > 0.0 and alpha < 1.0, "il apparaît en fondu (%.2f)" % alpha)
	await _seconds(0.4)
	_check(is_equal_approx(_main.bench_panel._root.modulate.a, 1.0) and _main.bench_panel._center.scale.is_equal_approx(Vector2.ONE), "transition terminée")
	_main.bench_panel.close()
	_check(not get_tree().paused, "fermer l'établi relance le jeu")
	Game.choose_ammo("rifle", "standard")


func _test_gadgets() -> void:
	print("Gadgets")
	var p := Game.player as Player
	p.global_position = Vector3(-18, 0.2, -8)
	p.rotation.y = 0.0  # Regarde vers le nord, le long du chemin.
	p._aim_pitch = 0.0
	await _frames(2)
	Game.gadget_slots = ["barricade", "leurre"]
	Game.gadget_cd.clear()
	_check(Gadgets.use(0, p), "barricade posée sur le chemin")
	var barricade: Barrier = null
	for b in get_tree().get_nodes_in_group("barriers"):
		if b.temporary:
			barricade = b
	_check(barricade != null, "la barricade bloque le chemin")
	_check(not Gadgets.use(0, p), "elle a un temps de recharge")
	_spawn("rodeur", Vector3(-18, 0.2, -19))
	await _seconds(5.0)
	_check(barricade.hp < barricade.max_hp, "un zombie s'arrête et la frappe (%d PV)" % int(barricade.hp))
	await _clear_zombies()
	_check(Gadgets.use(1, p), "leurre lancé")
	await _seconds(0.6)
	var decoys := get_tree().get_nodes_in_group("decoys")
	_check(decoys.size() == 1, "le leurre est au sol")
	if decoys.size() == 1:
		var decoy: Node3D = decoys[0]
		var r := _spawn("rodeur", decoy.global_position + Vector3(10, 0.2, 0))
		await _seconds(0.5)
		_check(r._decoy == decoy, "un zombie à 10 m va vers le leurre")
	await _clear_zombies()
	Game.set_gadget(0, "drone")
	Game.gadget_cd.clear()
	var scrap := Scrap.new()
	_main.add_child(scrap)
	scrap.setup(7, p.global_position + Vector3(8, 0, 0))
	var before := Game.scrap
	_check(Gadgets.use(0, p), "drone lancé")
	await _seconds(6.0)
	_check(Game.scrap >= before + 7, "le drone ramasse la ferraille (%d -> %d)" % [before, Game.scrap])
	for d in get_tree().get_nodes_in_group("drones"):
		d.queue_free()


func _test_settings() -> void:
	print("Réglages et touches")
	Settings.rebind("jump", {"kind": "k", "code": KEY_V})
	var found := false
	for ev in InputMap.action_get_events("jump"):
		if ev is InputEventKey and ev.keycode == KEY_V:
			found = true
	_check(found, "Sauter passe sur V")
	Settings.rebind("jump", {"kind": "k", "code": KEY_Z})
	_check(Settings.key_label("jump") == "Z" and Settings.key_label("move_forward") == "V", "touche déjà prise : les deux actions échangent")
	Settings.load_settings()
	_check(Settings.key_label("jump") == "Z", "les touches sont sauvegardées puis relues")
	Settings.reset_bindings()
	Settings.save_settings()
	_check(Settings.key_label("jump") == "Espace" and Settings.key_label("move_forward") == "Z", "touches par défaut restaurées")
	_check(Settings.key_label("fire") == "Clic gauche", "nom du bouton de souris en français")


func _test_score_and_menus() -> void:
	print("Score et menus")
	var before := Game.score
	var z := _spawn("rodeur", Vector3(-45, 0.2, 40))
	await _frames(1)
	z.take_damage(1e9)
	_check(Game.score == before + 10, "un rôdeur abattu rapporte 10 points")
	_main.pause_menu.open()
	_check(get_tree().paused and _main.pause_menu.visible, "Échap : menu pause")
	_main.pause_menu.close()
	_check(not get_tree().paused, "reprendre la partie")
	var menu: Control = load("res://scenes/menu.tscn").instantiate()
	add_child(menu)
	await _frames(2)
	var buttons := menu.find_children("*", "Button", true, false)
	_check(buttons.size() >= 4, "menu principal : %d boutons" % buttons.size())
	menu.queue_free()
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED


func _test_upgrades() -> void:
	print("Améliorations permanentes")
	Settings.insignes = 20
	_check(Upgrades.buy("vitalite") and Upgrades.rank("vitalite") == 1 and Settings.insignes == 17, "acheter Vitalité rang 1 coûte 3 insignes")
	_check(is_equal_approx(Upgrades.mult("vitalite"), 1.06), "Vitalité rang 1 : +6 % de PV")
	var p := Game.player as Player
	p.apply_implants()
	_check(is_equal_approx(p.max_hp, 106.0), "le joueur a 106 PV max (%.0f)" % p.max_hp)
	Upgrades.buy("dynamo")
	_check(Game.energy_cap() == Game.ENERGY_BASE + 2 * Game.generator + 1, "Dynamo : +1 énergie")
	Settings.insignes = 1
	_check(not Upgrades.buy("armurier"), "impossible d'acheter sans assez d'insignes")
	Upgrades.refund_all()
	_check(Settings.insignes == 1 + 3 + 6 and Upgrades.rank("vitalite") == 0, "tout rembourser rend les insignes (%d)" % Settings.insignes)
	p.apply_implants()
	_check(Upgrades.earned(0, 5, false) == 5, "défaite à la vague 6 du niveau 1 : 5 insignes")
	_check(Upgrades.earned(3, 10, true) == 50, "victoire au niveau 4 : 30 + 20 insignes")
	Settings.load_settings()  # Relit le fichier de test : les achats y sont bien enregistrés.
	_check(Settings.insignes == 10 and Upgrades.rank("vitalite") == 0, "insignes et améliorations sauvegardés")
	Settings.insignes = 0


func _test_levels() -> void:
	print("Niveaux")
	Settings.unlocked_level = 0
	Settings.levels_won = []
	_check(not Settings.finish_level(0, false, 4) and Settings.unlocked_level == 0, "une défaite ne débloque rien")
	_check(Settings.finish_level(0, true, 15) and Settings.unlocked_level == 1, "gagner le niveau 1 débloque le niveau 2")
	_check(not Settings.finish_level(0, true, 15), "le regagner ne débloque rien de plus")
	var wm: WaveManager = _main.get_node("WaveManager")
	var easy := wm._build_queue(1).size()
	Game.level = 3
	var hard := wm._build_queue(1).size()
	_check(hard > easy, "plus de zombies au niveau 4 (%d contre %d à la vague 1)" % [hard, easy])
	var bosses := wm._build_queue(10).count("boss")
	_check(bosses == 2, "deux boss à la dernière vague du niveau 4")
	# Chaque niveau se construit et ses zombies suivent le chemin jusqu'à la première barrière.
	_main.queue_free()
	await _frames(2)
	for i in Levels.count():
		Game.level = i
		_main = load("res://scenes/main.tscn").instantiate()
		add_child(_main)
		for c in _main.get_children():
			if c is Tutorial:
				c.free()
		Game.tutorial_hold = true
		await _frames(2)
		var name: String = Levels.get_level(i)["name"]
		var sockets := _main.get_children().filter(func(c): return c is Socket).size()
		_check(get_tree().get_nodes_in_group("gates").size() == 2 and sockets == 9, "%s : 2 barrières et 9 ancrages" % name)
		var gate := _gate("avant")
		var z := _spawn("rodeur", _main.path_points[0] + Vector3(0, 0.2, 0))
		var start := z.global_position.distance_to(gate.global_position)
		await _seconds(22.0)
		_check(gate.hp < gate.max_hp, "%s : le rôdeur suit le chemin et tape la barrière (%.0f m au départ)" % [name, start])
		_main.queue_free()
		await _frames(2)
	Game.level = 0
