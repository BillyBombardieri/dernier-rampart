"""Armes du joueur (pistolet lourd et fusil d'assaut), modélisées par le code dans Blender.

Pièces nommées pour que le jeu les anime : Glissiere (pistolet) ou Culasse (fusil) reculent à
chaque tir, Chargeur sort au rechargement, Detente et Chien (pistolet) bougent au tir.
Repères (objets vides) : Bouche, la sortie du canon ; Visee, le point à aligner sur le centre de
l'écran en visée (guidon du pistolet, réticule du viseur du fusil).

Axes : le canon pointe vers +Y (il pointera vers -Z dans Godot), le haut est +Z, la droite +X.
Unités : mètres, à taille réelle.
"""

import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

# Matériaux : couleur de base (sRGB), métal, rugosité, couleur d'émission (ou None).
MATERIALS = {
    "acier_noir": ((0.2, 0.2, 0.21), 0.75, 0.42, None),
    "acier_brut": ((0.52, 0.52, 0.53), 1.0, 0.28, None),
    "polymere": ((0.13, 0.13, 0.135), 0.0, 0.65, None),
    "caoutchouc": ((0.075, 0.075, 0.075), 0.0, 0.9, None),
    "verre": ((0.05, 0.07, 0.08), 0.0, 0.05, None),
    "tritium": ((0.6, 1.0, 0.5), 0.0, 0.4, (0.4, 1.0, 0.3)),
    "reticule": ((1.0, 0.15, 0.08), 0.0, 0.4, (1.0, 0.12, 0.05)),
}


def to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def material(name):
    mat = bpy.data.materials.get(name)
    if mat:
        return mat
    col, metal, rough, emit = MATERIALS[name]
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = [to_linear(c) for c in col] + [1.0]
    bsdf.inputs["Metallic"].default_value = metal
    bsdf.inputs["Roughness"].default_value = rough
    if emit:
        bsdf.inputs["Emission Color"].default_value = [to_linear(c) for c in emit] + [1.0]
        bsdf.inputs["Emission Strength"].default_value = 4.0
    return mat


# ---------------------------------------------------------------- formes de base

def _object(name, bm, mat):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    me.materials.append(material(mat))
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def box(name, lo, hi, mat="acier_noir"):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    lo, hi = Vector(lo), Vector(hi)
    c, s = (lo + hi) * 0.5, hi - lo
    for v in bm.verts:
        v.co = Vector((c.x + v.co.x * s.x, c.y + v.co.y * s.y, c.z + v.co.z * s.z))
    return _object(name, bm, mat)


def cylinder(name, a, b, r, mat="acier_noir", segments=24, r2=None, spin=0.0):
    """Cylindre (ou tronc de cône) de a vers b. spin tourne les facettes autour de l'axe."""
    a, b = Vector(a), Vector(b)
    axis = b - a
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segments,
                          radius1=r, radius2=r if r2 is None else r2, depth=axis.length)
    m = Matrix.Translation((a + b) * 0.5) @ Vector((0, 0, 1)).rotation_difference(axis.normalized()).to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=m @ Matrix.Rotation(spin, 4, "Z"), verts=bm.verts)
    return _object(name, bm, mat)


