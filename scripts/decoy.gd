class_name Decoy
extends Node3D
## Leurre sonore : un petit boîtier qui bipe et attire les zombies proches pendant quelques secondes.
## Les zombies l'attaquent ; il se casse s'il prend trop de coups.

const LIFETIME := 8.0

var alive := true
var hp := 120.0
var _time := 0.0
var _beep_cd := 0.0
var _led: MeshInstance3D
var _light: OmniLight3D


func _ready() -> void:
	var metal := Fx.textured("metal", 2.0, Color(0.6, 0.6, 0.6), false)
	var body := Fx.box_mat(Vector3(0.35, 0.22, 0.25), metal)
	body.position.y = 0.11
	add_child(body)
	var antenna := Fx.mesh_with(_cyl(0.012, 0.5), Fx.material(Color(0.1, 0.1, 0.1)))
	antenna.position = Vector3(0.12, 0.45, 0.05)
	add_child(antenna)
	var speaker := Fx.mesh_with(_cyl(0.08, 0.02), Fx.material(Color(0.05, 0.05, 0.05)))
	speaker.rotation.x = PI * 0.5
	speaker.position = Vector3(-0.05, 0.12, -0.13)
	add_child(speaker)
	_led = Fx.box(Vector3(0.05, 0.05, 0.05), Color(1.0, 0.15, 0.1), 6.0)
	_led.position = Vector3(0.12, 0.72, 0.05)
	add_child(_led)
	_light = OmniLight3D.new()
	_light.light_color = Color(1.0, 0.2, 0.1)
	_light.light_energy = 2.0
	_light.omni_range = 4.0
	_light.position = _led.position
	add_child(_light)
	add_to_group("decoys")


func _process(delta: float) -> void:
	if not alive:
		return
	_time += delta
	_beep_cd -= delta
	if _beep_cd <= 0.0:
		_beep_cd = 0.55
		Sfx.play_at(Game.main, "decoy", global_position + Vector3(0, 0.3, 0), 2.0, 0.02, 60.0)
		Fx.ring_wave(Game.main, global_position + Vector3(0, 0.1, 0), Color(1.0, 0.3, 0.2, 0.35), 3.0, 0.4)
	var blink := fmod(_time, 0.55) < 0.12
	_led.visible = blink
	_light.visible = blink
	if _time >= LIFETIME:
		_stop()


func take_damage(amount: float) -> void:
	if not alive:
		return
	hp -= amount
	Fx.burst(Game.main, global_position + Vector3(0, 0.2, 0), Vector3.UP, Color(1.0, 0.7, 0.3), 5, 3.0, 0.03)
	if hp <= 0.0:
		_stop()


func _stop() -> void:
	alive = false
	remove_from_group("decoys")
	Fx.burst(Game.main, global_position + Vector3(0, 0.2, 0), Vector3.UP, Color(0.4, 0.4, 0.4), 12, 4.0, 0.05)
	var tween := create_tween()
	tween.tween_property(self, "scale", Vector3.ONE * 0.01, 0.3)
	tween.tween_callback(queue_free)


func _cyl(radius: float, height: float) -> CylinderMesh:
	var c := CylinderMesh.new()
	c.top_radius = radius
	c.bottom_radius = radius
	c.height = height
	c.radial_segments = 8
	return c
