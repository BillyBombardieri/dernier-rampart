class_name Workbench
extends StaticBody3D
## Établi près du Cœur : on y améliore ses armes, ses munitions, ses gadgets et le générateur,
## avec de la ferraille. Il n'est utilisable qu'entre les vagues.


func _ready() -> void:
	collision_layer = Fx.LAYER_STRUCTURES | Fx.LAYER_WORLD
	collision_mask = 0
	var shape := CollisionShape3D.new()
	var bs := BoxShape3D.new()
	bs.size = Vector3(2.4, 1.0, 1.1)
	shape.shape = bs
	shape.position.y = 0.5
	add_child(shape)
	var wood := Fx.textured("cloth", 2.0, Color(0.55, 0.4, 0.25), false)
	var metal := Fx.textured("metal", 1.5, Color(0.7, 0.7, 0.7), false)
	var dark := Fx.textured("metal", 1.5, Color(0.35, 0.35, 0.35), false)
	# Plateau épais sur pieds en acier.
	var top := Fx.box_mat(Vector3(2.4, 0.1, 1.0), wood)
	top.position.y = 0.95
	add_child(top)
	for x in [-1.1, 1.1]:
		for z in [-0.42, 0.42]:
			var leg := Fx.box_mat(Vector3(0.08, 0.9, 0.08), dark)
			leg.position = Vector3(x, 0.45, z)
			add_child(leg)
	var shelf := Fx.box_mat(Vector3(2.2, 0.05, 0.85), dark)
	shelf.position.y = 0.3
	add_child(shelf)
	# Panneau à outils derrière, avec quelques outils et une arme en cours de montage.
	var board := Fx.box_mat(Vector3(2.4, 1.3, 0.06), wood)
	board.position = Vector3(0, 1.75, 0.5)
	add_child(board)
	for i in 6:
		var tool := Fx.box_mat(Vector3(0.05, randf_range(0.25, 0.5), 0.04), metal)
		tool.position = Vector3(-0.9 + i * 0.36, 1.85, 0.45)
		tool.rotation.z = randf_range(-0.3, 0.3)
		add_child(tool)
	var vise := Fx.box_mat(Vector3(0.25, 0.2, 0.3), dark)
	vise.position = Vector3(-0.85, 1.1, -0.1)
	add_child(vise)
	var gun := Fx.box_mat(Vector3(0.7, 0.09, 0.12), Fx.material(Color(0.12, 0.12, 0.13)))
	gun.position = Vector3(0.2, 1.05, -0.05)
	gun.rotation.y = 0.3
	add_child(gun)
	var mag := Fx.box_mat(Vector3(0.05, 0.2, 0.08), Fx.material(Color(0.1, 0.1, 0.1)))
	mag.position = Vector3(0.55, 1.05, 0.2)
	mag.rotation.x = PI * 0.5
	add_child(mag)
	for i in 3:
		var shell := Fx.mesh_with(_cyl(0.015, 0.06), Fx.material(Color(0.8, 0.6, 0.25)))
		shell.position = Vector3(0.7 + i * 0.05, 1.03, -0.25)
		add_child(shell)
	# Lampe d'atelier chaude.
	var lamp := Fx.box(Vector3(0.3, 0.08, 0.15), Color(1.0, 0.85, 0.6), 4.0)
	lamp.position = Vector3(0, 2.5, 0.35)
	add_child(lamp)
	var light := OmniLight3D.new()
	light.light_color = Color(1.0, 0.82, 0.55)
	light.light_energy = 2.2
	light.omni_range = 5.0
	light.shadow_enabled = true
	light.position = Vector3(0, 2.3, 0.1)
	add_child(light)
	var sign_label := Label3D.new()
	sign_label.text = "ÉTABLI"
	sign_label.font_size = 48
	sign_label.outline_size = 10
	sign_label.modulate = Color(0.55, 1.0, 0.65)
	sign_label.billboard = BaseMaterial3D.BILLBOARD_FIXED_Y
	sign_label.position = Vector3(0, 2.85, 0.4)
	add_child(sign_label)
	add_to_group("workbench")


func _cyl(radius: float, height: float) -> CylinderMesh:
	var c := CylinderMesh.new()
	c.top_radius = radius
	c.bottom_radius = radius
	c.height = height
	c.radial_segments = 8
	return c
