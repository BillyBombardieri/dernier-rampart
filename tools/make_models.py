"""Fabrique les modèles 3D animés du jeu avec Blender (zombies et armes), sans aucune ressource externe.

Lancer depuis la racine du dépôt (Blender 4.x avec numpy) :
    blender -b --factory-startup -P tools/make_models.py -- [--only rodeur,brute,pistolet] [--preview dossier]

Chaque zombie est sculpté par le code (tools/sculpt.py) : un corps fait de muscles fusionnés,
des vêtements plissés, des blessures. On en tire une version allégée pour le jeu, sur laquelle
les couleurs, le relief fin et l'occlusion ambiante sont « cuits » dans des textures. Le
squelette, les poids et les animations (marche, course, attaque, mort, cri...) sont créés ici.
Les modèles sont écrits dans assets/models/ (.gltf, .bin et textures JPEG), et les repères dont
le jeu a besoin (position des yeux, vitesse de marche des animations) dans scripts/zombie_models.gd.
"""

import math
import os
import sys
import time

import bpy
import numpy as np
from mathutils import Vector

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
OUT_DIR = os.path.join(ROOT, "assets", "models")
sys.path.insert(0, TOOLS)

import sculpt  # noqa: E402
import weapon_models  # noqa: E402
import zombie_anims  # noqa: E402


# ---------------------------------------------------------------- outils Blender

def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def link(obj):
    bpy.context.scene.collection.objects.link(obj)
    return obj


def select_only(*objs):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]


def apply_modifiers(obj):
    select_only(obj)
    for m in list(obj.modifiers):
        bpy.ops.object.modifier_apply(modifier=m.name)


def mesh_object(name, verts, faces):
    """Maillage à partir de tableaux numpy (faces toutes de même taille)."""
    mesh = bpy.data.meshes.new(name)
    nv, nf, k = len(verts), len(faces), faces.shape[1]
    mesh.vertices.add(nv)
    mesh.vertices.foreach_set("co", np.ascontiguousarray(verts, dtype=np.float32).ravel())
    mesh.loops.add(nf * k)
    mesh.loops.foreach_set("vertex_index", np.ascontiguousarray(faces, dtype=np.int32).ravel())
    mesh.polygons.add(nf)
    mesh.polygons.foreach_set("loop_start", np.arange(0, nf * k, k, dtype=np.int32))
    try:
        mesh.polygons.foreach_set("loop_total", np.full(nf, k, dtype=np.int32))
    except (AttributeError, RuntimeError, TypeError):
        pass
    mesh.update(calc_edges=True)
    mesh.validate()
    return link(bpy.data.objects.new(name, mesh))


def point_attr(obj, name, values, kind="FLOAT"):
    mesh = obj.data
    if kind == "COLOR":
        a = mesh.color_attributes.new(name, "FLOAT_COLOR", "POINT")
        rgba = np.ones((len(values), 4), np.float32)
        rgba[:, :3] = values
        a.data.foreach_set("color", rgba.ravel())
    else:
        a = mesh.attributes.new(name, "FLOAT", "POINT")
        a.data.foreach_set("value", np.ascontiguousarray(values, dtype=np.float32))
    return a


def to_linear(c):
    """Les couleurs sont choisies en sRGB ; Blender travaille en linéaire."""
    c = np.clip(c, 0.0, 1.0)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4).astype(np.float32)


def vertex_normals(obj):
    n = np.zeros(len(obj.data.vertices) * 3, np.float32)
    obj.data.vertices.foreach_get("normal", n)
    return n.reshape(-1, 3)


def vertex_coords(obj):
    v = np.zeros(len(obj.data.vertices) * 3, np.float32)
    obj.data.vertices.foreach_get("co", v)
    return v.reshape(-1, 3)


# ---------------------------------------------------------------- zombies : forme détaillée

