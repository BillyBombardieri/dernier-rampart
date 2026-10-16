class_name GroundCover
extends RefCounted
## Couverture du sol : touffes d'herbe sèche qui ondulent au vent et cailloux (ou gravats) semés
## sur toute la carte, avec plus de cailloux au bord du chemin pour casser ses bords trop nets.
## Réglée par chapitre (constante "cover" de Levels.CHAPTERS). Tout est dessiné en MultiMesh,
## découpé en carrés de 40 m qui disparaissent au loin : quelques appels de dessin seulement.

const CHUNK := 40.0
const HALF := 80.0  # La couverture va de -80 à +80 m autour du centre.
const VIEW_END := 85.0  # Distance où l'herbe s'efface (fondu).

const GRASS_SHADER := """
shader_type spatial;
render_mode cull_disabled;

uniform float sway = 0.12;

varying float tip;

void vertex() {
	tip = UV.y;
	vec3 world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	float t = TIME * 1.6 + world.x * 0.35 + world.z * 0.27;
	float gust = sin(t) * 0.6 + sin(t * 2.3 + 1.7) * 0.4;
	VERTEX.x += gust * sway * tip * tip;
	VERTEX.z += cos(t * 0.8) * sway * 0.5 * tip * tip;
	// Normales vers le haut : l'herbe prend la lumière comme le sol, sans faces noires.
	NORMAL = vec3(0.0, 1.0, 0.0);
}

void fragment() {
	ALBEDO = pow(COLOR.rgb, vec3(2.2));  // Couleurs des sommets données en sRGB.
	ROUGHNESS = 0.92;
	SPECULAR = 0.2;
	BACKLIGHT = vec3(0.25) * tip;
	AO = mix(0.75, 1.0, tip);
	AO_LIGHT_AFFECT = 0.6;
}
"""


static func build(main: Node3D, cover: Dictionary, path: Array[Vector3], avoid: Array, rng: RandomNumberGenerator) -> void:
	if cover.is_empty():
		return
	var grass_mat := ShaderMaterial.new()
	var shader := Shader.new()
	shader.code = GRASS_SHADER
	grass_mat.shader = shader
	grass_mat.set_shader_parameter("sway", float(cover.get("sway", 0.12)))
	var tuft := _tuft_mesh(cover, rng)
	tuft.surface_set_material(0, grass_mat)
	var stone := _stone_mesh(cover)
	# Positions, rangées par carré de 40 m.
	var grass_cells := {}
	var stone_cells := {}
	var tries := 0
	var placed := 0
	var want := int(cover.get("grass", 0))
	# L'herbe pousse en plaques : un centre, puis quelques touffes serrées autour.
	var center := Vector3.ZERO
	var left := 0
	while placed < want and tries < want * 4:
		tries += 1
		if left <= 0:
			center = Vector3(rng.randf_range(-HALF, HALF), 0, rng.randf_range(-HALF, HALF))
			left = rng.randi_range(3, 9)
		left -= 1
		var p := center + Vector3(rng.randfn(0.0, 0.9), 0, rng.randfn(0.0, 0.9))
		if _near_path(p, path) < 2.7 or _blocked(p, avoid):
			continue
		placed += 1
		var s := rng.randf_range(0.6, 1.25)
		var t := Transform3D(Basis(Vector3.UP, rng.randf() * TAU).scaled(Vector3(s, s * rng.randf_range(0.8, 1.3), s)), p)
		_add(grass_cells, p, t)
	# Cailloux : partout, et serrés le long des bords du chemin.
	var stones := int(cover.get("stones", 0))
	placed = 0
	tries = 0
	while placed < stones and tries < stones * 4:
		tries += 1
		var p: Vector3
		if rng.randf() < 0.45 and path.size() > 1:
			var i := rng.randi_range(0, path.size() - 2)
			var a: Vector3 = path[i]
			var b: Vector3 = path[i + 1]
			var along := a.lerp(b, rng.randf())
			var dir := (b - a).normalized()
			var side := Vector3(-dir.z, 0, dir.x) * (rng.randf_range(1.9, 3.2) * (1 if rng.randf() < 0.5 else -1))
			p = along + side
		else:
			p = Vector3(rng.randf_range(-HALF, HALF), 0, rng.randf_range(-HALF, HALF))
			if _near_path(p, path) < 2.2:
				continue
		if _blocked(p, avoid):
			continue
		placed += 1
		var s := rng.randf_range(0.5, 2.4) * (2.0 if rng.randf() < 0.06 else 1.0)
		var basis := Basis.from_euler(Vector3(rng.randf_range(-0.4, 0.4), rng.randf() * TAU, rng.randf_range(-0.4, 0.4)))
		basis = basis.scaled(Vector3(s * rng.randf_range(0.7, 1.3), s * rng.randf_range(0.5, 0.9), s))
		_add(stone_cells, p, Transform3D(basis, p + Vector3(0, -0.02 * s, 0)))
	for key in grass_cells:
		_flush(main, tuft, grass_cells[key], false)
	for key in stone_cells:
		_flush(main, stone, stone_cells[key], true)


