"""Sculpture des zombies par champs de distance (voir sdf.py) : anatomie, vêtements, blessures,
puis couleurs, rugosité et relief fin par sommet. Aucune dépendance à Blender.

Repère : Z vers le haut, le zombie regarde vers -Y, sa gauche est vers +X. Unités en mètres,
pose de référence en A (bras écartés à 45 degrés), environ 1,80 m pour un rôdeur.
"""

import numpy as np
from sdf import (F32, Capsule, Ellipsoid, Grid, Layer, Prim, RoundBox, RoundCone, Sphere,
                 basis, fbm, noise, normalize, rot, smoothstep, surface_nets, union_layers, vec)

UP = vec((0, 0, 1))
FRONT = vec((0, -1, 0))
BACK = vec((0, 1, 0))
OUT = vec((1, 0, 0))  # vers l'extérieur pour le côté gauche (+X)

# Réglages de chaque type. bulk : carrure, gaunt : maigreur (0 à 1), arms/legs : longueur,
# belly : ventre (m), jaw : taille de la mâchoire, mouth : ouverture de la bouche (m).
# shirt : manches (0 = débardeur, 0.35 = t-shirt, 1 = manches longues), None = torse nu.
# pants : 1 = pantalon long, 0.45 = pantacourt. shoes : None = pieds nus, "shoe", "boot", "sneaker".
TYPES = {
    "rodeur": {
        "seed": 11, "bulk": 1.0, "shoulders": 1.0, "arms": 1.0, "legs": 1.0, "belly": 0.0,
        "neck": 1.0, "head": 1.0, "jaw": 1.0, "mouth": 0.006, "gaunt": 0.55,
        "shirt": 1.0, "pants": 1.0, "shoes": "shoe", "overalls": False,
        "skin": (0.56, 0.6, 0.5), "shirt_col": (0.42, 0.4, 0.35), "pants_col": (0.16, 0.2, 0.28),
        "shoes_col": (0.13, 0.1, 0.08), "torn": 0.5, "blood": 0.6, "extras": ["wound_neck"],
    },
    # Deux autres rôdeurs, pour que la horde ne soit pas faite de clones.
    "rodeur_b": {
        "seed": 21, "bulk": 1.1, "shoulders": 1.04, "arms": 1.0, "legs": 0.98, "belly": 0.04,
        "neck": 1.1, "head": 1.02, "jaw": 1.08, "mouth": 0.012, "gaunt": 0.35,
        "shirt": 0.35, "pants": 1.0, "shoes": "sneaker", "overalls": False,
        "skin": (0.6, 0.57, 0.52), "shirt_col": (0.56, 0.54, 0.5), "pants_col": (0.33, 0.29, 0.21),
        "shoes_col": (0.3, 0.3, 0.32), "torn": 0.6, "blood": 0.85, "extras": [],
    },
    "rodeur_c": {
        "seed": 31, "bulk": 0.93, "shoulders": 0.97, "arms": 1.02, "legs": 1.02, "belly": 0.0,
        "neck": 0.95, "head": 0.98, "jaw": 0.95, "mouth": 0.004, "gaunt": 0.8,
        "shirt": 1.0, "pants": 1.0, "shoes": "boot", "overalls": False,
        "skin": (0.52, 0.55, 0.5), "shirt_col": (0.36, 0.12, 0.1), "pants_col": (0.13, 0.13, 0.14),
        "shoes_col": (0.1, 0.08, 0.06), "torn": 0.65, "blood": 0.5, "extras": ["wound_neck"],
    },
    "coureur": {
        "seed": 12, "bulk": 0.86, "shoulders": 0.94, "arms": 1.03, "legs": 1.05, "belly": 0.0,
        "neck": 0.9, "head": 0.97, "jaw": 1.0, "mouth": 0.009, "gaunt": 0.95,
        "shirt": 0.0, "pants": 1.0, "shoes": "sneaker", "overalls": False,
        "skin": (0.6, 0.58, 0.5), "shirt_col": (0.45, 0.13, 0.1), "pants_col": (0.12, 0.12, 0.14),
        "shoes_col": (0.62, 0.6, 0.56), "torn": 0.45, "blood": 0.7, "extras": ["ribs"],
    },
    "brute": {
        "seed": 13, "bulk": 1.42, "shoulders": 1.3, "arms": 1.03, "legs": 0.96, "belly": 0.05,
        "neck": 1.65, "head": 1.05, "jaw": 1.25, "mouth": 0.004, "gaunt": 0.1,
        "shirt": 0.35, "pants": 1.0, "shoes": "boot", "overalls": True,
        "skin": (0.55, 0.52, 0.46), "shirt_col": (0.5, 0.48, 0.44), "pants_col": (0.2, 0.26, 0.36),
        "shoes_col": (0.16, 0.11, 0.07), "torn": 0.5, "blood": 0.7, "extras": [],
    },
    "cracheur": {
        "seed": 14, "bulk": 0.95, "shoulders": 0.97, "arms": 1.0, "legs": 1.0, "belly": 0.02,
        "neck": 1.15, "head": 1.0, "jaw": 1.05, "mouth": 0.01, "gaunt": 0.7,
        "shirt": 1.0, "pants": 1.0, "shoes": "shoe", "overalls": False,
        "skin": (0.47, 0.56, 0.36), "shirt_col": (0.3, 0.33, 0.27), "pants_col": (0.22, 0.21, 0.2),
        "shoes_col": (0.12, 0.1, 0.09), "torn": 0.55, "blood": 0.25, "extras": ["sac"],
    },
    "hurleur": {
        "seed": 15, "bulk": 0.82, "shoulders": 0.98, "arms": 1.18, "legs": 1.04, "belly": 0.0,
        "neck": 1.0, "head": 1.02, "jaw": 1.6, "mouth": 0.03, "gaunt": 1.0,
        "shirt": 0.3, "pants": 1.0, "shoes": None, "overalls": False,
        "skin": (0.74, 0.71, 0.68), "shirt_col": (0.3, 0.14, 0.11), "pants_col": (0.17, 0.15, 0.14),
        "shoes_col": (0.1, 0.09, 0.08), "torn": 0.8, "blood": 0.65, "extras": ["ribs"],
    },
    "fouisseur": {
        "seed": 16, "bulk": 1.08, "shoulders": 1.08, "arms": 1.08, "legs": 0.93, "belly": 0.0,
        "neck": 1.15, "head": 1.0, "jaw": 1.1, "mouth": 0.012, "gaunt": 0.75,
        "shirt": None, "pants": 0.55, "shoes": None, "overalls": False,
        "skin": (0.46, 0.4, 0.33), "shirt_col": (0.3, 0.25, 0.2), "pants_col": (0.26, 0.21, 0.15),
        "shoes_col": (0.2, 0.15, 0.1), "torn": 0.7, "blood": 0.3, "extras": ["claws", "dirt"],
    },
    "boss": {
        "seed": 17, "bulk": 1.5, "shoulders": 1.38, "arms": 1.1, "legs": 0.97, "belly": 0.03,
        "neck": 1.8, "head": 1.08, "jaw": 1.35, "mouth": 0.012, "gaunt": 0.15,
        "shirt": None, "pants": 0.62, "shoes": None, "overalls": False,
        "skin": (0.5, 0.43, 0.45), "shirt_col": (0.2, 0.18, 0.17), "pants_col": (0.19, 0.16, 0.15),
        "shoes_col": (0.09, 0.08, 0.08), "torn": 0.75, "blood": 0.9, "extras": ["spikes", "wound_neck"],
    },
}

