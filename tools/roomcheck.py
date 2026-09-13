#!/usr/bin/env python3
"""Чи вирівнює поправка під кімнату звук: два заміри через увесь ланцюг —
без поправки й з нею (еквалайзер людини на час перевірки вимкнено) — і
нерівність АЧХ у смузі, яку динамік відтворює (500 Гц … 16 кГц)."""
import sys, os, time, math, glob, statistics
sys.path.insert(0, os.path.dirname(__file__))
from selftest import Port, sweep, points, eq_read, eq_restore

class O: level = -12; v = False

def octave_levels(pts):
    bands = {500: (400, 630), 1000: (800, 1250), 2000: (1600, 2500), 4000: (3150, 5000), 8000: (6300, 10000), 16000: (12500, 16000)}
    out = {}
    for c, (lo, hi) in bands.items():
        v = [10 ** (x["дб"] / 10) for x in pts if lo <= x["гц"] <= hi and x["дб"] - x["фон"] > 10]
        if v: out[c] = 10 * math.log10(sum(v) / len(v))
    return out

p = Port(os.environ.get("PORT") or sorted(glob.glob("/dev/cu.usbmodem*"))[0])
h, bands = eq_read(p)
res = {}
try:
    for name, room in (("без поправки", 0), ("з поправкою", 1)):
        p.ask(f"eqo 0 0 {h[3]} 0 {room}", until=r"звук:", wait=1)
        st, rows = sweep(p, "mictest -12 dsp", O, 25)
        lv = octave_levels(points(rows))
        vals = list(lv.values())
        res[name] = (lv, max(vals) - min(vals), statistics.pstdev(vals))
        print(f"{name}: " + "  ".join(f"{k}:{v:+.1f}" for k, v in lv.items()) + f"   розмах {res[name][1]:.1f} дБ, відхилення {res[name][2]:.1f} дБ")
finally:
    eq_restore(p, h, bands)
    p.close()
a, b = res["без поправки"], res["з поправкою"]
print(f"\nрозмах АЧХ 500 Гц…16 кГц: {a[1]:.1f} → {b[1]:.1f} дБ; стандартне відхилення {a[2]:.1f} → {b[2]:.1f} дБ")
