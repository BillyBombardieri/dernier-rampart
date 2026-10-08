extends Node
## Capture d'écran automatique de quelques points de vue (pour vérifier le rendu).
## Lancer : godot --path . res://tests/screenshot.tscn -- <dossier_de_sortie>

const SHOTS := [
	# [position du joueur, rotation Y (degrés), inclinaison caméra (degrés), arme, visée]
	[Vector3(0, 0.2, 27), 0.0, -4.0, "rifle", false],
	[Vector3(-14, 0.2, -16), 160.0, -6.0, "pistol", false],
	[Vector3(4, 0.2, -30), 10.0, -3.0, "rifle", true],
	[Vector3(-18, 0.2, -12), 0.0, -8.0, "pistol", false],
]

var _out := "user://"
var _frame := 0
var _shot := 0
var _main: Node
var _zombies: Array = []


func _ready() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		_out = args[0]
	_main = load("res://scenes/main.tscn").instantiate()
	add_child(_main)
	Game.scrap = 500
	var i := 0
	for c in _main.get_children():
		if c is Socket and i < 4:
			c.build("gun" if i % 2 == 0 else "cryo")
			i += 1
	Game.phase_time = 0.1
	# Quelques zombies proches pour voir leur modèle.
	for t in ["rodeur", "coureur", "brute"]:
		var z := Zombie.new()
		_main.add_child(z)
		z.setup(t, 1.0)
		_zombies.append(z)


func _process(_delta: float) -> void:
	_frame += 1
	var p := Game.player as Player
	if _frame == 2:
		_place(p)
	# Laisse les zombies arriver avant les captures.
	if _frame < 400:
		return
	if _frame == 400:
		# Lance l'assaut même si le tutoriel est affiché, pour avoir des zombies à l'écran.
		_main.get_node("WaveManager")._enter("assault", 0.0)
	if (_frame - 400) % 40 == 0:
		if _shot > 0:
			var img := get_viewport().get_texture().get_image()
			img.save_png("%s/shot_%d.png" % [_out, _shot - 1])
			print("Capture ", _shot - 1)
		if _shot >= SHOTS.size():
			get_tree().quit()
			return
		_place(p)
		_shot += 1


func _place(p: Player) -> void:
	var s: Array = SHOTS[min(_shot, SHOTS.size() - 1)]
	# Pose les zombies de démonstration devant la caméra.
	var fwd := Vector3(sin(deg_to_rad(s[1])), 0, cos(deg_to_rad(s[1]))) * -1.0
	var right := Vector3(-fwd.z, 0, fwd.x)
	for i in _zombies.size():
		var z: Zombie = _zombies[i]
		if is_instance_valid(z):
			z.global_position = s[0] + fwd * (5.0 + i * 1.5) + right * (i - 1) * 1.8
			z.set_physics_process(false)
			z.look_at(Vector3(s[0].x, z.global_position.y, s[0].z), Vector3.UP)
			z._walk_phase = i * 1.3
			z.velocity = -z.global_transform.basis.z * 2.0
			z._animate(0.1)
	p.global_position = s[0]
	p.rotation.y = deg_to_rad(s[1])
	p.get_child(1).rotation.x = deg_to_rad(s[2])
	if p.weapon != s[3]:
		p._switch(s[3])
	p.aiming = s[4]