ARM_DIR = normalize((0.7, -0.07, -0.7))


def joints(p):
    """Articulations du côté gauche (+X) et de l'axe ; le côté droit est leur reflet."""
    L, A, sh, b = p["legs"], p["arms"], p["shoulders"], p["bulk"]
    j = {}
    ankle_z = 0.085
    knee_z = ankle_z + 0.415 * L
    hip_z = knee_z + 0.43 * L
    P = hip_z + 0.04
    j["P"] = P
    j["ankle"] = vec((0.1, 0.025, ankle_z))
    j["knee"] = vec((0.098, -0.012, knee_z))
    j["hip"] = vec((0.09 * b ** 0.35, 0.0, hip_z))
    j["heel"] = vec((0.1, 0.07, 0.03))
    j["ball"] = vec((0.11, -0.105, 0.022))
    j["toe"] = vec((0.114, -0.175, 0.02))
    j["pelvis"] = vec((0, 0.01, P))
    j["spine"] = vec((0, 0.02, P + 0.12))
    j["chest"] = vec((0, 0.03, P + 0.3))
    j["neck"] = vec((0, 0.035, P + 0.51))
    j["head"] = vec((0, 0.015, P + 0.63))
    j["hc"] = vec((0, -0.005, P + 0.72))  # centre du crâne
    j["head_top"] = j["hc"] + vec((0, 0, 0.11 * p["head"]))
    j["clav"] = vec((0.02, -0.03, P + 0.485))
    j["shoulder"] = vec((0.172 * sh * b ** 0.15, 0.025, P + 0.485))
    j["elbow"] = j["shoulder"] + ARM_DIR * 0.29 * A
    fore = normalize(ARM_DIR + vec((0, -0.16, 0)))
    j["fore_dir"] = fore
    j["wrist"] = j["elbow"] + fore * 0.255 * A
    j["hand"] = j["wrist"] + fore * 0.095
    j["fingers"] = j["wrist"] + fore * 0.19
    return j


class Mirror(Prim):
    """Reflet d'un volume par rapport au plan X = 0 (côté droit)."""

    def __init__(self, prim):
        self.prim = prim
        self.lo = vec((-prim.hi[0], prim.lo[1], prim.lo[2]))
        self.hi = vec((-prim.lo[0], prim.hi[1], prim.hi[2]))

    def dist(self, X, Y, Z):
        return self.prim.dist(-X, Y, Z)


def both(layer, prim, k=0.0, op="add"):
    getattr(layer, op)(prim, k)
    getattr(layer, op)(Mirror(prim), k)


def along(a, b, t):
    return vec(a) + (vec(b) - vec(a)) * t


def frame(axis, hint=FRONT):
    return basis(axis, hint)


# ---------------------------------------------------------------- corps