def torus(name, center, r_major, r_minor, mat, segments=32):
    """Anneau dans le plan XZ, face à l'arrière de l'arme (vers -Y)."""
    bm = bmesh.new()
    rings = []
    for i in range(segments):
        a = 2 * math.pi * i / segments
        ring = []
        for k in range(6):
            b = 2 * math.pi * k / 6
            rad = r_major + r_minor * math.cos(b)
            ring.append(bm.verts.new((center[0] + rad * math.cos(a), center[1] + r_minor * math.sin(b), center[2] + rad * math.sin(a))))
        rings.append(ring)
    for i in range(segments):
        r0, r1 = rings[i], rings[(i + 1) % segments]
        for k in range(6):
            bm.faces.new((r0[k], r1[k], r1[(k + 1) % 6], r0[(k + 1) % 6]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return _object(name, bm, mat)


def transform(obj, m):
    obj.data.transform(m)
    obj.data.update()
    return obj


def rotate_x(obj, angle, pivot):
    p = Vector(pivot)
    return transform(obj, Matrix.Translation(p) @ Matrix.Rotation(angle, 4, "X") @ Matrix.Translation(-p))


def apply_modifier(obj, mod):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)


def cut(obj, *cutters, keep=False):
    """Creuse obj avec chaque volume (booléen exact)."""
    for c in cutters:
        m = obj.modifiers.new("creux", "BOOLEAN")
        m.operation = "DIFFERENCE"
        m.solver = "EXACT"
        m.object = c
        apply_modifier(obj, m)
        if not keep:
            bpy.data.objects.remove(c)
    return obj


def fuse(obj, *others):
    """Réunit des volumes en une seule pièce pleine (booléen exact)."""
    for o in others:
        m = obj.modifiers.new("union", "BOOLEAN")
        m.operation = "UNION"
        m.solver = "EXACT"
        m.object = o
        apply_modifier(obj, m)
        bpy.data.objects.remove(o)
    return obj


def bevel(obj, width, segments=2, angle=40.0):
    """Arêtes adoucies : elles accrochent la lumière comme sur une vraie arme."""
    obj.data.use_auto_smooth = True
    obj.data.auto_smooth_angle = math.radians(angle)
    m = obj.modifiers.new("chanfrein", "BEVEL")
    m.width = width
    m.segments = segments
    m.limit_method = "ANGLE"
    m.angle_limit = math.radians(angle)
    m.harden_normals = True
    m.use_clamp_overlap = True
    apply_modifier(obj, m)
    return obj


def join(name, objs):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    obj = objs[0]
    obj.name = name
    obj.data.name = name
    return obj


def marker(name, pos):
    e = bpy.data.objects.new(name, None)
    e.location = Vector(pos)
    e.empty_display_size = 0.01
    bpy.context.scene.collection.objects.link(e)
    return e


def set_origin(obj, pivot):
    """Place l'origine de la pièce sur son axe de rotation ou de glissement."""
    p = Vector(pivot)
    obj.data.transform(Matrix.Translation(-p))
    obj.location = p
    return obj


def slots(prefix, x0, x1, ys, width, z0, z1):
    """Rainures transversales (rails, stries) : une boîte par rainure."""
    return [box("%s%d" % (prefix, i), (x0, y - width * 0.5, z0), (x1, y + width * 0.5, z1)) for i, y in enumerate(ys)]


def bend(obj, radius):
    """Courbe une pièce verticale vers l'avant (+Y) à mesure qu'elle descend (chargeur courbe)."""
    for v in obj.data.vertices:
        y, z = v.co.y, v.co.z
        if z >= 0.0:
            continue
        a = -z / radius
        d = radius - y
        v.co.y = radius - d * math.cos(a)
        v.co.z = -d * math.sin(a)
    obj.data.update()
    return obj


# ---------------------------------------------------------------- pistolet lourd

GRIP_ANGLE = math.radians(-17.0)  # La crosse part vers l'arrière en descendant.
GRIP_PIVOT = (0.0, -0.02, 0.0)


def build_pistol():
    parts = []
    # Glissière : chanfreins sur le dessus, fenêtre d'éjection, stries arrière et avant, organes de visée.
    slide = box("Glissiere", (-0.0155, -0.06, 0.014), (0.0155, 0.172, 0.05))
    chamfers = []
    for sx in (1, -1):
        c = box("chanfrein%d" % sx, (-0.004, -0.07, -0.004), (0.004, 0.18, 0.004))
        transform(c, Matrix.Translation((0.0155 * sx, 0.0, 0.05)) @ Matrix.Rotation(math.radians(45), 4, "Y"))
        chamfers.append(c)
    nose = box("nez", (-0.03, 0.15, -0.01), (0.03, 0.2, 0.03))
    rotate_x(nose, math.radians(-35), (0.0, 0.16, 0.014))
    cut(slide, *chamfers, nose)
    cut(slide, box("ejection", (0.0, 0.03, 0.031), (0.03, 0.078, 0.06)))
    serr = []
    for sx in (1, -1):
        x0, x1 = (0.0138, 0.02) if sx > 0 else (-0.02, -0.0138)
        serr += slots("stries_ar%d_" % sx, x0, x1, [-0.054 + k * 0.0042 for k in range(8)], 0.0017, 0.018, 0.047)
        serr += slots("stries_av%d_" % sx, x0, x1, [0.112 + k * 0.0042 for k in range(5)], 0.0017, 0.018, 0.045)
    cut(slide, *serr)
    cut(slide, cylinder("bouche", (0, 0.14, 0.03), (0, 0.2, 0.03), 0.0076))
    front = box("guidon", (-0.0018, 0.153, 0.049), (0.0018, 0.162, 0.0575))
    rear = box("hausse", (-0.0095, -0.052, 0.049), (0.0095, -0.04, 0.0575))
    cut(rear, box("cran", (-0.0019, -0.06, 0.052), (0.0019, -0.03, 0.06)))
    fuse(slide, front, rear)
    bevel(slide, 0.0009, 2)
    # Points de visée : vus depuis l'œil en visée (repère Visee), les trois points sont alignés.
    dots = [cylinder("point_av", (0, 0.1527, 0.0538), (0, 0.1532, 0.0538), 0.0011, "tritium", 12)]
    for sx in (1, -1):
        dots.append(cylinder("point_ar%d" % sx, (0.0058 * sx, -0.0523, 0.0559), (0.0058 * sx, -0.0518, 0.0559), 0.001, "tritium", 12))
    slide = join("Glissiere", [slide] + dots)
    parts.append(set_origin(slide, (0, 0, 0.03)))

    # Carcasse : couvre-canon avec rail, pontet, crosse inclinée avec puits du chargeur, queue de castor.
    frame = box("Carcasse", (-0.0145, -0.035, -0.004), (0.0145, 0.158, 0.014), "polymere")
    rail = box("rail", (-0.0115, 0.085, -0.008), (0.0115, 0.152, 0.0), "polymere")
    cut(rail, *slots("rail_", -0.013, 0.013, [0.097, 0.114, 0.131], 0.004, -0.009, -0.0035))
    guard = box("pontet", (-0.006, -0.004, -0.041), (0.006, 0.076, 0.002), "polymere")
    cut(guard, box("pontet_int", (-0.01, 0.0045, -0.0345), (0.01, 0.0675, 0.003)))
    bevel(guard, 0.0028, 3)
    # Crosse aux angles très arrondis, pour la main.
    grip = box("crosse", (-0.0135, -0.053, -0.12), (0.0135, 0.0, 0.004), "polymere")
    bevel(grip, 0.0075, 4)
    rotate_x(grip, GRIP_ANGLE, GRIP_PIVOT)
    tail = box("queue", (-0.012, -0.072, -0.006), (0.012, -0.03, 0.011), "polymere")
    bevel(tail, 0.005, 3)
    rotate_x(tail, math.radians(8), (0, -0.035, 0.0))
    fuse(frame, rail, guard, grip, tail)
    well = box("puits", (-0.0112, -0.047, -0.135), (0.0112, -0.009, -0.092))
    rotate_x(well, GRIP_ANGLE, GRIP_PIVOT)
    cut(frame, well)
    bevel(frame, 0.001, 2)
    panels = []
    for sx in (1, -1):
        x0, x1 = (0.0125, 0.0158) if sx > 0 else (-0.0158, -0.0125)
        p = box("plaquette%d" % sx, (x0, -0.044, -0.104), (x1, -0.009, -0.016), "caoutchouc")
        rotate_x(p, GRIP_ANGLE, GRIP_PIVOT)
        bevel(p, 0.0015, 3)
        panels.append(p)
    lever = box("arretoir", (-0.0178, 0.012, 0.004), (-0.0142, 0.046, 0.0125))
    safety = box("surete", (-0.0182, -0.048, 0.001), (-0.0142, -0.027, 0.0115))
    for o in (lever, safety):
        bevel(o, 0.0008, 2)
    barrel = cylinder("canon", (0, 0.155, 0.03), (0, 0.1738, 0.03), 0.0069, "acier_brut", 24)
    cut(barrel, cylinder("ame", (0, 0.15, 0.03), (0, 0.18, 0.03), 0.0057))
    hood = box("chambre", (-0.0092, 0.028, 0.026), (0.0092, 0.08, 0.0445), "acier_brut")
    bevel(hood, 0.001, 2)
    frame = join("Carcasse", [frame] + panels + [lever, safety, barrel, hood])
    parts.append(frame)

    # Chien (marteau extérieur), détente, chargeur.
    hammer = box("Chien", (-0.0042, -0.068, 0.02), (0.0042, -0.054, 0.046))
    cut(hammer, cylinder("anneau", (-0.01, -0.0615, 0.039), (0.01, -0.0615, 0.039), 0.0035))
    bevel(hammer, 0.001, 2)
    parts.append(set_origin(hammer, (0, -0.058, 0.024)))
    trigger = box("Detente", (-0.0034, 0.027, -0.029), (0.0034, 0.036, -0.002))
    rotate_x(trigger, math.radians(-12), (0, 0.032, -0.002))
    bevel(trigger, 0.0012, 3)
    parts.append(set_origin(trigger, (0, 0.032, -0.002)))
    mag = box("Chargeur", (-0.0104, -0.046, -0.13), (0.0104, -0.01, 0.0))
    base = box("talon", (-0.0136, -0.05, -0.137), (0.0136, -0.005, -0.128), "polymere")
    bevel(mag, 0.0008, 2)
    bevel(base, 0.002, 2)
    mag = join("Chargeur", [mag, base])
    rotate_x(mag, GRIP_ANGLE, GRIP_PIVOT)
    parts.append(set_origin(mag, GRIP_PIVOT))

    marks = [marker("Bouche", (0, 0.182, 0.03)), marker("Visee", (0, 0.1575, 0.0575))]
    return parts, marks


# ---------------------------------------------------------------- fusil d'assaut

def build_rifle():
    parts = []
    # Boîte de culasse supérieure avec rail, fenêtre d'éjection, levier d'armement.
    upper = box("haut", (-0.021, -0.13, -0.004), (0.021, 0.15, 0.036))
    rail = box("rail", (-0.0105, -0.13, 0.036), (0.0105, 0.47, 0.046))
    cut(rail, *slots("cran", -0.012, 0.012, [-0.12 + k * 0.01 for k in range(60)], 0.0052, 0.0412, 0.047))
    deflector = box("deflecteur", (0.02, -0.078, 0.006), (0.027, -0.05, 0.031))
    assist = cylinder("assist", (0.022, -0.115, 0.024), (0.022, -0.085, 0.024), 0.0065, "acier_noir", 16)
    fuse(upper, deflector, assist)
    cut(upper, box("ejection", (0.012, -0.042, 0.004), (0.03, 0.046, 0.028)))
    bevel(upper, 0.0012, 2)
    bevel(rail, 0.0005, 1)
    handle = box("armement", (-0.0115, -0.152, 0.027), (0.0115, -0.128, 0.0355))
    wings = box("ailettes", (-0.022, -0.156, 0.027), (0.022, -0.146, 0.0355))
    fuse(handle, wings)
    bevel(handle, 0.0012, 2)
    # Boîte inférieure, puits du chargeur, pontet, poignée.
    lower = box("bas", (-0.0205, -0.13, -0.04), (0.0205, 0.11, -0.004))
    well = box("puits", (-0.019, 0.022, -0.078), (0.019, 0.108, -0.035))
    guard = box("pontet", (-0.0068, -0.048, -0.072), (0.0068, 0.024, -0.04))
    cut(guard, box("pontet_int", (-0.01, -0.042, -0.066), (0.01, 0.018, -0.035)))
    fuse(lower, well, guard)
    cut(lower, box("trou_puits", (-0.0122, 0.029, -0.09), (0.0122, 0.101, -0.03)))
    bevel(lower, 0.0012, 2)
    grip = box("poignee", (-0.0135, -0.104, -0.155), (0.0135, -0.062, -0.035), "polymere")
    rotate_x(grip, math.radians(-22), (0, -0.075, -0.04))
    bevel(grip, 0.004, 3)
    # Tube de crosse, crosse réglable et plaque de couche.
    tube = cylinder("tube", (0, -0.335, -0.008), (0, -0.13, -0.008), 0.0145, "acier_noir", 24)
    stock = box("crosse", (-0.022, -0.372, -0.058), (0.022, -0.2, 0.019), "polymere")
    toe = box("talon", (-0.016, -0.37, -0.085), (0.016, -0.27, -0.05), "polymere")
    fuse(stock, toe)
    cut(stock, box("joue", (-0.03, -0.32, 0.008), (0.03, -0.19, 0.03)))
    bevel(stock, 0.004, 3)
    pad = box("plaque", (-0.023, -0.386, -0.088), (0.023, -0.371, 0.022), "caoutchouc")
    bevel(pad, 0.003, 2)
    # Garde-main octogonal ajouré (fentes sur les flancs), canon et cache-flamme.
    guard_tube = cylinder("garde_main", (0, 0.15, 0.012), (0, 0.47, 0.012), 0.027, "acier_noir", 8, spin=math.radians(22.5))
    cut(guard_tube, cylinder("creux", (0, 0.14, 0.012), (0, 0.48, 0.012), 0.0225, "acier_noir", 24))
    holes = []
    for k, ang in enumerate((90, -90, 45, -45, 135, -135)):
        a = math.radians(ang)
        for j in range(4):
            y0 = 0.18 + j * 0.07
            h = box("fente%d_%d" % (k, j), (-0.004, y0, 0.0), (0.004, y0 + 0.042, 0.04))
            transform(h, Matrix.Translation((0, 0, 0.012)) @ Matrix.Rotation(-a + math.pi * 0.5, 4, "Y"))
            holes.append(h)
    cut(guard_tube, *holes)
    bevel(guard_tube, 0.0012, 2)
    barrel = cylinder("canon", (0, 0.12, 0.0), (0, 0.61, 0.0), 0.0088, "acier_noir", 20)
    hider = cylinder("cache_flamme", (0, 0.6, 0.0), (0, 0.658, 0.0), 0.0108, "acier_noir", 20)
    cut(hider, cylinder("ame", (0, 0.62, 0.0), (0, 0.67, 0.0), 0.0046))
    hider_slots = []
    for k in range(4):
        s = box("fente_cf%d" % k, (-0.0016, 0.628, 0.0), (0.0016, 0.666, 0.02))
        transform(s, Matrix.Rotation(math.pi * 0.5 * k + math.pi * 0.25, 4, "Y"))
        hider_slots.append(s)
    cut(hider, *hider_slots)
    bevel(hider, 0.0006, 1)
    # Viseur holographique : embase avec boutons, capot court autour d'une vitre teintée (un cadre
    # mince en visée, pas un tunnel) et réticule rouge lumineux.
    sight = box("viseur", (-0.0185, -0.11, 0.046), (0.0185, -0.035, 0.0545))
    for sx in (1, -1):
        x0, x1 = (0.0153, 0.0185) if sx > 0 else (-0.0185, -0.0153)
        fuse(sight, box("montant%d" % sx, (x0, -0.06, 0.0545), (x1, -0.037, 0.0905)))
    fuse(sight, box("capot", (-0.0185, -0.06, 0.0873), (0.0185, -0.037, 0.0905)))
    for k, x in enumerate((-0.0085, 0.0015)):
        fuse(sight, box("bouton%d" % k, (x, -0.1, 0.0545), (x + 0.007, -0.088, 0.0565)))
    bevel(sight, 0.0012, 2)
    glass = box("vitre", (-0.0153, -0.0425, 0.0545), (0.0153, -0.0415, 0.0873), "verre")
    ring = torus("anneau", (0, -0.0435, 0.0725), 0.0026, 0.00016, "reticule")
    dot = cylinder("point", (0, -0.0437, 0.0725), (0, -0.0433, 0.0725), 0.00042, "reticule", 12)
    body = join("Corps", [upper, rail, handle, lower, grip, tube, stock, pad, guard_tube, barrel, hider, sight, glass, ring, dot])
    parts.append(body)
    # Pièces mobiles : porte-culasse visible par la fenêtre d'éjection, détente, chargeur courbe.
    bolt = box("Culasse", (-0.012, -0.04, 0.003), (0.0168, 0.05, 0.0275), "acier_brut")
    cut(bolt, box("gorge", (0.014, -0.03, 0.009), (0.02, 0.04, 0.021)))
    bevel(bolt, 0.0008, 2)
    parts.append(set_origin(bolt, (0, 0.0, 0.015)))
    trigger = box("Detente", (-0.003, -0.016, -0.064), (0.003, -0.008, -0.039))
    rotate_x(trigger, math.radians(-10), (0, -0.012, -0.039))
    bevel(trigger, 0.0012, 3)
    parts.append(set_origin(trigger, (0, -0.012, -0.039)))
    mag = box("Chargeur", (-0.0118, -0.033, -0.2), (0.0118, 0.033, 0.0), "polymere")
    base = box("semelle", (-0.0138, -0.037, -0.211), (0.0138, 0.036, -0.2), "polymere")
    fuse(mag, base)
    ribs = []
    for k in range(5):
        z = -0.06 - k * 0.026
        ribs.append(box("nervure%d" % k, (-0.02, -0.04, z - 0.0015), (0.02, 0.04, z + 0.0015)))
    cut(mag, *ribs)
    for k in range(5):
        z = -0.06 - k * 0.026
        r = box("bourrelet%d" % k, (-0.0126, -0.03, z - 0.004), (0.0126, 0.03, z + 0.004), "polymere")
        cut(r, box("vide%d" % k, (-0.011, -0.04, z - 0.01), (0.011, 0.04, z + 0.01)))
        fuse(mag, r)
    bevel(mag, 0.0016, 2)
    bend(mag, 0.55)
    transform(mag, Matrix.Translation((0, 0.065, -0.042)) @ Matrix.Rotation(math.radians(6), 4, "X"))
    parts.append(set_origin(mag, (0, 0.065, -0.042)))

    marks = [marker("Bouche", (0, 0.665, 0.0)), marker("Visee", (0, -0.0435, 0.0725))]
    return parts, marks


WEAPONS = {"pistolet": build_pistol, "fusil": build_rifle}


def export_weapon(name, parts, marks, out_dir):
    bpy.ops.object.select_all(action="DESELECT")
    for o in parts + marks:
        o.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    path = os.path.join(out_dir, "arme_%s.gltf" % name)
    bpy.ops.export_scene.gltf(
        filepath=path, export_format="GLTF_SEPARATE", use_selection=True, export_yup=True,
        export_apply=True, export_normals=True, export_texcoords=False, export_tangents=False,
        export_materials="EXPORT", export_skins=False, export_animations=False,
        export_cameras=False, export_lights=False)
    tris = 0
    for p in parts:
        p.data.calc_loop_triangles()
        tris += len(p.data.loop_triangles)
    print("  écrit %s (%d triangles)" % (os.path.basename(path), tris))
    return path
