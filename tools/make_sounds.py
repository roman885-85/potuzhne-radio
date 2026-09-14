#!/usr/bin/env python3
"""Стандартні звуки подій ПОТУЖНОГО РАДІО.

Синтезуються тут, а не беруться звідкись: жодних чужих ліцензій, і будь-який
звук можна переробити правкою кількох чисел. Тембр — «маримба з дзвіночком»:
основний тон і негармонічні обертони (2,76 і 5,4 основного), що гаснуть
швидше, — так звучать дерев'яні й металеві пластини. Маленький динамік радіо
низу не відтворює, тому всі ноти вище 500 Гц.

  python3 tools/make_sounds.py   →   source/yoRadio/assets/snd/default/*.wav
"""
import math, os, random, struct, wave

RATE = 22050
OUT = os.path.join(os.path.dirname(__file__), "..", "source", "yoRadio", "assets", "snd", "default")

NOTE = {"C5": 523.25, "D5": 587.33, "E5": 659.26, "G5": 783.99, "A5": 880.0,
        "C6": 1046.5, "D6": 1174.66, "E6": 1318.51, "G6": 1567.98, "A6": 1760.0,
        "B6": 1975.53, "C7": 2093.0, "E7": 2637.02}


def pluck(buf, start, f, dur, amp=1.0, decay=5.0, bright=1.0):
    """Одна нота: миттєвий напад 3 мс, експоненційне згасання."""
    n0 = int(start * RATE)
    n = int(dur * RATE)
    for i in range(n):
        if n0 + i >= len(buf):
            break
        t = i / RATE
        att = min(1.0, t / 0.003)
        v = math.sin(2 * math.pi * f * t) * math.exp(-t * decay)
        v += 0.30 * bright * math.sin(2 * math.pi * 2.76 * f * t) * math.exp(-t * decay * 3.2)
        v += 0.12 * bright * math.sin(2 * math.pi * 5.40 * f * t) * math.exp(-t * decay * 7.0)
        buf[n0 + i] += amp * att * v


def click(buf, start, amp=0.5):
    """Клацання дотику: 2 мс шуму й коротке «тік» на 2,6 кГц."""
    rnd = random.Random(7)
    n0 = int(start * RATE)
    for i in range(int(0.03 * RATE)):
        t = i / RATE
        v = math.sin(2 * math.pi * 2600 * t) * math.exp(-t * 180)
        if t < 0.002:
            v += (rnd.random() * 2 - 1) * 0.6 * (1 - t / 0.002)
        buf[n0 + i] += amp * v


def render(name, length, notes, peak_db=-3.0):
    buf = [0.0] * int(length * RATE)
    for fn in notes:
        fn(buf)
    # плавний кінець і нормування піку
    fade = int(0.02 * RATE)
    for i in range(fade):
        buf[-1 - i] *= i / fade
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


def seq(*items):
    """(час, нота, тривалість, гучність, згасання, яскравість)"""
    return [lambda b, it=it: pluck(b, it[0], NOTE[it[1]], it[2], *it[3:]) for it in items]


render("start", 1.6, seq((0.00, "C6", 1.2, 0.8, 4.0), (0.12, "E6", 1.2, 0.7, 4.0),
                         (0.24, "G6", 1.3, 0.7, 3.5), (0.40, "C7", 1.2, 0.45, 3.0, 0.6)), -3.0)
render("click", 0.04, [lambda b: click(b, 0.0)], -10.0)
render("gesture", 0.5, seq((0.00, "A6", 0.45, 0.8, 9.0), (0.09, "E7", 0.40, 0.6, 9.0, 0.7)), -5.0)
render("connect", 0.7, seq((0.00, "G5", 0.6, 0.7, 6.0), (0.07, "D6", 0.6, 0.7, 6.0), (0.14, "G6", 0.55, 0.6, 6.0, 0.7)), -5.0)
render("error", 0.7, seq((0.00, "E5", 0.5, 0.8, 7.0, 0.5), (0.18, "C5", 0.5, 0.8, 6.0, 0.5)), -5.0)
render("timer", 1.4, seq((0.00, "G6", 1.0, 0.6, 4.5, 0.6), (0.16, "E6", 1.0, 0.6, 4.5, 0.6), (0.32, "C6", 1.1, 0.7, 3.5, 0.6)), -6.0)
render("alarm", 1.8, seq((0.00, "C6", 0.4, 0.8, 7.0), (0.15, "E6", 0.4, 0.8, 7.0), (0.30, "G6", 0.4, 0.8, 7.0), (0.45, "C7", 0.5, 0.8, 6.0),
                         (0.90, "C6", 0.4, 0.8, 7.0), (1.05, "E6", 0.4, 0.8, 7.0), (1.20, "G6", 0.4, 0.8, 7.0), (1.35, "C7", 0.5, 0.8, 5.0)), -2.0)
