class_name Gadgets
extends RefCounted
## Utilisation des gadgets du joueur (2 emplacements, avec temps de recharge) :
## barricade dépliable, leurre sonore et drone de récolte.


## Utilise le gadget de l'emplacement slot (0 ou 1). Renvoie vrai s'il a servi.
static func use(slot: int, player: Player) -> bool:
	var id: String = Game.gadget_slots[slot]
	var data: Dictionary = Game.GADGETS[id]
	var left: float = Game.gadget_cd.get(id, 0.0)
	if left > 0.0:
		Game.say("%s : encore %d s de recharge" % [data["name"], int(ceil(left))])
		return false
	var ok := false
	match id:
		"barricade":
			ok = _barricade(player)
		"leurre":
			ok = _decoy(player)
		"drone":
			ok = _drone(player)
	if ok:
		Game.gadget_cd[id] = data["cooldown"]
		Game.changed.emit()
	return ok


## Pose une barricade en travers du chemin, devant le joueur.
static func _barricade(player: Player) -> bool:
	var fwd := player.look_direction()
	fwd.y = 0.0
	var spot := player.global_position + fwd.normalized() * 3.0
	var pts: Array[Vector3] = Game.main.path_points
	var best := INF
	var best_seg := -1
	var best_point := Vector3.ZERO
	for i in pts.size() - 1:
		var a := Vector2(pts[i].x, pts[i].z)
		var b := Vector2(pts[i + 1].x, pts[i + 1].z)
		var p := Vector2(spot.x, spot.z)
		var ab := b - a
		var t := clampf((p - a).dot(ab) / ab.length_squared(), 0.02, 0.98)
		var c := a + ab * t
		var d := p.distance_to(c)
		if d < best:
			best = d
			best_seg = i
			best_point = Vector3(c.x, 0, c.y)
	if best > 4.5:
		Game.say("La barricade se pose en travers du chemin des zombies : approche-toi du chemin de boue.")
		return false
	for b in Game.main.get_tree().get_nodes_in_group("barriers"):
		if (b as Node3D).global_position.distance_to(best_point) < 3.0:
			Game.say("Il y a déjà une barrière ici.")
			return false
	var barricade := Barrier.new()
	Game.main.add_child(barricade)
	barricade.global_position = best_point
	barricade.setup("Barricade", "", 250.0 * (1.0 + 0.1 * maxf(0.0, Game.wave - 1)), best_seg, 3.6, "barricade", 20.0)
	barricade.scale = Vector3(1, 0.05, 1)
	var tween := barricade.create_tween()
	tween.tween_property(barricade, "scale", Vector3.ONE, 0.35).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	Sfx.play_at(Game.main, "build", best_point + Vector3(0, 0.5, 0), 0.0, 0.1)
	Fx.puff(Game.main, best_point + Vector3(0, 0.3, 0), Color(0.45, 0.4, 0.35, 0.5), 10, 1.0)
	return true


## Lance le leurre devant soi : il atterrit au sol (jusqu'à 14 m).
static func _decoy(player: Player) -> bool:
	var from := player.eye_position()
	var dir := player.look_direction()
	var query := PhysicsRayQueryParameters3D.create(from, from + dir * 14.0, Fx.LAYER_WORLD | Fx.LAYER_STRUCTURES, [player.get_rid()])
	var hit := player.get_world_3d().direct_space_state.intersect_ray(query)
	var land: Vector3 = hit["position"] if not hit.is_empty() else from + dir * 14.0
	if not hit.is_empty():
		land -= dir * 0.4
	land.y = 0.02
	var decoy := Decoy.new()
	Game.main.add_child(decoy)
	decoy.global_position = from
	# Petit vol en cloche jusqu'au point d'arrivée.
	var start := from
	var tween := decoy.create_tween()
	tween.tween_method(func(t: float):
		if is_instance_valid(decoy):
			decoy.global_position = start.lerp(land, t) + Vector3(0, sin(t * PI) * 1.5, 0)
	, 0.0, 1.0, 0.45)
	Sfx.play(player, "throw", -6.0, 0.1)
	return true


static func _drone(player: Player) -> bool:
	var drone := Drone.new()
	Game.main.add_child(drone)
	drone.global_position = player.global_position + Vector3(0, 2.0, 0)
	Sfx.play_at(Game.main, "build", drone.global_position, -6.0, 0.1)
	return true
