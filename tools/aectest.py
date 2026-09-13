#!/usr/bin/env python3
"""Порівняння режимів віднімання луни (esp_aec) на самому радіо.

Поки грає станція, для кожного режиму й довжини фільтра: дати звикнути,
потім кілька разів прочитати, на скільки дБ віднято власний звук і що
лишилось від нього в мікрофоні.

  python3 tools/aectest.py              усі режими, фільтр 4
  python3 tools/aectest.py 0:4 3:8      лише задані «режим:фільтр»
"""
import sys, re, time, os
sys.path.insert(0, os.path.dirname(__file__))
from selftest import Port
import glob

MODES = {0: "SR low cost", 1: "SR high perf", 3: "VOIP low cost", 4: "VOIP high perf"}


def main():
    pairs = [tuple(int(v) for v in a.split(":")) for a in sys.argv[1:]] or [(0, 4), (1, 4), (3, 4), (4, 4)]
    path = os.environ.get("PORT") or sorted(glob.glob("/dev/cu.usbmodem*"))[0]
    p = Port(path)
    info = p.ask("info", until=r"heap=", wait=2)
    if not any("грає=1" in l for l in info):
        p.ask("play 66", wait=1)
        time.sleep(8)
    p.ask("micon 1", wait=0.5)
    p.ask("micset 0 0 1", wait=0.5)
    res = []
    for mode, ln in pairs:
        p.ask(f"aecmode {mode} {ln}", until=r"AEC режим", wait=1)
        time.sleep(6)                      # звикнути
        cuts, outs, ins = [], [], []
        for _ in range(8):
            r = p.ask("mic", until=r"мікрофон: слухає", wait=2)
            m = next((re.search(r"рівень=(-?\d+) .* режим=(\d+) .* віднято=(-?[\d.]+) дБ .* L=(-?\d+) R=(-?\d+)", l) for l in r if "мікрофон:" in l), None)
            if m and int(m.group(2)) == mode and int(m.group(5)) > -80:
                outs.append(int(m.group(1))); cuts.append(float(m.group(3))); ins.append(int(m.group(4)))
            time.sleep(1)
        load = p.ask("tasks", until=r"arduino_events|sys_evt", wait=3)
        mic = next((l.split()[2] for l in load if re.search(r"\smic\s", l)), "?")
        if cuts:
            avg = lambda v: sum(v) / len(v)
            res.append((mode, ln, avg(cuts), avg(ins), avg(outs), mic))
            print(f"режим {mode} ({MODES.get(mode, '?')}), фільтр {ln}: віднято {avg(cuts):5.1f} дБ; мікрофон {avg(ins):5.1f} → {avg(outs):5.1f} дБ; задача mic {mic}")
        else:
            print(f"режим {mode}, фільтр {ln}: немає даних (не грає?)")
    p.close()
    if res:
        best = max(res, key=lambda r: r[2])
        print(f"\nнайкраще: режим {best[0]} ({MODES.get(best[0], '?')}), фільтр {best[1]} — {best[2]:.1f} дБ")


if __name__ == "__main__":
    main()
