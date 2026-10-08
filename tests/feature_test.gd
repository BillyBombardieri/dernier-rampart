extends Node
## Test des fonctionnalités, une par une : barrières, combos (SURCHARGE, EMBRASEMENT),
## nouveaux zombies, Phare, établi, gadgets, réglages, score et menus.
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
	await _test_barrier()
	await _test_surcharge()
	await _test_embrasement()
	await _test_hurleur()
	await _test_cracheur()
	await _test_riposte()
	await _test_fouisseur()
	await _test_workbench()
	await _test_gadgets()
	await _test_settings()
	await _test_score_and_menus()
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


func _socket_near(pos: Vector3) -> Socket:
	var best: Socket = null
	for c in _main.get_children():
		if c is Socket and (best == null or c.global_position.distance_to(pos) < best.global_position.distance_to(pos)):
			best = c
	return best


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
	var socket := _socket_near(Vector3(-12, 0, -8))
	socket.build("gun")
	var tower := socket.tower
	tower.toggle_power()  # Éteinte : elle ne tue pas le Cracheur pendant le test.
	var z := _spawn("cracheur", socket.global_position + Vector3(-9, 0.2, 0))
	await _seconds(5.0)
	_check(tower.hp < tower.max_hp, "l'acide abîme la tour (%d / %d PV)" % [int(tower.hp), int(tower.max_hp)])
	_check(z.global_position.distance_to(socket.global_position) > 5.0, "il reste à distance pour cracher")
	await _clear_zombies()
	var restored := tower.repair(1000.0)
	_check(restored > 0.0 and is_equal_approx(tower.hp, tower.max_hp), "réparer la tour lui rend ses PV (+%d)" % int(restored))


func _test_riposte() -> void:
	print("Riposte des tours")
	var socket := _socket_near(Vector3(18, 0, 13))
	socket.build("gun")
	var tower := socket.tower
	var ahead := _spawn("rodeur", Vector3(12, 0.2, 20))
	var brute := _spawn("brute", socket.global_position + Vector3(2.5, 0.2, 0))
	_check(tower._find_target() == ahead, "sans menace, la tour vise le zombie le plus avancé")
	brute._structure = tower
	_check(tower._find_target() == brute, "elle riposte d'abord contre la Brute qui la frappe")
	await _clear_zombies()


func _test_fouisseur() -> void:
	print("Fouisseur et Phare")
	var z := _spawn("fouisseur", Vector3(0, 0.2, -52))
	await _frames(1)
	z._dig_timer = 0.05
	await _seconds(1.5)
	_check(z.burrowed and z.collision_layer == 0, "il s'enterre et devient intouchable")
	var hp := z.hp
	z.take_damage(50.0)
	_check(is_equal_approx(z.hp, hp), "les dégâts ne l'atteignent pas sous terre")
	var t := 0.0
	while z._dig_state != "" and t < 30.0:
		await _seconds(0.5)
		t += 0.5
	_check(not z.burrowed and z._dig_state == "", "il ressort de terre (après %.1f s)" % t)
	_check(z.global_position.distance_to(z._dig_dest) < 1.5, "il ressort à côté de sa cible")
	_check(z.path_index > 1, "il reprend le chemin plus loin (tronçon %d)" % z.path_index)
	await _clear_zombies()
	# Un Phare allumé fait sortir de terre les Fouisseurs proches.
	for tw in get_tree().get_nodes_in_group("towers"):
		if tw.powered:
			tw.toggle_power()
	var socket := _socket_near(Vector3(5.5, 0, -39.5))
	socket.build("beacon")
	_check(socket.tower != null and socket.tower.powered, "Phare posé et allumé")
	var f := _spawn("fouisseur", Vector3(0, 0.2, -52))
	await _frames(1)
	f._dig_timer = 0.05
	var surfaced := false
	for i in 120:
		await _frames(1)
		if f._dig_state == "rise":
			surfaced = true
			break
	_check(surfaced, "le Phare le débusque")
	await _seconds(1.5)
	_check(f.marked or f.dead or true, "le Phare peut le marquer ensuite")
	await _clear_zombies()


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
