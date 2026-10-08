"""Animations des zombies, écrites par le code et cuites image par image sur le squelette.

Les jambes sont résolues par cinématique inverse (deux os) : chaque pied suit une trajectoire
au sol (appui qui recule à la vitesse de marche, puis pas levé), ce qui évite les pieds qui
glissent quand le jeu règle la vitesse de lecture sur la vitesse réelle du zombie.
Le reste du corps est piloté par des rotations exprimées dans les axes du personnage.
Repère : Z en haut, le zombie regarde vers -Y, sa gauche est vers +X.
"""

import math

import bpy
from mathutils import Matrix, Quaternion, Vector

FPS = 30
FRONT = Vector((0, -1, 0))


def qx(a):
    return Quaternion((1, 0, 0), a)


def qy(a):
    return Quaternion((0, 1, 0), a)


def qz(a):
    return Quaternion((0, 0, 1), a)


def chain(*qs):
    r = Quaternion()
    for q in qs:
        r = r @ q
    return r


def ease(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


def ramp(t, a, b):
    """0 avant a, 1 après b, transition douce entre les deux."""
    if b == a:
        return 1.0 if t >= b else 0.0
    return ease((t - a) / (b - a))


def bump(t, a, m, b):
    """Monte de a à m puis redescend jusqu'à b (forme en cloche)."""
    return ramp(t, a, m) * (1.0 - ramp(t, m, b))


def frame_matrix(a, h):
    a = a.normalized()
    h = (h - a * h.dot(a)).normalized()
    return Matrix((a, h, a.cross(h))).transposed()


# Allure de chaque type : penché, bras, boiterie, foulée (m par cycle de deux pas) et durée du cycle.
STYLES = {
    "rodeur": {"lean": 0.2, "hunch": 0.12, "arms": "reach", "limp": 0.55, "stride": 1.3, "T": 0.82,
               "lift": 0.07, "sway": 0.045, "tilt": 0.22, "heavy": 0.2},
    "rodeur_b": {"lean": 0.16, "hunch": 0.14, "arms": "hang", "limp": 0.25, "stride": 1.25, "T": 0.86,
                 "lift": 0.07, "sway": 0.05, "tilt": 0.3, "heavy": 0.35},
    "rodeur_c": {"lean": 0.24, "hunch": 0.1, "arms": "mixed", "limp": 0.75, "stride": 1.2, "T": 0.84,
                 "lift": 0.06, "sway": 0.04, "tilt": 0.18, "heavy": 0.15},
    "coureur": {"lean": 0.42, "hunch": 0.1, "arms": "flail", "limp": 0.1, "stride": 2.4, "T": 0.64,
                "lift": 0.16, "sway": 0.03, "tilt": 0.12, "heavy": 0.0, "run": True,
                # Allure lente (gelé, ralenti) : une vraie marche plutôt qu'une course au ralenti.
                "slow": {"stride": 1.4, "T": 0.95, "lift": 0.07, "arms": "hang"}},
    "brute": {"lean": 0.14, "hunch": 0.2, "arms": "hang", "limp": 0.15, "stride": 1.2, "T": 1.3,
              "lift": 0.09, "sway": 0.07, "tilt": 0.1, "heavy": 1.0},
    "cracheur": {"lean": 0.12, "hunch": 0.25, "arms": "mixed", "limp": 0.35, "stride": 1.25, "T": 0.82,
                 "lift": 0.07, "sway": 0.05, "tilt": 0.3, "heavy": 0.2},
    "hurleur": {"lean": 0.08, "hunch": 0.15, "arms": "hang", "limp": 0.25, "stride": 1.25, "T": 0.92,
                "lift": 0.08, "sway": 0.04, "tilt": 0.35, "heavy": 0.1},
    "fouisseur": {"lean": 0.55, "hunch": 0.3, "arms": "low", "limp": 0.0, "stride": 1.3, "T": 0.66,
                  "lift": 0.1, "sway": 0.035, "tilt": 0.15, "heavy": 0.3},
    "boss": {"lean": 0.12, "hunch": 0.18, "arms": "hang", "limp": 0.2, "stride": 0.75, "T": 1.6,
             "lift": 0.1, "sway": 0.06, "tilt": 0.08, "heavy": 1.0},
}


class Rig:
    """Squelette d'un zombie : conversions entre rotations « personnage » et poses Blender."""

    def __init__(self, arm):
        self.arm = arm
        bones = arm.data.bones
        self.order = [b.name for b in bones]
        self.rest = {b.name: b.matrix_local.to_quaternion() for b in bones}
        self.head = {b.name: b.head_local.copy() for b in bones}
        self.tail = {b.name: b.tail_local.copy() for b in bones}
        self.parent = {b.name: (b.parent.name if b.parent else None) for b in bones}
        # Ordre parent avant enfant.
        done, order = set(), []
        while len(order) < len(self.order):
            for n in self.order:
                if n not in done and (self.parent[n] is None or self.parent[n] in done):
                    done.add(n)
                    order.append(n)
        self.order = order
        self.ground = min(self.head["toe_L"].z, self.tail["toe_L"].z)

    def dir(self, n):
        return (self.tail[n] - self.head[n]).normalized()

    def arm_hinge(self, side):
        """Axe du coude au repos : fléchir rapproche l'avant-bras de l'avant du corps."""
        return self.dir("upper_arm_" + side).cross(FRONT).normalized()

    def wrist_hinge(self, side):
        sx = 1.0 if side == "L" else -1.0
        back = Vector((0.7 * sx, 0.0, 0.7))
        w = self.dir("hand_" + side)
        palm = -(back - w * back.dot(w)).normalized()
        return w.cross(palm).normalized()

    def foot_target(self, side, ref, lift, pitch, x_off=0.0):
        """Position de la cheville pour un pied dont le point d'appui (talon ou plante) est à ref.y
        au sol, levé de lift, incliné de pitch (positif : talon levé, pivot sur la plante)."""
        ankle, ball, toe = self.head["foot_" + side], self.head["toe_" + side], self.tail["toe_" + side]
        heel = Vector((ankle.x, ankle.y + 0.05, self.ground))
        if pitch >= 0:
            pivot = Vector((ball.x + x_off, ball.y + ref, ball.z + lift))
            return pivot + qx(pitch) @ (ankle - ball)
        pivot = Vector((heel.x + x_off, heel.y + ref, heel.z + lift))
        return pivot + qx(pitch) @ (ankle - heel)

    def solve(self, pose):
        """pose : {"root": Vector, "q": {os: rotation relative au parent, dans les axes du personnage},
        "feet": {côté: (cheville visée, rotation du pied)}, "toe": {côté: angle}, "pole": {côté: genou}}.
        Renvoie les rotations locales Blender, la translation du bassin et les rotations globales."""
        q = pose.get("q", {})
        feet = pose.get("feet", {})
        toe = pose.get("toe", {})
        root = pose.get("root", Vector())
        D, posed = {}, {}
        for n in self.order:
            par = self.parent[n]
            if par is None:
                D[n] = q.get(n, Quaternion())
                posed[n] = self.head[n] + root
                continue
            posed[n] = posed[par] + D[par] @ (self.head[n] - self.head[par])
            kind, side = n.rsplit("_", 1)[0], n[-1]
            if kind in ("thigh", "shin", "foot") and side in feet:
                if kind == "thigh":
                    self._leg(side, D, posed, feet[side], pose.get("pole", {}).get(side))
                continue
            if kind == "toe" and side in toe:
                D[n] = D[par] @ qx(toe[side])
                continue
            D[n] = D[par] @ q.get(n, Quaternion())
        out = {}
        for n in self.order:
            par = self.parent[n]
            rel = D[n] if par is None else D[par].inverted() @ D[n]
            r = self.rest[n]
            out[n] = r.inverted() @ rel @ r
        loc = self.rest[self.order[0]].inverted() @ root
        return out, loc, D

    def _leg(self, side, D, posed, target, pole_dir=None):
        th, sh, ft = "thigh_" + side, "shin_" + side, "foot_" + side
        sx = 1.0 if side == "L" else -1.0
        H = posed[th]
        L1 = (self.head[sh] - self.head[th]).length
        L2 = (self.head[ft] - self.head[sh]).length
        T, foot_q = target
        d = T - H
        dist = max(abs(L1 - L2) + 1e-4, min(d.length, (L1 + L2) * 0.9995))
        e = d.normalized()
        pole = pole_dir if pole_dir is not None else (D["hips"] @ Vector((0.12 * sx, -1.0, 0.0)))
        p = (pole - e * pole.dot(e))
        if p.length < 1e-6:
            p = Vector((0, -1, 0))
        p.normalize()
        a = (L1 * L1 - L2 * L2 + dist * dist) / (2 * dist)
        hk = math.sqrt(max(L1 * L1 - a * a, 0.0))
        K = H + e * a + p * hk
        T2 = H + e * dist
        a0 = self.dir(th)
        b0 = self.dir(sh)
        h0 = a0.cross(Vector((0.12 * sx, -1.0, 0.0))).normalized()
        a1 = (K - H).normalized()
        b1 = (T2 - K).normalized()
        h1 = e.cross(p).normalized()
        D[th] = (frame_matrix(a1, h1) @ frame_matrix(a0, h0).transposed()).to_quaternion()
        D[sh] = (frame_matrix(b1, h1) @ frame_matrix(b0, h0).transposed()).to_quaternion()
        D[ft] = foot_q


def arm_dir(raise_fwd, spread, side):
    """Direction du bras (dans le repère du torse) : levé vers l'avant de raise_fwd, écarté de spread."""
    sx = 1.0 if side == "L" else -1.0
    return Vector((sx * math.sin(spread), -math.sin(raise_fwd) * math.cos(spread), -math.cos(raise_fwd) * math.cos(spread))).normalized()


def set_arm(rig, q, side, raise_fwd, spread, elbow, twist=0.0, wrist=0.0, clav=None):
    """Bras complet : direction de l'humérus, rotation sur lui-même, flexion du coude et du poignet."""
    if clav is not None:
        q["clavicle_" + side] = clav
    target = arm_dir(raise_fwd, spread, side)
    arc = rig.dir("upper_arm_" + side).rotation_difference(target)
    cq = q.get("clavicle_" + side, Quaternion())
    q["upper_arm_" + side] = cq.inverted() @ Quaternion(target, twist) @ arc
    q["forearm_" + side] = Quaternion(rig.arm_hinge(side), elbow)
    q["hand_" + side] = Quaternion(rig.wrist_hinge(side), wrist)


# ---------------------------------------------------------------- locomotion

def walk_pose(rig, st, t, run=False):
    """Pose de marche (ou de course) à la phase t (0..1) du cycle ; pied gauche posé à t = 0."""
    stride = st["stride"]
    beta = 0.33 if run else 0.6  # part du cycle où le pied est au sol
    lift = st["lift"]
    limp = st["limp"]
    heavy = st["heavy"]
    q = {}
    feet, toe = {}, {}
    hips_drop = 0.0
    front = 0.42  # le pied se pose un peu devant et part loin derrière (talon levé)
    for side, ph in (("L", 0.0), ("R", 0.5)):
        u = (t + ph) % 1.0
        drag = limp if side == "R" else 0.0
        L = stride * beta * (1.0 - 0.25 * drag)
        if u < beta:
            s = u / beta
            y = -L * front + L * s
            z = 0.0
            pitch = -0.28 * (1 - ramp(s, 0.0, 0.18)) * (1 - drag * 0.7) + 0.55 * ramp(s, 0.7, 1.0) * (1 - drag * 0.6)
            if run:
                pitch = 0.15 * (1 - ramp(s, 0.0, 0.3)) + 0.7 * ramp(s, 0.5, 1.0)
        else:
            s = (u - beta) / (1 - beta)
            y = L * (1 - front) - L * ease(s)
            z = lift * (1 - 0.75 * drag) * math.sin(math.pi * min(1.0, s * 1.1)) ** 0.8
            pitch = 0.55 * (1 - ramp(s, 0.0, 0.35)) * (1 - drag * 0.5) - 0.25 * ramp(s, 0.5, 1.0) * (1 - drag)
            if drag:
                # Le pied traîné reste pointé vers le bas et frotte presque le sol.
                pitch += 0.25 * drag * math.sin(math.pi * s)
            if run:
                pitch = 0.7 * (1 - ramp(s, 0.0, 0.4)) - 0.2 * ramp(s, 0.6, 1.0)
        out = 0.0 if side == "L" else -0.0
        yaw = qz((0.12 if side == "L" else -0.12) * (1 + drag))
        feet[side] = (rig.foot_target(side, y, z, pitch, out), yaw @ qx(pitch))
        toe[side] = -max(pitch, 0.0) if z < 0.01 else -0.1
        if side == "R":
            hips_drop += drag * 0.05 * math.sin(math.pi * min(1.0, u / beta)) if u < beta else 0.0
    w = 2 * math.pi * t
    bob = (0.035 + 0.03 * heavy) * (0.5 - 0.5 * math.cos(2 * w))
    if run:
        bob = 0.06 * (0.5 + 0.5 * math.cos(2 * w + 0.6))
    crouch = 0.1 if run else 0.065 + 0.03 * heavy
    root = Vector((st["sway"] * math.sin(w), 0.02 * math.sin(2 * w), -crouch + bob - hips_drop - 0.05 * st["lean"]))
    q["hips"] = chain(qz(0.1 * math.sin(w) * (1.5 if run else 1.0)), qy(-0.05 * math.sin(w) * (1 + heavy)), qx(0.06 * st["lean"]))
    lean = st["lean"]
    q["spine"] = chain(qx(lean * 0.55 + 0.03 * math.sin(2 * w)), qz(-0.12 * math.sin(w)), qy(0.04 * math.sin(w)))
    q["chest"] = chain(qx(lean * 0.45 + st["hunch"] * 0.5), qz(-0.08 * math.sin(w)))
    q["neck"] = chain(qx(st["hunch"] * 0.6 - lean * 0.4), qy(st["tilt"] * 0.5))
    q["head"] = chain(qx(-lean * 0.55 + 0.06 * math.sin(2 * w + 1.0)), qy(st["tilt"] * 0.5 + 0.06 * math.sin(w)), qz(0.06 * math.sin(w * 0.5)))
    q["jaw"] = qx(0.12 + 0.06 * math.sin(w * 1.5))
    swing = math.sin(w)
    arms = st["arms"]
    if run or arms == "flail":
        # Course : bras qui battent fort, mains crispées.
        for side, sgn in (("L", 1.0), ("R", -1.0)):
            a = swing * sgn
            set_arm(rig, q, side, 0.55 - 0.85 * a, 0.25, 1.25 + 0.35 * a, twist=0.3 * sgn, wrist=0.3)
    elif arms == "reach":
        # Bras tendus vers l'avant, qui ballottent ; le droit plus bas.
        set_arm(rig, q, "L", 1.3 + 0.08 * math.sin(w + 0.4), 0.18, 0.35 + 0.1 * math.sin(2 * w), twist=-0.5, wrist=0.45)
        set_arm(rig, q, "R", 1.0 + 0.1 * math.sin(w + 2.0), 0.2, 0.55 + 0.1 * math.sin(2 * w + 1), twist=0.5, wrist=0.5)
    elif arms == "mixed":
        set_arm(rig, q, "L", 1.15 + 0.08 * math.sin(w), 0.22, 0.5, twist=-0.4, wrist=0.5)
        set_arm(rig, q, "R", 0.15 - 0.3 * swing, 0.18, 0.25, twist=0.0, wrist=0.15)
    elif arms == "low":
        # Fouisseur : bras bas vers l'avant, griffes prêtes.
        for side, sgn in (("L", 1.0), ("R", -1.0)):
            set_arm(rig, q, side, 0.7 - 0.35 * swing * sgn, 0.3, 0.6 + 0.2 * swing * sgn, twist=0.4 * sgn, wrist=-0.3)
    else:
        # Bras ballants, lourds.
        for side, sgn in (("L", 1.0), ("R", -1.0)):
            set_arm(rig, q, side, 0.12 - 0.35 * swing * sgn, 0.16 + 0.05 * heavy, 0.3 + 0.15 * max(0.0, swing * sgn), twist=0.0, wrist=0.15)
    return {"root": root, "q": q, "feet": feet, "toe": toe}


def stand_feet(rig, crouch=0.0, spread=0.0, l_y=0.06, r_y=-0.08):
    """Pieds posés (légèrement décalés) pour les animations sur place."""
    feet, toe = {}, {}
    for side, y, sx in (("L", l_y, 1.0), ("R", r_y, -1.0)):
        feet[side] = (rig.foot_target(side, y, 0.0, 0.0, spread * sx), qz(0.18 * sx))
        toe[side] = 0.0
    return feet, toe


def idle_pose(rig, st, t):
    w = 2 * math.pi * t
    feet, toe = stand_feet(rig)
    q = {}
    root = Vector((0.025 * math.sin(w), 0.0, -0.04 - 0.012 * math.sin(2 * w) - 0.03 * st["lean"]))
    q["hips"] = chain(qy(0.05 * math.sin(w)), qx(0.04 * st["lean"]))
    q["spine"] = chain(qx(st["lean"] * 0.5 + 0.03 * math.sin(2 * w + 0.5)), qy(-0.04 * math.sin(w)))
    q["chest"] = chain(qx(st["lean"] * 0.4 + st["hunch"] * 0.5 + 0.02 * math.sin(2 * w + 1.0)))
    q["neck"] = chain(qx(st["hunch"] * 0.6 - st["lean"] * 0.4), qy(st["tilt"] * 0.5))
    # La tête roule lentement, avec un petit spasme.
    jerk = bump(t, 0.6, 0.63, 0.72)
    q["head"] = chain(qx(-st["lean"] * 0.5 + 0.08 * math.sin(w + 0.7) + 0.15 * jerk), qy(st["tilt"] * 0.6 + 0.12 * math.sin(w)), qz(0.2 * math.sin(w) - 0.25 * jerk))
    q["jaw"] = qx(0.15 + 0.1 * math.sin(2 * w))
    for side, sgn in (("L", 1.0), ("R", -1.0)):
        sway = math.sin(w + (0.0 if side == "L" else 1.3))
        if st["arms"] == "reach":
            set_arm(rig, q, side, (0.9 if side == "L" else 0.4) + 0.08 * sway, 0.2, 0.5, twist=-0.4 * sgn, wrist=0.4)
        elif st["arms"] == "low":
            set_arm(rig, q, side, 0.5 + 0.05 * sway, 0.3, 0.7, twist=0.4 * sgn, wrist=-0.2)
        else:
            set_arm(rig, q, side, 0.08 + 0.06 * sway, 0.15, 0.25, wrist=0.15)
    return {"root": root, "q": q, "feet": feet, "toe": toe}


# ---------------------------------------------------------------- actions sur place

def attack_pose(rig, st, t):
    """Coup de griffes : armer, frapper vers le bas et l'avant, revenir. Coup porté vers t = 0.4."""
    heavy = st["heavy"] >= 0.9
    feet, toe = stand_feet(rig, l_y=0.1, r_y=-0.14)
    q = {}
    wind = bump(t, 0.0, 0.3, 0.45)
    hit = bump(t, 0.3, 0.42, 0.85)
    lunge = ramp(t, 0.3, 0.42) * (1 - ramp(t, 0.6, 1.0))
    root = Vector((0, -0.12 * lunge, -0.05 - 0.08 * lunge - 0.03 * wind))
    lean = st["lean"]
    q["hips"] = chain(qz(0.25 * wind - 0.35 * hit), qx(0.05 + 0.1 * lunge))
    q["spine"] = chain(qx(lean * 0.5 - 0.25 * wind + 0.4 * hit), qz(0.2 * wind - 0.3 * hit))
    q["chest"] = chain(qx(lean * 0.4 + st["hunch"] * 0.5 - 0.15 * wind + 0.25 * hit), qz(0.15 * wind - 0.2 * hit))
    q["neck"] = qx(st["hunch"] * 0.5 - lean * 0.3 - 0.15 * wind)
    q["head"] = chain(qx(-lean * 0.4 - 0.25 * wind + 0.2 * hit), qz(-0.1 * wind))
    q["jaw"] = qx(0.15 + 0.45 * bump(t, 0.2, 0.4, 0.7))
    if heavy:
        # Frappe à deux poings levés au-dessus de la tête.
        for side, sgn in (("L", 1.0), ("R", -1.0)):
            set_arm(rig, q, side, 0.2 + 2.6 * wind - 1.4 * hit + 0.9 * lunge * (1 - wind), 0.25 - 0.1 * wind, 0.4 + 0.9 * wind - 0.3 * hit, twist=0.3 * sgn, wrist=0.2 * hit)
    else:
        set_arm(rig, q, "R", 1.0 + 1.3 * wind - 1.0 * hit + 0.4 * lunge, 0.5 * wind + 0.1, 0.4 + 1.0 * wind - 0.4 * hit, twist=0.6, wrist=0.6 * hit)
        lag = bump(t, 0.4, 0.55, 0.95)
        set_arm(rig, q, "L", 1.1 + 0.6 * wind - 0.9 * lag + 0.6 * lunge, 0.3 * wind + 0.15, 0.5 + 0.6 * wind - 0.3 * lag, twist=-0.5, wrist=0.5 * lag)
    return {"root": root, "q": q, "feet": feet, "toe": toe}


def spit_pose(rig, st, t):
    """Cracheur : se cambre en gonflant la gorge, puis projette la tête en avant (crachat à t = 0.5)."""
    feet, toe = stand_feet(rig, l_y=0.08, r_y=-0.1)
    q = {}
    back = bump(t, 0.0, 0.4, 0.55)
    thrust = bump(t, 0.42, 0.52, 0.9)
    root = Vector((0, 0.04 * back - 0.08 * thrust, -0.05 - 0.04 * thrust))
    q["hips"] = qx(0.04)
    q["spine"] = qx(st["lean"] * 0.5 - 0.3 * back + 0.35 * thrust)
    q["chest"] = qx(st["hunch"] * 0.5 - 0.25 * back + 0.3 * thrust)
    q["neck"] = qx(-0.3 * back + 0.35 * thrust)
    q["head"] = qx(-0.45 * back + 0.1 * thrust)
    q["jaw"] = qx(0.1 + 0.2 * back + 0.55 * thrust)
    for side, sgn in (("L", 1.0), ("R", -1.0)):
        set_arm(rig, q, side, 0.3 + 0.5 * back - 0.2 * thrust, 0.35 + 0.3 * back, 0.6 + 0.4 * back, twist=0.3 * sgn, wrist=0.3)
    return {"root": root, "q": q, "feet": feet, "toe": toe}


def scream_pose(rig, st, t):
    """Hurleur : se redresse, bras écartés, tête en arrière, mâchoire grande ouverte, tremblements."""
    feet, toe = stand_feet(rig, l_y=0.12, r_y=-0.12, spread=0.04)
    q = {}
    up = ramp(t, 0.0, 0.25) * (1 - ramp(t, 0.8, 1.0))
    shake = math.sin(t * 2 * math.pi * 14) * 0.05 * up
    root = Vector((0, 0.03 * up, -0.06 + 0.03 * up))
    q["hips"] = qx(-0.05 * up)
    q["spine"] = chain(qx(st["lean"] * 0.5 * (1 - up) - 0.25 * up + shake), qz(shake))
    q["chest"] = chain(qx(st["hunch"] * 0.5 * (1 - up) - 0.25 * up), qy(shake))
    q["neck"] = qx(-0.35 * up)
    q["head"] = chain(qx(-0.45 * up + shake * 2), qz(shake * 2))
    q["jaw"] = qx(0.15 + 0.75 * up + shake)
    for side, sgn in (("L", 1.0), ("R", -1.0)):
        set_arm(rig, q, side, 0.2 + 0.8 * up, 0.3 + 0.9 * up, 0.3 + 0.6 * up, twist=-0.6 * sgn * up, wrist=-0.4 * up, clav=qy(-0.25 * sgn * up))
    return {"root": root, "q": q, "feet": feet, "toe": toe}


def dig_pose(rig, st, t):
    """Fouisseur : accroupi, il gratte le sol des deux mains à tour de rôle (boucle)."""
    w = 2 * math.pi * t
    feet, toe = stand_feet(rig, l_y=0.12, r_y=-0.12, spread=0.06)
    q = {}
    root = Vector((0.03 * math.sin(w), 0.08, -0.42 + 0.02 * math.sin(2 * w)))
    q["hips"] = qx(0.75)
    q["spine"] = chain(qx(0.35), qz(0.15 * math.sin(w)))
    q["chest"] = qx(0.25)
    q["neck"] = qx(-0.5)
    q["head"] = qx(-0.55 + 0.08 * math.sin(2 * w))
    q["jaw"] = qx(0.25)
    for side, ph, sgn in (("L", 0.0, 1.0), ("R", math.pi, -1.0)):
        c = math.sin(w + ph)
        set_arm(rig, q, side, 0.55 + 0.55 * c, 0.25, 0.9 - 0.6 * c, twist=0.4 * sgn, wrist=-0.5 + 0.6 * c)
    return {"root": root, "q": q, "feet": feet, "toe": toe, "pole": {"L": Vector((0.4, -1, 0)), "R": Vector((-0.4, -1, 0))}}


def die_pose(rig, st, t, forward=False):
    """Mort : les genoux lâchent et le corps s'effondre en arrière (ou en avant), puis reste au sol."""
    q = {}
    hip_z = rig.head["hips"].z
    if not forward:
        jolt = bump(t, 0.0, 0.08, 0.35)
        fall = ease(ramp(t, 0.12, 0.7) ** 1.6)
        settle = ramp(t, 0.7, 0.85)
        root = Vector((0.0, 0.32 * fall, -(hip_z - 0.13) * fall - 0.08 * ramp(t, 0.05, 0.3) * (1 - fall)))
        q["hips"] = chain(qx(-1.57 * fall), qz(0.25 * fall))
        q["spine"] = qx(-0.25 * jolt + 0.1 * fall)
        q["chest"] = qx(-0.15 * jolt + 0.05 * fall)
        q["neck"] = qx(-0.3 * jolt - 0.2 * fall * (1 - settle) + 0.15 * settle)
        q["head"] = chain(qx(-0.4 * jolt - 0.3 * fall + 0.25 * settle), qz(0.6 * settle))
        q["jaw"] = qx(0.2 + 0.4 * jolt + 0.25 * fall)
        knee = 1.1 * bump(t, 0.05, 0.4, 0.8) + 0.15 * settle
        for side, sgn in (("L", 1.0), ("R", -1.0)):
            q["thigh_" + side] = chain(qx(-0.9 * bump(t, 0.05, 0.4, 0.8) - 0.1 * settle), qy(0.15 * sgn * fall))
            q["shin_" + side] = qx(knee * (1.0 if side == "L" else 0.7))
            q["foot_" + side] = qx(0.3 * fall)
            set_arm(rig, q, side, 0.4 + 1.6 * jolt * (1 - fall) + 1.0 * fall * (1 - settle) - 0.3 * settle, 0.4 + 0.5 * fall + 0.5 * settle, 0.5 - 0.2 * fall, twist=-0.4 * sgn, wrist=0.3)
        return {"root": root, "q": q}
    buckle = ramp(t, 0.0, 0.35)
    fall = ease(ramp(t, 0.3, 0.8) ** 1.4)
    settle = ramp(t, 0.8, 0.95)
    kneel_z = (rig.head["shin_L"].z - 0.06)
    root = Vector((0.0, -0.25 * buckle - 0.35 * fall, -kneel_z * buckle * (1 - fall) - (hip_z - 0.14) * fall))
    q["hips"] = chain(qx(0.35 * buckle + 1.22 * fall), qz(-0.2 * fall))
    q["spine"] = qx(0.3 * buckle * (1 - fall) + 0.05 * fall)
    q["chest"] = qx(0.25 * buckle * (1 - fall))
    q["neck"] = qx(0.3 * buckle - 0.3 * fall)
    q["head"] = chain(qx(0.2 * buckle - 0.5 * fall), qz(-0.7 * settle))
    q["jaw"] = qx(0.2 + 0.3 * fall)
    for side, sgn in (("L", 1.0), ("R", -1.0)):
        q["thigh_" + side] = qx(-1.1 * buckle * (1 - fall) - 0.25 * fall)
        q["shin_" + side] = qx(2.2 * buckle * (1 - fall) + 0.2 * fall)
        q["foot_" + side] = qx(0.6 * buckle)
        set_arm(rig, q, side, 0.2 + 1.2 * fall * (1 - settle) + 1.6 * settle, 0.3 + 0.5 * settle, 0.6 - 0.3 * settle, twist=0.3 * sgn, wrist=0.2)
    return {"root": root, "q": q}


# ---------------------------------------------------------------- cuisson des actions

def bake_action(rig, name, duration, fn, loop):
    arm = rig.arm
    bpy.context.scene.render.fps = FPS
    bpy.context.scene.render.fps_base = 1.0
    frames = max(2, int(round(duration * FPS)))
    act = bpy.data.actions.new(name)
    act.use_fake_user = True
    if arm.animation_data is None:
        arm.animation_data_create()
    arm.animation_data.action = act
    prev = {}
    for f in range(frames + 1):
        t = f / frames
        pose = fn(t if not loop else t % 1.0 if f < frames else 0.0)
        rots, loc, _ = rig.solve(pose)
        for n, qv in rots.items():
            pb = arm.pose.bones[n]
            if n in prev and prev[n].dot(qv) < 0:
                qv = -qv
            prev[n] = qv
            pb.rotation_quaternion = qv
            pb.keyframe_insert("rotation_quaternion", frame=f)
        hb = arm.pose.bones[rig.order[0]]
        hb.location = loc
        hb.keyframe_insert("location", frame=f)
    for fc in act.fcurves:
        for kp in fc.keyframe_points:
            kp.interpolation = "LINEAR"
    return act


def make_actions(arm, kind):
    """Crée toutes les actions d'un zombie. Renvoie {nom: (durée, boucle)}."""
    rig = Rig(arm)
    st = STYLES[kind]
    made = {}

    def add(name, dur, fn, loop):
        bake_action(rig, name, dur, fn, loop)
        made[name] = (dur, loop)

    add("idle", 3.0, lambda t: idle_pose(rig, st, t), True)
    if st.get("run"):
        add("run", st["T"], lambda t: walk_pose(rig, st, t, run=True), True)
        slow = dict(st, **st["slow"])
        add("walk", slow["T"], lambda t: walk_pose(rig, slow, t), True)
    else:
        add("walk", st["T"], lambda t: walk_pose(rig, st, t), True)
    add("attack", 1.0 if st["heavy"] < 0.9 else 1.25, lambda t: attack_pose(rig, st, t), False)
    add("die", 1.4, lambda t: die_pose(rig, st, t), False)
    add("die_front", 1.5, lambda t: die_pose(rig, st, t, forward=True), False)
    if kind == "cracheur":
        add("spit", 0.9, lambda t: spit_pose(rig, st, t), False)
    if kind == "hurleur":
        add("scream", 1.6, lambda t: scream_pose(rig, st, t), False)
    if kind == "fouisseur":
        add("dig", 0.8, lambda t: dig_pose(rig, st, t), True)
    # Pose de repos rangée : l'action active à l'export n'impose rien.
    arm.animation_data.action = None
    for pb in arm.pose.bones:
        pb.rotation_quaternion = Quaternion()
        pb.location = Vector()
    return made
