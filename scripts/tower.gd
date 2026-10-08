class_name Tower
extends StaticBody3D
## Tour posée sur un ancrage. Elle consomme de l'énergie et tire en priorité sur les zombies marqués,
## puis sur les Hurleurs, puis sur le zombie le plus avancé vers le Cœur. Les zombies ne l'attaquent pas.

const STATS := {
	"gun": {
		"name": "Mitrailleuse", "short": "Tir rapide sur une cible", "cost": 25, "energy": 2,
		"range": 16.0, "rate": 0.18, "damage": 9.0, "color": Color(0.85, 0.6, 0.2),
	},
	"cryo": {
		"name": "Cryo", "short": "Gèle et ralentit (combo BRISÉ)", "cost": 30, "energy": 2,
		"range": 12.0, "rate": 0.9, "damage": 6.0, "color": Color(0.4, 0.8, 1.0),
		"freeze": 2.5,
	},
	"flame": {
		"name": "Lance-flammes", "short": "Cône de feu à courte portée, enflamme", "cost": 35, "energy": 2,
		"range": 8.0, "range_up": 1.0, "rate": 0.12, "damage": 2.2, "color": Color(1.0, 0.45, 0.15),
		"burn": 6.0,
	},
	"arc": {
		"name": "Arc électrique", "short": "Saute sur 4 zombies et les charge", "cost": 40, "energy": 3,
		"range": 13.0, "rate": 1.1, "damage": 22.0, "color": Color(0.6, 0.55, 1.0),
		"chain": 3,
	},
	"mortar": {
		"name": "Mortier", "short": "Obus de zone qui étourdit, très longue portée", "cost": 45, "energy": 3,
		"range": 28.0, "range_up": 3.0, "min_range": 6.0, "rate": 3.2, "damage": 55.0,
		"color": Color(0.62, 0.66, 0.42), "radius": 3.5,
	},
	"beacon": {
		"name": "Phare", "short": "Renforce les tours proches, marque, débusque", "cost": 30, "energy": 1,
		"range": 10.0, "range_up": 1.5, "rate": 6.0, "damage": 0.0, "color": Color(1.0, 0.9, 0.55),
	},
}
const BUILD_ORDER := ["gun", "cryo", "flame", "arc", "mortar", "beacon"]
const MAX_LEVEL := 3
const MARK_BONUS := 1.25
const BEACON_REVEAL := 24.0  # Le Phare marque et débusque les zombies jusqu'à cette distance.
const MODEL_SCALE := 0.75  # Taille du modèle (les tours étaient un peu trop massives).

var type := "gun"
var level := 1
var socket: Socket
var powered := false

var _cooldown := 0.0
var _model: Node3D
var _head: Node3D
var _muzzle: Node3D
var _shots := 0
var _light: MeshInstance3D
var _label: Label3D
var _range_ring: MeshInstance3D
var _hover := 0.0
var _range_mult := 1.0
var _dmg_mult := 1.0
var _bonus_cd := 0.0
var _flame: CPUParticles3D
var _flame_light: OmniLight3D
var _flame_on := 0.0
var _sound_cd := 0.0
var _beam: SpotLight3D
var _reveal_cd := 0.0
var _target: Zombie
var _target_cd := 0.0


func setup(p_type: String, p_socket: Socket) -> void:
	type = p_type
	socket = p_socket
	collision_layer = Fx.LAYER_STRUCTURES
	collision_mask = 0
	var shape := CollisionShape3D.new()
	var bs := BoxShape3D.new()
	bs.size = Vector3(1.4, 2.2 if type != "beacon" else 3.8, 1.4) * MODEL_SCALE
	shape.shape = bs
	shape.position.y = bs.size.y * 0.5
	add_child(shape)
	_model = Node3D.new()
	_model.scale = Vector3.ONE * MODEL_SCALE
	add_child(_model)
	_build_visual(STATS[type]["color"])
	_light = Fx.box(Vector3(0.12, 0.12, 0.12), Color.GREEN, 4.0)
	_light.position = Vector3(0.5, 0.55, 0.5)
	_model.add_child(_light)
	_label = Label3D.new()
	_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	_label.font_size = 32
	_label.outline_size = 6
	_label.position.y = (2.8 if type != "beacon" else 4.6) * MODEL_SCALE + 0.2
	add_child(_label)
	_range_ring = range_ring(tower_range(), STATS[type]["color"])
	_range_ring.visible = false
	add_child(_range_ring)
	add_to_group("towers")
	if type == "beacon":
		add_to_group("beacons")
	powered = Game.request_energy(energy_cost())
	Sfx.play_at(Game.main, "build", global_position + Vector3(0, 1, 0), -2.0, 0.1)
	_refresh()


