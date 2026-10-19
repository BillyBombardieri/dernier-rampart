class_name Buildings
extends RefCounted
## Bâtiments construits par le code, pensés pour paraître vrais : immeubles éventrés de la Ville
## Morte (vraies fenêtres en creux, cadres, vitres cassées, planches, rideaux de fer, balcons,
## corniches, toit arraché, gravats), ruines du Col Gelé (murs au sommet effondré en escalier),
## murs anti-souffle en béton préfabriqué autour du Cœur, et le bunker du Cœur.
## Chaque bâtiment est un seul maillage, une surface par matériau (Kit), avec des matériaux
## vieillis (weathered.gdshader) : pied des murs sali, coulures, et selon le chapitre neige,
## sable ou pluie (climate, réglé par main.gd).

const FLOOR_H := 3.2  # Hauteur d'un étage.
const GROUND_H := 3.6  # Rez-de-chaussée, plus haut (boutiques).
const WALL := 0.3  # Épaisseur des murs de façade.

static var climate := {}  # Paramètres du shader propres au chapitre : {"wet": 0.6} sous la pluie...
static var _mats := {}
static var _noise: NoiseTexture2D
static var _unit_box: BoxMesh
static var _unit_cyl: CylinderMesh


## Assemble des boîtes et des cylindres en un seul maillage : une surface par matériau.
class Kit:
	var _tools := {}

	func box(mat: Material, center: Vector3, size: Vector3, basis := Basis.IDENTITY) -> void:
		_add(mat, Buildings._box_mesh(), Transform3D(basis * Basis.from_scale(size), center))

	## Cylindre vertical (axe Y) de rayon r et de hauteur h ; basis pour le coucher.
	func cyl(mat: Material, center: Vector3, r: float, h: float, basis := Basis.IDENTITY) -> void:
		_add(mat, Buildings._cyl_mesh(), Transform3D(basis * Basis.from_scale(Vector3(r * 2.0, h, r * 2.0)), center))

	func _add(mat: Material, mesh: Mesh, xf: Transform3D) -> void:
		var st: SurfaceTool = _tools.get(mat)
		if st == null:
			st = SurfaceTool.new()
			st.begin(Mesh.PRIMITIVE_TRIANGLES)
			_tools[mat] = st
		st.append_from(mesh, 0, xf)

	func mesh() -> ArrayMesh:
		var out := ArrayMesh.new()
		for mat in _tools:
			var st: SurfaceTool = _tools[mat]
			st.set_material(mat)
			st.commit(out)
		return out

	func instance() -> MeshInstance3D:
		var mi := MeshInstance3D.new()
		mi.mesh = mesh()
		return mi


static func _box_mesh() -> BoxMesh:
	if _unit_box == null:
		_unit_box = BoxMesh.new()
	return _unit_box


static func _cyl_mesh() -> CylinderMesh:
	if _unit_cyl == null:
		_unit_cyl = CylinderMesh.new()
		_unit_cyl.top_radius = 0.5
		_unit_cyl.bottom_radius = 0.5
		_unit_cyl.height = 1.0
		_unit_cyl.radial_segments = 12
		_unit_cyl.rings = 1
	return _unit_cyl


## Matériau vieilli d'une texture d'assets/textures (enduit, briques, béton...), partagé.
static func weathered(tex: String, scale: float, tint := Color.WHITE, opts := {}) -> ShaderMaterial:
	var all := climate.duplicate()
	all.merge(opts, true)
	var key := "%s %s %s %s" % [tex, scale, tint, all]
	if _mats.has(key):
		return _mats[key]
	var m := ShaderMaterial.new()
	m.shader = preload("res://scripts/weathered.gdshader")
	m.set_shader_parameter("albedo_tex", load("res://assets/textures/%s_albedo.png" % tex))
	m.set_shader_parameter("normal_tex", load("res://assets/textures/%s_normal.png" % tex))
	m.set_shader_parameter("rough_tex", load("res://assets/textures/%s_roughness.png" % tex))
	m.set_shader_parameter("noise_tex", _noise_tex())
	m.set_shader_parameter("uv_scale", scale)
	m.set_shader_parameter("tint", tint)
	for k in all:
		m.set_shader_parameter(k, all[k])
	_mats[key] = m
	return m


