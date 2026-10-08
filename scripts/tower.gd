class_name Tower
extends StaticBody3D
## Tour posée sur un ancrage. Elle consomme de l'énergie et tire en priorité sur les zombies marqués.

const STATS := {
	"gun": {
		"name": "Mitrailleuse", "cost": 25, "energy": 2, "range": 16.0,
		"rate": 0.18, "damage": 9.0, "hp": 220.0, "color": Color(0.85, 0.6, 0.2),
	},
	"cryo": {
		"name": "Cryo", "cost": 30, "energy": 2, "range": 12.0,
		"rate": 0.9, "damage": 6.0, "hp": 200.0, "color": Color(0.4, 0.8, 1.0),
		"freeze": 2.5,
	},
}
const MAX_LEVEL := 3
const MARK_BONUS := 1.25

var type := "gun"
var level := 1
var socket: Socket
var powered := false
var hp := 200.0
var max_hp := 200.0

var _cooldown := 0.0
var _head: Node3D
var _light: MeshInstance3D
var _label: Label3D


func setup(p_type: String, p_socket: Socket) -> void:
	type = p_type
	socket = p_socket
	max_hp = STATS[type]["hp"]
	hp = max_hp
	collision_layer = Fx.LAYER_STRUCTURES
	collision_mask = 0
	var shape := CollisionShape3D.new()
	var bs := BoxShape3D.new()
	bs.size = Vector3(1.4, 2.2, 1.4)
	shape.shape = bs
	shape.position.y = 1.1
	add_child(shape)
	var color: Color = STATS[type]["color"]
	var base := Fx.box(Vector3(1.4, 1.4, 1.4), color.darkened(0.4))
	base.position.y = 0.7
	add_child(base)
	_head = Node3D.new()
	_head.position.y = 1.7
	add_child(_head)
	_head.add_child(Fx.box(Vector3(0.8, 0.6, 0.8), color))
	var barrel := Fx.box(Vector3(0.18, 0.18, 1.2), color.lightened(0.3))
	barrel.position.z = -0.7
	_head.add_child(barrel)
	_light = Fx.box(Vector3(0.3, 0.12, 0.3), Color.GREEN, 3.0)
	_light.position.y = 2.1
	add_child(_light)
	_label = Label3D.new()
	_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	_label.font_size = 32
	_label.outline_size = 6
	_label.position.y = 2.8
	add_child(_label)
	add_to_group("towers")
	add_to_group("attackable")
	powered = Game.request_energy(energy_cost())
	_refresh()


func display_name() -> String:
	return STATS[type]["name"]


func energy_cost() -> int:
	return STATS[type]["energy"]


func upgrade_cost() -> int:
	return 30 * level


func is_active() -> bool:
	if not powered:
		return false
	var relay: Structure = Game.main.relay_for_ring(socket.ring)
	return relay == null or relay.alive


func tower_range() -> float:
	return STATS[type]["range"] + 2.0 * (level - 1)


func tower_damage() -> float:
	return STATS[type]["damage"] * pow(1.5, level - 1)


func upgrade() -> void:
	if level >= MAX_LEVEL:
		Game.say("Niveau maximum atteint.")
		return
	if not Game.try_spend(upgrade_cost()):
		return
	level += 1
	max_hp *= 1.3
	hp = max_hp
	_head.scale = Vector3.ONE * (1.0 + 0.15 * (level - 1))
	Game.say("%s niveau %d" % [display_name(), level])
	_refresh()


func toggle_power() -> void:
	if powered:
		powered = false
		Game.release_energy(energy_cost())
	else:
		powered = Game.request_energy(energy_cost())
		if not powered:
			Game.say("Pas assez d'énergie. Coupe une autre tour avec X.")
	_refresh()


func take_damage(amount: float) -> void:
	hp -= amount
	if hp <= 0.0:
		Game.say("%s détruite !" % display_name())
		if powered:
			Game.release_energy(energy_cost())
		socket.clear_tower()
		queue_free()
		return
	_refresh()


func _physics_process(delta: float) -> void:
	_cooldown -= delta
	var active := is_active()
	_light.visible = active
	if not active or Game.is_over:
		return
	var target := _find_target()
	if target == null:
		return
	var aim := target.global_position + Vector3(0, 1.0, 0)
	var flat := Vector3(aim.x, _head.global_position.y, aim.z)
	if _head.global_position.distance_to(flat) > 0.1:
		_head.look_at(flat, Vector3.UP)
	if _cooldown > 0.0:
		return
	_cooldown = STATS[type]["rate"]
	var dmg := tower_damage()
	if target.marked:
		dmg *= MARK_BONUS
	Fx.tracer(Game.main, _head.global_position, aim, STATS[type]["color"])
	if type == "cryo":
		target.freeze(STATS["cryo"]["freeze"] + 0.5 * (level - 1))
	target.take_damage(dmg, false, false)


func _find_target() -> Zombie:
	var best: Zombie = null
	var best_score := -INF
	var r := tower_range()
	for node in get_tree().get_nodes_in_group("zombies"):
		var z := node as Zombie
		if z == null or z.dead:
			continue
		if global_position.distance_to(z.global_position) > r:
			continue
		# Priorité : marqué > (pour la Cryo) pas encore gelé > le plus avancé vers le Cœur.
		var score := z.progress()
		if z.marked:
			score += 100000.0
		if type == "cryo" and z.frozen_time <= 0.0:
			score += 50000.0
		if score > best_score:
			best_score = score
			best = z
	return best


func _refresh() -> void:
	var state := "" if powered else "  [ÉTEINTE]"
	_label.text = "%s niv.%d%s\n%d PV" % [display_name(), level, state, int(hp)]
