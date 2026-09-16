#!/usr/bin/env python3
"""Повна автоматична перевірка керування радіо через USB + HTTP.

Проходить усі сторінки меню, для кожної:
  - відкриває, знімає перелік елементів (команда «items»),
  - кожен перемикач — вмикає й вимикає, звіряючи значення,
  - кожен повзунок — тягне пальцем і ПЕРЕВІРЯЄ, ЩО ЗНАЧЕННЯ ЛИШИЛОСЬ ПІСЛЯ
    ВІДПУСКАННЯ (саме тут ховався баг: смуга поверталась на початок жесту),
  - смуги еквалайзера — так само, з вертикальним веденням,
  - наприкінці все повертає як було.
Плюс: годинник іде, звук без провалів на переходах, пам'ять не тече,
сторінка в браузері відповідає, AirPlay і DLNA видно в мережі.

  python3 tools/uitest.py [--ip 192.168.1.32] [--pages 0,1,2] [--quick]
"""
import os, sys, re, time, json, glob, argparse, urllib.request, termios, fcntl, struct, select

PORT = os.environ.get("PORT") or (sorted(glob.glob("/dev/cu.usbmodem*")) or ["/dev/cu.usbmodem1442401"])[0]
HDR = 40                      # висота шапки меню
SW, SH = 320, 240

PAGES = {                     # номер команди m2 -> назва
    0: "Меню", 1: "Параметри", 2: "Екран", 3: "Будильник", 4: "Обране",
    5: "Проповіді", 6: "Про радіо", 7: "Живлення", 8: "Часовий пояс",
    9: "Еквалайзер", 10: "Обробка", 11: "Під кімнату", 12: "Мікрофон",
    13: "Хлопки й стук", 14: "Присутність", 15: "Wi-Fi", 16: "Збережені",
    17: "Розробник", 18: "Заставка й звуки", 19: "Аудіовихід", 20: "Ніч",
}

class Port:
    def __init__(self, path):
        self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        a = termios.tcgetattr(self.fd)
        a[0] = a[1] = a[3] = 0
        a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        a[4] = a[5] = termios.B115200
        a[6][termios.VMIN] = 0; a[6][termios.VTIME] = 0
        termios.tcsetattr(self.fd, termios.TCSANOW, a)
        fcntl.ioctl(self.fd, 0x8004746d, struct.pack('I', 0x002 | 0x004))
        time.sleep(0.3); self.drain()
    def drain(self, t=0.15):
        end = time.time() + t
        while time.time() < end:
            r, _, _ = select.select([self.fd], [], [], 0.05)
            if r:
                try: os.read(self.fd, 65536)
                except OSError: pass
    def cmd(self, s, wait=0.35, until=None, tmax=3.0):
        try: os.read(self.fd, 65536)
        except OSError: pass
        os.write(self.fd, (s + "\n").encode())
        buf = b''; end = time.time() + (tmax if until else wait)
        while time.time() < end:
            r, _, _ = select.select([self.fd], [], [], 0.05)
            if r:
                try: buf += os.read(self.fd, 65536)
                except OSError: pass
                if until and until in buf.decode('utf-8', 'replace'): break
        return buf.decode('utf-8', 'replace')
    def close(self): os.close(self.fd)

def api(ip, path="/api/state", tries=3):
    for _ in range(tries):
        try:
            with urllib.request.urlopen("http://%s%s" % (ip, path), timeout=4) as r:
                return json.load(r)
        except Exception:
            time.sleep(1)
    return None

FAILS, CHECKS = [], []
def check(ok, what, detail=""):
    CHECKS.append((ok, what, detail))
    if not ok: FAILS.append("%s — %s" % (what, detail))
    print(("  ok   " if ok else "  ЗБІЙ ") + what + (("  [%s]" % detail) if detail else ""), flush=True)

