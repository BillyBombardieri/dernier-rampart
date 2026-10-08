"""Petit moteur de sculpture par champs de distance signée (numpy uniquement).

Une forme est décrite par des volumes simples (ellipsoïdes, cônes arrondis, boîtes arrondies) que
l'on fusionne en douceur ou que l'on creuse dans une grille de voxels. La surface finale est
extraite avec l'algorithme « surface nets » (des quadrilatères, un maillage fermé).
Valeur négative = à l'intérieur de la matière.
"""

import numpy as np

F32 = np.float32


def vec(v):
    return np.asarray(v, dtype=float)


def normalize(v):
    v = vec(v)
    return v / np.linalg.norm(v)


def basis(axis, hint=(0.0, -1.0, 0.0)):
    """Repère orthonormé (colonnes u, v, w) dont w suit axis et u est proche de hint."""
    w = normalize(axis)
    h = vec(hint)
    h = h - w * np.dot(h, w)
    if np.linalg.norm(h) < 1e-6:
        h = vec((1.0, 0.0, 0.0)) if abs(w[0]) < 0.9 else vec((0.0, 0.0, 1.0))
        h = h - w * np.dot(h, w)
    u = normalize(h)
    v = np.cross(w, u)
    return np.stack([u, v, w], axis=1)


def rot(x=0.0, y=0.0, z=0.0):
    """Matrice de rotation (radians), appliquée dans l'ordre X puis Y puis Z."""
    cx, sx, cy, sy, cz, sz = np.cos(x), np.sin(x), np.cos(y), np.sin(y), np.cos(z), np.sin(z)
    rx = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]])
    ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
    rz = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])
    return rz @ ry @ rx


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


# ---------------------------------------------------------------- volumes de base

class Prim:
    """Volume avec une boîte englobante (lo, hi), un centre c et un repère local R (colonnes)."""

    c = np.zeros(3)
    R = np.eye(3)

    def local(self, X, Y, Z):
        dx, dy, dz = X - F32(self.c[0]), Y - F32(self.c[1]), Z - F32(self.c[2])
        R = self.R.astype(F32)
        lx = dx * R[0, 0] + dy * R[1, 0] + dz * R[2, 0]
        ly = dx * R[0, 1] + dy * R[1, 1] + dz * R[2, 1]
        lz = dx * R[0, 2] + dy * R[1, 2] + dz * R[2, 2]
        return lx, ly, lz

    def at(self, pts):
        pts = np.asarray(pts, dtype=F32)
        return self.dist(pts[:, 0], pts[:, 1], pts[:, 2])


class Ellipsoid(Prim):
    def __init__(self, c, r, R=None):
        self.c = vec(c)
        self.r = vec(r)
        self.R = np.eye(3) if R is None else np.asarray(R, float)
        ext = np.abs(self.R) @ self.r
        self.lo, self.hi = self.c - ext, self.c + ext

    def dist(self, X, Y, Z):
        lx, ly, lz = self.local(X, Y, Z)
        rx, ry, rz = (F32(v) for v in self.r)
        k0 = np.sqrt((lx / rx) ** 2 + (ly / ry) ** 2 + (lz / rz) ** 2)
        k1 = np.sqrt((lx / (rx * rx)) ** 2 + (ly / (ry * ry)) ** 2 + (lz / (rz * rz)) ** 2)
        return k0 * (k0 - 1.0) / np.maximum(k1, F32(1e-6))


def Sphere(c, r):
    return Ellipsoid(c, (r, r, r))


class RoundCone(Prim):
    """Tronc de cône aux bouts arrondis, de a (rayon ra) à b (rayon rb). flat aplatit la section
    (facteurs sur les axes u et v du repère, u étant proche de hint)."""

    def __init__(self, a, b, ra, rb, flat=(1.0, 1.0), hint=(0.0, -1.0, 0.0)):
        a, b = vec(a), vec(b)
        self.c = a
        self.L = float(np.linalg.norm(b - a))
        self.R = basis(b - a, hint)
        self.ra, self.rb = float(ra), float(rb)
        self.flat = (float(flat[0]), float(flat[1]))
        r = max(ra, rb) * max(self.flat)
        self.lo, self.hi = np.minimum(a, b) - r, np.maximum(a, b) + r

    def dist(self, X, Y, Z):
        lx, ly, lz = self.local(X, Y, Z)
        sx, sy = self.flat
        s = min(sx, sy)
        if sx != 1.0:
            lx = lx / F32(sx)
        if sy != 1.0:
            ly = ly / F32(sy)
        L = self.L
        l2 = L * L
        rr = self.ra - self.rb
        a2 = l2 - rr * rr
        il2 = 1.0 / l2
        y = lz * F32(L)
        z = y - F32(l2)
        x2 = (lx * lx + ly * ly) * F32(l2 * l2)
        y2 = y * y * F32(l2)
        z2 = z * z * F32(l2)
        k = F32(np.sign(rr) * rr * rr) * x2
        d1 = np.sqrt(x2 + z2) * F32(il2) - F32(self.rb)
        d2 = np.sqrt(x2 + y2) * F32(il2) - F32(self.ra)
        d3 = (np.sqrt(x2 * F32(a2 * il2)) + y * F32(rr)) * F32(il2) - F32(self.ra)
        d = np.where(np.sign(z) * F32(a2) * z2 > k, d1, np.where(np.sign(y) * F32(a2) * y2 < k, d2, d3))
        return d * F32(s)