def zombie_high(name, h):
    """Sculpte le zombie et renvoie le maillage détaillé coloré (et l'objet Zombie de sculpt)."""
    t = time.time()
    z = sculpt.Zombie(name, h)
    verts, quads = z.build()
    obj = mesh_object(name + "_high", verts, quads)
    # Un remaillage par voxels rend la surface parfaitement fermée (aucune arête partagée par
    # plus de deux faces), ce dont ont besoin le dépliage et le calcul des poids.
    mod = obj.modifiers.new("remesh", "REMESH")
    mod.mode = "VOXEL"
    mod.voxel_size = h
    mod.adaptivity = 0.0
    apply_modifiers(obj)
    mod = obj.modifiers.new("smooth", "SMOOTH")
    mod.factor = 0.5
    mod.iterations = 2
    apply_modifiers(obj)
    verts = vertex_coords(obj)
    col, rough, bump, emit, layer = z.paint(verts, vertex_normals(obj))
    point_attr(obj, "col", to_linear(col), "COLOR")
    point_attr(obj, "rough", rough)
    point_attr(obj, "bump", bump)
    point_attr(obj, "emit", emit)
    for p in obj.data.polygons:
        p.use_smooth = True
    print("%s : forme détaillée %d faces en %.1f s" % (name, len(quads), time.time() - t))
    return obj, z


def preview_material(name="apercu"):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    ca = nt.nodes.new("ShaderNodeAttribute")
    ca.attribute_name = "col"
    ra = nt.nodes.new("ShaderNodeAttribute")
    ra.attribute_name = "rough"
    nt.links.new(ca.outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(ra.outputs["Fac"], bsdf.inputs["Roughness"])
    return mat


# ---------------------------------------------------------------- version allégée et textures

def make_low(high, target_tris, name):
    """Copie allégée (décimation symétrique) du maillage détaillé, sans ses attributs."""
    low = link(bpy.data.objects.new(name, high.data.copy()))
    for a in [a.name for a in low.data.color_attributes]:
        low.data.color_attributes.remove(low.data.color_attributes[a])
    for a in [a.name for a in low.data.attributes if a.name in ("rough", "bump", "emit")]:
        low.data.attributes.remove(low.data.attributes[a])
    mod = low.modifiers.new("dec", "DECIMATE")
    mod.decimate_type = "COLLAPSE"
    mod.ratio = target_tris / (2.0 * len(high.data.polygons))
    mod.use_symmetry = True
    mod.symmetry_axis = "X"
    mod.use_collapse_triangulate = True
    apply_modifiers(low)
    # Nettoyage : faces en double, faces dégénérées (aires nulles) et morceaux isolés laissés
    # par la décimation, puis petits trous rebouchés.
    low.data.validate()
    select_only(low)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.dissolve_degenerate(threshold=0.0008)
    bpy.ops.mesh.delete_loose(use_verts=True, use_edges=True, use_faces=True)
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.fill_holes(sides=6)
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.quads_convert_to_tris()
    bpy.ops.object.mode_set(mode="OBJECT")
    if low.data.validate():
        print("  maillage allégé corrigé une seconde fois")
    for p in low.data.polygons:
        p.use_smooth = True
    return low


def body_segments(j):
    """Segments (nom de partie, a, b) du squelette de référence, côtés gauche et droit."""
    segs = [
        ("tete", Vector(j["neck"]), Vector(j["head_top"])),
        ("torse", Vector(j["pelvis"]) + Vector((0, 0, -0.05)), Vector(j["neck"])),
    ]
    for side, sx in (("G", 1.0), ("D", -1.0)):
        def m(v):
            return Vector((v[0] * sx, v[1], v[2]))
        segs += [
            ("bras" + side, m(j["shoulder"]), m(j["elbow"])),
            ("avbras" + side, m(j["elbow"]), m(j["wrist"])),
            ("main" + side, m(j["wrist"]), m(j["fingers"])),
            ("cuisse" + side, m(j["hip"]), m(j["knee"])),
            ("jambe" + side, m(j["knee"]), m(j["ankle"])),
            ("pied" + side, m(j["heel"]), m(j["toe"])),
        ]
    return segs


def seg_dist(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-9)))
    return (p - (a + ab * t)).length


