class_name Barrier
extends StaticBody3D
## Barrière qui coupe le chemin des zombies : porte fortifiée d'un avant-poste ou barricade
## dépliable (gadget). Les zombies doivent la casser pour passer, pendant que les tours tirent.
## Détruite, elle laisse passer les zombies. Le joueur la relève en maintenant E dessus.

signal destroyed

const REBUILD_RATIO := 0.5  # Une barrière détruite se relève quand elle retrouve 50 % de ses PV.

var display_name := "Barrière"
var ring := ""
var style := "gate"  # "gate" (porte d'avant-poste) ou "barricade" (gadget)
var max_hp := 600.0
var hp := 600.0
var alive := true
var temporary := false
var life := 0.0  # Durée restante d'une barricade.
var dir := Vector3.BACK  # Sens du chemin (vers le Cœur), à plat.
var half_width := 3.6
var progress_value := 0.0  # Avancée sur le chemin (même échelle que Zombie.progress()).

var _parts: Array[Node3D] = []
var _rest: Array[Transform3D] = []
var _fallen: Array[Transform3D] = []
var _label: Label3D
var _alert_cd := 0.0
var _shake := 0.0

static var _fence_tex: ImageTexture


## La position doit déjà être placée. segment = index du tronçon du chemin coupé par la barrière.
func setup(p_name: String, p_ring: String, p_hp: float, segment: int, width: float, p_style := "gate", p_life := 0.0) -> void:
	display_name = p_name
	ring = p_ring
	style = p_style
	max_hp = p_hp
	hp = p_hp
	temporary = p_life > 0.0
	life = p_life
	half_width = width * 0.5
	var pts: Array[Vector3] = Game.main.path_points
	var a := pts[segment]
	var b := pts[segment + 1]
	dir = Vector3(b.x - a.x, 0, b.z - a.z).normalized()
	progress_value = 1000.0 * (segment + 1) - Vector2(global_position.x - b.x, global_position.z - b.z).length()
	# Face avant (côté portail) = -Z local ; la largeur suit l'axe X local.
	rotation.y = atan2(dir.x, dir.z)
	collision_layer = Fx.LAYER_STRUCTURES
	collision_mask = 0
	var shape := CollisionShape3D.new()
	var bs := BoxShape3D.new()
	var height := 2.5 if style == "gate" else 1.2
	bs.size = Vector3(width, height, 0.9)
	shape.shape = bs
	shape.position.y = height * 0.5
	add_child(shape)
	if style == "gate":
		_build_gate(width)
	else:
		_build_barricade(width)
	for part in _parts:
		_rest.append(part.transform)
		# Position une fois effondrée : le grillage et les barbelés tombent à plat,
		# les blocs de béton glissent et pivotent un peu.
		var heavy: bool = part.get_meta("heavy", false)
		var t := part.transform
		var tilt := randf_range(0.15, 0.35) if heavy else randf_range(1.15, 1.5)
		t = t.rotated_local(Vector3.RIGHT, tilt * (1.0 if randf() < 0.5 else -1.0))
		t = t.rotated_local(Vector3.UP, randf_range(-0.5, 0.5))
		t.origin += Vector3(randf_range(-0.3, 0.3), -part.position.y * 0.85, randf_range(-0.6, 0.6))
		_fallen.append(t)
	_label = Label3D.new()
	_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	_label.font_size = 30
	_label.outline_size = 6
	_label.position.y = height + 0.9
	add_child(_label)
	add_to_group("barriers")
	if not temporary:
		add_to_group("gates")
	_refresh()


## Vrai si le point est derrière la barrière (côté Cœur).
func is_behind(p: Vector3) -> bool:
	return (p - global_position).dot(dir) > 0.0


## Distance entre un point situé devant la barrière et sa face avant.
func front_distance(p: Vector3) -> float:
	return -(p - global_position).dot(dir)


## Point de la barrière le plus proche, pour que les zombies se répartissent sur sa largeur.
func contact_point(p: Vector3) -> Vector3:
	var side := Vector3(dir.z, 0, -dir.x)
	var lateral := clampf((p - global_position).dot(side), -half_width + 0.4, half_width - 0.4)
	return global_position + side * lateral


## Vrai si la barrière arrête ce zombie (il ne l'a pas encore passée).
func blocks(z: Zombie) -> bool:
	return alive and z.progress() < progress_value


func take_damage(amount: float) -> void:
	if not alive or Game.is_over:
		return
	hp = maxf(0.0, hp - amount)
	_shake = minf(1.0, _shake + 0.35)
	if randf() < 0.35:
		Sfx.play_at(Game.main, "barrier_hit", global_position + Vector3(0, 1.0, 0), -5.0, 0.2)
	if not temporary and _alert_cd <= 0.0 and hp > 0.0:
		_alert_cd = 25.0
		Game.say("%s attaquée ! Tes tours doivent tenir." % display_name)
	if hp <= 0.0:
		_break()
	_refresh()