static func _noise_tex() -> NoiseTexture2D:
	if _noise == null:
		var n := FastNoiseLite.new()
		n.noise_type = FastNoiseLite.TYPE_SIMPLEX_SMOOTH
		n.frequency = 0.015
		n.fractal_octaves = 4
		n.seed = 5
		_noise = NoiseTexture2D.new()
		_noise.width = 256
		_noise.height = 256
		_noise.seamless = true
		_noise.generate_mipmaps = true
		_noise.noise = n
	return _noise


static func _plain(color: Color, rough := 0.8, metal := 0.0, emission := 0.0) -> StandardMaterial3D:
	var key := "uni %s %s %s %s" % [color, rough, metal, emission]
	if not _mats.has(key):
		var m := Fx.material(color, emission)
		m.roughness = rough
		m.metallic = metal
		_mats[key] = m
	return _mats[key]


static func _body(size: Vector3, center: Vector3) -> StaticBody3D:
	var body := StaticBody3D.new()
	body.collision_layer = Fx.LAYER_WORLD
	body.collision_mask = 0
	var shape := CollisionShape3D.new()
	var bs := BoxShape3D.new()
	bs.size = size
	shape.shape = bs
	shape.position = center
	body.add_child(shape)
	return body


# ---------------------------------------------------------------- immeuble (Ville Morte)

