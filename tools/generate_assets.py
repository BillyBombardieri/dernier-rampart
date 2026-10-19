"""Génère les textures (PBR) et les sons du jeu de façon procédurale.

Tout est créé ici à partir de bruit mathématique : aucune ressource externe, donc aucun
problème de droits. Relancer ce script régénère assets/textures et assets/sounds.

    python tools/generate_assets.py

Dépendances : numpy, pillow.
"""
from pathlib import Path
import wave

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
TEX = ROOT / "assets" / "textures"
SND = ROOT / "assets" / "sounds"
SIZE = 512
RNG = np.random.default_rng(1337)


# ---------------------------------------------------------------- textures

def fractal_noise(size: int, power: float, rng=RNG) -> np.ndarray:
    """Bruit fractal raccordable (tileable) : bruit blanc filtré en 1/f^power dans l'espace de Fourier."""
    white = rng.standard_normal((size, size))
    f = np.fft.fftfreq(size)
    fx, fy = np.meshgrid(f, f)
    radius = np.sqrt(fx * fx + fy * fy)
    radius[0, 0] = 1.0
    spectrum = np.fft.fft2(white) / radius ** power
    spectrum[0, 0] = 0.0
    out = np.real(np.fft.ifft2(spectrum))
    out -= out.min()
    return out / out.max()


def cells(size: int, count: int, rng=RNG) -> np.ndarray:
    """Distance aux points les plus proches (cailloux, gravier), raccordable."""
    pts = rng.random((count, 2)) * size
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    best = np.full((size, size), 1e9, dtype=np.float32)
    for px, py in pts:
        dx = np.abs(xx - px)
        dy = np.abs(yy - py)
        dx = np.minimum(dx, size - dx)
        dy = np.minimum(dy, size - dy)
        best = np.minimum(best, np.sqrt(dx * dx + dy * dy))
    return best / best.max()


