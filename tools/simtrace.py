#!/usr/bin/env python3
"""Трасування імітатора жестів: рівень мікрофона по блоках і що з цього почуто."""
import sys, os, time, glob, select
sys.path.insert(0, os.path.dirname(__file__))
from selftest import Port
p = Port(os.environ.get("PORT") or sorted(glob.glob('/dev/cu.usbmodem*'))[0])
p.ask("stop", wait=1); p.ask("micon 1", wait=.5); p.ask("micset 1 1 0", wait=.5); p.ask("micdbg 1", wait=.5)
for cmd in sys.argv[1:]:
    time.sleep(2.5)
    p.buf = b''
    os.write(p.fd, (f"micsim {cmd}\n").encode())
    t0 = time.time(); buf = b''
    while time.time() - t0 < 3.5:
        r, _, _ = select.select([p.fd], [], [], 0.2)
        if r: buf += os.read(p.fd, 65536)
    lines = buf.decode('utf-8', 'replace').splitlines()
    peaks = [l.split('\t')[1] for l in lines if '##LVL#' in l and int(l.split('\t')[1].split()[1]) > -45]
    t = [int(x.split()[0]) for x in peaks]
    starts = [t[i] for i in range(len(t)) if i == 0 or t[i] - t[i-1] > 100]
    res = [l.strip() for l in lines if '##MIC#' in l or 'MICSIM' in l]
    print(f"{cmd:>16}: удари чутно в " + ", ".join(str(x - starts[0]) for x in starts) + " мс" if starts else f"{cmd:>16}: нічого не чутно", "|", "; ".join(res))
p.ask("micdbg 0", wait=.5); p.ask("micset 0 0 0", wait=.5)
p.close()
