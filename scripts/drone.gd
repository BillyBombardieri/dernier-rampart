class_name Drone
extends Node3D
## Drone de récolte : il vole d'une pièce de ferraille à l'autre autour du joueur et la ramasse
## à sa place, lentement, puis revient vers lui et disparaît.

const LIFETIME := 25.0
const SPEED := 4.0
const SEARCH_RADIUS := 25.0
const HEIGHT := 2.4

var _time := 0.0
var _target: Node3D = null
var _rotors: Array[Node3D] = []
var _hum: AudioStreamPlayer3D
var _leaving := false


func _ready() -> void:
	var dark := Fx.material(Color(0.12, 0.13, 0.14))
	dark.metallic = 0.6
	dark.roughness = 0.4
	var body := Fx.box_mat(Vector3(0.36, 0.12, 0.36), dark)
	add_child(body)
	var lens := Fx.box(Vector3(0.1, 0.06, 0.04), Color(0.3, 1.0, 0.6), 4.0)
	lens.position = Vector3(0, -0.03, -0.19)
	add_child(lens)
	for i in 4:
		var a := PI * 0.25 + i * PI * 0.5
		var arm := Fx.box_mat(Vector3(0.05, 0.03, 0.42), dark)
		arm.rotation.y = a
		arm.position = Vector3(cos(a), 0, -sin(a)) * 0.22
		add_child(arm)
		var rotor := Node3D.new()
		rotor.position = Vector3(cos(a), 0.06, -sin(a)) * 0.42 + Vector3(0, 0.06, 0)
		var blade := Fx.box_mat(Vector3(0.32, 0.01, 0.04), Fx.material(Color(0.2, 0.2, 0.2)))
		rotor.add_child(blade)
		add_child(rotor)
		_rotors.append(rotor)
	var light := OmniLight3D.new()
	light.light_color = Color(0.4, 1.0, 0.7)
	light.light_energy = 1.0
	light.omni_range = 3.0
	light.position.y = -0.2
	add_child(light)
	var hum := Sfx.stream("drone")
	if hum is AudioStreamWAV:
		var wav := hum as AudioStreamWAV
		wav.loop_mode = AudioStreamWAV.LOOP_FORWARD
		wav.loop_end = wav.data.size() / 2
		_hum = AudioStreamPlayer3D.new()
		_hum.stream = wav
		_hum.volume_db = -14.0
		_hum.unit_size = 4.0
		_hum.max_distance = 30.0
		add_child(_hum)
		_hum.play()
	add_to_group("drones")


func _process(delta: float) -> void:
	_time += delta
	for r in _rotors:
		r.rotation.y += delta * 40.0
	var player := Game.player as Player
	if player == null:
		queue_free()
		return
	if _time >= LIFETIME and not _leaving:
		_leaving = true
		Game.say("Le drone de récolte rentre.")
	var goal: Vector3
	if _leaving:
		goal = player.global_position + Vector3(0, 1.6, 0)
		if global_position.distance_to(goal) < 1.0:
			queue_free()
			return
	else:
		if not is_instance_valid(_target) or _target.is_queued_for_deletion():
			_target = _find_scrap(player.global_position)
		if _target:
			goal = _target.global_position + Vector3(0, 0.6, 0)
			if Vector2(global_position.x - goal.x, global_position.z - goal.z).length() < 0.5 and global_position.y < goal.y + 0.8:
				_collect(_target)
				_target = null
		else:
			# Rien à ramasser : il tourne au-dessus du joueur.
			goal = player.global_position + Vector3(cos(_time) * 2.0, HEIGHT, sin(_time) * 2.0)
	var travel_goal := goal
	if not _leaving and _target and Vector2(global_position.x - goal.x, global_position.z - goal.z).length() > 1.5:
		travel_goal.y = goal.y + HEIGHT * 0.6  # Vole en hauteur, puis descend sur la pièce.
	var to := travel_goal - global_position
	global_position += to.normalized() * minf(to.length(), SPEED * delta)
	if Vector2(to.x, to.z).length() > 0.1:
		rotation.y = lerp_angle(rotation.y, atan2(-to.x, -to.z), minf(1.0, delta * 5.0))
	rotation.x = lerpf(rotation.x, -0.25 if to.length() > 0.5 else 0.0, minf(1.0, delta * 4.0))


func _find_scrap(around: Vector3) -> Node3D:
	var best: Node3D = null
	var best_d := SEARCH_RADIUS
	for s in get_tree().get_nodes_in_group("scrap"):
		var n := s as Node3D
		if n.is_queued_for_deletion() or n.get_meta("drone_taken", false):
			continue
		var d := Vector2(n.global_position.x - around.x, n.global_position.z - around.z).length()
		if d < best_d:
			best_d = d
			best = n
	if best:
		best.set_meta("drone_taken", true)
	return best


func _collect(s: Node3D) -> void:
	var value: int = s.value
	Game.add_scrap(value)
	Game.scrap_picked.emit(value)
	Sfx.play_at(Game.main, "pickup", global_position, -6.0, 0.1)
	Fx.popup(Game.main, global_position + Vector3(0, 0.6, 0), "+%d" % value, Color(0.5, 1.0, 0.7), 28)
	s.queue_free()