def Capsule(a, b, r, flat=(1.0, 1.0), hint=(0.0, -1.0, 0.0)):
    return RoundCone(a, b, r, r, flat, hint)


class RoundBox(Prim):
    def __init__(self, c, half, r=0.0, R=None):
        self.c = vec(c)
        self.half = vec(half)
        self.r = float(r)
        self.R = np.eye(3) if R is None else np.asarray(R, float)
        ext = np.abs(self.R) @ self.half
        self.lo, self.hi = self.c - ext, self.c + ext

    def dist(self, X, Y, Z):
        lx, ly, lz = self.local(X, Y, Z)
        r = F32(self.r)
        qx = np.abs(lx) - F32(self.half[0] - self.r)
        qy = np.abs(ly) - F32(self.half[1] - self.r)
        qz = np.abs(lz) - F32(self.half[2] - self.r)
        outside = np.sqrt(np.maximum(qx, 0) ** 2 + np.maximum(qy, 0) ** 2 + np.maximum(qz, 0) ** 2)
        inside = np.minimum(np.maximum(qx, np.maximum(qy, qz)), 0)
        return outside + inside - r


class Bent(Prim):
    """Applique un autre volume après une déformation de l'espace (fonction warp(X, Y, Z))."""

    def __init__(self, prim, warp, pad=0.0):
        self.prim = prim
        self.warp = warp
        self.lo, self.hi = prim.lo - pad, prim.hi + pad

    def dist(self, X, Y, Z):
        X2, Y2, Z2 = self.warp(X, Y, Z)
        return self.prim.dist(X2, Y2, Z2)


# ---------------------------------------------------------------- bruit

def _hash(ix, iy, iz, seed):
    h = (ix * 374761393 + iy * 668265263 + iz * 1440865009 + seed * 2654435761) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    h = h ^ (h >> 16)
    return (h & 0xFFFF).astype(F32) * F32(1.0 / 32767.5) - F32(1.0)


def noise(X, Y, Z, seed=0):
    """Bruit de valeur lissé dans [-1, 1] (les tableaux peuvent être diffusés)."""
    X, Y, Z = np.broadcast_arrays(np.asarray(X, F32), np.asarray(Y, F32), np.asarray(Z, F32))
    xi, yi, zi = np.floor(X), np.floor(Y), np.floor(Z)
    xf, yf, zf = X - xi, Y - yi, Z - zi
    xi, yi, zi = xi.astype(np.int64), yi.astype(np.int64), zi.astype(np.int64)
    u = xf * xf * xf * (xf * (xf * 6 - 15) + 10)
    v = yf * yf * yf * (yf * (yf * 6 - 15) + 10)
    w = zf * zf * zf * (zf * (zf * 6 - 15) + 10)
    out = 0.0
    for dz in (0, 1):
        wz = w if dz else 1 - w
        acc_y = 0.0
        for dy in (0, 1):
            wy = v if dy else 1 - v
            a = _hash(xi, yi + dy, zi + dz, seed)
            b = _hash(xi + 1, yi + dy, zi + dz, seed)
            acc_y = acc_y + wy * (a + (b - a) * u)
        out = out + wz * acc_y
    return out


def fbm(X, Y, Z, freq=1.0, octaves=3, seed=0, gain=0.5):
    total = 0.0
    amp = 1.0
    norm = 0.0
    f = F32(freq)
    for o in range(octaves):
        total = total + noise(X * f, Y * f, Z * f, seed + o * 31) * F32(amp)
        norm += amp
        amp *= gain
        f = f * F32(2.03)
    return total / F32(norm)


# ---------------------------------------------------------------- grille et calques

def smin(a, b, k):
    if k <= 0.0:
        return np.minimum(a, b)
    h = np.clip(0.5 + 0.5 * (b - a) / F32(k), 0.0, 1.0)
    return b + (a - b) * h - F32(k) * h * (1.0 - h)


def smax(a, b, k):
    return -smin(-a, -b, k)