def unwrap(obj, j, margin=0.004, head_scale=1.6):
    """Dépliage par parties du corps : chaque membre, la tête et le torse sont coupés en deux
    moitiés (avant / arrière, dessus / dessous pour les mains et les pieds). On obtient une
    trentaine de grandes îles propres plutôt qu'une poussière de petits morceaux."""
    import bmesh
    segs = body_segments(j)
    axis = {n: (Vector(a), Vector(b)) for n, a, b in segs}
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.faces.ensure_lookup_table()
    label = {}
    for f in bm.faces:
        c = f.calc_center_median()
        best, name = 1e9, ""
        for n, a, b in segs:
            d = seg_dist(c, a, b)
            if d < best:
                best, name = d, n
        if name.startswith("main"):
            u_back = Vector((0.7 if name.endswith("G") else -0.7, 0.0, 0.7))
            name += "_dos" if f.normal.dot(u_back) > 0 else "_paume"
        elif name.startswith("pied"):
            name += "_dessous" if f.normal.z < -0.6 else "_dessus"
        else:
            a, b = axis[name]
            w = (b - a).normalized()
            front = Vector((0, -1, 0))
            e1 = (front - w * front.dot(w)).normalized()
            name += "_av" if (c - a).dot(e1) > 0 else "_ar"
        label[f.index] = name

    def neighbours(f):
        out = []
        for e in f.edges:
            for g in e.link_faces:
                if g is not f:
                    out.append(g)
        return out

    # Lissage : une face isolée prend l'étiquette de ses voisines.
    for _ in range(4):
        changed = 0
        for f in bm.faces:
            counts = {}
            for g in neighbours(f):
                counts[label[g.index]] = counts.get(label[g.index], 0) + 1
            best = max(counts, key=counts.get) if counts else label[f.index]
            if best != label[f.index] and counts[best] >= 2:
                label[f.index] = best
                changed += 1
        if not changed:
            break
    # Les petits morceaux (moins de 15 faces) rejoignent la partie voisine la plus présente.
    for _ in range(6):
        seen = set()
        moved = 0
        for f in bm.faces:
            if f.index in seen:
                continue
            comp, stack = [], [f]
            seen.add(f.index)
            while stack:
                x = stack.pop()
                comp.append(x)
                for g in neighbours(x):
                    if g.index not in seen and label[g.index] == label[f.index]:
                        seen.add(g.index)
                        stack.append(g)
            if len(comp) >= 15:
                continue
            counts = {}
            ids = {x.index for x in comp}
            for x in comp:
                for g in neighbours(x):
                    if g.index not in ids:
                        counts[label[g.index]] = counts.get(label[g.index], 0) + 1
            if counts:
                new = max(counts, key=counts.get)
                for x in comp:
                    label[x.index] = new
                moved += 1
        if not moved:
            break
    for e in bm.edges:
        lf = e.link_faces
        e.seam = len(lf) == 2 and label[lf[0].index] != label[lf[1].index]
    bm.to_mesh(obj.data)
    bm.free()
    select_only(obj)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.unwrap(method="ANGLE_BASED", margin=margin)
    bpy.ops.uv.average_islands_scale()
    bpy.ops.object.mode_set(mode="OBJECT")
    me = obj.data
    uv = me.uv_layers.active.data
    # La tête reçoit plus de détail : ses îles sont agrandies avant l'assemblage.
    if head_scale != 1.0:
        for f in me.polygons:
            if label[f.index].startswith("tete"):
                for li in f.loop_indices:
                    uv[li].uv *= head_scale
    # Une face très étirée isolée par le dépliage est réduite pour ne pas gâcher la place.
    from bpy_extras import mesh_utils
    for isl in mesh_utils.mesh_linked_uv_islands(me):
        if len(isl) >= 4:
            continue
        loops = [li for f in isl for li in me.polygons[f].loop_indices]
        c = sum((uv[li].uv for li in loops), Vector((0, 0))) / len(loops)
        ext = max(max(abs(uv[li].uv.x - c.x), abs(uv[li].uv.y - c.y)) for li in loops)
        if ext > 0.004:
            for li in loops:
                uv[li].uv = c + (uv[li].uv - c) * (0.004 / ext)
    select_only(obj)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.select_all(action="SELECT")
    bpy.ops.uv.pack_islands(margin=margin, rotate=True)
    bpy.ops.object.mode_set(mode="OBJECT")
    return label