## Immeuble éventré de w x d mètres et floors niveaux, posé au sol (origine au pied, au centre).
static func city_building(rng: RandomNumberGenerator, w: float, d: float, floors: int) -> StaticBody3D:
	var h := GROUND_H + (floors - 1) * FLOOR_H
	var kit := Kit.new()
	var brick := rng.randf() < 0.4
	var tints := [Color(0.92, 0.86, 0.74), Color(0.78, 0.78, 0.76), Color(0.9, 0.78, 0.66), Color(0.7, 0.72, 0.7)]
	var m := {
		"wall": weathered("brick", 0.8, Color(0.9, 0.85, 0.82)) if brick else weathered("plaster", 0.35, tints[rng.randi() % tints.size()]),
		"trim": weathered("beton", 0.5, Color(0.85, 0.84, 0.8), {"grime": 0.3}),
		"plinth": weathered("beton", 0.5, Color(0.55, 0.54, 0.52)),
		"inside": weathered("plaster", 0.35, Color(0.3, 0.29, 0.28), {"grime": 0.8, "streaks": 0.0}),
		"slab": weathered("beton", 0.5, Color(0.6, 0.59, 0.57), {"grime": 0.0}),
		"glass": _plain(Color(0.05, 0.06, 0.07), 0.06, 0.4),
		"frame": _plain(Color(0.12, 0.11, 0.1), 0.6, 0.3),
		"wood": weathered("plaster", 0.6, Color(0.42, 0.3, 0.2), {"grime": 0.0, "streaks": 0.2}),
		"metal": Fx.textured("metal", 0.7, Color(0.62, 0.6, 0.56)),
		"lit": _plain(Color(0.9, 0.55, 0.28), 0.8, 0.0, 0.7),
		"dark": _plain(Color(0.025, 0.025, 0.03), 0.9),
	}
	# Quatre façades : devant et derrière sur toute la largeur, les côtés entre les deux.
	var sides := [
		[Transform3D(Basis.IDENTITY, Vector3(0, 0, d * 0.5)), w, true],
		[Transform3D(Basis(Vector3.UP, PI), Vector3(0, 0, -d * 0.5)), w, true],
		[Transform3D(Basis(Vector3.UP, PI * 0.5), Vector3(w * 0.5, 0, 0)), d - WALL * 2.0, false],
		[Transform3D(Basis(Vector3.UP, -PI * 0.5), Vector3(-w * 0.5, 0, 0)), d - WALL * 2.0, false],
	]
	for s in sides:
		_facade(kit, rng, m, s[0], s[1], floors, s[2])
	# Dedans : planchers, un refend qui coupe la vue, toit en partie arraché.
	for f in range(1, floors):
		var y := GROUND_H + (f - 1) * FLOOR_H
		kit.box(m["slab"], Vector3(0, y - 0.12, 0), Vector3(w - WALL * 2.0, 0.25, d - WALL * 2.0))
	kit.box(m["inside"], Vector3(0, h * 0.5, 0), Vector3(w - WALL * 2.0, h, 0.2))
	var roof := rng.randf_range(0.35, 0.8)
	var depth := (d - WALL * 2.0) * roof
	kit.box(m["slab"], Vector3(0, h - 0.12, -(d - WALL * 2.0) * 0.5 + depth * 0.5), Vector3(w - WALL * 2.0, 0.25, depth))
	var hang := Basis(Vector3.RIGHT, rng.randf_range(0.4, 0.8))
	kit.box(m["slab"], Vector3(rng.randf_range(-w, w) * 0.2, h - 1.0, -(d - WALL * 2.0) * 0.5 + depth + 0.6), Vector3(w * 0.4, 0.22, 2.2), hang)
	# Corniche et acrotère, cassé par endroits.
	for s in sides:
		var xf: Transform3D = s[0]
		var length: float = s[1]
		kit.box(m["trim"], xf * Vector3(0, h + 0.15, 0.05), Vector3(length + 0.2, 0.3, 0.45), xf.basis)
		var x := -length * 0.5
		while x < length * 0.5 - 0.5:
			var seg := minf(rng.randf_range(1.5, 4.0), length * 0.5 - x)
			if rng.randf() < 0.8:
				var ph := rng.randf_range(0.7, 1.0)
				kit.box(m["wall"], xf * Vector3(x + seg * 0.5, h + 0.3 + ph * 0.5, -0.15), Vector3(seg, ph, 0.25), xf.basis)
			x += seg
	# Gouttière, blocs de climatisation sur le toit, gravats au pied.
	kit.box(m["frame"], Vector3(-w * 0.5 + 0.25, h * 0.5, d * 0.5 + 0.08), Vector3(0.1, h, 0.1))
	for i in rng.randi_range(1, 2):
		kit.box(m["metal"], Vector3(rng.randf_range(-w, w) * 0.3, h + 0.35, -d * 0.35 + i * 1.4), Vector3(1.1, 0.7, 0.7))
	for i in rng.randi_range(5, 10):
		var side := 1.0 if rng.randf() < 0.5 else -1.0
		var c := Vector3(rng.randf_range(-w, w) * 0.5, 0.1, side * (d * 0.5 + rng.randf_range(0.3, 1.6)))
		var sz := Vector3(rng.randf_range(0.3, 0.9), rng.randf_range(0.15, 0.45), rng.randf_range(0.3, 0.8))
		var b := Basis.from_euler(Vector3(rng.randf_range(-0.3, 0.3), rng.randf() * TAU, rng.randf_range(-0.3, 0.3)))
		kit.box(m["trim"] if rng.randf() < 0.6 else m["wall"], c, sz, b)
	var body := _body(Vector3(w, h, d), Vector3(0, h * 0.5, 0))
	body.add_child(kit.instance())
	return body