class Grid:
    """Grille régulière commune à tous les calques d'un modèle (pas h, coin lo)."""

    def __init__(self, lo, hi, h):
        self.h = float(h)
        self.lo = vec(lo)
        self.n = (np.ceil((vec(hi) - self.lo) / h).astype(int) + 1)

    def coords(self, i0, i1):
        h = F32(self.h)
        xs = (F32(self.lo[0]) + h * np.arange(i0[0], i1[0], dtype=F32))[:, None, None]
        ys = (F32(self.lo[1]) + h * np.arange(i0[1], i1[1], dtype=F32))[None, :, None]
        zs = (F32(self.lo[2]) + h * np.arange(i0[2], i1[2], dtype=F32))[None, None, :]
        return xs, ys, zs

    def index_box(self, lo, hi, clip_lo=None, clip_hi=None):
        i0 = np.floor((vec(lo) - self.lo) / self.h).astype(int)
        i1 = np.ceil((vec(hi) - self.lo) / self.h).astype(int) + 1
        i0 = np.maximum(i0, 0 if clip_lo is None else clip_lo)
        i1 = np.minimum(i1, self.n if clip_hi is None else clip_hi)
        return i0, i1


class Layer:
    """Un calque de matière (peau, chemise, chaussures...) sur une partie de la grille."""

    def __init__(self, grid, lo, hi, fill=0.05):
        self.grid = grid
        self.i0, self.i1 = grid.index_box(lo, hi)
        self.a = np.full(tuple(self.i1 - self.i0), F32(fill), dtype=F32)
        self.fill = fill

    def _block(self, lo, hi):
        i0, i1 = self.grid.index_box(lo, hi, self.i0, self.i1)
        if np.any(i1 <= i0):
            return None, None
        sl = tuple(slice(a - o, b - o) for a, b, o in zip(i0, i1, self.i0))
        return sl, self.grid.coords(i0, i1)

    def add(self, prim, k=0.0):
        """Fusion douce (k = rayon de raccord, en mètres)."""
        pad = k + 2 * self.grid.h
        sl, P = self._block(prim.lo - pad, prim.hi + pad)
        if sl is None:
            return
        self.a[sl] = smin(self.a[sl], prim.dist(*P), k)

    def cut(self, prim, k=0.0):
        """Creuse le volume (soustraction douce)."""
        pad = k + 2 * self.grid.h
        sl, P = self._block(prim.lo - pad, prim.hi + pad)
        if sl is None:
            return
        self.a[sl] = smax(self.a[sl], -prim.dist(*P), k)

    def keep(self, prim, k=0.0):
        """Ne garde que ce qui est dans le volume (intersection) : tout le calque est touché."""
        P = self.grid.coords(self.i0, self.i1)
        self.a = smax(self.a, prim.dist(*P), k).astype(F32)

    def displace(self, lo, hi, fn, band=0.03):
        """Ajoute fn(X, Y, Z) au champ près de la surface (|valeur| < band) dans la boîte lo..hi."""
        sl, P = self._block(vec(lo), vec(hi))
        if sl is None:
            return
        A = self.a[sl]
        idx = np.nonzero(np.abs(A) < band)
        if len(idx[0]) == 0:
            return
        X = P[0][:, 0, 0][idx[0]]
        Y = P[1][0, :, 0][idx[1]]
        Z = P[2][0, 0, :][idx[2]]
        A[idx] += fn(X, Y, Z).astype(F32)

    def points(self, band=None):
        """Coordonnées de tout le calque (diffusables)."""
        return self.grid.coords(self.i0, self.i1)

    def sub(self, other):
        """Valeurs du calque other sur la zone de self (fill de other là où il ne couvre rien)."""
        if np.all(other.i0 <= self.i0) and np.all(other.i1 >= self.i1):
            sl = tuple(slice(a - o, b - o) for a, b, o in zip(self.i0, self.i1, other.i0))
            return other.a[sl]
        out = np.full(self.a.shape, F32(other.fill), dtype=F32)
        lo = np.maximum(self.i0, other.i0)
        hi = np.minimum(self.i1, other.i1)
        if np.all(hi > lo):
            dst = tuple(slice(a - o, b - o) for a, b, o in zip(lo, hi, self.i0))
            src = tuple(slice(a - o, b - o) for a, b, o in zip(lo, hi, other.i0))
            out[dst] = other.a[src]
        return out

    def sample(self, pts):
        """Valeur du champ aux points (interpolation trilinéaire) ; hors calque : fill."""
        g = self.grid
        f = (np.asarray(pts, float) - g.lo) / g.h - self.i0
        i = np.floor(f).astype(int)
        t = (f - i).astype(F32)
        shape = np.array(self.a.shape)
        ok = np.all((i >= 0) & (i < shape - 1), axis=1)
        out = np.full(len(pts), F32(self.fill), dtype=F32)
        ii, tt = i[ok], t[ok]
        acc = np.zeros(len(ii), F32)
        for dx in (0, 1):
            wx = tt[:, 0] if dx else 1 - tt[:, 0]
            for dy in (0, 1):
                wy = tt[:, 1] if dy else 1 - tt[:, 1]
                for dz in (0, 1):
                    wz = tt[:, 2] if dz else 1 - tt[:, 2]
                    acc += wx * wy * wz * self.a[ii[:, 0] + dx, ii[:, 1] + dy, ii[:, 2] + dz]
        out[ok] = acc
        return out


