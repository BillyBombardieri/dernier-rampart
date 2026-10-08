class_name Flicker
extends OmniLight3D
## Lumière qui vacille (feu de baril, néon de secours).

var base_energy := 2.0
var speed := 12.0
var amount := 0.35
var _t := randf() * 100.0


func _process(delta: float) -> void:
	_t += delta * speed
	light_energy = base_energy * (1.0 + amount * (sin(_t) * 0.6 + sin(_t * 2.7) * 0.4) * randf_range(0.8, 1.0))
