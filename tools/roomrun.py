#!/usr/bin/env python3
"""Запустити «під кімнату» через консоль і показати, як воно йшло (##ROOM#)."""
import sys, time, glob, os, select
sys.path.insert(0, os.path.dirname(__file__))
from selftest import Port
p = Port(os.environ.get("PORT") or sorted(glob.glob('/dev/cu.usbmodem*'))[0])
print(p.ask("roomtune", until=r"ROOM", wait=2)[-1])
t0 = time.time(); buf = b''
END = ('рівніше', 'не потрібна', 'спотворює', 'перервано', 'не чує', 'не зупиняється')
while time.time() - t0 < 150:
    r, _, _ = select.select([p.fd], [], [], 0.5)
    if r:
        try: buf += os.read(p.fd, 65536)
        except OSError: pass
    if any(e in buf.decode('utf-8', 'replace') for e in END):
        time.sleep(1); break
for l in buf.decode('utf-8', 'replace').splitlines():
    if '##ROOM' in l: print(l.strip())
print(f"тривало {time.time() - t0:.0f} с")
r = p.ask("eq", until=r"EQB 16000", wait=3)
for l in r:
    if l.startswith("EQ ") or l.startswith("EQB"): print(l)
p.close()