## Anneau lumineux au sol qui montre une portée.
static func range_ring(r: float, color: Color) -> MeshInstance3D:
	var torus := TorusMesh.new()
	torus.inner_radius = r - 0.08
	torus.outer_radius = r
	torus.rings = 64
	torus.ring_segments = 4
	var mat := StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.albedo_color = Color(color.r, color.g, color.b, 0.55)
	mat.no_depth_test = true
	torus.material = mat
	var ring := MeshInstance3D.new()
	ring.mesh = torus
	ring.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	ring.position.y = 0.08
	ring.scale = Vector3(1, 0.05, 1)
	return ring


func display_name() -> String:
	return STATS[type]["name"]


func energy_cost() -> int:
	return STATS[type]["energy"]


func upgrade_cost() -> int:
	return int(round(STATS[type]["cost"] * 0.6 + 15.0)) * level


func is_active() -> bool:
	return powered


func tower_range() -> float:
	return (STATS[type]["range"] + STATS[type].get("range_up", 2.0) * (level - 1)) * _range_mult


func tower_damage() -> float:
	return STATS[type]["damage"] * pow(1.5, level - 1) * _dmg_mult


## Bonus que donne un Phare aux tours dans son halo : [portée, dégâts].
func beacon_bonus() -> Vector2:
	return Vector2(1.2 + 0.05 * level, 1.1 + 0.05 * level)


## Le joueur vise la tour : affiche sa portée un court instant.
func hover() -> void:
	_hover = 0.15


func upgrade() -> void:
	if level >= MAX_LEVEL:
		Game.say("Niveau maximum atteint.")
		return
	if not Game.try_spend(upgrade_cost()):
		return
	level += 1
	_head.scale = Vector3.ONE * (1.0 + 0.15 * (level - 1))
	Game.say("%s niveau %d" % [display_name(), level])
	Sfx.play_at(Game.main, "upgrade", global_position + Vector3(0, 1, 0), -2.0, 0.05)
	_refresh()


func toggle_power() -> void:
	if powered:
		powered = false
		Game.release_energy(energy_cost())
	else:
		powered = Game.request_energy(energy_cost())
		if not powered:
			Game.say("Pas assez d'énergie. Coupe une autre tour avec %s." % Settings.key_label("toggle_power"))
	_refresh()


func _physics_process(delta: float) -> void:
	_cooldown -= delta
	_hover -= delta
	# Le halo du Phare (zone renforcée) reste toujours visible, plus discret.
	_range_ring.visible = _hover > 0.0 or type == "beacon"
	if type == "beacon":
		(_range_ring.mesh.material as StandardMaterial3D).albedo_color.a = 0.55 if _hover > 0.0 else 0.16
	var active := is_active()
	_light.visible = active
	_bonus_cd -= delta
	if _bonus_cd <= 0.0:
		_bonus_cd = 0.5
		_update_beacon_bonus()
	if type == "flame":
		_flame_on -= delta
		_flame.emitting = _flame_on > 0.0
		_flame_light.visible = _flame_on > 0.0
	if type == "beacon":
		_update_beacon(delta, active)
		return
	if not active or Game.is_over:
		return
	_target_cd -= delta
	if _target_cd <= 0.0 or not _valid_target(_target):
		_target_cd = 0.15
		_target = _find_target()
	var target := _target
	if target == null:
		return
	var aim := target.global_position + Vector3(0, 1.0 * target.size, 0)
	if type != "arc":
		var flat := Vector3(aim.x, _head.global_position.y, aim.z)
		if _head.global_position.distance_to(flat) > 0.1:
			_head.look_at(flat, Vector3.UP)
	if _cooldown > 0.0:
		return
	_cooldown = STATS[type]["rate"]
	match type:
		"gun":
			_fire_gun(target, aim)
		"cryo":
			_fire_cryo(target, aim)
		"flame":
			_fire_flame()
		"arc":
			_fire_arc(target)
		"mortar":
			_fire_mortar(target)