## Une façade de longueur length dans son repère (x le long du mur, z vers l'extérieur, le mur
## entre z = -WALL et 0) : rez-de-chaussée (boutiques sur les grandes façades), puis à chaque
## étage un allège, des trumeaux entre les fenêtres et un linteau.
static func _facade(kit: Kit, rng: RandomNumberGenerator, m: Dictionary, xf: Transform3D, length: float, floors: int, shops: bool) -> void:
	var t := WALL
	# Soubassement en béton.
	_part(kit, m["plinth"], xf, Vector3(0, 0.25, -t * 0.5 + 0.02), Vector3(length, 0.5, t + 0.04))
	# Rez-de-chaussée.
	if shops:
		var bays := maxi(1, int(length / 3.6))
		var bw := length / bays
		var ow := bw - 0.8
		var oh := 2.8
		_row(kit, m["wall"], xf, length, 0.5, oh - 0.5, bays, ow, t)
		_part(kit, m["wall"], xf, Vector3(0, (oh + GROUND_H) * 0.5, -t * 0.5), Vector3(length, GROUND_H - oh, t))
		for i in bays:
			var xc := -length * 0.5 + (i + 0.5) * bw
			_shop(kit, rng, m, xf, xc, ow, oh, t)
	else:
		_part(kit, m["wall"], xf, Vector3(0, (0.5 + GROUND_H) * 0.5, -t * 0.5), Vector3(length, GROUND_H - 0.5, t))
	_part(kit, m["trim"], xf, Vector3(0, GROUND_H, 0.03), Vector3(length, 0.22, 0.08))
	# Étages.
	var bays := maxi(1, int(length / 2.6))
	var bw := length / bays
	var ww := minf(1.3, bw - 0.9)
	var wh := 1.6
	var sill := 0.85
	for f in range(1, floors):
		var y0 := GROUND_H + (f - 1) * FLOOR_H
		_part(kit, m["wall"], xf, Vector3(0, y0 + sill * 0.5, -t * 0.5), Vector3(length, sill, t))
		var top := y0 + sill + wh
		_part(kit, m["wall"], xf, Vector3(0, (top + y0 + FLOOR_H) * 0.5, -t * 0.5), Vector3(length, y0 + FLOOR_H - top, t))
		_row(kit, m["wall"], xf, length, y0 + sill, wh, bays, ww, t)
		if f > 1:
			_part(kit, m["trim"], xf, Vector3(0, y0, 0.03), Vector3(length, 0.16, 0.06))
		for i in bays:
			var xc := -length * 0.5 + (i + 0.5) * bw
			_window(kit, rng, m, xf, Vector3(xc, y0 + sill, 0), ww, wh, t)
			if shops and f >= 2 and rng.randf() < 0.25:
				_balcony(kit, m, xf, Vector3(xc, y0, 0), ww)


## Une boîte posée dans le repère xf (une façade, une fenêtre).
static func _part(kit: Kit, mat: Material, xf: Transform3D, c: Vector3, size: Vector3, extra := Basis.IDENTITY) -> void:
	kit.box(mat, xf * c, size, xf.basis * extra)


## Trumeaux d'une rangée d'ouvertures : les pans de mur entre les fenêtres, de y0 à y0 + oh.
static func _row(kit: Kit, mat: Material, xf: Transform3D, length: float, y0: float, oh: float, bays: int, ow: float, t: float) -> void:
	var bw := length / bays
	var start := -length * 0.5
	for i in bays + 1:
		var end := length * 0.5 if i == bays else -length * 0.5 + (i + 0.5) * bw - ow * 0.5
		if end - start > 0.01:
			kit.box(mat, xf * Vector3((start + end) * 0.5, y0 + oh * 0.5, -t * 0.5), Vector3(end - start, oh, t), xf.basis)
		start = end + ow