## Répare et renvoie les PV réellement rendus.
func repair(amount: float) -> float:
	var before := hp
	hp = minf(max_hp, hp + amount)
	if not alive and hp >= max_hp * REBUILD_RATIO:
		alive = true
		collision_layer = Fx.LAYER_STRUCTURES
		for i in _parts.size():
			var tween := _parts[i].create_tween()
			tween.tween_property(_parts[i], "transform", _rest[i], 0.6).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
		Game.say("%s relevée : les zombies sont de nouveau bloqués." % display_name)
	_refresh()
	return hp - before


func _break() -> void:
	alive = false
	# Les débris ne bloquent plus le passage, mais on peut encore viser la barrière pour la relever.
	collision_layer = Fx.LAYER_INTERACT
	for i in _parts.size():
		var tween := _parts[i].create_tween()
		tween.tween_property(_parts[i], "transform", _fallen[i], randf_range(0.35, 0.7)).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_IN)
	Fx.burst(Game.main, global_position + Vector3(0, 1.0, 0), Vector3.UP, Color(0.55, 0.52, 0.48), 30, 6.0, 0.12)
	Fx.puff(Game.main, global_position + Vector3(0, 0.8, 0), Color(0.45, 0.42, 0.38, 0.6), 18, 1.6)
	Sfx.play_at(Game.main, "barrier_break", global_position + Vector3(0, 1.0, 0), 4.0, 0.1)
	destroyed.emit()
	if temporary:
		Game.say("La barricade a cédé.")
		var tween := create_tween()
		tween.tween_interval(1.2)
		tween.tween_property(self, "scale", Vector3(1, 0.01, 1), 0.4)
		tween.tween_callback(queue_free)
	else:
		Game.say("%s détruite ! Les zombies passent. Maintiens %s dessus pour la relever." % [display_name, Settings.key_label("interact")])


func _process(delta: float) -> void:
	_alert_cd = maxf(0.0, _alert_cd - delta)
	if _shake > 0.0:
		_shake = move_toward(_shake, 0.0, delta * 3.0)
		if alive:
			for i in _parts.size():
				var t := _rest[i]
				t.origin += Vector3(randf_range(-1, 1), 0, randf_range(-1, 1)) * _shake * 0.03
				_parts[i].transform = t
	if temporary and alive:
		life -= delta
		_refresh()
		if life <= 0.0:
			alive = false
			collision_layer = 0
			var tween := create_tween()
			tween.tween_property(self, "scale", Vector3(1, 0.01, 1), 0.5)
			tween.tween_callback(queue_free)


func _refresh() -> void:
	if not is_instance_valid(_label):
		return
	if temporary:
		_label.text = "Barricade  %d PV  %d s" % [int(hp), int(ceil(life))]
	else:
		var state := "" if alive else "  (DÉTRUITE)"
		_label.text = "%s%s\n%d / %d" % [display_name, state, int(hp), int(max_hp)]
	_label.modulate = Color(1, 1, 1) if alive else Color(1, 0.35, 0.3)
	Game.changed.emit()


# ---------------------------------------------------------------- apparence