func _hit_damage(z: Zombie) -> float:
	return tower_damage() * (MARK_BONUS if z.marked else 1.0)


func _fire_gun(target: Zombie, aim: Vector3) -> void:
	_shots += 1
	var from := _muzzle.global_position
	Fx.tracer(Game.main, from, aim, Color(1.0, 0.8, 0.5))
	if _shots % 3 == 0:
		Fx.flash(Game.main, from, Color(1.0, 0.7, 0.4), 3.0, 5.0)
		Sfx.play_at(Game.main, "tower_gun", from, -4.0, 0.1)
	target.take_damage(_hit_damage(target))


func _fire_cryo(target: Zombie, aim: Vector3) -> void:
	var from := _muzzle.global_position
	Fx.tracer(Game.main, from, aim, STATS["cryo"]["color"])
	Fx.burst(Game.main, aim, Vector3.UP, Color(0.75, 0.9, 1.0), 10, 2.0, 0.08)
	Sfx.play_at(Game.main, "cryo", from, -4.0, 0.1)
	target.freeze(STATS["cryo"]["freeze"] + 0.5 * (level - 1))
	target.take_damage(_hit_damage(target))


## Lance-flammes : brûle tous les zombies dans un cône devant la buse.
func _fire_flame() -> void:
	_flame_on = 0.3
	_sound_cd -= STATS["flame"]["rate"]
	if _sound_cd <= 0.0:
		_sound_cd = 0.45
		Sfx.play_at(Game.main, "flame", _muzzle.global_position, -5.0, 0.1)
	var origin := _head.global_position
	var fwd := -_head.global_transform.basis.z
	fwd.y = 0.0
	fwd = fwd.normalized()
	var r := tower_range()
	for node in get_tree().get_nodes_in_group("zombies"):
		var z := node as Zombie
		if z.dead or z.burrowed:
			continue
		var to := z.global_position - origin
		to.y = 0.0
		var d := to.length()
		if d > r or d < 0.01:
			continue
		if fwd.dot(to / d) < cos(deg_to_rad(28.0)) and d > 1.5:
			continue
		z.ignite(3.0, STATS["flame"]["burn"] * pow(1.4, level - 1))
		z.take_damage(_hit_damage(z))


## Arc électrique : l'éclair saute de zombie en zombie et les charge.
func _fire_arc(target: Zombie) -> void:
	var hit: Array[Zombie] = [target]
	var dmg := _hit_damage(target)
	var from := _muzzle.global_position
	var current := target
	for i in STATS["arc"]["chain"] + (level - 1):
		var next: Zombie = null
		var best := 6.0
		for node in get_tree().get_nodes_in_group("zombies"):
			var z := node as Zombie
			if z.dead or z.burrowed or hit.has(z):
				continue
			var d := current.global_position.distance_to(z.global_position)
			if d < best:
				best = d
				next = z
		if next == null:
			break
		hit.append(next)
		current = next
	var prev := from
	for i in hit.size():
		var z := hit[i]
		var point := z.global_position + Vector3(0, 1.2 * z.size, 0)
		Fx.lightning(Game.main, prev, point, Color(0.7, 0.65, 1.0), 6, 0.3)
		prev = point
		z.charge(4.0)
		z.take_damage(dmg * pow(0.75, i))
	Fx.flash(Game.main, from, Color(0.6, 0.55, 1.0), 5.0, 8.0, 0.08)
	Sfx.play_at(Game.main, "zap", from, -3.0, 0.15)


## Mortier : obus en cloche vers le groupe de zombies le plus dense, en anticipant leur marche.
func _fire_mortar(target: Zombie) -> void:
	var from := _muzzle.global_position
	var dist := from.distance_to(target.global_position)
	var flight := 1.0 + dist / 28.0 * 0.8
	var lead := target.velocity * flight
	lead.y = 0.0
	var aim := target.global_position + lead
	aim.y = 0.05
	Projectile.launch(Game.main, "shell", from, aim, flight, tower_damage(), null, STATS["mortar"]["radius"] + 0.25 * (level - 1), 1.2)
	Fx.flash(Game.main, from, Color(1.0, 0.7, 0.4), 6.0, 8.0, 0.08)
	Fx.puff(Game.main, from, Color(0.5, 0.48, 0.45, 0.6), 8, 0.8, 1.2)
	Sfx.play_at(Game.main, "mortar_fire", from, 0.0, 0.1, 100.0)


