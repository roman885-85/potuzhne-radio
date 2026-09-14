#!/usr/bin/env python3
"""Стандартні звуки подій ПОТУЖНОГО РАДІО: басові, синтетичні, м'які.

Жодних дзвіночків, клацань і тріску — низькі синуси й теплі пилки крізь
м'який фільтр, як «увімкнення потужності». Синтезуються тут: жодних чужих
ліцензій, будь-який звук переробляється правкою чисел.

Маленький динамік справжнього низу не відтворює, тож «бас» тут подвійний:
основний тон і його гармоніки 150–600 Гц — за ними вухо саме добудовує низ
(той самий прийом, що й «віртуальний бас» у звуковій обробці радіо). Усе,
що нижче 80 Гц, зрізається: там динамік лише хрипів би.

  python3 tools/make_sounds.py   →   source/yoRadio/assets/snd/default/*.wav
"""
import math, os, random, struct, wave

RATE = 22050
OUT = os.path.join(os.path.dirname(__file__), "..", "source", "yoRadio", "assets", "snd", "default")
TAU = 2 * math.pi


def hz(note):
    names = {"C": -9, "C#": -8, "D": -7, "D#": -6, "E": -5, "F": -4, "F#": -3, "G": -2, "G#": -1, "A": 0, "A#": 1, "B": 2}
    n, o = note[:-1], int(note[-1])
    return 440.0 * 2 ** ((names[n] + (o - 4) * 12) / 12)


def put(buf, i, v):
    if 0 <= i < len(buf):
        buf[i] += v


def boom(buf, start, dur, f0, f1, amp=1.0, attack=0.008, decay=4.0, harm=((2, 0.55), (3, 0.3), (4, 0.15))):
    """Низький удар: синус, що падає з f0 до f1, з гармоніками для «баса» на малому динаміку."""
    n0, n = int(start * RATE), int(dur * RATE)
    ph = 0.0
    for i in range(n):
        t = i / RATE
        f = f1 + (f0 - f1) * math.exp(-t * 18)
        ph += TAU * f / RATE
        v = math.sin(ph)
        for k, g in harm:
            v += g * math.sin(k * ph) * math.exp(-t * decay * 0.6 * k)
        e = min(1.0, t / attack) * math.exp(-t * decay)
        put(buf, n0 + i, amp * e * v)


def glide(buf, start, dur, f0, f1, amp=0.5, attack=0.05, release=0.2, harm=((2, 0.5), (3, 0.25)), curve=1.0):
    """Протяжний тон, що ковзає з f0 до f1 (підйом «набору потужності» чи спад)."""
    n0, n = int(start * RATE), int(dur * RATE)
    ph = 0.0
    for i in range(n):
        x = i / n
        f = f0 * (f1 / f0) ** (x ** curve)
        ph += TAU * f / RATE
        v = math.sin(ph)
        for k, g in harm:
            v += g * math.sin(k * ph)
        t = i / RATE
        e = min(1.0, t / attack) * min(1.0, (dur - t) / release)
        put(buf, n0 + i, amp * e * v)


def pad(buf, start, dur, notes, amp=0.5, a=0.08, r=0.8, cut0=300, cut1=1600, cut_t=0.6, cut_end=None, detune=8.0, maxf=2400):
    """Теплий акорд: розстроєні пилки (гармоніки до maxf) крізь м'який фільтр 12 дБ/окт,
    що плавно відкривається; без резонансу — нічого не дзвенить."""
    n0, n = int(start * RATE), int(dur * RATE)
    voices = []
    for nt in notes:
        f = hz(nt) if isinstance(nt, str) else nt
        for c in (-detune, 0.0, detune):
            ks = [k for k in range(1, 60) if k * f < maxf]
            voices.append([f * 2 ** (c / 1200), random.random() * TAU, ks])
    norm = 0.5 / max(1, len(voices)) ** 0.5
    s1 = s2 = 0.0
    for i in range(n):
        t = i / RATE
        x = 0.0
        for vo in voices:
            vo[1] += TAU * vo[0] / RATE
            for k in vo[2]:
                x += math.sin(k * vo[1]) / k
        x *= norm
        if t < cut_t:
            fc = cut0 * (cut1 / cut0) ** (t / cut_t)
        elif cut_end:
            fc = cut1 * (cut_end / cut1) ** min(1.0, (t - cut_t) / max(0.001, dur - cut_t))
        else:
            fc = cut1
        g = 1 - math.exp(-TAU * fc / RATE)          # два однополюсні — м'який спад
        s1 += g * (x - s1)
        s2 += g * (s1 - s2)
        e = min(1.0, t / a) * min(1.0, max(0.0, (dur - t) / r))
        put(buf, n0 + i, amp * e * s2)


def rumble(buf, start, dur, amp=0.3, fc0=200, fc1=1200, seed=5):
    """М'який гул, що наростає: шум крізь фільтр нижніх частот, що відкривається."""
    rnd = random.Random(seed)
    n0, n = int(start * RATE), int(dur * RATE)
    s1 = s2 = 0.0
    for i in range(n):
        x = i / n
        fc = fc0 * (fc1 / fc0) ** x
        g = 1 - math.exp(-TAU * fc / RATE)
        s1 += g * ((rnd.random() * 2 - 1) - s1)
        s2 += g * (s1 - s2)
        e = (x ** 1.8) * min(1.0, (n - i) / (0.04 * RATE))
        put(buf, n0 + i, amp * e * s2 * 4.0)