## Fenêtre en creux : appui saillant, cadre en retrait ; vitre intacte, cassée (un éclat
## resté dans le coin), condamnée par des planches, ou éclairée de l'intérieur.
static func _window(kit: Kit, rng: RandomNumberGenerator, m: Dictionary, xf: Transform3D, at: Vector3, ww: float, wh: float, t: float) -> void:
	var w := Transform3D(xf.basis, xf * at)
	_part(kit, m["trim"], w, Vector3(0, -0.04, 0.03), Vector3(ww + 0.24, 0.08, 0.16))
	var z := -t * 0.65
	var f := 0.06
	_part(kit, m["frame"], w, Vector3(0, f * 0.5, z), Vector3(ww, f, 0.08))
	_part(kit, m["frame"], w, Vector3(0, wh - f * 0.5, z), Vector3(ww, f, 0.08))
	_part(kit, m["frame"], w, Vector3(-ww * 0.5 + f * 0.5, wh * 0.5, z), Vector3(f, wh, 0.08))
	_part(kit, m["frame"], w, Vector3(ww * 0.5 - f * 0.5, wh * 0.5, z), Vector3(f, wh, 0.08))
	_part(kit, m["frame"], w, Vector3(0, wh * 0.62, z), Vector3(ww, 0.05, 0.07))
	var roll := rng.randf()
	if roll < 0.06:  # Une lueur chaude derrière un rideau tiré à moitié : quelqu'un, encore ?
		_part(kit, m["lit"], w, Vector3(0, wh * 0.5, z - 0.3), Vector3(ww - f, wh - f, 0.01))
		_part(kit, m["wood"], w, Vector3(-ww * 0.22, wh * 0.5, z - 0.12), Vector3(ww * 0.5, wh - f, 0.02))
	elif roll < 0.42:
		_part(kit, m["glass"], w, Vector3(0, wh * 0.5, z), Vector3(ww - f, wh - f, 0.01))
	elif roll < 0.58:  # Planches clouées en travers.
		for k in rng.randi_range(2, 4):
			var tilt := Basis(Vector3.BACK, rng.randf_range(-0.25, 0.25))
			_part(kit, m["wood"], w, Vector3(rng.randf_range(-0.05, 0.05), wh * (0.2 + 0.22 * k), 0.03), Vector3(ww + 0.3, 0.16, 0.03), tilt)
	else:  # Vitre brisée : il ne reste qu'un éclat dans un coin, le noir derrière.
		var shard := Basis(Vector3.BACK, rng.randf_range(0.5, 1.0))
		_part(kit, m["glass"], w, Vector3(-ww * 0.3, wh * 0.2, z), Vector3(ww * 0.35, wh * 0.18, 0.01), shard)


## Vitrine du rez-de-chaussée : rideau de fer baissé (parfois à moitié levé sur le noir de la
## boutique) avec son coffre, ou vitrine éventrée.
static func _shop(kit: Kit, rng: RandomNumberGenerator, m: Dictionary, xf: Transform3D, xc: float, ow: float, oh: float, t: float) -> void:
	_part(kit, m["metal"], xf, Vector3(xc, oh + 0.17, -0.05), Vector3(ow + 0.1, 0.34, 0.36))
	if rng.randf() < 0.65:
		var lift := 0.0 if rng.randf() < 0.6 else rng.randf_range(0.6, 1.4)
		var hgt := oh - 0.5 - lift
		_part(kit, m["metal"], xf, Vector3(xc, 0.5 + lift + hgt * 0.5, -0.1), Vector3(ow, hgt, 0.03))
		var y := 0.5 + lift + 0.06
		while y < oh - 0.05:  # Lames du rideau.
			_part(kit, m["frame"], xf, Vector3(xc, y, -0.08), Vector3(ow, 0.025, 0.02))
			y += 0.11
		if lift > 0.0:
			_part(kit, m["dark"], xf, Vector3(xc, 0.5 + lift * 0.5, -t - 0.6), Vector3(ow, lift, 0.05))
	else:
		_part(kit, m["dark"], xf, Vector3(xc, (oh + 0.5) * 0.5, -t - 0.6), Vector3(ow, oh - 0.5, 0.05))
		_part(kit, m["frame"], xf, Vector3(xc, 0.55, -0.12), Vector3(ow, 0.1, 0.1))
		_part(kit, m["frame"], xf, Vector3(xc - ow * 0.5 + 0.05, (oh + 0.5) * 0.5, -0.12), Vector3(0.1, oh - 0.5, 0.1))
		_part(kit, m["frame"], xf, Vector3(xc + ow * 0.5 - 0.05, (oh + 0.5) * 0.5, -0.12), Vector3(0.1, oh - 0.5, 0.1))
		_part(kit, m["glass"], xf, Vector3(xc - ow * 0.3, 0.9, -0.12), Vector3(ow * 0.3, 0.6, 0.01))