## Phare : fait tourner sa lumière, marque le zombie le plus avancé et débusque les Fouisseurs.
func _update_beacon(delta: float, active: bool) -> void:
	_head.rotation.y += delta * (1.4 if active else 0.0)
	_beam.visible = active
	if not active or Game.is_over:
		return
	_reveal_cd -= delta
	for node in get_tree().get_nodes_in_group("zombies"):
		var z := node as Zombie
		if z.burrowed and _flat_dist(z.global_position) < BEACON_REVEAL:
			z.force_surface()
	if _reveal_cd > 0.0:
		return
	_reveal_cd = STATS["beacon"]["rate"] - (level - 1)
	var best: Zombie = null
	var best_score := -INF
	for node in get_tree().get_nodes_in_group("zombies"):
		var z := node as Zombie
		if z.dead or z.burrowed or z.marked or _flat_dist(z.global_position) > BEACON_REVEAL:
			continue
		var score := z.progress() + (5000.0 if z.type in ["hurleur", "brute", "boss"] else 0.0)
		if score > best_score:
			best_score = score
			best = z
	if best:
		best.mark()
		Fx.tracer(Game.main, _head.global_position, best.global_position + Vector3(0, 1.4 * best.size, 0), Color(1.0, 0.92, 0.6))
		Sfx.play_at(Game.main, "ping", _head.global_position, -6.0, 0.05)


## Les tours dans le halo d'un Phare allumé gagnent de la portée et des dégâts (pas de cumul).
func _update_beacon_bonus() -> void:
	var bonus := Vector2.ONE
	if type != "beacon":
		for node in get_tree().get_nodes_in_group("beacons"):
			var b := node as Tower
			if b.is_active() and _flat_dist(b.global_position) <= b.tower_range():
				var v := b.beacon_bonus()
				bonus = Vector2(maxf(bonus.x, v.x), maxf(bonus.y, v.y))
	if not is_equal_approx(bonus.x, _range_mult) or not is_equal_approx(bonus.y, _dmg_mult):
		_range_mult = bonus.x
		_dmg_mult = bonus.y
		_refresh()


func _find_target() -> Zombie:
	var best: Zombie = null
	var best_score := -INF
	var r := tower_range()
	var min_r: float = STATS[type].get("min_range", 0.0)
	for node in get_tree().get_nodes_in_group("zombies"):
		var z := node as Zombie
		if z == null or z.dead or z.burrowed:
			continue
		var d := global_position.distance_to(z.global_position)
		if d > r or d < min_r:
			continue
		# Priorité : marqué > Hurleur > (Cryo) pas encore gelé > (Mortier) groupe dense > le plus avancé.
		var score := z.progress()
		if z.marked:
			score += 100000.0
		if z.type == "hurleur":
			score += 60000.0
		if type == "cryo" and z.frozen_time <= 0.0:
			score += 50000.0
		if type == "mortar":
			score += 4000.0 * _neighbors(z, STATS["mortar"]["radius"])
		if score > best_score:
			best_score = score
			best = z
	return best


func _valid_target(z: Variant) -> bool:  # Pas typé : la cible peut avoir été libérée entre-temps.
	if not is_instance_valid(z) or z.dead or z.burrowed:
		return false
	var d := global_position.distance_to(z.global_position)
	return d <= tower_range() and d >= STATS[type].get("min_range", 0.0)


func _neighbors(z: Zombie, r: float) -> int:
	var n := 0
	for node in get_tree().get_nodes_in_group("zombies"):
		if node != z and z.global_position.distance_to(node.global_position) < r:
			n += 1
	return n


func _flat_dist(p: Vector3) -> float:
	return Vector2(global_position.x - p.x, global_position.z - p.z).length()


func _refresh() -> void:
	var state := "" if powered else "  [ÉTEINTE]"
	var boost := "  ★" if _range_mult > 1.0 else ""
	_label.text = "%s niv.%d%s%s" % [display_name(), level, state, boost]
	if _range_ring:
		var torus := _range_ring.mesh as TorusMesh
		torus.outer_radius = tower_range()
		torus.inner_radius = tower_range() - 0.08


# ---------------------------------------------------------------- apparence