def bake_source_material():
    """Matériau du modèle détaillé pendant la cuisson : une émission qu'on branche selon la passe."""
    mat = bpy.data.materials.new("cuisson_source")
    mat.use_nodes = True
    nt = mat.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    emit = nt.nodes.new("ShaderNodeEmission")
    col = nt.nodes.new("ShaderNodeAttribute")
    col.attribute_name = "col"
    rough = nt.nodes.new("ShaderNodeAttribute")
    rough.attribute_name = "rough"
    bump_amt = nt.nodes.new("ShaderNodeAttribute")
    bump_amt.attribute_name = "bump"
    # Relief fin (grain du tissu, plis de la peau) ajouté pendant la cuisson des normales.
    coord = nt.nodes.new("ShaderNodeTexCoord")
    tex = nt.nodes.new("ShaderNodeTexNoise")
    tex.inputs["Scale"].default_value = 260.0
    tex.inputs["Detail"].default_value = 3.0
    tex.inputs["Roughness"].default_value = 0.6
    nt.links.new(coord.outputs["Object"], tex.inputs["Vector"])
    mul = nt.nodes.new("ShaderNodeMath")
    mul.operation = "MULTIPLY"
    nt.links.new(tex.outputs["Fac"], mul.inputs[0])
    nt.links.new(bump_amt.outputs["Fac"], mul.inputs[1])
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.35
    bump.inputs["Distance"].default_value = 0.002
    nt.links.new(mul.outputs["Value"], bump.inputs["Height"])
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    nodes = {"out": out, "emit": emit, "col": col, "rough": rough, "bsdf": bsdf}
    return mat, nodes


def bake_pass(kind, nodes, mat_low_nodes, image, high, low, samples=1):
    scene = bpy.context.scene
    nt = nodes["out"].id_data
    for l in list(nodes["out"].inputs["Surface"].links):
        nt.links.remove(l)
    for l in list(nodes["emit"].inputs["Color"].links):
        nt.links.remove(l)
    if kind == "COL":
        nt.links.new(nodes["col"].outputs["Color"], nodes["emit"].inputs["Color"])
        nt.links.new(nodes["emit"].outputs["Emission"], nodes["out"].inputs["Surface"])
        btype = "EMIT"
    elif kind == "ROUGH":
        nt.links.new(nodes["rough"].outputs["Color"], nodes["emit"].inputs["Color"])
        nt.links.new(nodes["emit"].outputs["Emission"], nodes["out"].inputs["Surface"])
        btype = "EMIT"
    else:
        nt.links.new(nodes["bsdf"].outputs["BSDF"], nodes["out"].inputs["Surface"])
        btype = kind
    mat_low_nodes["target"].image = image
    mat_low_nodes["tree"].nodes.active = mat_low_nodes["target"]
    scene.cycles.samples = samples
    select_only(low)
    high.select_set(True)
    bpy.context.view_layer.objects.active = low
    t = time.time()
    bpy.ops.object.bake(type=btype, use_selected_to_active=True, cage_extrusion=0.012,
                        max_ray_distance=0.04, margin=6, normal_space="TANGENT", use_clear=True)
    print("  cuisson %s : %.1f s" % (kind, time.time() - t))


def image_array(img):
    a = np.zeros(img.size[0] * img.size[1] * 4, np.float32)
    img.pixels.foreach_get(a)
    return a.reshape(img.size[1], img.size[0], 4)


def set_image(img, arr):
    img.pixels.foreach_set(np.ascontiguousarray(arr, dtype=np.float32).ravel())
    img.update()


def to_srgb(c):
    c = np.clip(c, 0.0, 1.0)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1 / 2.4) - 0.055)