## Porte d'avant-poste : blocs de béton, grillage, barbelés et barrière rayée rouge et blanche.
func _build_gate(width: float) -> void:
	var concrete := Fx.textured("concrete", 0.6, Color(0.86, 0.84, 0.8))
	var metal := Fx.textured("metal", 1.5, Color(0.55, 0.55, 0.55), false)
	var wire_mat := Fx.material(Color(0.32, 0.32, 0.33))
	wire_mat.metallic = 0.9
	wire_mat.roughness = 0.35
	var reflector := Fx.material(Color(0.9, 0.12, 0.08), 0.6)
	# Blocs de béton type « glissière » posés côte à côte.
	var count := int(ceil(width / 2.2))
	var block_len := width / count
	for i in count:
		var block := Node3D.new()
		block.position = Vector3(-width * 0.5 + block_len * (i + 0.5), 0, -0.05)
		var low := Fx.box_mat(Vector3(block_len - 0.06, 0.45, 0.72), concrete)
		low.position.y = 0.225
		block.add_child(low)
		var high := Fx.box_mat(Vector3(block_len - 0.06, 0.55, 0.32), concrete)
		high.position.y = 0.72
		block.add_child(high)
		var stripe := Fx.box_mat(Vector3(block_len * 0.5, 0.08, 0.02), reflector)
		stripe.position = Vector3(0, 0.85, -0.17)
		block.add_child(stripe)
		block.rotation.y = randf_range(-0.03, 0.03)
		block.set_meta("heavy", true)
		_add_part(block)
	# Grillage tendu entre des poteaux, derrière les blocs.
	var fence := Node3D.new()
	fence.position.z = 0.28
	var posts := int(ceil(width / 1.8)) + 1
	for i in posts:
		var post := Fx.mesh_with(_cyl(0.05, 2.5), metal)
		post.position = Vector3(-width * 0.5 + width * i / (posts - 1), 1.25, 0)
		fence.add_child(post)
	var quad := QuadMesh.new()
	quad.size = Vector2(width, 1.45)
	var mesh := Fx.mesh_with(quad, _fence_material(width, 1.45))
	mesh.position.y = 1.6
	fence.add_child(mesh)
	_add_part(fence)
	# Barbelés en spirale au sommet.
	var wire := Node3D.new()
	wire.position = Vector3(0, 2.62, 0.28)
	var loops := int(width / 0.32)
	for i in loops:
		var torus := TorusMesh.new()
		torus.inner_radius = 0.2
		torus.outer_radius = 0.23
		torus.rings = 12
		torus.ring_segments = 4
		var loop := Fx.mesh_with(torus, wire_mat)
		loop.rotation = Vector3(randf_range(-0.2, 0.2), 0, PI * 0.5 + randf_range(-0.25, 0.25))
		loop.position.x = -width * 0.5 + 0.16 + i * 0.32
		loop.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		wire.add_child(loop)
	_add_part(wire)
	# Barrière levante rayée, devant les blocs.
	var boom := Node3D.new()
	boom.position = Vector3(0, 1.15, -0.55)
	var red := Fx.material(Color(0.75, 0.08, 0.06))
	var white := Fx.material(Color(0.9, 0.9, 0.88))
	var stripes := 10
	var seg_len := (width + 0.4) / stripes
	for i in stripes:
		var seg := Fx.mesh_with(_cyl(0.06, seg_len), red if i % 2 == 0 else white)
		seg.rotation.z = PI * 0.5
		seg.position.x = -width * 0.5 - 0.2 + seg_len * (i + 0.5)
		boom.add_child(seg)
	var stand := Fx.box_mat(Vector3(0.3, 1.15, 0.3), metal)
	stand.position = Vector3(-width * 0.5 - 0.35, -0.575, 0)
	boom.add_child(stand)
	_add_part(boom)


## Barricade dépliable : trois panneaux d'acier jaunes et noirs posés en zigzag.
func _build_barricade(width: float) -> void:
	var steel := Fx.textured("metal", 1.5, Color(0.7, 0.7, 0.7), false)
	var yellow := Fx.material(Color(0.95, 0.72, 0.1))
	yellow.roughness = 0.5
	var black := Fx.material(Color(0.06, 0.06, 0.06))
	var panel_w := width / 3.0
	for i in 3:
		var panel := Node3D.new()
		panel.position = Vector3(-width * 0.5 + panel_w * (i + 0.5), 0, 0.12 * (1 if i % 2 == 0 else -1))
		panel.rotation.y = 0.18 * (1 if i % 2 == 0 else -1)
		var plate := Fx.box_mat(Vector3(panel_w - 0.05, 1.05, 0.06), steel)
		plate.position.y = 0.62
		panel.add_child(plate)
		for k in 4:
			var band := Fx.box_mat(Vector3(0.16, 1.0, 0.065), yellow if k % 2 == 0 else black)
			band.position = Vector3(-panel_w * 0.5 + 0.22 + k * (panel_w - 0.3) / 3.0, 0.62, -0.01)
			band.rotation.z = 0.6
			band.scale = Vector3(1, 0.85, 1)
			panel.add_child(band)
		var foot := Fx.box_mat(Vector3(0.12, 0.08, 0.7), steel)
		foot.position.y = 0.04
		panel.add_child(foot)
		_add_part(panel)


func _add_part(part: Node3D) -> void:
	add_child(part)
	_parts.append(part)


func _fence_material(width: float, height: float) -> StandardMaterial3D:
	if _fence_tex == null:
		# Maillage de grillage dessiné une fois : losanges de fil d'acier sur fond transparent.
		var img := Image.create(64, 64, false, Image.FORMAT_RGBA8)
		img.fill(Color(0, 0, 0, 0))
		for i in 64:
			for w in 3:
				var c := Color(0.62, 0.62, 0.6, 1.0 if w == 1 else 0.6)
				img.set_pixel((i + w) % 64, i, c)
				img.set_pixel((63 - i + w) % 64, i, c)
		_fence_tex = ImageTexture.create_from_image(img)
	var mat := StandardMaterial3D.new()
	mat.albedo_texture = _fence_tex
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_HASH
	mat.cull_mode = BaseMaterial3D.CULL_DISABLED
	mat.metallic = 0.7
	mat.roughness = 0.45
	mat.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR
	mat.uv1_scale = Vector3(width / 0.45, height / 0.45, 1)
	return mat


func _cyl(radius: float, height: float) -> CylinderMesh:
	var c := CylinderMesh.new()
	c.top_radius = radius
	c.bottom_radius = radius
	c.height = height
	c.radial_segments = 10
	return c
