#!/usr/bin/env python3
"""Список станцій без рук: дотик до рядка, дотик під час пружини, кидок,
повільне ведення, дотик по смузі (грати). З середини списку."""
import sys, os, time, glob, re, select
sys.path.insert(0, os.path.dirname(__file__))
from selftest import Port
p = Port(os.environ.get("PORT") or sorted(glob.glob('/dev/cu.usbmodem*'))[0])

def run(cmd, wait):
    os.write(p.fd, (cmd + "\n").encode()); t0 = time.time(); buf = b''
    while time.time() - t0 < wait:
        r, _, _ = select.select([p.fd], [], [], 0.1)
        if r: buf += os.read(p.fd, 65536)
    return buf.decode('utf-8', 'replace')

def st():
    run("", 0.3)
    os.write(p.fd, b"plstate\n"); t0 = time.time(); buf = b''
    while time.time() - t0 < 5:
        r, _, _ = select.select([p.fd], [], [], 0.1)
        if r: buf += os.read(p.fd, 65536)
        m = re.search(r"позиція=([\d.]+) \S+ режим=(\d+) станція=(\d+)", buf.decode('utf-8', 'replace'))
        if m: return int(m.group(3))
    print("      [plstate без відповіді]", buf.decode('utf-8','replace')[-200:].replace("\n"," | "))
    return None

ok = 0; n = 0
def check(name, cond, extra=""):
    global ok, n
    n += 1; ok += bool(cond)
    print(f"{'ОК     ' if cond else 'ПОМИЛКА'} {name} {extra}")

run("list", 1.5)
for _ in range(8):                             # у середину списку
    if (st() or 0) <= 35: break
    run("drag 120 50 230 700", 3.2)
s = st(); print("старт:", s)
run("shold 120 184 60", 1.2); a = st(); check("дотик до рядка на 2 нижче", a == s + 2, f"{s} → {a}")
run("shold 120 56 60", 1.2); b = st(); check("дотик до рядка на 2 вище", b == s, f"{a} → {b}")
run("shold 120 152 60", 1.2); c = st(); check("дотик до сусіднього рядка", c == s + 1, f"{b} → {c}")
os.write(p.fd, b"shold 120 184 60\n"); time.sleep(0.22)
run("shold 120 184 60", 1.4); d = st(); check("другий дотик під час пружини вибирає, а не зупиняє", d is not None and d >= c + 3, f"{c} → {d}")
run("shold 120 139 60", 1.2); e = st(); check("дотик на 3 px нижче смуги — наступна станція, не через одну", e == d + 1, f"{d} → {e}")
run("shold 120 100 60", 1.2); e2 = st(); check("дотик на 4 px вище смуги — попередня, не через одну", e2 == e - 1, f"{e} → {e2}")
e = e2
t = run("drag 120 210 50 180", 3.2); f = st()
check("кидок угору", f is not None and f - e >= 6, f"{e} → {f} (" + ";".join(l.split('\t')[1] for l in t.splitlines() if '##PL#' in l) + ")")
t = run("drag 120 60 188 900", 3.4); g = st(); check("повільне ведення вниз на 4 рядки", g is not None and abs((f - g) - 4) <= 1, f"{f} → {g}")
t = run("shold 120 120 60", 2.0); check("дотик по смузі — грати", "смуга=1" in t, ";".join(l.split('\t')[1] for l in t.splitlines() if '##PL#' in l))
run("stop", 1.0)
print(f"\nпідсумок: {ok} з {n}")
p.close()