static func _add(cells: Dictionary, p: Vector3, t: Transform3D) -> void:
	var key := Vector2i(floori(p.x / CHUNK), floori(p.z / CHUNK))
	if not cells.has(key):
		cells[key] = []
	cells[key].append(t)


static func _flush(main: Node3D, mesh: Mesh, transforms: Array, shadows: bool) -> void:
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.mesh = mesh
	mm.instance_count = transforms.size()
	for i in transforms.size():
		mm.set_instance_transform(i, transforms[i])
	var mi := MultiMeshInstance3D.new()
	mi.multimesh = mm
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON if shadows else GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	mi.visibility_range_end = VIEW_END
	mi.visibility_range_end_margin = 10.0
	mi.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
	main.add_child(mi)


## Une touffe : 12 brins (triangles fins), plus foncés au pied qu'à la pointe.
static func _tuft_mesh(cover: Dictionary, rng: RandomNumberGenerator) -> ArrayMesh:
	var base: Color = cover.get("grass_base", Color(0.25, 0.22, 0.12))
	var top: Color = cover.get("grass_tip", Color(0.6, 0.52, 0.3))
	var hmin: float = cover.get("grass_h", Vector2(0.25, 0.55)).x
	var hmax: float = cover.get("grass_h", Vector2(0.25, 0.55)).y
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var blades := 12
	for i in blades:
		var ang := TAU * i / blades + rng.randf_range(-0.3, 0.3)
		var r := rng.randf_range(0.0, 0.16)
		var root := Vector3(cos(ang) * r, 0, sin(ang) * r)
		var h := rng.randf_range(hmin, hmax)
		var lean := Vector3(cos(ang), 0, sin(ang)) * h * rng.randf_range(0.15, 0.45)
		var w := rng.randf_range(0.025, 0.045)
		var across := Vector3(-sin(ang), 0, cos(ang)) * w
		var tip_col := top.lerp(base, rng.randf_range(0.0, 0.35))
		for v in [[root - across, base, 0.0], [root + across, base, 0.0], [root + lean + Vector3(0, h, 0), tip_col, 1.0]]:
			st.set_color(v[1])
			st.set_uv(Vector2(0.5, v[2]))
			st.set_normal(Vector3.UP)
			st.add_vertex(v[0])
	return st.commit()


## Un caillou rond et bosselé, ou un éclat de béton (Ville Morte).
static func _stone_mesh(cover: Dictionary) -> Mesh:
	var tint: Color = cover.get("stone_tint", Color(0.5, 0.48, 0.45))
	var mat := Fx.textured("concrete", 2.0, tint, false)
	if cover.get("stone_shape", "round") == "box":
		var box := BoxMesh.new()
		box.size = Vector3(0.12, 0.07, 0.09)
		box.material = mat
		return box
	var sphere := SphereMesh.new()
	sphere.radius = 0.06
	sphere.height = 0.08
	sphere.radial_segments = 7
	sphere.rings = 3
	sphere.material = mat
	return sphere


static func _near_path(p: Vector3, path: Array[Vector3]) -> float:
	var best := INF
	var p2 := Vector2(p.x, p.z)
	for i in path.size() - 1:
		var a := Vector2(path[i].x, path[i].z)
		var b := Vector2(path[i + 1].x, path[i + 1].z)
		var ab := b - a
		var t := clampf((p2 - a).dot(ab) / maxf(ab.length_squared(), 0.001), 0.0, 1.0)
		best = minf(best, p2.distance_to(a + ab * t))
	return best


## Rien sur les ancrages, les barrières, ni dans la base autour du Cœur.
static func _blocked(p: Vector3, avoid: Array) -> bool:
	for a in avoid:
		if Vector2(p.x - a[0].x, p.z - a[0].z).length() < a[1]:
			return true
	return false