def union_layers(grid, layers, fill=0.05):
    """Champ final : la matière la plus à l'extérieur l'emporte."""
    F = np.full(tuple(grid.n), F32(fill), dtype=F32)
    for L in layers:
        sl = tuple(slice(a, b) for a, b in zip(L.i0, L.i1))
        np.minimum(F[sl], L.a, out=F[sl])
    return F


# ---------------------------------------------------------------- extraction de la surface

_CORNERS = [(0, 0, 0), (1, 0, 0), (0, 1, 0), (1, 1, 0), (0, 0, 1), (1, 0, 1), (0, 1, 1), (1, 1, 1)]
_EDGES = [(0, 1), (2, 3), (4, 5), (6, 7), (0, 2), (1, 3), (4, 6), (5, 7), (0, 4), (1, 5), (2, 6), (3, 7)]


def surface_nets(F, origin, h):
    """Renvoie (sommets (N, 3), quadrilatères (M, 4)) de la surface F = 0, normales vers l'extérieur."""
    inside = F < 0
    nx, ny, nz = F.shape
    count = np.zeros((nx - 1, ny - 1, nz - 1), np.uint8)
    for dx, dy, dz in _CORNERS:
        count += inside[dx:nx - 1 + dx, dy:ny - 1 + dy, dz:nz - 1 + dz]
    active = (count > 0) & (count < 8)
    del count
    ci, cj, ck = np.nonzero(active)
    del active
    lin = (ci.astype(np.int64) * (ny - 1) + cj) * (nz - 1) + ck
    nv = len(ci)
    acc = np.zeros((nv, 3), F32)
    cnt = np.zeros(nv, F32)
    for a, b in _EDGES:
        ca, cb = _CORNERS[a], _CORNERS[b]
        fa = F[ci + ca[0], cj + ca[1], ck + ca[2]]
        fb = F[ci + cb[0], cj + cb[1], ck + cb[2]]
        m = (fa < 0) != (fb < 0)
        t = fa[m] / (fa[m] - fb[m])
        for ax in range(3):
            acc[m, ax] += ca[ax] + t * (cb[ax] - ca[ax])
        cnt[m] += 1
    pos = vec(origin) + h * (np.stack([ci, cj, ck], axis=1) + acc / cnt[:, None])

    def vid(i, j, k):
        key = (i.astype(np.int64) * (ny - 1) + j) * (nz - 1) + k
        return np.searchsorted(lin, key)

    quads = []
    # Pour chaque arête de la grille qui traverse la surface : un quadrilatère reliant les 4 cellules.
    ex = inside[:-1, 1:-1, 1:-1] != inside[1:, 1:-1, 1:-1]
    i, j, k = np.nonzero(ex)
    j, k = j + 1, k + 1
    q = np.stack([vid(i, j - 1, k - 1), vid(i, j, k - 1), vid(i, j, k), vid(i, j - 1, k)], axis=1)
    flip = ~inside[i, j, k]
    q[flip] = q[flip][:, ::-1]
    quads.append(q)
    ey = inside[1:-1, :-1, 1:-1] != inside[1:-1, 1:, 1:-1]
    i, j, k = np.nonzero(ey)
    i, k = i + 1, k + 1
    q = np.stack([vid(i - 1, j, k - 1), vid(i - 1, j, k), vid(i, j, k), vid(i, j, k - 1)], axis=1)
    flip = ~inside[i, j, k]
    q[flip] = q[flip][:, ::-1]
    quads.append(q)
    ez = inside[1:-1, 1:-1, :-1] != inside[1:-1, 1:-1, 1:]
    i, j, k = np.nonzero(ez)
    i, j = i + 1, j + 1
    q = np.stack([vid(i - 1, j - 1, k), vid(i, j - 1, k), vid(i, j, k), vid(i - 1, j, k)], axis=1)
    flip = ~inside[i, j, k]
    q[flip] = q[flip][:, ::-1]
    quads.append(q)
    return pos.astype(F32), np.concatenate(quads).astype(np.int32)
