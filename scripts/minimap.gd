class_name Minimap
extends Control
## Minimap ronde qui tourne avec le joueur : couloir, ancrages, tours, relais, Cœur, portail, zombies et ferraille.

const RADIUS := 95.0
const METERS := 55.0  # Distance visible du centre au bord.

var _pulse := 0.0


func _ready() -> void:
	custom_minimum_size = Vector2(RADIUS * 2, RADIUS * 2)
	size = custom_minimum_size
	mouse_filter = Control.MOUSE_FILTER_IGNORE


func _process(delta: float) -> void:
	_pulse += delta
	queue_redraw()


func _draw() -> void:
	var center := Vector2(RADIUS, RADIUS)
	draw_circle(center, RADIUS, Color(0.04, 0.05, 0.07, 0.78))
	var player := Game.player as Player
	if player == null or Game.main == null:
		return
	var px := RADIUS / METERS
	var yaw := player.global_rotation.y
	var right := Vector3(cos(yaw), 0, -sin(yaw))
	var fwd := Vector3(-sin(yaw), 0, -cos(yaw))
	var origin := player.global_position
	var to_map := func(p: Vector3) -> Vector2:
		var rel := p - origin
		return center + Vector2(rel.dot(right), -rel.dot(fwd)) * px

	# Couloir des zombies.
	var pts: Array[Vector3] = Game.main.path_points
	for i in pts.size() - 1:
		var steps := int(pts[i].distance_to(pts[i + 1]))
		for k in steps:
			var a: Vector2 = to_map.call(pts[i].lerp(pts[i + 1], float(k) / steps))
			var b: Vector2 = to_map.call(pts[i].lerp(pts[i + 1], float(k + 1) / steps))
			if a.distance_to(center) < RADIUS - 3 and b.distance_to(center) < RADIUS - 3:
				draw_line(a, b, Color(0.55, 0.4, 0.25, 0.9), 4.0)

	for node in get_tree().get_nodes_in_group("scrap"):
		_dot(to_map.call(node.global_position), 1.5, Color(1.0, 0.8, 0.3), center)
	for node in Game.main.get_children():
		if node is Socket:
			var s := node as Socket
			var col := Color(0.5, 0.5, 0.5)
			if s.tower:
				col = Color(1.0, 0.65, 0.2) if s.tower.type == "gun" else Color(0.4, 0.85, 1.0)
				if not s.tower.is_active():
					col = col.darkened(0.6)
			_square(to_map.call(s.global_position), 3.5 if s.tower else 2.5, col, center)
		elif node is Structure:
			var st := node as Structure
			var col := Color(0.35, 0.65, 1.0) if st.kind == "core" else Color(1.0, 0.85, 0.3)
			if not st.alive:
				col = Color(0.4, 0.4, 0.4)
			_square(to_map.call(st.global_position), 6.0 if st.kind == "core" else 4.0, col, center, true)
	for node in get_tree().get_nodes_in_group("zombies"):
		var z := node as Zombie
		var r := 2.5 if z.type in ["rodeur", "coureur"] else 4.5
		_dot(to_map.call(z.global_position), r, Color(1.0, 0.3, 0.25) if not z.marked else Color(1, 1, 1), center)

	# Portail : toujours visible, collé au bord s'il est loin.
	var portal: Vector2 = to_map.call(pts[0])
	var off := portal - center
	if off.length() > RADIUS - 10:
		portal = center + off.normalized() * (RADIUS - 10)
	var glow := 0.6 + 0.4 * sin(_pulse * (6.0 if Game.phase == "assault" else 2.5))
	draw_circle(portal, 8.0, Color(0.9, 0.1, 0.05, 0.35 * glow + 0.2))
	draw_circle(portal, 5.0, Color(1.0, 0.2, 0.1))
	draw_string(get_theme_default_font(), portal + Vector2(-3, 5), "!", HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Color.WHITE)

	# Nord.
	var n_dir := Vector2(Vector3(0, 0, -1).dot(right), -Vector3(0, 0, -1).dot(fwd))
	var north := center + n_dir.normalized() * (RADIUS - 9)
	draw_string(get_theme_default_font(), north + Vector2(-5, 5), "N", HORIZONTAL_ALIGNMENT_LEFT, -1, 13, Color(0.85, 0.85, 0.85))

	# Joueur au centre (flèche vers le haut = là où tu regardes).
	draw_colored_polygon(PackedVector2Array([center + Vector2(0, -8), center + Vector2(6, 6), center + Vector2(0, 3), center + Vector2(-6, 6)]), Color.WHITE)
	draw_arc(center, RADIUS - 1, 0, TAU, 64, Color(1, 1, 1, 0.35), 2.0, true)


func _dot(p: Vector2, r: float, col: Color, center: Vector2) -> void:
	if p.distance_to(center) < RADIUS - r - 2:
		draw_circle(p, r, col)


func _square(p: Vector2, half: float, col: Color, center: Vector2, diamond := false) -> void:
	if p.distance_to(center) >= RADIUS - half - 2:
		return
	if diamond:
		draw_colored_polygon(PackedVector2Array([p + Vector2(0, -half), p + Vector2(half, 0), p + Vector2(0, half), p + Vector2(-half, 0)]), col)
	else:
		draw_rect(Rect2(p - Vector2(half, half), Vector2(half, half) * 2), col)