def bake_textures(high, low, name, size):
    """Cuit couleur (avec occlusion), rugosité + occlusion, et relief (normales) sur la version allégée."""
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.use_denoising = False
    if scene.world is None:
        scene.world = bpy.data.worlds.new("w")
    scene.world.light_settings.distance = 0.25
    src_mat, nodes = bake_source_material()
    high.data.materials.clear()
    high.data.materials.append(src_mat)
    tmp = bpy.data.materials.new("cuisson_cible")
    tmp.use_nodes = True
    target = tmp.node_tree.nodes.new("ShaderNodeTexImage")
    low.data.materials.clear()
    low.data.materials.append(tmp)
    lown = {"target": target, "tree": tmp.node_tree}

    def img(label, data, float_buf=False):
        im = bpy.data.images.new("%s_%s" % (name, label), size, size, alpha=False, float_buffer=float_buf)
        if data:
            im.colorspace_settings.name = "Non-Color"
        return im

    col_f = img("col_f", True, True)
    ao_f = img("ao_f", True, True)
    rough_f = img("rough_f", True, True)
    nrm = img("normal", True)
    bake_pass("COL", nodes, lown, col_f, high, low)
    bake_pass("ROUGH", nodes, lown, rough_f, high, low)
    bake_pass("NORMAL", nodes, lown, nrm, high, low, samples=4)
    bake_pass("AO", nodes, lown, ao_f, high, low, samples=24)
    col = image_array(col_f)[..., :3]
    ao = image_array(ao_f)[..., 0]
    rough = image_array(rough_f)[..., 0]
    shade = 1.0 - 0.6 * (1.0 - ao)
    base = img("base", False)
    out = np.ones((size, size, 4), np.float32)
    out[..., :3] = to_srgb(col * shade[..., None])
    set_image(base, out)
    orm = img("orm", True)
    o = np.ones((size, size, 4), np.float32)
    o[..., 0] = ao
    o[..., 1] = rough
    o[..., 2] = 0.0
    set_image(orm, o)
    for im in (col_f, ao_f, rough_f):
        bpy.data.images.remove(im)
    bpy.data.materials.remove(tmp)
    return base, orm, nrm


def game_material(name, base, orm, nrm, emissive=None):
    """Matériau exporté en glTF : couleur, rugosité/métal + occlusion (une image), normales."""
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    t_base = nt.nodes.new("ShaderNodeTexImage")
    t_base.image = base
    t_orm = nt.nodes.new("ShaderNodeTexImage")
    t_orm.image = orm
    t_n = nt.nodes.new("ShaderNodeTexImage")
    t_n.image = nrm
    sep = nt.nodes.new("ShaderNodeSeparateColor")
    nmap = nt.nodes.new("ShaderNodeNormalMap")
    nt.links.new(t_base.outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(t_orm.outputs["Color"], sep.inputs["Color"])
    nt.links.new(sep.outputs["Green"], bsdf.inputs["Roughness"])
    nt.links.new(sep.outputs["Blue"], bsdf.inputs["Metallic"])
    nt.links.new(t_n.outputs["Color"], nmap.inputs["Color"])
    nt.links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])
    # Occlusion ambiante pour glTF : groupe « glTF Material Output » reconnu par l'exporteur.
    grp = bpy.data.node_groups.get("glTF Material Output")
    if grp is None:
        grp = bpy.data.node_groups.new("glTF Material Output", "ShaderNodeTree")
        grp.interface.new_socket("Occlusion", in_out="INPUT", socket_type="NodeSocketFloat")
    gn = nt.nodes.new("ShaderNodeGroup")
    gn.node_tree = grp
    nt.links.new(sep.outputs["Red"], gn.inputs["Occlusion"])
    if emissive:
        bsdf.inputs["Emission Color"].default_value = (*emissive, 1.0)
        bsdf.inputs["Emission Strength"].default_value = 1.0
        nt.links.new(t_base.outputs["Color"], bsdf.inputs["Emission Color"])
    return mat


# ---------------------------------------------------------------- squelette et poids

def head_point(z, x, y, zz):
    s = z.p["head"]
    return Vector(z.j["hc"]) + Vector((x, y, zz)) * s


def rig_layout(z):
    """Os du zombie : (nom, tête, queue, parent). Côté gauche = _L (+X), droit = _R."""
    j = z.j

    def v(key, sx=1.0):
        a = j[key]
        return Vector((a[0] * sx, a[1], a[2]))

    bones = [
        ("hips", v("pelvis"), v("spine"), None),
        ("spine", v("spine"), v("chest"), "hips"),
        ("chest", v("chest"), v("neck"), "spine"),
        ("neck", v("neck"), v("head"), "chest"),
        ("head", v("head"), v("head_top"), "neck"),
        ("jaw", head_point(z, 0, 0.006, -0.024), head_point(z, 0, -0.072, -0.094), "head"),
    ]
    for side, sx in (("L", 1.0), ("R", -1.0)):
        bones += [
            ("clavicle_" + side, v("clav", sx), v("shoulder", sx), "chest"),
            ("upper_arm_" + side, v("shoulder", sx), v("elbow", sx), "clavicle_" + side),
            ("forearm_" + side, v("elbow", sx), v("wrist", sx), "upper_arm_" + side),
            ("hand_" + side, v("wrist", sx), v("fingers", sx), "forearm_" + side),
            ("thigh_" + side, v("hip", sx), v("knee", sx), "hips"),
            ("shin_" + side, v("knee", sx), v("ankle", sx), "thigh_" + side),
            ("foot_" + side, v("ankle", sx), v("ball", sx), "shin_" + side),
            ("toe_" + side, v("ball", sx), v("toe", sx), "foot_" + side),
        ]
    return bones