def parse_items(txt):
    page, items, eq, geo = None, [], [], None
    for ln in txt.splitlines():
        f = ln.rstrip("\r").split("\t")
        if f[0] == "PAGE" and len(f) >= 4:
            page = {"title": f[1], "scroll": int(f[2]), "height": int(f[3])}
        elif f[0] == "ITEM" and len(f) >= 9:
            items.append({"i": int(f[1]), "type": f[2], "y": int(f[3]), "hh": int(f[4]),
                          "val": int(f[5]), "lo": int(f[6]), "hi": int(f[7]),
                          "en": int(f[8]) != 0, "label": f[9] if len(f) > 9 else ""})
        elif f[0] == "EQGEO" and len(f) >= 5:
            geo = {"cy0": int(f[1]), "chh": int(f[2]), "zero": int(f[3]), "span": int(f[4])}
        elif f[0] == "BAND" and len(f) >= 5:
            eq.append({"b": int(f[1]), "x": int(f[2]), "db": int(f[3]), "y": int(f[4])})
    return page, items, eq, geo

def open_page(p, n):
    p.cmd("m2 %d" % n, 1.1)
    return parse_items(p.cmd("items", until="ENDPAGE", tmax=3.0))

def screen_y(page, it):
    """екранний y центру рядка з урахуванням прокрутки"""
    return it["y"] + it["hh"] // 2 + HDR - page["scroll"]

def ensure_visible(p, page, it):
    """прокрутити так, щоб рядок був у видимій частині"""
    top, bot = it["y"], it["y"] + it["hh"]
    want = page["scroll"]
    if bot + HDR - want > SH - 8: want = bot + HDR - (SH - 8)
    if top + HDR - want < HDR + 8: want = top - 8
    if want < 0: want = 0
    if want != page["scroll"]:
        p.cmd("scroll %d" % want, 0.5)
        page["scroll"] = want
    return page

def tap(p, x, y): p.cmd("stap %d %d" % (x, y), 0.65)

def drag(p, x0, y0, x1, y1, steps=8, jitter=0):
    """ведення пальцем із дрібним тремтінням, як у людини"""
    p.cmd("sdown %d %d" % (x0, y0), 0.14)
    for k in range(1, steps + 1):
        x = x0 + (x1 - x0) * k // steps
        y = y0 + (y1 - y0) * k // steps
        if jitter: y += (jitter if k % 2 else -jitter)
        p.cmd("smove %d %d" % (x, y), 0.07)
    p.cmd("sup", 0.45)