func _build_visual(color: Color) -> void:
	var metal := Fx.textured("metal", 1.2, Color(0.85, 0.85, 0.85), false)
	var dark_metal := Fx.textured("metal", 1.2, Color(0.45, 0.45, 0.45), false)
	# Socle trapu commun à toutes les tours.
	var base := Fx.box_mat(Vector3(1.4, 0.5, 1.4), dark_metal)
	base.position.y = 0.25
	_model.add_child(base)
	_head = Node3D.new()
	_muzzle = Node3D.new()
	match type:
		"gun", "cryo":
			_column(metal, 1.0)
			_head.position.y = 1.7
			_model.add_child(_head)
			_head.add_child(Fx.box_mat(Vector3(0.9, 0.55, 1.0), metal))
			if type == "gun":
				for x in [-0.15, 0.15]:
					var barrel := Fx.mesh_with(_cyl(0.06, 1.3), dark_metal)
					barrel.rotation_degrees.x = 90
					barrel.position = Vector3(x, 0.05, -1.0)
					_head.add_child(barrel)
				var ammo_box := Fx.box_mat(Vector3(0.35, 0.35, 0.5), dark_metal)
				ammo_box.position = Vector3(0.6, -0.05, 0.1)
				_head.add_child(ammo_box)
			else:
				var tank := Fx.mesh_with(_cyl(0.28, 0.9), Fx.material(color, 1.5))
				tank.rotation_degrees.z = 90
				tank.position = Vector3(0, 0.45, 0.2)
				_head.add_child(tank)
				var nozzle := Fx.mesh_with(_cyl(0.12, 0.9, 0.2), dark_metal)
				nozzle.rotation_degrees.x = 90
				nozzle.position = Vector3(0, 0.0, -0.9)
				_head.add_child(nozzle)
			_muzzle.position = Vector3(0, 0.05, -1.7)
		"flame":
			_column(metal, 0.9)
			_head.position.y = 1.55
			_model.add_child(_head)
			_head.add_child(Fx.box_mat(Vector3(0.8, 0.5, 0.9), metal))
			# Réservoirs de carburant rouges sur les côtés, buse évasée devant.
			var red := Fx.material(Color(0.55, 0.1, 0.07))
			red.roughness = 0.45
			for x in [-0.55, 0.55]:
				var tank := Fx.mesh_with(_cyl(0.2, 0.85), red)
				tank.rotation_degrees.x = 90
				tank.position = Vector3(x, 0.05, 0.15)
				_head.add_child(tank)
			var nozzle := Fx.mesh_with(_cyl(0.09, 0.9, 0.16), dark_metal)
			nozzle.rotation_degrees.x = 90
			nozzle.position = Vector3(0, 0.0, -0.85)
			_head.add_child(nozzle)
			var pilot := Fx.mesh_with(_cyl(0.05, 0.06), Fx.material(Color(0.4, 0.6, 1.0), 4.0))
			pilot.position = Vector3(0, -0.12, -1.28)
			_head.add_child(pilot)
			_muzzle.position = Vector3(0, 0.0, -1.35)
			_flame = Fx.fire_particles(120, 0.4, 0.55)
			_flame.direction = Vector3(0, 0, -1)
			_flame.spread = 11.0
			_flame.initial_velocity_min = 9.0
			_flame.initial_velocity_max = 13.0
			_flame.gravity = Vector3(0, 2.5, 0)
			_flame.damping_min = 6.0
			_flame.damping_max = 9.0
			_flame.emission_sphere_radius = 0.08
			_flame.scale_amount_min = 0.6
			_flame.scale_amount_max = 1.6
			# Le jet s'élargit en s'éloignant de la buse.
			var spread := Curve.new()
			spread.add_point(Vector2(0.0, 0.3))
			spread.add_point(Vector2(0.6, 1.2))
			spread.add_point(Vector2(1.0, 1.5))
			_flame.scale_amount_curve = spread
			_flame.emitting = false
			_flame.scale = Vector3.ONE / MODEL_SCALE  # Le jet garde sa longueur réelle (portée de 8 m).
			_muzzle.add_child(_flame)
			_flame_light = OmniLight3D.new()
			_flame_light.light_color = Color(1.0, 0.55, 0.2)
			_flame_light.light_energy = 3.0
			_flame_light.omni_range = 7.0
			_flame_light.position = Vector3(0, 0, -2.5)
			_flame_light.visible = false
			_muzzle.add_child(_flame_light)
		"arc":
			# Bobine Tesla : noyau, anneaux de cuivre et sphère qui luit.
			_head.position.y = 0.5
			_model.add_child(_head)
			var copper := Fx.material(Color(0.72, 0.42, 0.25))
			copper.metallic = 0.9
			copper.roughness = 0.3
			var core := Fx.mesh_with(_cyl(0.16, 1.6), dark_metal)
			core.position.y = 0.8
			_head.add_child(core)
			for i in 4:
				var torus := TorusMesh.new()
				torus.inner_radius = 0.2
				torus.outer_radius = 0.34 - i * 0.03
				var ring := Fx.mesh_with(torus, copper)
				ring.position.y = 0.4 + i * 0.32
				_head.add_child(ring)
			var orb := SphereMesh.new()
			orb.radius = 0.3
			orb.height = 0.6
			var orb_mesh := Fx.mesh_with(orb, Fx.material(color, 3.0))
			orb_mesh.position.y = 1.9
			_head.add_child(orb_mesh)
			var glow := OmniLight3D.new()
			glow.light_color = color
			glow.light_energy = 1.5
			glow.omni_range = 4.0
			glow.position.y = 1.9
			_head.add_child(glow)
			_muzzle.position = Vector3(0, 1.9, 0)
		"mortar":
			# Tube trapu incliné vers le ciel, sur une plaque de base.
			var plate := Fx.box_mat(Vector3(1.2, 0.15, 1.2), metal)
			plate.position.y = 0.58
			_model.add_child(plate)
			_head.position.y = 0.75
			_model.add_child(_head)
			var pivot := Node3D.new()
			pivot.rotation.x = deg_to_rad(-55.0)
			_head.add_child(pivot)
			var tube := Fx.mesh_with(_cyl(0.2, 1.3, 0.17), dark_metal)
			tube.position.y = 0.55
			pivot.add_child(tube)
			var ring_mesh := TorusMesh.new()
			ring_mesh.inner_radius = 0.19
			ring_mesh.outer_radius = 0.26
			var collar := Fx.mesh_with(ring_mesh, metal)
			collar.position.y = 0.2
			pivot.add_child(collar)
			for x in [-0.3, 0.3]:
				var leg := Fx.mesh_with(_cyl(0.04, 0.9), metal)
				leg.position = Vector3(x, 0.25, -0.35)
				leg.rotation.x = -0.6
				_head.add_child(leg)
			_muzzle.position = Vector3(0, 1.2, 0)
			pivot.add_child(_muzzle)
			# Caisses d'obus au pied.
			var crate := Fx.box_mat(Vector3(0.5, 0.3, 0.35), Fx.textured("cloth", 1.0, Color(0.4, 0.42, 0.28), false))
			crate.position = Vector3(0.5, 0.65, 0.45)
			_model.add_child(crate)
		"beacon":
			# Mât avec un projecteur qui tourne en permanence.
			var mast := Fx.mesh_with(_cyl(0.1, 3.2), metal)
			mast.position.y = 2.1
			_model.add_child(mast)
			_head.position.y = 3.75
			_model.add_child(_head)
			var housing := Fx.mesh_with(_cyl(0.28, 0.5), dark_metal)
			housing.rotation_degrees.x = 90
			_head.add_child(housing)
			var lens := Fx.mesh_with(_cyl(0.24, 0.04), Fx.material(color, 5.0))
			lens.rotation_degrees.x = 90
			lens.position.z = -0.27
			_head.add_child(lens)
			_beam = SpotLight3D.new()
			_beam.light_color = color
			_beam.light_energy = 5.0
			_beam.spot_range = 30.0
			_beam.spot_angle = 14.0
			_beam.light_volumetric_fog_energy = 3.0
			_beam.position.z = -0.3
			_beam.rotation.x = deg_to_rad(-12.0)
			_head.add_child(_beam)
			_muzzle.position = Vector3(0, 0, -0.3)
	if _muzzle.get_parent() == null:
		_head.add_child(_muzzle)


func _column(mat: Material, height: float) -> void:
	var column := Fx.mesh_with(_cyl(0.35, height), mat)
	column.position.y = 0.5 + height * 0.5
	_model.add_child(column)


func _cyl(radius: float, height: float, top := -1.0) -> CylinderMesh:
	var c := CylinderMesh.new()
	c.bottom_radius = radius
	c.top_radius = radius if top < 0.0 else top
	c.height = height
	c.radial_segments = 12
	return c