class Zombie:
    def __init__(self, name, h=0.004):
        self.name = name
        self.p = TYPES[name]
        self.j = joints(self.p)
        self.rng = np.random.default_rng(self.p["seed"])
        self.h = h
        p, j = self.p, self.j
        span = abs(j["fingers"][0]) + 0.06
        top = j["head_top"][2] + 0.05 + (0.1 if "spikes" in p["extras"] else 0.0)
        depth = 0.25 * p["bulk"] + p["belly"] + (0.12 if "spikes" in p["extras"] else 0.0)
        self.grid = Grid((-span, -depth - 0.05, -0.01), (span, depth, top), h)
        self.layers = {}
        self.marks = {}  # volumes utiles pour les couleurs (bouche, plaies, sang...)

    def layer(self, name, lo, hi):
        L = Layer(self.grid, lo, hi, fill=0.1)
        self.layers[name] = L
        return L

    # -------------------------------------------------------- anatomie
    def build_body(self):
        p, j = self.p, self.j
        b, g, sh, P = p["bulk"], p["gaunt"], p["shoulders"], j["P"]
        g_lo, g_hi = self.grid.lo, self.grid.lo + self.grid.h * (self.grid.n - 1)
        B = self.layer("skin", g_lo, g_hi)
        bel = p["belly"]
        thin = 1.0 - 0.18 * g
        # Bassin, ventre, cage thoracique, ceinture scapulaire.
        B.add(Ellipsoid((0, 0.012, P - 0.03), (0.148 * b, 0.1 * b, 0.11)))
        B.add(Ellipsoid((0, -0.004 - bel * 0.4, P + 0.12), ((0.126 - 0.012 * g) * b, (0.094 - 0.012 * g) * b + bel, 0.14)), 0.05)
        B.add(Ellipsoid((0, 0.016, P + 0.27), (0.138 * b, 0.104 * b, 0.15)), 0.05)
        B.add(Ellipsoid((0, 0.02, P + 0.38), (0.15 * b * sh ** 0.5, 0.098 * b, 0.125)), 0.05)
        B.add(Ellipsoid((0, 0.03, P + 0.465), (0.165 * sh * b ** 0.6, 0.072 * b, 0.06)), 0.04)
        # Pectoraux, grands dorsaux, omoplates, fessiers.
        pec_r = rot(y=-0.32)
        both(B, Ellipsoid((0.07 * b, -0.068 * b, P + 0.395), (0.074 * b, 0.032 * b * (1 - 0.45 * g), 0.054), pec_r), 0.03)
        both(B, Ellipsoid((0.118 * b, 0.045, P + 0.3), (0.045 * b * thin, 0.066 * b, 0.12)), 0.04)
        both(B, Ellipsoid((0.085 * sh, 0.084 * b, P + 0.415), (0.058, 0.024, 0.075)), 0.025)
        both(B, Ellipsoid((0.066 * b, 0.072 * b, P - 0.085), (0.074 * b, 0.062 * b, 0.094)), 0.03)
        # Trapèzes : pente du cou vers les épaules.
        both(B, RoundCone((0.022, 0.05, P + 0.56), (0.15 * sh, 0.035, P + 0.497), 0.04 * b ** 0.5, 0.03 * b ** 0.5, (1.0, 0.75)), 0.045)
        # Cou, sterno-cléido-mastoïdiens, pomme d'Adam.
        n = p["neck"]
        B.add(RoundCone((0, 0.036, P + 0.47), (0, 0.014, P + 0.645), 0.062 * n ** 0.8, 0.052 * n ** 0.8, (1.0, 0.94)), 0.035)
        both(B, Capsule((0.05 * n ** 0.6, 0.022, P + 0.665), (0.018, -0.045 * n ** 0.4, P + 0.5), 0.011 * n ** 0.7), 0.012)
        B.add(Ellipsoid((0, -0.027 * n ** 0.6, P + 0.585), (0.01, 0.009, 0.017)), 0.01)
        # Clavicules (visibles chez les maigres).
        both(B, Capsule((0.022, -0.045, P + 0.488), (0.15 * sh, -0.012, P + 0.5), 0.011 + 0.002 * g), 0.012)
        self._head(B)
        self._arm(B)
        self._leg(B)
        if p["shirt"] is None:
            # Torse nu : sillon de la colonne et nombril.
            B.cut(Capsule((0, 0.112 * b, P + 0.02), (0, 0.118 * b, P + 0.43), 0.006), 0.02)
            B.cut(Sphere((0, -0.098 * b - bel, P + 0.1), 0.006), 0.006)
        if "ribs" in p["extras"] or g > 0.7:
            # Sous un haut, des côtes marquées feraient coller le tissu comme une combinaison.
            amount = 1.0 if "ribs" in p["extras"] else 0.5
            self._ribs(B, amount if p["shirt"] is None else amount * 0.35)
        self.skin = B
        return B

    def _head(self, B):
        p, j = self.p, self.j
        s = p["head"]
        jw = p["jaw"]
        g = p["gaunt"]
        hc = j["hc"]

        def H(x, y, z):
            return hc + vec((x, y, z)) * s

        B.add(Ellipsoid(H(0, 0.012, 0.015), vec((0.074, 0.092, 0.094)) * s), 0.03)
        B.add(Ellipsoid(H(0, -0.04, 0.025), vec((0.066, 0.06, 0.07)) * s), 0.02)
        B.add(Ellipsoid(H(0, -0.048, -0.035), vec((0.059, 0.058, 0.068)) * s), 0.025)
        # Mâchoire en U : branches, angles, menton.
        jaw_w = 0.054 * (1 + 0.25 * (jw - 1))
        drop = 0.02 * (jw - 1)
        chin = H(0, -0.077 - 0.01 * (jw - 1), -0.098 - drop)
        both(B, Capsule(H(jaw_w, 0.014, -0.07 - drop * 0.5), chin + vec((0.012, 0, 0)), 0.015 * jw ** 0.5), 0.02)
        both(B, Capsule(H(jaw_w + 0.004, 0.018, -0.022), H(jaw_w, 0.014, -0.07 - drop * 0.5), 0.013 * jw ** 0.5), 0.015)
        B.add(Ellipsoid(chin, vec((0.022, 0.016, 0.017)) * jw ** 0.7), 0.015)
        # Pommettes et arcades zygomatiques.
        both(B, Ellipsoid(H(0.051, -0.06, -0.012), (0.021, 0.026, 0.014)), 0.016)
        both(B, Capsule(H(0.052, -0.05, -0.012), H(0.068, -0.002, -0.008), 0.0085), 0.012)
        # Front et arcades sourcilières.
        B.add(Capsule(H(-0.046, -0.086, 0.024), H(0.046, -0.086, 0.024), 0.011, (1.0, 0.8), hint=(0, 0, 1)), 0.014)
        # Orbites creusées, paupières.
        both(B, Ellipsoid(H(0.032, -0.101, 0.002), (0.019, 0.018, 0.0135)), 0.008, op="cut")
        both(B, Ellipsoid(H(0.032, -0.0875, 0.0115), (0.0155, 0.0085, 0.006)), 0.004)
        both(B, Ellipsoid(H(0.032, -0.0885, -0.0085), (0.0145, 0.0075, 0.0048)), 0.004)
        # Tempes et joues creuses (plus marquées chez les maigres).
        both(B, Ellipsoid(H(0.083, -0.03, 0.02), (0.012, 0.024, 0.02)), 0.015, op="cut")
        both(B, Ellipsoid(H(0.063 - 0.004 * g, -0.066, -0.05), (0.012, 0.022, 0.018)), 0.015, op="cut")
        # Nez (rongé chez certains).
        B.add(RoundCone(H(0, -0.088, 0.012), H(0, -0.106, -0.026), 0.0075, 0.0095), 0.01)
        B.add(Sphere(H(0, -0.106, -0.032), 0.0115), 0.008)
        both(B, Sphere(H(0.0135, -0.098, -0.037), 0.0085), 0.006)
        both(B, Ellipsoid(H(0.0065, -0.106, -0.043), (0.0045, 0.006, 0.0032)), 0.002, op="cut")
        # Bouche : lèvres, fente ouverte, cavité sombre derrière.
        mo = p["mouth"]
        mz = -0.066 - drop * 0.4
        B.add(Ellipsoid(H(0, -0.088, mz + 0.006), (0.024, 0.01, 0.0065)), 0.008)
        B.add(Ellipsoid(H(0, -0.085 - 0.004 * (jw - 1), mz - 0.007 - mo), (0.021 * jw ** 0.5, 0.009, 0.0065)), 0.008)
        slit = Ellipsoid(H(0, -0.098, mz - mo * 0.5), (0.024 * (1 + 0.4 * (jw - 1)), 0.03, 0.003 + mo * 0.55))
        cavity = Ellipsoid(H(0, -0.062, mz - mo * 0.4), (0.027 * jw ** 0.5, 0.032, 0.012 + mo * 0.5))
        B.cut(slit, 0.004)
        B.cut(cavity, 0.006)
        self.marks["mouth"] = Ellipsoid(H(0, -0.075, mz - mo * 0.4), (0.03 * jw ** 0.5, 0.045, 0.016 + mo * 0.5))
        self.marks["mouth_c"] = H(0, -0.09, mz - mo * 0.5)
        # Oreilles (une déchirée chez certains).
        ear_r = rot(x=0.2, z=0.25)
        torn = self.rng.random() < 0.5
        both(B, Ellipsoid(H(0.071, 0.008, -0.01), (0.009, 0.021, 0.029), ear_r), 0.005)
        both(B, Ellipsoid(H(0.078, 0.009, -0.014), (0.0055, 0.011, 0.016), ear_r), 0.003, op="cut")
        if torn:
            B.cut(Ellipsoid(H(-0.08, 0.02, 0.02), (0.02, 0.02, 0.018)), 0.004)
        # Yeux laiteux, dents.
        E = self.layer("eyes", H(-0.06, -0.11, -0.02), H(0.06, -0.06, 0.025))
        both(E, Sphere(H(0.032, -0.0795, 0.001), 0.0125 * s))
        T = self.layer("teeth", H(-0.04, -0.1, mz - 0.03 - mo), H(0.04, -0.05, mz + 0.02))
        for row, z0 in ((0, mz + 0.0005), (1, mz - mo - 0.0015)):
            for k in range(8):
                a = (k - 3.5) / 3.5
                x = a * 0.019 * (1 + 0.3 * (jw - 1))
                y = -0.082 + 0.017 * a * a
                if self.rng.random() < 0.15:
                    continue
                hgt = 0.0045 * (1.2 if abs(a) < 0.4 else 1.0)
                c = H(x, y, z0 - (hgt if row == 0 else -hgt) * 0.6)
                T.add(RoundBox(c, (0.0024, 0.0022, hgt), 0.0012, rot(z=-a * 0.6)), 0.0)

    def _arm(self, B):
        p, j = self.p, self.j
        b, g = p["bulk"], p["gaunt"]
        mus = b * (1.0 - 0.3 * g)
        sh, el, wr = j["shoulder"], j["elbow"], j["wrist"]
        d = ARM_DIR
        fore = j["fore_dir"]
        ua = frame(d)
        both(B, Ellipsoid(sh + d * 0.032 + UP * 0.012 + OUT * 0.004, vec((0.052, 0.056, 0.074)) * b ** 0.9 * (1 - 0.15 * g), ua), 0.03)
        both(B, RoundCone(sh, el, 0.043 * b * (1 - 0.15 * g), 0.034 * b ** 0.8), 0.025)
        both(B, Ellipsoid(along(sh, el, 0.52) + FRONT * 0.017 * b, vec((0.029, 0.029, 0.072)) * mus, ua), 0.02)
        both(B, Ellipsoid(along(sh, el, 0.42) + BACK * 0.019 * b, vec((0.032, 0.029, 0.085)) * mus, ua), 0.02)
        both(B, Sphere(el + BACK * 0.012, 0.03 * b ** 0.7), 0.015)
        fa = frame(fore)
        both(B, RoundCone(el, wr, 0.039 * b * (1 - 0.15 * g), 0.024 * b ** 0.6, (1.0, 0.82), hint=FRONT), 0.02)
        both(B, Ellipsoid(along(el, wr, 0.27) + FRONT * 0.008 - OUT * 0.006, vec((0.034, 0.03, 0.07)) * mus, fa), 0.02)
        both(B, Ellipsoid(wr, (0.027 * b ** 0.5, 0.018 * b ** 0.5, 0.02), fa), 0.012)
        self._hand(B)

    def hand_frame(self):
        """Repère de la main gauche : w vers les doigts, v = dos de la main, u vers l'arrière."""
        fore = self.j["fore_dir"]
        back = normalize(vec((0.7, 0.0, 0.7)) - fore * np.dot(vec((0.7, 0.0, 0.7)), fore))
        u = np.cross(back, fore)
        return u, back, fore

    def _hand(self, B):
        p, j = self.p, self.j
        b = p["bulk"]
        hb = b ** 0.45
        u, v, w = self.hand_frame()
        wr = j["wrist"]
        R = np.stack([u, v, w], axis=1)
        palm_c = wr + w * 0.047 + v * 0.002
        both(B, RoundBox(palm_c, (0.041 * hb, 0.0125 * hb, 0.046), 0.011, R), 0.012)
        claws = "claws" in p["extras"]
        Nm = self.layer("nails", vec((-wr[0] - 0.25, wr[1] - 0.25, wr[2] - 0.25)), wr + 0.25)
        tips = []
        # Doigts : de l'index (devant, -u) à l'auriculaire (derrière, +u), légèrement repliés.
        for k, (off, length, curl) in enumerate(((-0.028, 0.096, 0.35), (-0.009, 0.104, 0.42), (0.01, 0.098, 0.48), (0.027, 0.08, 0.55))):
            base = wr + w * 0.09 + u * off * hb
            seg = (0.45, 0.3, 0.25)
            rad = (0.0095, 0.0083, 0.0072)
            pos = base
            dirn = w + u * off * 0.6
            ang = 0.0
            for s_i in range(3):
                ang += curl * (0.6 + 0.4 * s_i)
                dn = normalize(normalize(dirn) * np.cos(ang) - v * np.sin(ang))
                nxt = pos + dn * length * seg[s_i]
                r0, r1 = rad[s_i] * hb, (rad[s_i + 1] if s_i < 2 else rad[2] * 0.85) * hb
                both(B, RoundCone(pos, nxt, r0, r1), 0.004 if s_i else 0.008)
                pos = nxt
            tips.append((pos, dn))
            if claws:
                both(Nm, RoundCone(pos - dn * 0.006, pos + dn * 0.045, 0.0065, 0.0012), 0.003)
            else:
                both(Nm, RoundBox(pos - dn * 0.006 + v * 0.0045, (0.0055, 0.0018, 0.007), 0.0015, np.stack([np.cross(v, dn), v, dn], axis=1)), 0.0)
        # Pouce, vers l'avant et vers la paume.
        tb = wr + w * 0.022 - u * 0.03 * hb - v * 0.006
        t1 = tb + normalize(w * 0.7 - u * 0.6 - v * 0.45) * 0.042
        t2 = t1 + normalize(w * 0.9 - u * 0.25 - v * 0.6) * 0.032
        t3 = t2 + normalize(w * 0.8 - u * 0.1 - v * 0.8) * 0.026
        both(B, RoundCone(tb, t1, 0.014 * hb, 0.0105 * hb), 0.012)
        both(B, RoundCone(t1, t2, 0.0105 * hb, 0.009 * hb), 0.004)
        both(B, RoundCone(t2, t3, 0.009 * hb, 0.0078 * hb), 0.003)
        if claws:
            both(Nm, RoundCone(t3 - (t3 - t2) * 0.2, t3 + normalize(t3 - t2) * 0.04, 0.007, 0.0012), 0.003)
        self.marks["fingertips"] = tips

    def _leg(self, B):
        p, j = self.p, self.j
        b, g = p["bulk"], p["gaunt"]
        mus = b * (1.0 - 0.3 * g)
        hip, knee, ankle = j["hip"], j["knee"], j["ankle"]
        th = frame(knee - hip)
        both(B, RoundCone(hip, knee, 0.083 * b * (1 - 0.15 * g), 0.051 * b ** 0.8), 0.045)
        both(B, Ellipsoid(along(hip, knee, 0.48) + FRONT * 0.022 * b + OUT * 0.008, vec((0.055, 0.045, 0.16)) * mus, th), 0.03)
        both(B, Ellipsoid(along(hip, knee, 0.82) + FRONT * 0.012 - OUT * 0.024, vec((0.03, 0.03, 0.05)) * mus, th), 0.02)
        both(B, Ellipsoid(along(hip, knee, 0.25) - OUT * 0.035 + BACK * 0.005, vec((0.045, 0.05, 0.1)) * b, th), 0.03)
        both(B, Ellipsoid(along(hip, knee, 0.5) + BACK * 0.026 * b, vec((0.05, 0.04, 0.15)) * mus, th), 0.03)
        both(B, Ellipsoid(knee, vec((0.047, 0.044, 0.04)) * b ** 0.5), 0.02)
        both(B, Sphere(knee + FRONT * 0.03, 0.025), 0.015)
        sh = frame(ankle - knee)
        both(B, RoundCone(knee, ankle, 0.048 * b ** 0.9, 0.03 * b ** 0.6), 0.025)
        both(B, Ellipsoid(along(knee, ankle, 0.3) + BACK * 0.03 * b - OUT * 0.004, vec((0.041, 0.035, 0.085)) * mus, sh), 0.025)
        both(B, Capsule(knee + FRONT * 0.028, ankle + FRONT * 0.017, 0.011), 0.02)
        both(B, Sphere(ankle + OUT * 0.024 + vec((0, 0.004, -0.005)), 0.013), 0.01)
        both(B, Sphere(ankle - OUT * 0.022 + vec((0, 0.0, 0.004)), 0.013), 0.01)
        # Pied nu : talon, voûte, orteils.
        heel, ball, toe = j["heel"], j["ball"], j["toe"]
        both(B, Sphere(heel + UP * 0.006, 0.031 * b ** 0.4), 0.02)
        fr = frame(ball - heel, UP)
        both(B, RoundBox(along(heel, ball, 0.5) + UP * 0.016 + OUT * 0.004, (0.024, 0.041 * b ** 0.4, 0.085), 0.02, fr), 0.025)
        both(B, RoundCone(ankle, along(heel, ball, 0.55) + UP * 0.02, 0.03, 0.03), 0.02)
        for k, off in enumerate((-0.03, -0.014, 0.0, 0.013, 0.025)):
            base = ball + OUT * off * 1.05 + UP * 0.006
            ln = (0.05, 0.042, 0.038, 0.033, 0.027)[k]
            r = (0.0115, 0.0085, 0.008, 0.0075, 0.007)[k]
            tip = base + normalize(FRONT + OUT * off * 0.6 - UP * 0.15) * ln
            both(B, RoundCone(base, tip, r, r * 0.85), 0.006)

    def _ribs(self, B, amount):
        """Côtes saillantes sur les flancs (torse maigre)."""
        p, j = self.p, self.j
        P, b = j["P"], p["bulk"]
        for k in range(6):
            z = P + 0.4 - k * 0.045
            r = 0.006 * amount
            a = vec((0.035, -0.095 * b + 0.004 * k, z + 0.01))
            m = vec((0.12 * b, -0.05 * b, z - 0.02))
            e = vec((0.14 * b, 0.03, z - 0.04))
            both(B, Capsule(a, m, r), 0.012)
            both(B, Capsule(m, e, r), 0.012)

    # -------------------------------------------------------- vêtements
    def build_clothes(self):
        p, j = self.p, self.j
        b, P = p["bulk"], j["P"]
        rng = self.rng
        seed = p["seed"]
        skin = self.skin
        if p["pants"] is not None:
            self._pants()
        if p["shirt"] is not None:
            self._shirt()
        if p["overalls"]:
            self._overalls()
        if p["shoes"]:
            self._shoes()

    def _cloth(self, name, lo, hi, thick, hulls, region, folds, holes=None, under=None):
        """Vêtement = corps gonflé (plus des volumes « tombants »), plissé, limité à une zone.
        La zone n'est calculée que près de la peau : plus loin à l'intérieur, rien ne se voit."""
        L = self.layer(name, lo, hi)
        body = np.array(L.sub(self.skin))
        base = body if under is None else np.minimum(body, L.sub(under))
        S = (base - F32(thick)).astype(F32)
        for hp in hulls:
            sl, P = L._block(hp.lo, hp.hi)
            if sl is not None:
                S[sl] = np.minimum(S[sl], hp.dist(*P))
        X, Y, Z = L.points()
        band = np.nonzero(np.abs(S) < 0.025)
        xs, ys, zs = X[:, 0, 0][band[0]], Y[0, :, 0][band[1]], Z[0, 0, :][band[2]]
        S[band] -= folds(xs, ys, zs).astype(F32)
        near = np.nonzero((S < 0.03) & (body > -0.02))
        xs, ys, zs = X[:, 0, 0][near[0]], Y[0, :, 0][near[1]], Z[0, 0, :][near[2]]
        R = region(xs, ys, zs)
        if holes is not None:
            R = np.maximum(R, holes(xs, ys, zs))
        S[near] = np.maximum(S[near], R.astype(F32))
        L.a = S
        return L

    def _pants(self):
        p, j = self.p, self.j
        b, P, seed = p["bulk"], j["P"], p["seed"]
        hip, knee, ankle = j["hip"], j["knee"], j["ankle"]
        length = p["pants"]
        hem_z = ankle[2] + 0.025 + (1.0 - length) * (hip[2] - ankle[2])
        waist = P + 0.035
        hulls = []
        for s in (1, -1):
            hulls.append(RoundCone(hip * vec((s, 1, 1)) + vec((0, 0, 0.02)), ankle * vec((s, 1, 1)) + UP * 0.03, 0.09 * b, 0.058 * b ** 0.5))
        hulls.append(Ellipsoid((0, 0.015, P - 0.04), (0.158 * b, 0.112 * b, 0.12)))
        torn = p["torn"]

        def folds(X, Y, Z):
            leg = smoothstep(waist - 0.1, waist - 0.25, Z)
            # Plis verticaux qui tombent, plis en accordéon au genou et à la cheville.
            v = fbm(X * 0.6, Y * 0.6, Z * 0.08, 18.0, 3, seed) * 0.006 * leg
            knee_band = np.exp(-((Z - j["knee"][2]) / 0.06) ** 2)
            ank = np.exp(-((Z - (hem_z + 0.06)) / 0.07) ** 2) if length >= 1.0 else 0.0
            wob = fbm(X, Y, Z, 9.0, 2, seed + 3)
            acc = (np.sin(Z * 140.0 + wob * 9.0 + np.arctan2(Y, np.abs(X) - 0.1) * 1.5) * 0.5 + 0.5) * 0.003 * (knee_band + ank) * (0.4 + 0.6 * np.abs(wob) * 2)
            waist_g = np.exp(-((Z - waist + 0.03) / 0.025) ** 2) * 0.002
            return v + acc + waist_g + 0.0015

        def region(X, Y, Z):
            top = Z - F32(waist) + fbm(X, Y, Z, 6.0, 2, seed + 5) * F32(0.004)
            edge = fbm(X, Y, Z, 14.0, 3, seed + 7) * F32(0.02 * torn)
            bottom = F32(hem_z) - Z + edge
            side = np.abs(X) - F32(0.3 * b)
            return np.maximum(np.maximum(top, bottom), side)

        def holes(X, Y, Z):
            n = fbm(X, Y, Z, 7.0, 3, seed + 11)
            return (n - F32(0.62 - 0.25 * torn)) * F32(0.08)

        lo = vec((-0.32 * b, -0.2 * b - 0.05, hem_z - 0.06))
        hi = vec((0.32 * b, 0.2 * b, waist + 0.03))
        self.pants = self._cloth("pants", lo, hi, 0.007, hulls, region, folds, holes if torn > 0.6 else None)

    def _shirt(self):
        p, j = self.p, self.j
        b, P, seed, sh = p["bulk"], j["P"], p["seed"], p["shoulders"]
        sleeve = p["shirt"]
        torn = p["torn"]
        hem = P - 0.06
        shoulder, elbow, wrist = j["shoulder"], j["elbow"], j["wrist"]
        arm_len = np.linalg.norm(elbow - shoulder) + np.linalg.norm(wrist - elbow)
        hulls = [Ellipsoid((0, 0.012, P + 0.17), (0.152 * b, 0.11 * b + p["belly"], 0.25))]
        reach = sleeve * arm_len
        upper = np.linalg.norm(elbow - shoulder)
        if sleeve > 0.05:
            for s in (1, -1):
                m = vec((s, 1, 1))
                end = shoulder + ARM_DIR * min(reach, upper)
                hulls.append(RoundCone(shoulder * m + vec((0, 0, -0.01)), end * m, 0.052 * b, 0.046 * b))
                if reach > upper:
                    hulls.append(RoundCone(elbow * m, (elbow + j["fore_dir"] * (reach - upper)) * m, 0.046 * b, 0.04 * b))

        def folds(X, Y, Z):
            n1 = fbm(X, Y, Z, 16.0, 3, seed + 21) * 0.005
            # Plis horizontaux sur le ventre (tissu tassé) et au creux des coudes.
            wob = fbm(X, Y, Z, 8.0, 2, seed + 22)
            belly_f = (np.sin(Z * 110.0 + wob * 10.0 + X * 25.0) * 0.5 + 0.5) * 0.0025 * np.exp(-((Z - P - 0.06) / 0.07) ** 2) * (0.3 + np.abs(wob) * 1.5)
            ex = np.abs(X)
            elbow_d = np.sqrt((ex - elbow[0]) ** 2 + (Y - elbow[1]) ** 2 + (Z - elbow[2]) ** 2)
            elbow_f = (np.sin(elbow_d * 180.0) * 0.5 + 0.5) * 0.004 * np.exp(-(elbow_d / 0.07) ** 2)
            return n1 + belly_f + elbow_f + 0.002

        def region(X, Y, Z):
            ex = np.abs(X)
            body = np.maximum(F32(hem) - Z + fbm(X, Y, Z, 12.0, 3, seed + 23) * F32(0.025 * torn + 0.004), ex - F32(0.205 * b * sh))
            # Encolure ronde, un peu plus basse devant.
            neck = Ellipsoid((0, 0.0, P + 0.585), (0.085 * p["neck"] ** 0.6, 0.1, 0.075))
            body = np.maximum(body, -neck.dist(X, Y, Z))
            if sleeve > 0.05:
                arm_axis = ARM_DIR
                rel_x = ex - F32(shoulder[0])
                t = rel_x * F32(arm_axis[0]) + (Y - F32(shoulder[1])) * F32(arm_axis[1]) + (Z - F32(shoulder[2])) * F32(arm_axis[2])
                cut = t - F32(reach) + fbm(X, Y, Z, 15.0, 3, seed + 25) * F32(0.02 * torn + 0.003)
                tube = np.minimum(Capsule(shoulder, elbow, 0.095 * b).dist(ex, Y, Z), Capsule(elbow, wrist, 0.085 * b).dist(ex, Y, Z))
                body = np.minimum(body, np.maximum(cut, tube))
            return body

        def holes(X, Y, Z):
            n = fbm(X, Y, Z, 6.0, 3, seed + 27)
            return (n - F32(0.5 - 0.2 * torn)) * F32(0.08)

        span = shoulder[0] + reach + 0.1 if sleeve > 0.05 else 0.25 * b * sh
        lo = vec((-span, -0.17 * b - p["belly"] - 0.03, hem - 0.06))
        hi = vec((span, 0.17 * b, P + 0.6))
        self.shirt = self._cloth("shirt", lo, hi, 0.006, hulls, region, folds, holes if torn > 0.3 else None)

    def _overalls(self):
        p, j = self.p, self.j
        b, P, seed = p["bulk"], j["P"], p["seed"]
        hip, ankle = j["hip"], j["ankle"]
        hulls = []
        for s in (1, -1):
            hulls.append(RoundCone(hip * vec((s, 1, 1)) + UP * 0.02, ankle * vec((s, 1, 1)) + UP * 0.03, 0.095 * b, 0.062 * b ** 0.5))
        hulls.append(Ellipsoid((0, 0.012, P + 0.1), (0.16 * b, 0.12 * b + p["belly"], 0.22)))

        def folds(X, Y, Z):
            return fbm(X * 0.7, Y * 0.7, Z * 0.15, 14.0, 3, seed + 31) * 0.006 + 0.003

        def region(X, Y, Z):
            ex = np.abs(X)
            legs = np.maximum(F32(ankle[2] + 0.03) - Z + fbm(X, Y, Z, 12, 2, seed + 33) * F32(0.012), Z - F32(P + 0.06))
            legs = np.maximum(legs, ex - F32(0.33 * b))
            bib = np.maximum(np.maximum(ex - F32(0.1 * b), Z - F32(P + 0.36)), Y - F32(-0.02))
            back = np.maximum(np.maximum(ex - F32(0.12 * b), Z - F32(P + 0.2)), F32(0.0) - Y)
            straps = []
            for s in (1.0, -1.0):
                a = vec((0.075 * b * s, -0.11 * b, P + 0.34))
                m = vec((0.105 * b * s, 0.0, P + 0.53))
                c = vec((0.06 * b * s, 0.11 * b, P + 0.2))
                straps.append(Capsule(a, m, 0.022).dist(X, Y, Z))
                straps.append(Capsule(m, c, 0.022).dist(X, Y, Z))
            r = np.minimum(np.minimum(legs, bib), back)
            for st in straps:
                r = np.minimum(r, st)
            return r

        lo = vec((-0.34 * b, -0.2 * b - 0.08, ankle[2] - 0.03))
        hi = vec((0.34 * b, 0.2 * b, P + 0.6))
        L = self._cloth("overalls", lo, hi, 0.012, hulls, region, folds, under=self.layers.get("shirt"))
        # Boucles métalliques des bretelles.
        M = self.layer("metal", vec((-0.15 * b, -0.2 * b - 0.05, P + 0.25)), vec((0.15 * b, -0.02, P + 0.42)))
        for s in (1.0, -1.0):
            c = vec((0.075 * b * s, -0.11 * b, P + 0.34))
            M.add(RoundBox(c + vec((0, -0.012, 0)), (0.016, 0.004, 0.012), 0.003))
        self.overalls = L

    def _shoes(self):
        p, j = self.p, self.j
        b = p["bulk"]
        kind = p["shoes"]
        ankle, heel, toe, ball = j["ankle"], j["heel"], j["toe"], j["ball"]
        lo = vec((-0.25, -0.26, -0.01))
        hi = vec((0.25, 0.16, 0.26 if kind == "boot" else 0.17))
        U = self.layer("shoes", lo, hi)
        S = self.layer("soles", lo, vec((0.25, 0.16, 0.05)))
        w = 0.046 * b ** 0.3
        collar = 0.11 if kind == "boot" else 0.035
        for s in (1, -1):
            m = vec((s, 1, 1))
            fr = frame((ball - heel) * m, UP)
            hl, tt = (heel + vec((0, 0.012, 0))) * m, (toe + vec((0, -0.012, 0))) * m
            mid = along(hl, tt, 0.5)
            U.add(RoundBox(mid + UP * 0.04, (0.03, w, 0.135), 0.026, fr))
            U.add(Ellipsoid(along(hl, tt, 0.38) + UP * 0.078, (w * 0.9, 0.075, 0.04)), 0.03)
            U.add(Ellipsoid(along(hl, tt, 0.86) + UP * 0.038, (w * 0.95, 0.06, 0.03)), 0.02)
            U.add(Ellipsoid(hl + UP * 0.05 + FRONT * 0.012, (w * 0.88, 0.04, 0.045)), 0.02)
            U.add(RoundCone(ankle * m + vec((0, 0.006, -0.03)), ankle * m + UP * collar, 0.046 * b ** 0.3, 0.043 * b ** 0.3), 0.025)
            # Ouverture autour de la cheville, languette, lacets.
            U.cut(RoundCone(ankle * m + UP * (collar - 0.012), ankle * m + UP * 0.3, 0.035 * b ** 0.4, 0.035 * b ** 0.4), 0.004)
            for k in range(4):
                c = along(hl, tt, 0.45 + 0.08 * k) + UP * (0.105 - 0.017 * k)
                U.add(Capsule(c - OUT * 0.022, c + OUT * 0.022, 0.0035), 0.002)
            S.add(RoundBox(mid + vec((0, 0.002, 0.012)), (0.012, w + 0.004, 0.137), 0.009, fr))
            S.add(RoundBox(hl + vec((0, 0.01, 0.016)), (0.016, w + 0.003, 0.036), 0.009, fr), 0.01)
        self.shoes = U

    # -------------------------------------------------------- extras
    def build_extras(self):
        p, j = self.p, self.j
        b, P = p["bulk"], j["P"]
        ex = p["extras"]
        rng = self.rng
        B = self.skin
        if "sac" in ex:
            n = p["neck"]
            c = vec((0, -0.05 * n ** 0.6, P + 0.585))
            Sc = self.layer("sac", c - 0.09, c + 0.09)
            Sc.add(Ellipsoid(c + vec((0, -0.012, 0)), (0.05, 0.043, 0.048)))
            Sc.add(Ellipsoid(c + vec((0.02, -0.03, 0.015)), (0.022, 0.02, 0.022)), 0.012)
            Sc.add(Ellipsoid(c + vec((-0.022, -0.025, -0.01)), (0.02, 0.018, 0.02)), 0.012)
            Sc.displace(c - 0.09, c + 0.09, lambda X, Y, Z: fbm(X, Y, Z, 60.0, 2, 3) * 0.002, 0.02)
            self.marks["sac"] = Ellipsoid(c + vec((0, -0.012, 0)), (0.058, 0.05, 0.056))
        if "spikes" in ex:
            Bo = self.layer("bone", self.grid.lo, self.grid.lo + self.grid.h * (self.grid.n - 1))
            for k in range(6):
                t = k / 5.0
                z = P + 0.05 + 0.42 * t
                for s in (1.0, -1.0):
                    base = vec((0.035 * s, 0.085 * b + 0.01 * np.sin(t * 3), z))
                    tip = base + normalize((0.45 * s, 1.0, 0.35)) * (0.07 + 0.04 * np.sin(t * np.pi) + 0.02 * rng.random())
                    Bo.add(RoundCone(base, tip, 0.016, 0.002), 0.004)
            sh = j["shoulder"]
            for s in (1.0, -1.0):
                for k in range(3):
                    base = vec((sh[0] * s - 0.03 * s + 0.025 * k * s, 0.01 + 0.01 * k, sh[2] + 0.035))
                    tip = base + normalize((0.4 * s, 0.25, 1.0)) * (0.09 - 0.015 * k)
                    Bo.add(RoundCone(base, tip, 0.017, 0.002), 0.004)
            self.bone = Bo
        # Plaies : morsures et chair arrachée.
        wounds = []
        if "wound_neck" in ex:
            c = vec((-0.045 * p["neck"] ** 0.7, -0.01, P + 0.56))
            wounds.append(Ellipsoid(c, (0.03, 0.03, 0.04)))
        for k in range(3 if p["shirt"] is None else 2):
            # Une sur un avant-bras, une sur un mollet ou le torse.
            pick = k % 3
            if pick == 0:
                side = 1 if rng.random() < 0.5 else -1
                c = along(j["elbow"], j["wrist"], 0.4 + 0.3 * rng.random()) * vec((side, 1, 1)) + FRONT * 0.025
                wounds.append(Ellipsoid(c, (0.024, 0.024, 0.024)))
            elif pick == 1:
                side = 1 if rng.random() < 0.5 else -1
                c = vec((0.09 * side * b, -0.07 * b, P + 0.2 + 0.1 * rng.random()))
                wounds.append(Ellipsoid(c, (0.035, 0.03, 0.03)))
            else:
                side = 1 if rng.random() < 0.5 else -1
                c = along(j["shoulder"], j["elbow"], 0.5) * vec((side, 1, 1)) + BACK * 0.03
                wounds.append(Ellipsoid(c, (0.03, 0.03, 0.03)))
        for wd in wounds:
            B.cut(wd, 0.008)
            B.displace(wd.lo - 0.02, wd.hi + 0.02, lambda X, Y, Z, wd=wd: fbm(X, Y, Z, 90.0, 2, 41) * 0.003 * np.exp(-np.maximum(wd.dist(X, Y, Z), 0) / 0.01), 0.02)
        self.marks["wounds"] = wounds
        # Relief fin de la peau : petites bosses, plis.
        B.displace(self.grid.lo, self.grid.lo + self.grid.h * (self.grid.n - 1),
                   lambda X, Y, Z: fbm(X, Y, Z, 45.0, 2, 51) * 0.0012, 0.01)

    # -------------------------------------------------------- maillage et couleurs
    def build(self):
        self.build_body()
        self.build_extras()
        self.build_clothes()
        F = union_layers(self.grid, list(self.layers.values()), 0.1)
        verts, quads = surface_nets(F, self.grid.lo, self.grid.h)
        del F
        return verts, quads

    def classify(self, verts):
        names = list(self.layers.keys())
        vals = np.stack([self.layers[n].sample(verts) for n in names], axis=1)
        idx = np.argmin(vals, axis=1)
        return names, idx

    def paint(self, verts, normals):
        """Couleur, rugosité et force du relief fin pour chaque sommet."""
        p, j = self.p, self.j
        P, b = j["P"], p["bulk"]
        names, idx = self.classify(verts)
        X, Y, Z = verts[:, 0], verts[:, 1], verts[:, 2]
        seed = p["seed"]
        n = len(verts)
        col = np.zeros((n, 3), F32)
        rough = np.full(n, F32(0.6))
        bump = np.zeros(n, F32)
        emit = np.zeros(n, F32)
        layer = np.array(names)[idx]

        def mix(a, c, t):
            t = np.clip(t, 0, 1)[:, None] if np.ndim(t) else t
            return a * (1 - t) + np.asarray(c, F32) * t

        big = fbm(X, Y, Z, 3.0, 3, seed + 61)
        mid = fbm(X, Y, Z, 11.0, 3, seed + 62)
        fine = fbm(X, Y, Z, 40.0, 2, seed + 63)
        dirt = np.clip(0.5 + 0.8 * mid + 0.5 * big - 0.4 * smoothstep(0.3, 1.4, Z), 0, 1)
        # Sang : autour de la bouche, coulures sur le menton, la poitrine et les mains.
        mc = self.marks["mouth_c"]
        d_mouth = np.sqrt((X - mc[0]) ** 2 + (Y - mc[1]) ** 2 + (Z - mc[2]) ** 2)
        drip = np.exp(-(np.abs(X) / (0.05 + 0.04 * (big + 1))) ** 2) * smoothstep(mc[2] + 0.01, mc[2] - 0.05, Z) * smoothstep(P + 0.05, P + 0.3, Z) * (Y < 0)
        drip *= np.clip(0.5 + np.sin(X * 90 + mid * 4) * 0.5 + mid, 0, 1)
        blood = np.clip(1.4 - d_mouth / 0.03, 0, 1) + drip * p["blood"]
        for tip, _d in self.marks["fingertips"]:
            dd = np.sqrt((np.abs(X) - tip[0]) ** 2 + (Y - tip[1]) ** 2 + (Z - tip[2]) ** 2)
            blood += np.clip(1.0 - dd / 0.07, 0, 1) * 0.6 * p["blood"]
        wound_m = np.zeros(n, F32)
        for wd in self.marks["wounds"]:
            dw = wd.at(verts)
            wound_m = np.maximum(wound_m, np.clip(1.0 - dw / 0.012, 0, 1))
            blood += np.clip(1.0 - dw / 0.045, 0, 1) * 0.7 * (0.6 + 0.4 * mid)
        splat = np.clip((fbm(X, Y, Z, 7.0, 3, seed + 64) - 0.25) * 4, 0, 1) * p["blood"]
        blood = np.clip(blood + splat * 0.8, 0, 1)

        def skin_col():
            base = np.asarray(p["skin"], F32)
            c = base[None, :] * (1.0 + 0.16 * mid[:, None] + 0.08 * fine[:, None])
            bruise = np.clip((fbm(X, Y, Z, 5.0, 3, seed + 65) - 0.15) * 2.5, 0, 1)
            c = mix(c, base * np.asarray((0.62, 0.5, 0.62), F32), bruise * 0.7)
            vein = np.clip(1.0 - np.abs(fbm(X, Y, Z, 22.0, 2, seed + 66)) * 16.0, 0, 1)
            vein *= np.clip(fbm(X, Y, Z, 4.0, 2, seed + 68) * 2.0 + 0.3, 0, 1)
            c = mix(c, base * np.asarray((0.5, 0.55, 0.68), F32), vein * 0.3)
            c = mix(c, (0.2, 0.15, 0.1), dirt * 0.45)
            return c

        m = layer == "skin"
        if m.any():
            c = skin_col()
            # Intérieur de la bouche sombre.
            dmo = self.marks["mouth"].at(verts)
            inner = np.clip(1.0 - (dmo + 0.004) / 0.006, 0, 1)
            c = mix(c, (0.12, 0.025, 0.025), inner)
            c = mix(c, (0.3, 0.035, 0.025), blood * 0.85)
            c = mix(c, (0.2, 0.02, 0.015), wound_m)
            col[m] = c[m]
            rough[m] = (0.55 - 0.3 * blood - 0.25 * wound_m)[m]
            bump[m] = 0.5
        for name, base_key, r0 in (("shirt", "shirt_col", 0.92), ("pants", "pants_col", 0.9), ("overalls", "pants_col", 0.88)):
            m = layer == name
            if not m.any():
                continue
            base = np.asarray(p[base_key], F32)
            c = base[None, :] * (0.82 + 0.2 * mid[:, None] + 0.1 * fine[:, None] + 0.12 * big[:, None])
            c = mix(c, (0.23, 0.18, 0.12), dirt * (0.55 if name != "shirt" else 0.35))
            stain = np.clip(blood * 1.3 - 0.15, 0, 1)
            c = mix(c, (0.22, 0.03, 0.02), stain * 0.85)
            col[m] = c[m]
            rough[m] = (r0 - 0.35 * stain)[m]
            bump[m] = 1.0
        m = layer == "shoes"
        if m.any():
            base = np.asarray(p["shoes_col"], F32)
            c = base[None, :] * (0.85 + 0.2 * mid[:, None])
            c = mix(c, (0.25, 0.2, 0.14), dirt * 0.5)
            col[m] = c[m]
            rough[m] = 0.55 if p["shoes"] != "sneaker" else 0.75
            bump[m] = 0.3
        m = layer == "soles"
        if m.any():
            c = np.tile(np.asarray((0.08, 0.075, 0.07) if p["shoes"] != "sneaker" else (0.55, 0.53, 0.5), F32), (n, 1))
            col[m] = mix(c, (0.2, 0.16, 0.12), dirt * 0.5)[m]
            rough[m] = 0.85
        m = layer == "eyes"
        if m.any():
            hc = j["hc"]
            ex = np.abs(X) - 0.032 * p["head"]
            front = np.clip((-(Y - (hc[1] - 0.077 * p["head"])) - 0.0085) / 0.003, 0, 1)
            c = np.tile(np.asarray((0.72, 0.7, 0.6), F32), (n, 1))
            c = mix(c, (0.55, 0.57, 0.55), front * 0.6)
            c = mix(c, (0.5, 0.12, 0.1), np.clip(np.abs(ex) / 0.012 - 0.5, 0, 1) * 0.4)
            col[m] = c[m]
            rough[m] = 0.1
        m = layer == "teeth"
        if m.any():
            col[m] = mix(np.tile(np.asarray((0.66, 0.58, 0.42), F32), (n, 1)), (0.35, 0.08, 0.05), blood * 0.4)[m]
            rough[m] = 0.35
        m = layer == "nails"
        if m.any():
            col[m] = np.asarray((0.3, 0.26, 0.2) if "claws" not in p["extras"] else (0.16, 0.13, 0.1), F32)
            rough[m] = 0.4
        m = layer == "bone"
        if m.any():
            c = np.tile(np.asarray((0.72, 0.66, 0.52), F32), (n, 1)) * (0.85 + 0.2 * mid[:, None])
            col[m] = mix(c, (0.3, 0.04, 0.03), smoothstep(0.03, 0.0, self.skin.sample(verts)) * 0.9)[m]
            rough[m] = 0.55
        m = layer == "sac"
        if m.any():
            c = np.tile(np.asarray((0.5, 0.68, 0.16), F32), (n, 1)) * (0.85 + 0.25 * mid[:, None])
            vein = np.clip(1.0 - np.abs(fbm(X, Y, Z, 30.0, 2, seed + 67)) * 7.0, 0, 1)
            c = mix(c, (0.2, 0.3, 0.06), vein * 0.6)
            col[m] = c[m]
            rough[m] = 0.25
            emit[m] = 1.0
        m = layer == "metal"
        if m.any():
            col[m] = np.asarray((0.5, 0.48, 0.44), F32)
            rough[m] = 0.35
        if "dirt" in p["extras"]:
            # Le Fouisseur sort de terre : couvert de boue.
            mud = np.clip(0.3 + 0.9 * mid + 0.6 * big - 0.25 * smoothstep(0.6, 1.6, Z), 0, 1)
            col = mix(col, (0.24, 0.19, 0.13), mud * 0.75)
            rough = rough * (1 - 0.3 * mud) + 0.9 * 0.3 * mud
        return np.clip(col, 0, 1), np.clip(rough, 0.05, 1), bump, emit, layer