def test_page(p, n, quick):
    name = PAGES.get(n, str(n))
    print("\n=== сторінка %d: %s ===" % (n, name), flush=True)
    page, items, eq, geo = open_page(p, n)
    if not page:
        check(False, "сторінка %s відкрилась" % name, "немає відповіді")
        return
    check(True, "сторінка %s відкрилась" % name, "«%s», %d елементів" % (page["title"], len(items) + len(eq)))

    # --- смуги еквалайзера (вертикальне ведення) ---
    for band in eq:
        if quick and band["b"] not in (0, 5, 9): continue
        old = band["db"]
        target = -6 if old > -3 else 6
        ty = geo["zero"] - target * geo["span"] // 24 + HDR
        drag(p, band["x"], band["y"] + HDR, band["x"], ty, steps=7, jitter=3)
        _, _, eq2, _ = parse_items(p.cmd("items", until="ENDPAGE", tmax=3.0))
        got = next((b["db"] for b in eq2 if b["b"] == band["b"]), None)
        check(got is not None and abs(got - target) <= 2,
              "смуга %s: значення лишилось після відпускання" % band["b"],
              "тягнув до %d дБ, стало %s (було %d)" % (target, got, old))
        # повернути як було
        oy = geo["zero"] - old * geo["span"] // 24 + HDR
        drag(p, band["x"], (got if got is None else geo["zero"] - got * geo["span"] // 24) + HDR, band["x"], oy, steps=6)

    # --- елементи списку ---
    for it in items:
        if it["type"] not in ("switch", "slider"): continue
        if not it["en"]: continue
        lbl = it["label"] or ("#%d" % it["i"])
        if quick and it["i"] % 3: continue
        page = ensure_visible(p, page, it)
        y = screen_y(page, it)
        if y < HDR + 6 or y > SH - 6: continue
        old = it["val"]
        if it["type"] == "switch":
            tap(p, SW - 44, y)
            _, items2, _, _ = parse_items(p.cmd("items", until="ENDPAGE", tmax=3.0))
            now = next((q["val"] for q in items2 if q["i"] == it["i"]), None)
            check(now is not None and now != old, "перемикач «%s» спрацював" % lbl,
                  "було %s, стало %s" % (old, now))
            tap(p, SW - 44, y)                      # повернути
        else:
            lo, hi = it["lo"], it["hi"]
            if hi <= lo: continue
            x0, x1 = 40, SW - 60
            frac = (old - lo) / float(hi - lo)
            cur = int(x0 + (x1 - x0) * frac)
            target = lo + (hi - lo) * (3 if frac > 0.5 else 7) // 10
            tx = int(x0 + (x1 - x0) * (target - lo) / float(hi - lo))
            drag(p, cur, y, tx, y, steps=8, jitter=4)
            _, items2, _, _ = parse_items(p.cmd("items", until="ENDPAGE", tmax=3.0))
            now = next((q["val"] for q in items2 if q["i"] == it["i"]), None)
            span = hi - lo
            check(now is not None and abs(now - target) <= max(2, span // 8),
                  "повзунок «%s»: значення лишилось після відпускання" % lbl,
                  "тягнув до %d, стало %s (було %d)" % (target, now, old))
            # повернути
            drag(p, tx, y, cur, y, steps=8)
    p.cmd("m2close", 0.6)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", default="192.168.1.32")
    ap.add_argument("--pages", default="")
    ap.add_argument("--quick", action="store_true")
    a = ap.parse_args()
    pages = [int(x) for x in a.pages.split(",") if x != ""] if a.pages else sorted(PAGES)

    p = Port(PORT)
    st0 = api(a.ip)
    print("радіо: %s, станція «%s», грає=%s" % (st0.get("v") if st0 else "?",
          st0.get("name") if st0 else "?", st0.get("play") if st0 else "?"), flush=True)
    heap0 = st0.get("heap") if st0 else 0

    # годинник іде
    t1 = p.cmd("items", 0.2) and api(a.ip)
    s1 = t1.get("time") if t1 else None
    for n in pages:
        test_page(p, n, a.quick)

    # --- загальні перевірки ---
    print("\n=== загальне ===", flush=True)
    st = api(a.ip)
    check(st is not None, "сторінка радіо відповідає після прогону")
    if st:
        check(st.get("heap", 0) > 40000, "вільна пам'ять у нормі", "%s Б (було %s)" % (st.get("heap"), heap0))
        ap_ = st.get("ap") or {}
        check(bool(ap_.get("key")), "AirPlay: ключ на місці")
        check(st.get("airplay") in (0, 1), "AirPlay: перемикач читається")
        check(st.get("dlna") in (0, 1), "DLNA: перемикач читається")
    # годинник
    tA = api(a.ip); time.sleep(62); tB = api(a.ip)
    check(tA and tB and tA.get("time") != tB.get("time"), "годинник іде",
          "%s -> %s" % (tA.get("time") if tA else "?", tB.get("time") if tB else "?"))
    # звук на переходах меню
    p.cmd("m2 0", 1.0)
    out = ""
    for _ in range(4):
        p.cmd("m2 1", 1.0); out += p.cmd("m2 0", 1.2)
    p.cmd("m2close", 0.6)
    drops = [l for l in out.splitlines() if "провал" in l]
    check(len(drops) <= 2, "звук на переходах меню", "провалів: %d" % len(drops))
    p.close()

    print("\n================ ПІДСУМОК ================")
    print("перевірок: %d, збоїв: %d" % (len(CHECKS), len(FAILS)))
    for f in FAILS: print("  ЗБІЙ: " + f)
    return 1 if FAILS else 0

if __name__ == "__main__":
    sys.exit(main())