def normal_map(height: np.ndarray, strength: float) -> np.ndarray:
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * strength
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * strength
    n = np.stack([-dx, dy, np.ones_like(height)], axis=-1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return ((n * 0.5 + 0.5) * 255).astype(np.uint8)


def colorize(t: np.ndarray, dark, light) -> np.ndarray:
    dark = np.array(dark, dtype=np.float32)
    light = np.array(light, dtype=np.float32)
    return np.clip(dark + (light - dark) * t[..., None], 0, 255)


def save_set(name: str, albedo: np.ndarray, height: np.ndarray, rough: np.ndarray, strength: float) -> None:
    TEX.mkdir(parents=True, exist_ok=True)
    Image.fromarray(albedo.astype(np.uint8), "RGB").save(TEX / f"{name}_albedo.png")
    Image.fromarray(normal_map(height, strength), "RGB").save(TEX / f"{name}_normal.png")
    Image.fromarray((np.clip(rough, 0, 1) * 255).astype(np.uint8), "L").save(TEX / f"{name}_roughness.png")


def make_ground() -> None:
    big = fractal_noise(SIZE, 1.1)
    mid = fractal_noise(SIZE, 0.8)
    fine = fractal_noise(SIZE, 0.3)
    grass_mask = np.clip((big - 0.45) * 4.0, 0, 1)
    dirt = colorize(mid * 0.7 + fine * 0.3, (48, 38, 28), (98, 80, 60))
    grass = colorize(mid * 0.5 + fine * 0.5, (34, 44, 24), (78, 86, 46))
    albedo = dirt * (1 - grass_mask[..., None]) + grass * grass_mask[..., None]
    height = big * 0.4 + mid * 0.4 + fine * 0.2
    save_set("ground", albedo, height, 0.85 + fine * 0.15, 6.0)


def make_mud() -> None:
    mid = fractal_noise(SIZE, 0.9)
    fine = fractal_noise(SIZE, 0.35)
    stones = 1.0 - np.clip(cells(SIZE, 260) * 9.0, 0, 1)
    wet = np.clip((fractal_noise(SIZE, 1.2) - 0.55) * 5.0, 0, 1)
    albedo = colorize(mid * 0.6 + fine * 0.4, (36, 28, 20), (82, 66, 50))
    albedo = albedo * (1 - stones[..., None] * 0.6) + colorize(fine, (90, 86, 80), (140, 135, 125)) * stones[..., None] * 0.6
    albedo *= (1 - wet[..., None] * 0.35)
    height = mid * 0.5 + fine * 0.2 + stones * 0.6 - wet * 0.2
    rough = 0.9 - wet * 0.6
    save_set("mud", albedo, height, rough, 8.0)


def make_concrete() -> None:
    mid = fractal_noise(SIZE, 1.0)
    fine = fractal_noise(SIZE, 0.2)
    stains = np.clip((fractal_noise(SIZE, 1.3) - 0.6) * 3.0, 0, 1)
    pores = (fine > 0.82).astype(np.float32)
    base = colorize(mid * 0.5 + fine * 0.5, (105, 102, 96), (150, 147, 140))
    albedo = base * (1 - stains[..., None] * 0.45) * (1 - pores[..., None] * 0.3)
    # Joints de dalles tous les 128 px.
    yy, xx = np.mgrid[0:SIZE, 0:SIZE]
    joints = ((xx % 128) < 2) | ((yy % 128) < 2)
    albedo[joints] *= 0.55
    height = mid * 0.3 + fine * 0.3 - pores * 0.3 - joints * 0.8
    save_set("concrete", albedo, height, 0.8 + fine * 0.15, 4.0)


def make_metal() -> None:
    mid = fractal_noise(SIZE, 1.0)
    fine = fractal_noise(SIZE, 0.25)
    rust = np.clip((fractal_noise(SIZE, 1.15) - 0.5) * 3.0, 0, 1)
    scratches = np.clip(np.abs(np.sin(np.linspace(0, 90, SIZE)))[None, :] * fine, 0, 1) > 0.9
    paint = colorize(mid * 0.4 + fine * 0.6, (58, 64, 52), (84, 92, 76))
    rust_col = colorize(fine, (82, 40, 20), (140, 72, 34))
    albedo = paint * (1 - rust[..., None]) + rust_col * rust[..., None]
    albedo[scratches] = albedo[scratches] * 0.5 + 90
    height = fine * 0.3 + rust * 0.4
    rough = 0.45 + rust * 0.5 - scratches * 0.2
    save_set("metal", albedo, height, rough, 5.0)


def make_skin() -> None:
    mid = fractal_noise(256, 0.9)
    fine = fractal_noise(256, 0.3)
    veins = np.clip(1 - np.abs(fractal_noise(256, 1.0) - 0.5) * 30, 0, 1)
    wounds = np.clip((fractal_noise(256, 1.2) - 0.72) * 6.0, 0, 1)
    albedo = colorize(mid * 0.6 + fine * 0.4, (88, 96, 78), (140, 146, 120))
    albedo = albedo * (1 - veins[..., None] * 0.35)
    albedo = albedo * (1 - wounds[..., None]) + colorize(fine, (70, 10, 10), (120, 20, 18)) * wounds[..., None]
    height = mid * 0.4 + fine * 0.3 - wounds * 0.4
    Image.fromarray(albedo.astype(np.uint8), "RGB").save(TEX / "skin_albedo.png")
    Image.fromarray(normal_map(height, 5.0), "RGB").save(TEX / "skin_normal.png")


def make_cloth() -> None:
    fine = fractal_noise(256, 0.2)
    mid = fractal_noise(256, 0.9)
    yy, xx = np.mgrid[0:256, 0:256]
    weave = ((np.sin(xx * 1.6) * np.sin(yy * 1.6)) * 0.5 + 0.5)
    dirt = np.clip((mid - 0.5) * 3, 0, 1)
    tears = np.clip((fractal_noise(256, 1.1) - 0.78) * 8, 0, 1)
    gray = np.clip(0.55 + (fine - 0.5) * 0.4 + (weave - 0.5) * 0.15 - dirt * 0.35, 0, 1)
    gray = gray * (1 - tears) + 0.12 * tears
    Image.fromarray((gray * 255).astype(np.uint8), "L").convert("RGB").save(TEX / "cloth_albedo.png")
    Image.fromarray(normal_map(weave * 0.2 + fine * 0.3 - tears * 0.5, 4.0), "RGB").save(TEX / "cloth_normal.png")


# ---------------------------------------------------------------- façades et béton (bâtiments)

BIG = 1024  # Textures des bâtiments : plus fines, vues de près.


def streaks(size: int, rng) -> np.ndarray:
    """Coulures verticales : des colonnes plus ou moins sales qui pâlissent en descendant."""
    cols = fractal_noise(size, 0.9, rng)[0]  # Une ligne de bruit raccordable : une valeur par colonne.
    cols = np.clip((cols - 0.6) * 5.0, 0, 1) ** 1.5
    fade = np.clip((fractal_noise(size, 1.5, rng) - 0.3) * 2.0, 0, 1)
    return cols[None, :] * fade


def cracks(size: int, rng, width: float) -> np.ndarray:
    """Fissures fines : les crêtes d'un bruit fractal."""
    ridge = np.abs(fractal_noise(size, 1.25, rng) - 0.5)
    mask = np.clip((fractal_noise(size, 1.1, rng) - 0.55) * 4.0, 0, 1)
    return np.clip(1.0 - ridge / width, 0, 1) ** 3 * mask


def make_plaster() -> None:
    """Enduit de façade : crépi granuleux, plaques repeintes, fissures, coulures, éclats d'enduit
    qui laissent voir la brique."""
    rng = np.random.default_rng(2101)
    fine = fractal_noise(BIG, 0.15, rng)
    mid = fractal_noise(BIG, 0.95, rng)
    big = fractal_noise(BIG, 1.3, rng)
    patches = np.clip((fractal_noise(BIG, 1.2, rng) - 0.5) * 4.0, 0, 1)
    spall = np.clip((fractal_noise(BIG, 1.5, rng) - 0.7) * 8.0, 0, 1)  # Enduit tombé.
    crack = cracks(BIG, rng, 0.012)
    drip = streaks(BIG, rng)
    albedo = colorize(mid * 0.6 + fine * 0.4, (150, 142, 128), (196, 188, 172))
    albedo *= (1 - patches[..., None] * 0.12) * (0.92 + big[..., None] * 0.16)
    brick = colorize(fine, (92, 52, 38), (138, 82, 60))
    albedo = albedo * (1 - spall[..., None]) + brick * spall[..., None]
    albedo *= 1 - drip[..., None] * 0.3
    albedo *= 1 - crack[..., None] * 0.6
    height = fine * 0.35 + mid * 0.15 - spall * 0.5 - crack * 0.4
    rough = 0.88 + fine * 0.1 - drip * 0.08
    save_set("plaster", albedo, height, rough, 5.0)


def make_brick() -> None:
    """Mur de briques : 16 rangs et 5 briques par rang sur la tuile, joints en creux, chaque brique
    de sa couleur, arêtes ébréchées, suie et coulures."""
    rng = np.random.default_rng(2202)
    rows, per_row = 16, 5
    yy, xx = np.mgrid[0:BIG, 0:BIG].astype(np.float32)
    row_h = BIG / rows
    brick_w = BIG / per_row
    row = (yy // row_h).astype(int)
    shift = (row % 2) * brick_w * 0.5
    col = (((xx + shift) // brick_w).astype(int)) % per_row
    u = ((xx + shift) % brick_w) / brick_w
    v = (yy % row_h) / row_h
    mortar_w = 0.07
    edge = np.minimum(np.minimum(u, 1 - u) * brick_w / row_h, np.minimum(v, 1 - v))
    chip = fractal_noise(BIG, 0.6, rng)
    brick_mask = np.clip((edge - mortar_w * (0.8 + chip * 0.8)) * 14.0, 0, 1)
    ids = row * per_row + col
    tones = rng.uniform(0.75, 1.15, rows * per_row)
    hues = rng.uniform(-1, 1, rows * per_row)
    tone = tones[ids]
    hue = hues[ids]
    fine = fractal_noise(BIG, 0.2, rng)
    mid = fractal_noise(BIG, 1.0, rng)
    base = np.stack([128 + hue * 14, 66 + hue * 6, 48 - hue * 4], -1) * tone[..., None]
    base *= (0.85 + fine[..., None] * 0.3)
    mortar = colorize(fine, (108, 104, 96), (150, 145, 135))
    albedo = mortar * (1 - brick_mask[..., None]) + base * brick_mask[..., None]
    soot = np.clip((mid - 0.55) * 2.5, 0, 1)
    drip = streaks(BIG, rng)
    albedo *= (1 - soot[..., None] * 0.4) * (1 - drip[..., None] * 0.3)
    height = brick_mask * 0.7 + fine * 0.25 - (1 - brick_mask) * 0.2
    rough = 0.82 + fine * 0.12 - brick_mask * 0.04
    save_set("brick", albedo, height, rough, 7.0)


def make_beton() -> None:
    """Béton banché : panneaux de coffrage (un joint horizontal et un vertical par tuile), trous des
    tiges de coffrage, bulles, taches d'humidité et coulures."""
    rng = np.random.default_rng(2303)
    fine = fractal_noise(BIG, 0.18, rng)
    mid = fractal_noise(BIG, 1.0, rng)
    big = fractal_noise(BIG, 1.35, rng)
    yy, xx = np.mgrid[0:BIG, 0:BIG].astype(np.float32)
    seam = np.exp(-((yy % (BIG / 2)) ** 2) / 6.0) + np.exp(-((xx % BIG) ** 2) / 6.0)
    seam = np.clip(seam, 0, 1)
    # Trous de tiges de coffrage : grille de 4 x 4 par tuile.
    step = BIG / 4
    dx = (xx % step) - step / 2
    dy = (yy % step) - step / 2
    ties = np.clip(1.0 - np.sqrt(dx * dx + dy * dy) / 9.0, 0, 1)
    ties = (ties > 0).astype(np.float32) * np.clip(ties * 3.0, 0, 1)
    pores = (fine > 0.86).astype(np.float32) * (mid > 0.4)
    damp = np.clip((big - 0.55) * 3.0, 0, 1)
    drip = streaks(BIG, rng)
    albedo = colorize(mid * 0.55 + fine * 0.45, (118, 116, 110), (164, 162, 154))
    albedo *= (1 - damp[..., None] * 0.3) * (1 - drip[..., None] * 0.35)
    albedo *= (1 - seam[..., None] * 0.25) * (1 - ties[..., None] * 0.55) * (1 - pores[..., None] * 0.35)
    height = fine * 0.3 + mid * 0.1 + seam * 0.35 - ties * 0.8 - pores * 0.3
    rough = 0.86 + fine * 0.1 - damp * 0.1
    save_set("beton", albedo, height, rough, 5.0)


# ---------------------------------------------------------------- sons

RATE = 44100


def save_wav(name: str, samples: np.ndarray) -> None:
    SND.mkdir(parents=True, exist_ok=True)
    samples = samples / max(1e-6, np.max(np.abs(samples))) * 0.9
    data = (samples * 32767).astype(np.int16)
    with wave.open(str(SND / f"{name}.wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data.tobytes())


def t_axis(duration: float) -> np.ndarray:
    return np.arange(int(RATE * duration)) / RATE


def lowpass(x: np.ndarray, alpha: float) -> np.ndarray:
    y = np.empty_like(x)
    acc = 0.0
    for i, v in enumerate(x):
        acc += alpha * (v - acc)
        y[i] = acc
    return y


def envelope(t: np.ndarray, attack: float, decay: float) -> np.ndarray:
    return np.minimum(t / max(attack, 1e-4), 1.0) * np.exp(-t / decay)


def gunshot(duration: float, body_hz: float, decay: float, tail: float, seed: int) -> np.ndarray:
    rng = np.random.default_rng(seed)
    t = t_axis(duration)
    crack = rng.standard_normal(t.size) * envelope(t, 0.0005, 0.012)
    boom = np.sin(2 * np.pi * body_hz * t * np.exp(-t * 8)) * envelope(t, 0.001, decay)
    rumble = lowpass(rng.standard_normal(t.size), 0.02) * envelope(t, 0.005, tail) * 6
    return crack * 0.9 + boom * 0.8 + rumble


def bandpass(x: np.ndarray, lo: float, hi: float) -> np.ndarray:
    spec = np.fft.rfft(x)
    freqs = np.fft.rfftfreq(x.size, 1 / RATE)
    spec[(freqs < lo) | (freqs > hi)] = 0
    return np.fft.irfft(spec, n=x.size)


def footstep(rng, running: bool, heavy: bool = False) -> np.ndarray:
    """Pas réaliste : talon puis pointe en marchant, appui unique et plus lourd en courant."""
    dur = 0.32 if running else 0.28
    t = t_axis(dur)
    out = np.zeros(t.size)
    # Impacts sourds (talon, pointe).
    hits = [(0.0, 1.0)] if running else [(0.0, 0.8), (rng.uniform(0.035, 0.055), 0.55)]
    for start, amp in hits:
        tt = np.clip(t - start, 0, None)
        thud = bandpass(rng.standard_normal(t.size), 40, 220) * np.exp(-tt / (0.03 if not heavy else 0.06)) * (t >= start)
        out += thud * amp * (2.2 if heavy else (1.5 if running else 1.0))
    # Crissement : des centaines de micro-impacts de gravier filtrés dans les aigus.
    grains = np.zeros(t.size)
    count = 260 if running else 160
    times = rng.gamma(2.0, 0.025 if running else 0.03, count)
    for g in times:
        idx = int(g * RATE)
        if idx < t.size - 40:
            grains[idx:idx + 40] += rng.standard_normal(40) * np.exp(-np.arange(40) / 8) * rng.uniform(0.2, 1.0)
    crunch = bandpass(grains, 900, 6500)
    out += crunch * (0.35 if running else 0.25) / max(1e-6, np.max(np.abs(crunch))) * np.max(np.abs(out))
    # Fondu de sortie pour éviter les clics.
    out *= np.minimum(1.0, (t[-1] - t) / 0.03)
    return out


def make_sounds() -> None:
    rng = np.random.default_rng(7)
    save_wav("pistol", gunshot(0.9, 90, 0.08, 0.35, 1))
    save_wav("rifle", gunshot(0.6, 120, 0.05, 0.22, 2))
    save_wav("tower_gun", gunshot(0.35, 160, 0.03, 0.1, 3) * 0.7)

    t = t_axis(0.5)
    hiss = lowpass(rng.standard_normal(t.size), 0.3) * envelope(t, 0.02, 0.18)
    whine = np.sin(2 * np.pi * (1800 - 1200 * t) * t) * envelope(t, 0.01, 0.15) * 0.3
    save_wav("cryo", hiss + whine)

    t = t_axis(0.12)
    click = rng.standard_normal(t.size) * envelope(t, 0.0003, 0.008) + np.sin(2 * np.pi * 2400 * t) * envelope(t, 0.0003, 0.01)
    silence = np.zeros(int(RATE * 0.35))
    save_wav("reload", np.concatenate([click, silence, click * 0.8, silence * 0.5, click * 1.2]))

    t = t_axis(0.25)
    save_wav("hit_flesh", lowpass(rng.standard_normal(t.size), 0.15) * envelope(t, 0.001, 0.05) + np.sin(2 * np.pi * 70 * t) * envelope(t, 0.001, 0.06))

    # Pas sur terre et gravier : un impact sourd + un crissement de petits grains.
    for i in range(4):
        save_wav(f"step_walk_{i}", footstep(np.random.default_rng(200 + i), running=False))
        save_wav(f"step_run_{i}", footstep(np.random.default_rng(300 + i), running=True))
    save_wav("land", footstep(np.random.default_rng(400), running=True, heavy=True))

    t = t_axis(0.35)
    save_wav("pickup", (np.sin(2 * np.pi * 1320 * t) + 0.6 * np.sin(2 * np.pi * 1980 * t)) * envelope(t, 0.002, 0.08))

    # Râles de zombie : source rauque filtrée par des formants qui glissent.
    for i in range(3):
        r = np.random.default_rng(100 + i)
        dur = 1.4 + i * 0.3
        t = t_axis(dur)
        pitch = 70 + 25 * r.random() + 10 * np.sin(2 * np.pi * (0.8 + r.random()) * t)
        phase = np.cumsum(2 * np.pi * pitch / RATE)
        source = np.sign(np.sin(phase)) * 0.5 + r.standard_normal(t.size) * 0.5
        voiced = lowpass(source, 0.06)
        f1 = 400 + 200 * np.sin(2 * np.pi * 0.6 * t + i)
        formant = voiced * (0.6 + 0.4 * np.sin(np.cumsum(2 * np.pi * f1 / RATE)))
        amp = np.sin(np.pi * t / dur) ** 0.6
        save_wav(f"groan_{i}", formant * amp)

    t = t_axis(1.0)
    save_wav("zombie_death", lowpass(rng.standard_normal(t.size), 0.05) * np.exp(-t * 3) * (0.6 + 0.4 * np.sin(2 * np.pi * 9 * t)))

    t = t_axis(3.0)
    siren = np.sin(np.cumsum(2 * np.pi * (300 + 180 * np.sin(2 * np.pi * 0.5 * t)) / RATE))
    save_wav("siren", siren * np.minimum(t / 0.3, 1) * np.minimum((3.0 - t) / 0.5, 1))

    t = t_axis(0.5)
    save_wav("hurt", lowpass(rng.standard_normal(t.size), 0.1) * envelope(t, 0.005, 0.1) + np.sin(2 * np.pi * 55 * t) * envelope(t, 0.005, 0.15))

    t = t_axis(1.2)
    save_wav("structure_hit", lowpass(rng.standard_normal(t.size), 0.04) * envelope(t, 0.001, 0.2) + np.sin(2 * np.pi * 45 * t) * envelope(t, 0.001, 0.3))

    # Ambiance : vent qui souffle en boucle.
    t = t_axis(8.0)
    wind = lowpass(rng.standard_normal(t.size), 0.01) * (0.6 + 0.4 * np.sin(2 * np.pi * t / 8.0) ** 2)
    save_wav("wind", wind)


def make_extra_sounds() -> None:
    """Sons des tours, des nouveaux zombies, des barrières, des gadgets et de l'interface."""
    rng = np.random.default_rng(900)

    def noise(n):
        return rng.standard_normal(n)

    # Lance-flammes : souffle grave et grondant, avec des crépitements.
    t = t_axis(0.6)
    roar = bandpass(noise(t.size), 120, 1800) * (np.minimum(t / 0.05, 1) * np.exp(-np.maximum(t - 0.25, 0) / 0.15))
    crackle = np.zeros(t.size)
    for i in rng.integers(0, t.size - 200, 60):
        crackle[i:i + 200] += noise(200) * np.exp(-np.arange(200) / 30) * rng.uniform(0.2, 1.0)
    save_wav("flame", roar + bandpass(crackle, 1500, 7000) * 0.4)

    # Arc électrique : bourdonnement carré haché et grésillement aigu.
    t = t_axis(0.45)
    buzz = np.sign(np.sin(2 * np.pi * (120 + 40 * rng.random()) * t)) * (0.6 + 0.4 * (rng.random(t.size) > 0.7))
    hiss = bandpass(noise(t.size), 2500, 9000)
    env = envelope(t, 0.002, 0.12)
    save_wav("zap", (buzz * 0.5 + hiss) * env)

    # Mortier : coup sourd et profond.
    t = t_axis(0.8)
    thump = np.sin(2 * np.pi * 55 * t * np.exp(-t * 3)) * envelope(t, 0.002, 0.2)
    save_wav("mortar_fire", thump + lowpass(noise(t.size), 0.05) * envelope(t, 0.001, 0.08) * 3)

    # Explosion : déflagration, grondement puis débris qui retombent.
    t = t_axis(1.8)
    boom = lowpass(noise(t.size), 0.02) * envelope(t, 0.002, 0.55) * 8
    sub = np.sin(2 * np.pi * 38 * t * np.exp(-t * 1.5)) * envelope(t, 0.003, 0.4)
    debris = np.zeros(t.size)
    for start in rng.uniform(0.25, 1.4, 40):
        i = int(start * RATE)
        if i < t.size - 600:
            debris[i:i + 600] += noise(600) * np.exp(-np.arange(600) / 90) * rng.uniform(0.05, 0.3)
    save_wav("explosion", boom + sub + bandpass(debris, 800, 6000))

    # Crachat : bruit mouillé et « blop » qui descend.
    t = t_axis(0.35)
    wet = bandpass(noise(t.size), 300, 3000) * envelope(t, 0.005, 0.06)
    blop = np.sin(2 * np.pi * np.cumsum(400 - 700 * t) / RATE) * envelope(t, 0.005, 0.08)
    save_wav("spit", wet + blop * 0.6)

    # Acide qui grésille en touchant.
    t = t_axis(0.7)
    sizzle = bandpass(noise(t.size), 2000, 9000) * envelope(t, 0.01, 0.25) * (0.6 + 0.4 * np.sin(2 * np.pi * 37 * t))
    splat = lowpass(noise(t.size), 0.08) * envelope(t, 0.001, 0.03) * 2
    save_wav("acid_hit", sizzle + splat)

    # Cri du Hurleur : voix rauque qui monte puis retombe, avec vibrato.
    t = t_axis(1.6)
    pitch = 170 + 260 * np.sin(np.pi * np.clip(t / 1.4, 0, 1)) ** 0.7 + 12 * np.sin(2 * np.pi * 7 * t)
    phase = np.cumsum(2 * np.pi * pitch / RATE)
    saw = 2 * ((phase / (2 * np.pi)) % 1.0) - 1
    voice = bandpass(saw, 300, 3200) + bandpass(noise(t.size), 800, 4000) * 0.3
    amp = np.sin(np.pi * np.clip(t / 1.6, 0, 1)) ** 0.5
    save_wav("howl", voice * amp)

    # Terre qui se soulève (Fouisseur).
    t = t_axis(0.9)
    rumble = lowpass(noise(t.size), 0.01) * envelope(t, 0.05, 0.35) * 10
    grains = np.zeros(t.size)
    for g in rng.gamma(2.0, 0.08, 120):
        i = int(g * RATE)
        if i < t.size - 50:
            grains[i:i + 50] += noise(50) * np.exp(-np.arange(50) / 10)
    save_wav("dig", rumble + bandpass(grains, 700, 5000) * 0.5)

    # Phare : ping de sonar avec un écho.
    t = t_axis(0.9)
    ping = np.sin(2 * np.pi * 1400 * t) * envelope(t, 0.002, 0.18)
    echo = np.concatenate([np.zeros(int(0.22 * RATE)), ping[: t.size - int(0.22 * RATE)]]) * 0.3
    save_wav("ping", ping + echo)

    # Grillage frappé : partiels métalliques et cliquetis.
    t = t_axis(0.5)
    clang = sum(np.sin(2 * np.pi * f * t) * np.exp(-t / d) for f, d in [(310, 0.12), (523, 0.09), (847, 0.06), (1290, 0.04)])
    rattle = bandpass(noise(t.size), 2000, 7000) * envelope(t, 0.001, 0.1) * (rng.random(t.size) > 0.5)
    save_wav("barrier_hit", clang * envelope(t, 0.001, 0.2) + rattle * 0.6)

    # Barrière qui s'effondre : fracas de métal et de béton.
    t = t_axis(1.4)
    crash = lowpass(noise(t.size), 0.03) * envelope(t, 0.002, 0.4) * 6
    clangs = np.zeros(t.size)
    for start in rng.uniform(0.0, 0.8, 8):
        i = int(start * RATE)
        n = min(t.size - i, int(0.4 * RATE))
        tt = np.arange(n) / RATE
        f = rng.uniform(250, 900)
        clangs[i:i + n] += (np.sin(2 * np.pi * f * tt) + 0.5 * np.sin(2 * np.pi * f * 2.76 * tt)) * np.exp(-tt / 0.1) * rng.uniform(0.3, 1.0)
    save_wav("barrier_break", crash + clangs * 0.5)

    # Leurre : bip électronique court.
    t = t_axis(0.12)
    beep = np.sign(np.sin(2 * np.pi * 2200 * t)) * 0.5 + np.sin(2 * np.pi * 2200 * t) * 0.5
    save_wav("decoy", beep * np.minimum(t / 0.005, 1) * np.minimum((0.12 - t) / 0.02, 1))

    # Drone : bourdonnement qui boucle sans coupure (fréquences entières sur 1 s).
    t = t_axis(1.0)
    hum = sum(np.sin(2 * np.pi * f * t) * a for f, a in [(180, 1.0), (360, 0.5), (540, 0.3), (1260, 0.15)])
    save_wav("drone", hum * (0.8 + 0.2 * np.sin(2 * np.pi * 30 * t)))

    # Interface : clic, amélioration, construction, lancer.
    t = t_axis(0.06)
    save_wav("ui_click", np.sin(2 * np.pi * 1800 * t) * envelope(t, 0.0005, 0.012) + bandpass(noise(t.size), 3000, 8000) * envelope(t, 0.0003, 0.004) * 0.5)
    t = t_axis(0.7)
    clicks = np.zeros(t.size)
    for k in range(3):
        i = int((0.03 + k * 0.08) * RATE)
        clicks[i:i + 300] += noise(300) * np.exp(-np.arange(300) / 25)
    chime = np.sin(2 * np.pi * np.cumsum(880 + 440 * np.clip((t - 0.25) / 0.1, 0, 1)) / RATE) * envelope(np.clip(t - 0.25, 0, None), 0.005, 0.2) * (t > 0.25)
    save_wav("upgrade", bandpass(clicks, 1500, 7000) + chime * 0.6)
    t = t_axis(0.6)
    clank = sum(np.sin(2 * np.pi * f * t) * np.exp(-t / d) for f, d in [(140, 0.15), (410, 0.08), (980, 0.05)])
    save_wav("build", clank * envelope(t, 0.001, 0.25) + lowpass(noise(t.size), 0.05) * envelope(t, 0.001, 0.05) * 3)
    t = t_axis(0.3)
    save_wav("throw", bandpass(noise(t.size), 400, 3000) * np.sin(np.pi * t / 0.3) ** 2)


def make_ambience_sounds() -> None:
    """Ambiances en boucle propres à un chapitre (pluie de la Ville Morte)."""
    rng = np.random.default_rng(1200)
    # Pluie : souffle large et doux, plus des gouttes qui claquent sur l'asphalte et la tôle.
    # 9 s générées, la dernière seconde fondue dans la première pour une boucle sans raccord.
    t = t_axis(9.0)
    hiss = bandpass(rng.standard_normal(t.size), 500, 7000) * 0.5
    hiss += lowpass(rng.standard_normal(t.size), 0.02) * 0.8
    drops = np.zeros(t.size)
    for i in rng.integers(0, t.size - 400, 2600):
        n = np.arange(400)
        hz = rng.uniform(1500, 5000)
        drops[i:i + 400] += np.sin(2 * np.pi * hz * n / RATE) * np.exp(-n / rng.uniform(15, 60)) * rng.uniform(0.05, 0.4)
    rain = hiss + drops
    fade = RATE
    loop = rain[:-fade].copy()
    w = np.linspace(0.0, 1.0, fade)
    loop[:fade] = loop[:fade] * w + rain[-fade:] * (1.0 - w)
    save_wav("rain", loop)


if __name__ == "__main__":
    import sys
    if "batiments" in sys.argv:  # Seulement les textures des bâtiments.
        make_plaster()
        make_brick()
        make_beton()
        sys.exit(0)
    make_ground()
    make_mud()
    make_concrete()
    make_metal()
    make_skin()
    make_cloth()
    make_plaster()
    make_brick()
    make_beton()
    make_sounds()
    make_extra_sounds()
    make_ambience_sounds()
    print("Textures :", sorted(p.name for p in TEX.iterdir()))
    print("Sons :", sorted(p.name for p in SND.iterdir()))