def build_armature(z, name):
    data = bpy.data.armatures.new(name + "_squelette")
    arm = link(bpy.data.objects.new(name + "_squelette", data))
    select_only(arm)
    bpy.ops.object.mode_set(mode="EDIT")
    eb = data.edit_bones
    for n, head, tail, parent in rig_layout(z):
        b = eb.new(n)
        b.head, b.tail = head, tail
        if parent:
            b.parent = eb[parent]
        b.use_connect = False
    bpy.ops.armature.select_all(action="SELECT")
    bpy.ops.armature.calculate_roll(type="GLOBAL_NEG_Y")
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
    return arm


def skin_mesh(low, arm, z):
    """Poids automatiques (diffusion de chaleur), puis la mâchoire est peinte à la main."""
    arm.data.bones["jaw"].use_deform = False
    select_only(low)
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.parent_set(type="ARMATURE_AUTO")
    arm.data.bones["jaw"].use_deform = True
    groups = {g.name: g for g in low.vertex_groups}
    jaw = groups.get("jaw") or low.vertex_groups.new(name="jaw")
    s = z.p["head"]
    hc = Vector(z.j["hc"])
    mo = z.p["mouth"]
    jw = z.p["jaw"]
    drop = 0.02 * (jw - 1)
    mz = -0.066 - drop * 0.4
    split = mz - mo * 0.45
    count = 0
    for v in low.data.vertices:
        loc = (v.co - hc) / s
        if abs(loc.x) > 0.075 or loc.z > split + 0.012 or loc.z < -0.17 or loc.y > 0.03:
            continue
        w = sculpt.smoothstep(split + 0.004, split - 0.006, loc.z)
        w *= sculpt.smoothstep(0.025, -0.005, loc.y)
        # Sous le menton, la peau suit à moitié ; le cou reste en place.
        under = sculpt.smoothstep(-0.115 - drop, -0.09 - drop, loc.z)
        w *= 0.35 + 0.65 * under
        w *= sculpt.smoothstep(-0.16, -0.12, loc.z)
        if w <= 0.01:
            continue
        for g in v.groups:
            g.weight *= (1.0 - w)
        jaw.add([v.index], float(w), "REPLACE")
        count += 1
    # Normalise : la somme des poids de chaque sommet vaut 1.
    select_only(low)
    bpy.ops.object.vertex_group_normalize_all(lock_active=False)
    missing = sum(1 for v in low.data.vertices if not any(g.weight > 0.001 for g in v.groups))
    print("  poids : %d sommets sur la mâchoire, %d sans os" % (count, missing))


# ---------------------------------------------------------------- export

def export_gltf(objs, path):
    """Écrit un .gltf, son .bin et ses textures JPEG (dossier textures/) : Godot importe les
    textures séparément, sans les dupliquer dans le dépôt."""
    select_only(*objs)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=path, export_format="GLTF_SEPARATE", export_texture_dir="textures",
        use_selection=True, export_yup=True,
        export_apply=False, export_texcoords=True, export_normals=True, export_tangents=False,
        export_materials="EXPORT", export_image_format="JPEG", export_jpeg_quality=88,
        export_skins=True, export_def_bones=True, export_influence_nb=4, export_morph=False,
        export_animations=True, export_animation_mode="ACTIONS", export_force_sampling=True,
        export_frame_step=1, export_optimize_animation_size=True, export_anim_single_armature=True,
        export_reset_pose_bones=True, export_cameras=False, export_lights=False)
    total = os.path.getsize(path) + os.path.getsize(path.replace(".gltf", ".bin"))
    print("  écrit %s (%.0f Ko sans les textures)" % (os.path.relpath(path, ROOT), total / 1024))


