#!/usr/bin/env python3
"""Хлопки й стук «із кімнати» поверх того, що радіо саме грає.

Удар домішується цифрово до сигналу мікрофона (micext), а не грається
динаміком: так власний звук радіо в опорному каналі лишається справжнім,
і видно, чи відрізняє радіо чужий удар від ударних у своїй пісні.
Дії жестів на час перевірки вимкнено («нічого»), потім повертаються."""
import sys, os, time, glob, re, select, json, urllib.request
sys.path.insert(0, os.path.dirname(__file__))
from selftest import Port

IP = os.environ.get("IP", "192.168.1.87")
p = Port(os.environ.get("PORT") or sorted(glob.glob('/dev/cu.usbmodem*'))[0])

def api(path):
    for i in range(6):
        try: return json.load(urllib.request.urlopen(f"http://{IP}{path}", timeout=5))
        except (OSError, ValueError):
            if i == 5: raise
            time.sleep(1.0)

def drain(sec):
    t0 = time.time(); buf = b''
    while time.time() - t0 < sec:
        rr, _, _ = select.select([p.fd], [], [], 0.2)
        if rr:
            try: buf += os.read(p.fd, 65536)
            except OSError: pass
    return buf.decode('utf-8', 'replace')

def run(kind, n, gap, peak, expect):
    drain(1.5)
    r = p.ask(f"micext {kind} {n} {gap} {peak}", until=r"MICEXT", wait=6)
    started = any("почато" in l for l in r)
    txt = "\n".join(r) + "\n" + drain(n * gap / 1000 + 2.2)
    got = re.findall(r"##MIC#\t(.+)", txt)
    ons = [l.split("#\t")[-1].strip() for l in txt.splitlines() if ("##ONSET#" in l or (ALL and "##LVL#" in l)) and (ALL or "УДАР" in l or "рішення" in l or "своє" in l)]
    heard = got[0].strip() if got else "нічого"
    good = started and ((expect is None and not got) or (expect is not None and bool(got) and expect in got[0]))
    print(f"{'ОК     ' if good else 'ПОМИЛКА'} {kind} ×{n}, крок {gap} мс, пік {peak} дБ: почуто «{heard}»; очікувалось «{expect or 'нічого'}»" + ("" if started else "  [перевірка не почалась]"))
    if not good or VERB:
        for l in ons: print("         " + l)
    return 1 if good else 0

VERB = "-v" in sys.argv or "-a" in sys.argv
ALL = "-a" in sys.argv
ONLY = next((a for a in sys.argv[1:] if not a.startswith("-")), None)
before = api("/api/state")["mic"]
st = api("/api/state")
print(f"грає={st.get('play')}, гучність={st.get('vol')}, станція «{st.get('name')}»")
for k in ("clap2", "clap3", "knock2", "knock3"):
    api(f"/api/set?{k}=1")
p.ask("micdbg 1", wait=0.5)
drain(3.0)
res = []
SOAK = next((int(a.split("=")[1]) for a in sys.argv[1:] if a.startswith("--soak=")), 0)
try:
    if SOAK:
        txt = drain(SOAK)
        got = re.findall(r"##MIC#\t(.+)", txt)
        ons = [l.split("##ONSET#\t")[1] for l in txt.splitlines() if "##ONSET#" in l]
        hits = [l for l in ons if "УДАР" in l]
        own = [l for l in ons if "своє" in l and "тихо" not in l]
        ex = sorted(float(re.search(r"понад=(-?[\d.]+)/(-?[\d.]+)", l).group(2 if "вид=1" in l else 1)) for l in own if "понад=" in l)
        print(f"{'ОК     ' if not got else 'ПОМИЛКА'} музика {SOAK} с: гучних своїх ударів {len(own)}, прийнято ударів {len(hits)}, команд {len(got)}" + (": " + "; ".join(got) if got else ""))
        if ex: print(f"         перевищення у своїй смузі (гучні свої): медіана {ex[len(ex)//2]:.0f}, 90% {ex[int(len(ex)*0.9)]:.0f}, макс {ex[-1]:.0f}")
        for l in hits: print("         " + l)
        res.append(0 if got else 1)
    if ONLY and not SOAK:
        k, n, pk = ONLY.split(":"); res.append(run(k, int(n), 350, int(pk), f"{n} {'хлопки' if k == 'clap' else 'стуки'}"))
    for peak in (() if ONLY or SOAK else (-6, -12, -18)):
        res.append(run("clap", 2, 350, peak, "2 хлопки"))
        res.append(run("clap", 3, 350, peak, "3 хлопки"))
        res.append(run("knock", 2, 350, peak, "2 стуки"))
    if not ONLY and not SOAK:
        res.append(run("clap", 1, 350, -6, None))
        res.append(run("clap", 2, 900, -6, None))
finally:
    p.ask("micdbg 0", wait=0.5)
    for k in ("clap2", "clap3", "knock2", "knock3"):
        api(f"/api/set?{k}=0")
    time.sleep(1.0)
    after = api("/api/state")["mic"]
    same = all(after[k] == before[k] for k in ("clap2", "clap3", "knock2", "knock3"))
    print("дії жестів повернуто" if same else f"УВАГА: дії жестів не повернулись: {after}")
print(f"\nпідсумок: {sum(res)} з {len(res)}")
p.close()
