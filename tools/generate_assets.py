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

    t = t_axis(0.2)
    save_wav("footstep", lowpass(rng.standard_normal(t.size), 0.08) * envelope(t, 0.002, 0.03))

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


if __name__ == "__main__":
    make_ground()
    make_mud()
    make_concrete()
    make_metal()
    make_skin()
    make_cloth()
    make_sounds()
    print("Textures :", sorted(p.name for p in TEX.iterdir()))
    print("Sons :", sorted(p.name for p in SND.iterdir()))