## Balcon : dalle en saillie et garde-corps à barreaux.
static func _balcony(kit: Kit, m: Dictionary, xf: Transform3D, at: Vector3, ww: float) -> void:
	var b := xf.basis
	var bw := ww + 0.7
	kit.box(m["slab"], xf * (at + Vector3(0, -0.08, 0.45)), Vector3(bw, 0.16, 0.9), b)
	kit.box(m["frame"], xf * (at + Vector3(0, 1.0, 0.86)), Vector3(bw, 0.05, 0.05), b)
	for side in [-1.0, 1.0]:
		kit.box(m["frame"], xf * (at + Vector3(side * bw * 0.5, 1.0, 0.45)), Vector3(0.05, 0.05, 0.85), b)
	var x := -bw * 0.5
	while x <= bw * 0.5 + 0.01:
		kit.box(m["frame"], xf * (at + Vector3(x, 0.5, 0.86)), Vector3(0.025, 1.0, 0.025), b)
		x += 0.13


# ---------------------------------------------------------------- ruine (Col Gelé)

## Maison en ruine : deux ou trois murs dont le haut s'est effondré en escalier, une fenêtre
## béante avec son linteau de bois, une poutre tombée et des gravats.
static func ruin(rng: RandomNumberGenerator, w: float) -> Node3D:
	var ruin := Node3D.new()
	var kit := Kit.new()
	var wall := weathered("brick", 0.8, Color(0.8, 0.78, 0.76)) if rng.randf() < 0.5 else weathered("plaster", 0.35, Color(0.72, 0.7, 0.66))
	var wood := weathered("plaster", 0.6, Color(0.3, 0.22, 0.15), {"grime": 0.0, "streaks": 0.0})
	var stone := weathered("beton", 0.5, Color(0.6, 0.59, 0.57))
	var spots := [[Vector3(0, 0, -w * 0.5), 0.0], [Vector3(-w * 0.5, 0, 0), PI * 0.5], [Vector3(w * 0.5, 0, 0), PI * 0.5]]
	for i in rng.randi_range(2, 3):
		var h := rng.randf_range(2.2, 3.8)
		var length := w * rng.randf_range(0.6, 1.0)
		var xf := Transform3D(Basis(Vector3.UP, spots[i][1]), spots[i][0])
		# Le mur est fait de colonnes de 40 cm ; leur hauteur tombe vers les bouts cassés.
		var peak := rng.randf_range(-0.3, 0.3) * length
		var open := rng.randf() < 0.6 and length > 3.5
		var gap := rng.randf_range(-0.2, 0.2) * length
		var n := int(length / 0.4)
		var total := 0.0
		for k in n:
			var x := -length * 0.5 + (k + 0.5) * length / n
			var fall := clampf(1.15 - absf(x - peak) / length * rng.randf_range(0.8, 1.4), 0.15, 1.0)
			var ch := snappedf(h * fall + rng.randf_range(-0.2, 0.2), 0.2)
			ch = maxf(ch, 0.3)
			total += ch
			if open and absf(x - gap) < 0.6 and ch > 1.0:
				kit.box(wall, xf * Vector3(x, 0.45, 0), Vector3(length / n, 0.9, 0.5), xf.basis)
				if ch > 2.4:
					kit.box(wall, xf * Vector3(x, (2.2 + ch) * 0.5, 0), Vector3(length / n, ch - 2.2, 0.5), xf.basis)
			else:
				kit.box(wall, xf * Vector3(x, ch * 0.5, 0), Vector3(length / n, ch, 0.5), xf.basis)
		if open:
			kit.box(wood, xf * Vector3(gap, 2.15, 0), Vector3(1.6, 0.18, 0.4), xf.basis)
		var avg := total / n
		var body := _body(Vector3(length, avg, 0.5), Vector3(0, avg * 0.5, 0))
		body.transform = xf
		ruin.add_child(body)
	# Poutre du toit tombée en travers, gravats au pied des murs.
	kit.box(wood, Vector3(rng.randf_range(-1.0, 1.0), 0.8, rng.randf_range(-1.0, 0.5)), Vector3(0.22, 0.22, w * 0.8),
		Basis.from_euler(Vector3(0.3, rng.randf() * TAU, 0.1)))
	for i in rng.randi_range(8, 14):
		var c := Vector3(rng.randf_range(-w, w) * 0.5, 0.08, rng.randf_range(-w, w) * 0.5)
		var sz := Vector3(rng.randf_range(0.2, 0.55), rng.randf_range(0.12, 0.3), rng.randf_range(0.2, 0.45))
		var basis := Basis.from_euler(Vector3(rng.randf_range(-0.4, 0.4), rng.randf() * TAU, rng.randf_range(-0.4, 0.4)))
		kit.box(wall if rng.randf() < 0.6 else stone, c, sz, basis)
	ruin.add_child(kit.instance())
	return ruin