def emissive_faces(low, mark, material_index):
    """Les faces dans le volume mark reçoivent un second matériau (lumineux)."""
    me = low.data
    cents = np.zeros(len(me.polygons) * 3, np.float32)
    me.polygons.foreach_get("center", cents)
    d = mark.at(cents.reshape(-1, 3))
    idx = np.zeros(len(me.polygons), np.int32)
    idx[d < 0.006] = material_index
    me.polygons.foreach_set("material_index", idx)
    return int((idx == material_index).sum())


def build_zombie(name, h, preview=None):
    t0 = time.time()
    reset_scene()
    high, z = zombie_high(name, h)
    low = make_low(high, 11000 if name == "boss" else 8000, "zombie_" + name)
    unwrap(low, z.j)
    print("%s : version allégée %d faces" % (name, len(low.data.polygons)))
    size = 2048 if name == "boss" else 1024
    base, orm, nrm = bake_textures(high, low, "zombie_" + name, size)
    low.data.materials.clear()
    low.data.materials.append(game_material("zombie_" + name, base, orm, nrm))
    if "sac" in z.marks:
        low.data.materials.append(game_material("zombie_%s_poche" % name, base, orm, nrm, emissive=(0.6, 0.9, 0.2)))
        print("  poche lumineuse : %d faces" % emissive_faces(low, z.marks["sac"], 1))
    arm = build_armature(z, "zombie_" + name)
    skin_mesh(low, arm, z)
    made = zombie_anims.make_actions(arm, name)
    print("  animations : " + ", ".join("%s (%.2f s%s)" % (n, d, ", boucle" if l else "") for n, (d, l) in made.items()))
    bpy.data.objects.remove(high)
    export_gltf([arm, low], os.path.join(OUT_DIR, "zombie_%s.gltf" % name))
    if preview:
        preview_zombie(low, z, preview, "_jeu")
    print("%s terminé en %.0f s" % (name, time.time() - t0))


# ---------------------------------------------------------------- aperçus (rendus Cycles)

def setup_preview_scene():
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 24
    scene.cycles.device = "CPU"
    scene.cycles.use_denoising = False
    scene.render.film_transparent = False
    world = bpy.data.worlds.new("w") if scene.world is None else scene.world
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.22, 0.22, 0.24, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.7
    if "key" not in bpy.data.objects:
        key = link(bpy.data.objects.new("key", bpy.data.lights.new("key", "SUN")))
        key.data.energy = 3.5
        key.rotation_euler = (math.radians(50), 0, math.radians(-35))
        fill = link(bpy.data.objects.new("fill", bpy.data.lights.new("fill", "SUN")))
        fill.data.energy = 1.2
        fill.rotation_euler = (math.radians(70), 0, math.radians(120))
    return scene


def render_views(path, target, height, views, lens=60, res=(420, 630), dist=None):
    scene = setup_preview_scene()
    scene.render.resolution_x, scene.render.resolution_y = res
    cam_data = bpy.data.cameras.new("cam")
    cam_data.lens = lens
    cam = link(bpy.data.objects.new("cam", cam_data))
    scene.camera = cam
    files = []
    d = dist if dist else 4.6 * height / 1.9
    for angle, label in views:
        a = math.radians(angle)
        cam.location = Vector(target) + Vector((math.sin(a) * d, -math.cos(a) * d, 0))
        cam.rotation_euler = (math.radians(90), 0, a)
        f = path.replace(".png", "_%s.png" % label)
        scene.render.filepath = f
        bpy.ops.render.render(write_still=True)
        files.append(f)
    bpy.data.objects.remove(cam)
    return files


