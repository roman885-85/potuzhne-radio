#!/usr/bin/env python3
"""Хлопки й стук без рук: радіо грає їх через свій динамік (micsim), а
мікрофон має їх розпізнати й виконати дію. Перевіряє розпізнавання,
ритм (двічі / тричі) і хибні спрацювання (одиночний удар, повільний ритм)."""
import sys, os, time, glob, re, select
sys.path.insert(0, os.path.dirname(__file__))
from selftest import Port

p = Port(os.environ.get("PORT") or sorted(glob.glob('/dev/cu.usbmodem*'))[0])

def state():
    r = p.ask("info", until=r"heap=", wait=2)
    m = next((re.search(r"станція=(\d+) гучність=(\d+) грає=(\d)", l) for l in r if "станція=" in l), None)
    return (int(m.group(1)), int(m.group(3))) if m else (None, None)

def drain(sec):
    t0 = time.time(); buf = b''
    while time.time() - t0 < sec:
        rr, _, _ = select.select([p.fd], [], [], 0.2)
        if rr:
            try: buf += os.read(p.fd, 65536)
            except OSError: pass
    return buf.decode('utf-8', 'replace')

def stop_player():
    for _ in range(6):
        p.ask("stop", wait=0.8)
        drain(1.2)
        if state()[1] == 0: return True
    return False

def run(kind, n, gap, expect):
    stop_player()
    drain(2.5)                                  # звук стих, поріг устиг опуститись
    st0 = state()
    started = False
    for _ in range(5):
        r = p.ask(f"micsim {kind} {n} {gap}", until=r"MICSIM", wait=2)
        if any("почато" in l for l in r): started = True; break
        drain(1.0)
    txt = drain(n * gap / 1000 + 1.8)
    got = re.findall(r"##MIC#\t(.+)", txt)
    ons = [l for l in txt.splitlines() if "##ONSET#" in l and ("УДАР" in l or "рішення" in l)]
    st1 = state()
    heard = got[0].strip() if got else "нічого"
    good = started and ((expect is None and not got) or (expect is not None and bool(got) and expect in got[0]))
    print(f"{'ОК     ' if good else 'ПОМИЛКА'} {kind} ×{n}, крок {gap} мс: почуто «{heard}»; очікувалось «{expect or 'нічого'}»" + ("" if started else "  [перевірка не почалась]"))
    if not good or VERB:
        for l in ons: print("         " + l.split("##ONSET#")[1].strip())
    return 1 if good else 0

def music(sec):
    """Станція грає з увімкненими хлопками й стуком: жодної команди бути не має."""
    p.ask("play 66", wait=1)
    for _ in range(20):                         # дочекатися, доки справді заграє
        if state()[1] == 1: break
        drain(1.0)
    if state()[1] != 1:
        print("ПОМИЛКА музика: станція не заграла — перевірку не зроблено"); return 0
    drain(3.0)                                  # зв'язок «динамік → мікрофон» вивчився
    txt = drain(sec)
    got = re.findall(r"##MIC#\t(.+)", txt)
    hits = [l for l in txt.splitlines() if "УДАР" in l]
    own = sum(1 for l in txt.splitlines() if "##ONSET#" in l and "своє" in l)
    print(f"{'ОК     ' if not got else 'ПОМИЛКА'} музика {sec} с: ударів у пісні відкинуто як своє — {own}, прийнято ударів — {len(hits)}, команд — {len(got)}" + (": " + "; ".join(got) if got else ""))
    return 0 if got else 1

VERB = "-v" in sys.argv
import json, urllib.request
IP = os.environ.get("IP", "192.168.1.87")
def api(path):
    for i in range(6):
        try: return json.load(urllib.request.urlopen(f"http://{IP}{path}", timeout=5))
        except (OSError, ValueError):
            if i == 5: return None
            time.sleep(1.0)
before = api("/api/state")                      # що було до перевірки — поверну наприкінці
p.ask("micon 1", wait=0.5)
# Імітація йде через ЦАП радіо, а його гучність — гучність власника: на 14 з 254
# хлопок із динаміка тихіший за відлуння, і перевірка міряла б не те.
p.ask("vol 150", wait=0.5)
p.ask("micset 1 1 0", wait=0.5)
p.ask("micdbg 1", wait=0.5)
drain(4.0)                                      # мікрофон прогрівся: фон і поріг устоялись
res = []
res.append(run("clap", 2, 300, "2 хлопки"))
res.append(run("clap", 3, 300, "3 хлопки"))
res.append(run("clap", 1, 300, None))
res.append(run("clap", 2, 800, None))
res.append(run("knock", 2, 300, "2 стуки"))
res.append(run("knock", 3, 280, "3 стуки"))
res.append(music(int(os.environ.get("MUSIC", "60"))))
stop_player()
p.ask("micdbg 0", wait=0.5)
if before:
    p.ask(f"vol {before['vol']}", wait=0.5)
    m = before["mic"]
    p.ask(f"micset {m['clapOn']} {m['knockOn']} {m['play']}", wait=0.5)
    if not m["on"]: p.ask("micon 0", wait=0.5)
    if before.get("play"): p.ask(f"play {before['idx']}", wait=1.0)
    print(f"повернуто: хлопки={m['clapOn']} стук={m['knockOn']} під час звуку={m['play']}" + (f", грає станція {before['idx']}" if before.get("play") else ""))
else:
    p.ask("micset 0 0 0", wait=0.5)
print(f"\nпідсумок: {sum(res)} з {len(res)}")
p.close()