# ---------------------------------------------------------------- base

## Mur anti-souffle en éléments de béton préfabriqués posés côte à côte (collision : la boîte
## size, centrée). Semelle large, voile plus fin, chanfrein en haut, anneaux de levage.
static func blast_wall(size: Vector3, rng: RandomNumberGenerator) -> StaticBody3D:
	var body := _body(size, Vector3.ZERO)
	var kit := Kit.new()
	var concrete := weathered("beton", 0.42, Color(0.92, 0.9, 0.86))
	var steel := _plain(Color(0.25, 0.2, 0.17), 0.7, 0.6)
	var along_x := size.x >= size.z
	var length := size.x if along_x else size.z
	var thick := size.z if along_x else size.x
	var turn := Basis.IDENTITY if along_x else Basis(Vector3.UP, PI * 0.5)
	var n := maxi(1, roundi(length / 1.5))
	var seg := length / n
	var y0 := -size.y * 0.5
	for i in n:
		var x := -length * 0.5 + (i + 0.5) * seg
		var hgt := size.y + rng.randf_range(-0.05, 0.04)
		var lean := Basis(Vector3.RIGHT, rng.randf_range(-0.012, 0.012))
		var w := seg - 0.035
		kit.box(concrete, turn * Vector3(x, y0 + 0.17, 0), Vector3(w, 0.34, thick), turn)
		kit.box(concrete, turn * Vector3(x, y0 + 0.5, 0), Vector3(w, 0.34, thick * 0.7), turn * lean)
		kit.box(concrete, turn * Vector3(x, y0 + (0.6 + hgt - 0.12) * 0.5, 0), Vector3(w, hgt - 0.72, thick * 0.45), turn * lean)
		kit.box(concrete, turn * Vector3(x, y0 + hgt - 0.06, 0), Vector3(w - 0.04, 0.12, thick * 0.32), turn * lean)
		for side in [-0.25, 0.25]:
			var loop := turn * Vector3(x + side * seg, y0 + hgt + 0.03, 0)
			kit.box(steel, loop, Vector3(0.12, 0.025, 0.025), turn)
	body.add_child(kit.instance())
	return body