def build_weapon(name, preview=None):
    """Pistolet ou fusil : pièces nommées et repères, écrits dans assets/models/arme_<nom>.gltf."""
    t0 = time.time()
    reset_scene()
    parts, marks = weapon_models.WEAPONS[name]()
    weapon_models.export_weapon(name, parts, marks, OUT_DIR)
    if preview:
        os.makedirs(preview, exist_ok=True)
        center = Vector((0, 0.055, -0.035)) if name == "pistolet" else Vector((0, 0.14, -0.04))
        dist = 0.8 if name == "pistolet" else 2.4
        render_views(os.path.join(preview, "arme_%s.png" % name), center, 1.0,
                     ((90, "droite"), (-90, "gauche"), (20, "tireur"), (135, "avant")), lens=60, res=(640, 400), dist=dist)
    print("%s terminé en %.0f s" % (name, time.time() - t0))


def preview_zombie(obj, z, folder, tag=""):
    os.makedirs(folder, exist_ok=True)
    top = z.j["head_top"][2]
    base = os.path.join(folder, z.name + tag + ".png")
    files = render_views(base, (0, 0, top * 0.52), top, ((0, "face"), (90, "profil"), (35, "trois-quarts"), (180, "dos")))
    hc = z.j["hc"]
    files += render_views(base.replace(".png", "_tete.png"), hc, 0.3, ((0, "face"), (40, "biais"), (90, "profil")), lens=85, res=(400, 400), dist=0.75)
    return files


def write_game_data():
    """Écrit scripts/zombie_models.gd : les repères des modèles dont le jeu a besoin.
    Blender (X, Y, Z) devient (X, Z, -Y) dans Godot, comme dans le fichier glTF."""

    def gd(v):
        return "Vector3(%.4f, %.4f, %.4f)" % (v[0], v[2], -v[1])

    lines = [
        "## Généré par tools/make_models.py : ne pas modifier à la main.",
        "## Repères des modèles de zombies, en mètres dans l'espace du modèle importé (Y vers le haut,",
        "## le zombie regarde vers +Z). walk et run : vitesse au sol d'un cycle d'animation joué à",
        "## vitesse normale, pour un modèle à l'échelle 1 (0 = pas d'animation de course).",
        "",
        "## Moment où le coup porte dans l'animation \"attack\" et où part le crachat dans \"spit\"",
        "## (fraction de la durée de l'animation).",
        "const ATTACK_HIT := 0.42",
        "const SPIT_RELEASE := 0.5",
        "",
        "const DATA := {",
    ]
    for name, p in sculpt.TYPES.items():
        j = sculpt.joints(p)
        s = p["head"]
        st = zombie_anims.STYLES[name]
        walk = st.get("slow", st)
        run = st["stride"] / st["T"] if st.get("run") else 0.0
        eyes = [j["hc"] + sculpt.vec((sx * 0.032, -0.0795, 0.001)) * s for sx in (1.0, -1.0)]
        lines.append('\t"%s": {"height": %.3f, "eyes": [%s, %s], "eye_radius": %.4f, "walk": %.3f, "run": %.3f},' % (
            name, j["head_top"][2], gd(eyes[0]), gd(eyes[1]), 0.0135 * s, walk["stride"] / walk["T"], run))
    lines.append("}")
    path = os.path.join(ROOT, "scripts", "zombie_models.gd")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("repères écrits dans scripts/zombie_models.gd")


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    args = {"only": None, "preview": None, "stage": "all", "h": 0.003}
    k = 0
    while k < len(argv):
        if argv[k] == "--only":
            args["only"] = argv[k + 1].split(",")
            k += 1
        elif argv[k] == "--preview":
            args["preview"] = argv[k + 1]
            k += 1
        elif argv[k] == "--stage":
            args["stage"] = argv[k + 1]
            k += 1
        elif argv[k] == "--h":
            args["h"] = float(argv[k + 1])
            k += 1
        k += 1
    return args


def main():
    args = parse_args()
    names = args["only"] or list(sculpt.TYPES.keys()) + list(weapon_models.WEAPONS.keys())
    for name in names:
        if name in weapon_models.WEAPONS:
            build_weapon(name, args["preview"])
            continue
        if args["stage"] == "shape":
            reset_scene()
            obj, z = zombie_high(name, args["h"])
            obj.data.materials.append(preview_material())
            if args["preview"]:
                preview_zombie(obj, z, args["preview"])
            continue
        build_zombie(name, args["h"], args["preview"])
    if args["stage"] != "shape" and any(n in sculpt.TYPES for n in names):
        write_game_data()


if __name__ == "__main__":
    main()