def finish(buf):
    """Зріз нижче 80 Гц, тепле насичення, м'який кінець."""
    # високочастотний однополюсний на 80 Гц (двічі)
    for _ in range(2):
        a = math.exp(-TAU * 80 / RATE)
        y = xp = 0.0
        for i, x in enumerate(buf):
            y = a * (y + x - xp)
            xp = x
            buf[i] = y
    pk = max(abs(x) for x in buf) or 1.0
    out = [math.tanh(1.8 * x / pk) / math.tanh(1.8) for x in buf]
    fade = int(0.02 * RATE)
    for i in range(fade):
        out[-1 - i] *= i / fade
    return out


def render(name, length, parts, peak_db=-3.0):
    random.seed(sum(map(ord, name)))
    buf = [0.0] * int(length * RATE)
    for fn in parts:
        fn(buf)
    buf = finish(buf)
    pk = max(abs(x) for x in buf) or 1.0
    gain = (10 ** (peak_db / 20)) / pk
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", max(-32768, min(32767, int(x * gain * 32767)))) for x in buf))
    print(f"{path}: {len(buf) / RATE:.2f} с, {os.path.getsize(path)} байт")


def stab(t0, notes, dur, amp, cut=1700, cut_end=650, a=0.006, r=None):
    """Короткий акорд у тембрі розкритого акорду: той самий теплий синтезатор, без ударів."""
    return lambda b: pad(b, t0, dur, notes, amp, a=a, r=r if r is not None else dur * 0.85, cut0=cut, cut1=cut, cut_t=0.005, cut_end=cut_end)


# Увімкнення — у лад із заставкою (tools/make_splash.py, 25 кадр/с). Власникові сподобався
# розкритий акорд («другий такт»); перший, із гулом і низькими ударами, «бубонів». Тепер увесь
# звук — один тембр:
#   0–0,36 с  той самий синтезатор наростає, фільтр відкривається — набір потужності;
#   0,35 с    точка засвітилась — короткий акорд;
#   0,46/0,66 розгортаються хвилі — два короткі акорди вгору;
#   0,98 с    з'являється напис — потужний низький акорд, що розкривається фільтром.
render("start", 2.8, [
    lambda b: pad(b, 0.00, 0.37, ["D2", "D3"], 0.55, a=0.28, r=0.05, cut0=140, cut1=1000, cut_t=0.37),
    stab(0.35, ["D3", "A3", "D4"], 0.40, 0.85, cut=1800, cut_end=600),
    stab(0.46, ["F#3", "D4"], 0.20, 0.45, cut=1500, cut_end=700),
    stab(0.66, ["A3", "D4"], 0.22, 0.45, cut=1500, cut_end=700),
    lambda b: pad(b, 0.98, 1.8, ["D2", "D3", "A3", "D4"], 0.95, a=0.09, r=1.1, cut0=250, cut1=1500, cut_t=0.7, cut_end=700),
    lambda b: glide(b, 0.98, 1.8, 73.4, 73.4, 0.35, attack=0.12, release=1.2, harm=((2, 0.6), (3, 0.3))),
], -3.0)
# Дотик: м'яке коротке «туп» того ж синтезатора
render("click", 0.07, [stab(0.0, ["D3"], 0.06, 0.9, cut=1200, cut_end=400, a=0.002)], -14.0)
# Жест прийнято: два короткі акорди вгору
render("gesture", 0.5, [
    stab(0.00, ["D3", "A3"], 0.20, 0.8, cut=1600, cut_end=700),
    stab(0.13, ["A3", "D4"], 0.30, 0.8, cut=1700, cut_end=650),
], -6.0)
# Мережа з'явилась: синтезатор наростає й розкривається акордом
render("connect", 1.0, [
    lambda b: pad(b, 0.00, 0.28, ["A2", "A3"], 0.5, a=0.2, r=0.04, cut0=150, cut1=900, cut_t=0.28),
    lambda b: pad(b, 0.26, 0.72, ["A2", "E3", "A3"], 0.8, a=0.03, r=0.45, cut0=300, cut1=1400, cut_t=0.25, cut_end=600),
], -6.0)
# Мережа зникла: два акорди вниз, фільтр закривається
render("error", 1.0, [
    stab(0.00, ["A3", "E4"], 0.30, 0.7, cut=1500, cut_end=500),
    stab(0.22, ["F3", "C4"], 0.70, 0.7, cut=1300, cut_end=220),
], -6.0)
# Таймер сну: глибокий акорд повільно гасне разом із фільтром — вимкнення
render("timer", 2.2, [
    lambda b: pad(b, 0.00, 2.1, ["D2", "A2", "D3", "F#3"], 0.8, a=0.15, r=1.4, cut0=1400, cut1=1400, cut_t=0.01, cut_end=180),
], -7.0)
# Будильник: короткі акорди, що наростають, і довгий розкритий акорд
render("alarm", 4.0, [stab(0.6 * k, ["D3", "A3", "D4"], 0.3, 0.5 + 0.1 * k, cut=1700, cut_end=700) for k in range(5)] +
       [lambda b: pad(b, 3.0, 1.0, ["D3", "A3", "D4"], 0.9, a=0.04, r=0.6, cut0=400, cut1=1800, cut_t=0.3, cut_end=900)], -3.0)