## Bunker du Cœur (size : sa boîte de collision) : socle, bloc de béton chanfreiné, porte
## blindée côté champ de bataille, grilles d'aération, groupe électrogène sur le toit avec ses
## ailettes, son échappement, ses câbles et une antenne. Renvoie [maillage, voyants bleus] ;
## les voyants s'éteignent quand le Cœur tombe.
static func core_bunker(size: Vector3, color: Color, concrete: Material) -> Array:
	var kit := Kit.new()
	var steel := Fx.textured("metal", 0.9, Color(0.5, 0.52, 0.5))
	var dark := _plain(Color(0.07, 0.07, 0.075), 0.55, 0.6)
	var rubber := _plain(Color(0.03, 0.03, 0.03), 0.9)
	var sx := size.x
	var sz := size.z
	kit.box(concrete, Vector3(0, 0.15, 0), Vector3(sx + 0.5, 0.3, sz + 0.5))
	kit.box(concrete, Vector3(0, 1.75, 0), Vector3(sx, 2.9, sz))
	kit.box(concrete, Vector3(0, 3.32, 0), Vector3(sx - 0.3, 0.25, sz - 0.3))
	# Porte blindée (face -Z) avec son encadrement, ses gonds et son volant.
	var front := -sz * 0.5
	kit.box(dark, Vector3(0, 1.4, front - 0.04), Vector3(1.5, 2.3, 0.1))
	kit.box(steel, Vector3(0, 1.35, front - 0.1), Vector3(1.2, 2.05, 0.06))
	for y in [0.7, 2.0]:
		kit.cyl(dark, Vector3(-0.62, y, front - 0.12), 0.05, 0.3)
	kit.cyl(dark, Vector3(0.25, 1.3, front - 0.17), 0.16, 0.03, Basis(Vector3.RIGHT, PI * 0.5))
	kit.box(dark, Vector3(0.25, 1.3, front - 0.17), Vector3(0.3, 0.03, 0.03))
	kit.box(dark, Vector3(0.25, 1.3, front - 0.17), Vector3(0.03, 0.3, 0.03))
	# Grilles d'aération sur les côtés.
	for side in [-1.0, 1.0]:
		var x: float = side * (sx * 0.5 + 0.03)
		kit.box(dark, Vector3(x, 2.3, 0), Vector3(0.06, 0.8, 1.4))
		for k in 6:
			kit.box(steel, Vector3(x + side * 0.04, 2.0 + k * 0.12, 0), Vector3(0.04, 0.03, 1.3), Basis(Vector3.BACK, side * 0.6))
	# Groupe électrogène : capot, ailettes de refroidissement, échappement, câbles qui descendent.
	var roof := 3.45
	kit.box(steel, Vector3(0.2, roof + 0.45, 0.3), Vector3(2.2, 0.9, 1.4))
	kit.box(dark, Vector3(0.2, roof + 0.92, 0.3), Vector3(2.3, 0.06, 1.5))
	for k in 9:
		kit.box(dark, Vector3(-0.7 + k * 0.2, roof + 0.45, 0.3 - 0.73), Vector3(0.03, 0.7, 0.06))
	kit.cyl(dark, Vector3(1.0, roof + 1.2, 0.8), 0.07, 0.9)
	kit.cyl(dark, Vector3(1.0, roof + 1.68, 0.8), 0.1, 0.08)
	for k in 3:
		var x := -sx * 0.5 + 0.6 + k * 0.12
		kit.box(rubber, Vector3(x, roof * 0.5 + 0.2, -sz * 0.5 - 0.04), Vector3(0.05, roof - 0.3, 0.05))
	kit.box(rubber, Vector3(-sx * 0.5 + 0.72, roof + 0.03, -0.6), Vector3(0.3, 0.05, sz * 0.75))
	# Antenne fouet sur un coin.
	kit.box(dark, Vector3(-sx * 0.5 + 0.35, roof + 1.1, sz * 0.5 - 0.35), Vector3(0.04, 2.2, 0.04))
	var mesh := kit.instance()
	# Voyants : une fente bleue sous le chanfrein de chaque face, et deux près de la porte.
	var lights := Kit.new()
	var glow := _plain(color, 0.5, 0.0, 4.0)
	for k in 4:
		var b := Basis(Vector3.UP, k * PI * 0.5)
		lights.box(glow, b * Vector3(0, 2.95, sz * 0.5 + 0.01), Vector3(sx * 0.6, 0.07, 0.02), b)
	for y in [1.9, 2.1]:
		lights.box(glow, Vector3(0.95, y, front - 0.06), Vector3(0.08, 0.08, 0.04))
	var leds := lights.instance()
	leds.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	return [mesh, leds]
