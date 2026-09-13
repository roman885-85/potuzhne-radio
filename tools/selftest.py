#!/usr/bin/env python3
"""Самоперевірка ПОТУЖНОГО РАДІО через USB — без людини біля радіо.

Що перевіряє:
  пам'ять      — вільна внутрішня пам'ять і найбільший шматок (менше ~20 КБ — радіо падає)
  цикл і екран — обертів циклу за секунду, кадрів екрана, найдовші кроки
  мережа       — стан підключення, адреса
  кодек        — ES8311 відповідає, вивід підсилювача
  мікрофон     — слухає, рівень і фон, у якому слоті I2S сигнал
  звук         — (якщо не --no-sound) радіо грає 25 тонів 63 Гц…16 кГц через свій
                 динамік і міряє їх мікрофоном: АЧХ тракту, фон, спотворення

Запуск:
  python3 tools/selftest.py                 повна перевірка
  python3 tools/selftest.py --no-sound      без тонів
  python3 tools/selftest.py --stop          якщо грає — зупинити на час замірів і потім продовжити
  python3 tools/selftest.py --level -26     тихіші тони (дБ повної шкали, типово -20)
  python3 tools/selftest.py --eq            ще й перевірити еквалайзер на слух мікрофоном
  python3 tools/selftest.py --json out.json зберегти результат
Порт — сам (перший /dev/cu.usbmodem*), або змінна PORT.
"""
import os, sys, re, time, json, glob, math, termios, fcntl, struct, select, argparse


class Port:
    def __init__(self, path):
        self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        a = termios.tcgetattr(self.fd)
        a[0] = a[1] = a[3] = 0
        a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        a[4] = a[5] = termios.B115200
        a[6][termios.VMIN] = 0
        a[6][termios.VTIME] = 0
        termios.tcsetattr(self.fd, termios.TCSANOW, a)
        fcntl.ioctl(self.fd, 0x8004746d, struct.pack('I', 0x002 | 0x004))
        self.buf = b''
        time.sleep(0.3)
        self._drain(0.3)

    def _drain(self, t):
        end = time.time() + t
        while time.time() < end:
            r, _, _ = select.select([self.fd], [], [], 0.05)
            if r:
                try:
                    self.buf += os.read(self.fd, 65536)
                except OSError:
                    pass

    def ask(self, cmd, until=None, wait=2.0):
        """Команда → рядки відповіді. until — рядок-кінець (регулярний вираз)."""
        self.buf = b''
        os.write(self.fd, (cmd + "\n").encode())
        end = time.time() + wait
        while time.time() < end:
            r, _, _ = select.select([self.fd], [], [], 0.05)
            if r:
                try:
                    self.buf += os.read(self.fd, 65536)
                except OSError:
                    pass
                if until and re.search(until, self.buf.decode('utf-8', 'replace')):
                    self._drain(0.05)
                    break
        return self.buf.decode('utf-8', 'replace').splitlines()

    def close(self):
        os.close(self.fd)


OK, WARN, FAIL = "ОК", "УВАГА", "ПОМИЛКА"
report = []


def mark(level, what, detail):
    report.append({"рівень": level, "що": what, "деталі": detail})
    colour = {OK: "\033[32m", WARN: "\033[33m", FAIL: "\033[31m"}[level]
    print(f"  {colour}{level:8}\033[0m {what:14} {detail}")


def grab(lines, pattern, cast=int):
    for l in lines:
        m = re.search(pattern, l)
        if m:
            return [cast(g) for g in m.groups()]
    return None


def main():
    ap = argparse.ArgumentParser(description="Самоперевірка ПОТУЖНОГО РАДІО")
    ap.add_argument("--no-sound", action="store_true")
    ap.add_argument("--stop", action="store_true")
    ap.add_argument("--level", type=int, default=-20)
    ap.add_argument("--json")
    ap.add_argument("--eq", action="store_true", help="перевірити еквалайзер: замір через обробку проти розрахунку")
    ap.add_argument("--eq-only", action="store_true", help="лише перевірка еквалайзера, без загального заміру")
    ap.add_argument("-v", action="store_true", help="показувати сирі відповіді")
    o = ap.parse_args()

    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    path = os.environ.get("PORT") or (ports[0] if ports else None)
    if not path:
        print("радіо не під'єднане до USB")
        return 2
    p = Port(path)
    result = {"порт": path, "час": time.strftime("%Y-%m-%d %H:%M:%S")}
    print(f"ПОТУЖНЕ РАДІО — самоперевірка ({path})\n")

    def show(lines):
        if o.v:
            for l in lines:
                print("      │ " + l)

    # --- стан ---
    info = p.ask("info", until=r"heap=\d+")
    show(info)
    st = grab(info, r"станція=(\d+) гучність=(\d+) грає=(\d)")
    playing = bool(st and st[2])
    if st:
        mark(OK, "плеєр", f"станція {st[0]}, гучність {st[1]}, {'грає' if playing else 'стоїть'}")
    else:
        mark(FAIL, "консоль", "радіо не відповідає на команди (прошивка без YO_DEBUG?)")
        return 1
    result["плеєр"] = {"станція": st[0], "гучність": st[1], "грає": playing}

    # --- пам'ять ---
    h = grab(p.ask("heap", until=r"HEAP .*psblk=\d+"), r"internal=(\d+) blk=(\d+) min=(\d+) psram=(\d+) psblk=(\d+)")
    if h:
        result["пам'ять"] = dict(zip(["внутрішня", "шматок", "мінімум", "psram", "psram_шматок"], h))
        lvl = OK if h[0] >= 60000 and h[1] >= 24000 else WARN if h[1] >= 12000 else FAIL
        mark(lvl, "пам'ять", f"вільно {h[0]//1024} КБ, найбільший шматок {h[1]//1024} КБ, мінімум за роботу {h[2]//1024} КБ; PSRAM {h[3]//1024} КБ")

    # --- цикл і екран: скинути лічильники, почекати, прочитати ---
    p.ask("page", until=r"мережа: статус", wait=2)
    time.sleep(3)
    pg = p.ask("page", until=r"мережа: статус", wait=2)
    show(pg)
    loop = grab(pg, r"цикл: \d+ обертів за \d+ с \((\d+)/с\), найдовший проміжок (\d+) мс")
    dsp = grab(pg, r"екран: \d+ кадрів за \d+ с \((\d+)/с\), найдовший проміжок (\d+) мс")
    slow = next((l for l in pg if l.startswith("найдовший крок")), None)
    if loop:
        result["цикл"] = {"за_секунду": loop[0], "найдовший_мс": loop[1]}
        # поки грає, цикл іде в такт кадрам декодера (~38/с для MP3 44,1 кГц)
        mark(OK if loop[0] >= 30 and loop[1] < 300 else WARN, "цикл", f"{loop[0]}/с, найдовший проміжок {loop[1]} мс" + (f" ({slow})" if slow else ""))
    if dsp:
        result["екран"] = {"кадрів": dsp[0], "найдовший_мс": dsp[1]}
        mark(OK if dsp[1] < 500 else WARN, "екран", f"{dsp[0]} кадрів/с, найдовший проміжок {dsp[1]} мс")
    net = grab(pg, r"мережа: статус=(\d+) втрачено=(\d)")
    ssid = grab(pg, r"ssid='([^']*)'", str)
    ip = grab(info, r"ip=([\d.]+)", str)
    if net:
        connected = net[0] == 0 and ip and ip[0] != "0.0.0.0"
        result["мережа"] = {"статус": net[0], "ip": ip[0] if ip else None, "ssid": ssid[0] if ssid else ""}
        mark(OK if connected else WARN, "мережа", f"{ssid[0] if ssid else '-'}, {ip[0] if ip else '-'}" if connected else f"немає зв'язку (статус {net[0]})")

    # --- кодек ---
    es = p.ask("es", until=r"вивід підсилювача", wait=2)
    show(es)
    amp = grab(es, r"вивід підсилювача GPIO\d+ = (\S+)", str)
    esok = any("ES8311" in l for l in es)
    ampTxt = ("підсилювач " + ("увімкнено" if amp[0] == "низький" else "вимкнено")) if amp else "стан підсилювача невідомий"
    mark(OK if esok else FAIL, "кодек", f"ES8311 відповідає, {ampTxt}" if esok else "ES8311 не відповідає")

    # --- мікрофон ---
    m = p.ask("mic", until=r"мікрофон: слухає", wait=2)
    mm = grab(m, r"слухає=(\d) рівень=(-?\d+) дБ фон=(-?\d+) дБ .* слоти L=(-?\d+) R=(-?\d+)")
    micWasOff = False
    if mm and not mm[0]:
        micWasOff = True
        p.ask("micon 1", until=r"мікрофон", wait=1)
        time.sleep(1.5)
        mm = grab(p.ask("mic", until=r"мікрофон: слухає", wait=2), r"слухає=(\d) рівень=(-?\d+) дБ фон=(-?\d+) дБ .* слоти L=(-?\d+) R=(-?\d+)")
    if mm:
        result["мікрофон"] = {"слухає": mm[0], "рівень": mm[1], "фон": mm[2], "L": mm[3], "R": mm[4]}
        alive = mm[1] > -85 and mm[3] > -85
        mark(OK if alive else FAIL, "мікрофон", f"рівень {mm[1]} дБ, фон {mm[2]} дБ, слоти L={mm[3]} R={mm[4]}" if alive else "тиша в обох слотах — мікрофон не працює")

    # --- звук: тони через динамік → мікрофон ---
    if not o.no_sound:
        resume = None
        if playing:
            if not o.stop:
                mark(WARN, "звук", "грає станція — тони не пускаю (додайте --stop)")
                playing = None
            else:
                resume = st[0]
                p.ask("stop", wait=1.5)
        if playing is not None and o.eq_only:
            eq_check(p, o, [], result)
        elif playing is not None:
            state, rows = sweep(p, f"mictest {o.level}", o, 25)
            if state is not None and state[0] == 2:
                analyse(rows, state, result)
                # та сама перевірка з вимкненим підсилювачем: що мікрофон «чує» тоді —
                # це наводка всередині кодека, а не звук із динаміка
                st2, rows2 = sweep(p, f"mictest {o.level} noamp quick", o, 5)
                if st2 is not None and st2[0] == 2:
                    crosstalk(rows, rows2, result)
                if o.eq:
                    eq_check(p, o, rows, result)
        if resume is not None:
            p.ask(f"play {resume}", wait=1)
    if micWasOff:
        p.ask("micon 0", wait=1)

    p.close()
    fails = sum(1 for x in report if x["рівень"] == FAIL)
    warns = sum(1 for x in report if x["рівень"] == WARN)
    print(f"\nпідсумок: {len(report)} перевірок, помилок {fails}, увага {warns}")
    result["звіт"] = report
    if o.json:
        with open(o.json, "w") as f:
            json.dump(result, f, ensure_ascii=False, indent=1)
        print(f"результат: {o.json}")
    return 1 if fails else 0


def sweep(p, cmd, o, npts):
    """Запустити замір і дочекатися. → (state, рядки SW) або (None, [])."""
    for _ in range(40):                     # попередній замір ще доігрує — чекаємо
        rr = p.ask("micres", until=r"SWEEP end", wait=3)
        if not any("SWEEP state=1" in l for l in rr): break
        time.sleep(1)
    r = p.ask(cmd, until=r"самоперевірка звуку", wait=2)
    if o.v:
        for l in r:
            print("      │ " + l)
    bad = next((l for l in r if "не почато" in l), None)
    if bad or not any("самоперевірка звуку" in l for l in r):
        mark(FAIL, "звук", bad or "радіо не почало замір")
        return None, []
    print(f"  …радіо грає тони ({'підсилювач вимкнено, ' if 'noamp' in cmd else ''}близько {npts * 0.75 + 1:.0f} с)")
    t0 = time.time()
    state = None
    while time.time() - t0 < 60:
        time.sleep(2)
        rr = p.ask("micres", until=r"SWEEP end", wait=3)
        s = grab(rr, r"SWEEP state=(\d) pos=(\d+) of=(\d+) rate=(\d+) level=(-?\d+)")
        if not s:
            continue
        state = s
        if s[0] >= 2:
            print("\r" + " " * 30 + "\r", end="")
            return s, [l for l in rr if l.startswith("SW ")]
        sys.stdout.write(f"\r  …{s[1]}/{s[2]}")
        sys.stdout.flush()
    print()
    mark(FAIL, "звук", "замір перервано" if state and state[0] == 3 else "замір не закінчився за хвилину")
    return state, []


def eq_read(p):
    """Поточні налаштування звуку — щоб повернути їх після перевірки."""
    r = p.ask("eq", until=r"EQB 16000", wait=6)
    h = grab(r, r"EQ on=(\d+) preset=(\d+)\(.*\) loud=(\d+) guard=(\d+) vbass=(\d+) room=(\d+)")
    bands = [int(l.split()[2]) for l in r if l.startswith("EQB ")]
    return h, bands


def eq_restore(p, h, bands):
    """Повернути налаштування звуку людини — і перевірити: команда в порту
    може загубитись, а чужий еквалайзер лишати не можна."""
    for attempt in range(4):
        for i, b in enumerate(bands):
            p.ask(f"eqb {i} {b}", until=r"смуга", wait=1.5)
        p.ask(f"eqo {h[0]} {h[2]} {h[3]} {h[4]} {h[5]}", until=r"звук:", wait=1.5)
        if h[1]:
            p.ask(f"eqp {h[1]}", until=r"пресет", wait=1.5)
        h2, b2 = eq_read(p)
        if h2 and list(h2) == list(h) and b2 == bands:
            return True
    print("  УВАГА: налаштування звуку не вдалося повернути точно — перевірте еквалайзер")
    return False


def eq_check(p, o, raw_rows, result):
    """Грає тони через увесь ланцюг обробки з відомим пресетом і порівнює, що
    почув мікрофон, із розрахунковою АЧХ. Сирий замір — опора: так корпус,
    динамік і мікрофон віднімаються, лишається сама обробка."""
    h, bands = eq_read(p)
    if not h:
        mark(FAIL, "еквалайзер", "радіо не віддає налаштувань звуку")
        return
    # Опора — той самий ланцюг із рівними налаштуваннями: тон іде з тією ж
    # гучністю, тож нелінійність динаміка на низах (на -20 дБ він хрипить)
    # однакова в обох замірах і віднімається.
    p.ask("eqo 1 0 0 0 0", until=r"звук:", wait=1)
    p.ask("eqp 1", until=r"пресет", wait=1)
    st0, rows0 = sweep(p, f"mictest {o.level} dsp", o, 25)
    if not st0 or st0[0] != 2:
        eq_restore(p, h, bands)
        return
    raw = {x["гц"]: x for x in points(rows0)}
    tests = [("бас, захист динаміка", "eqo 1 0 1 0 0", "eqp 4"), ("голос", "eqo 1 0 0 0 0", "eqp 2"), ("яскраво, сильний захист", "eqo 1 0 2 0 0", "eqp 6")]
    out = []
    try:
        for name, opt, pre in tests:
            p.ask(opt, until=r"звук:", wait=1)
            p.ask(pre, until=r"пресет", wait=1)
            resp = {int(l.split()[1]): float(l.split()[2]) for l in p.ask("eqresp", until=r"RESP end", wait=3) if l.startswith("RESP ") and l.split()[1] != "end"}
            st, rows = sweep(p, f"mictest {o.level} dsp", o, 25)
            if not st or st[0] != 2:
                return
            meas = {x["гц"]: x for x in points(rows)}
            diffs = []
            for f, m in meas.items():
                if f not in raw or f not in resp:
                    continue
                r0 = raw[f]
                # і сирий, і оброблений тон мають добре вибиватися з фону
                if r0["дб"] - r0["фон"] < 15 or m["дб"] - m["фон"] < 15:
                    continue
                diffs.append((f, (m["дб"] - r0["дб"]) - resp[f]))
            if len(diffs) < 5:
                mark(WARN, "еквалайзер", f"{name}: замало чистих точок")
                continue
            mid = [d for f, d in diffs if 400 <= f <= 5000]
            off = sorted(mid)[len(mid) // 2] if mid else 0      # гучність і попереднє ослаблення — одна стала
            dev = [(f, d - off) for f, d in diffs]
            worst = max(dev, key=lambda t: abs(t[1]))
            mad = sorted(abs(d) for _, d in dev)[len(dev) // 2]
            out.append({"тест": name, "стала": round(off, 1), "медіана_відхилення": round(mad, 2),
                        "точки": [{"гц": f, "відхилення": round(d, 1)} for f, d in dev]})
            print(f"\n  {name}: розрахунок проти заміру (стала {off:+.1f} дБ)")
            print("     Гц  розрахунок  почуто  різниця")
            for f, d in dev:
                print(f"  {f:6}   {resp[f]:+6.1f}   {resp[f] + d:+6.1f}   {d:+5.1f}")
            lvl = OK if mad <= 1.5 and abs(worst[1]) <= 4 else WARN if mad <= 3 else FAIL
            mark(lvl, "еквалайзер", f"{name}: звучить як розраховано — медіана відхилення {mad:.1f} дБ, найбільше {worst[1]:+.1f} дБ на {worst[0]} Гц")
    finally:
        eq_restore(p, h, bands)
        result["еквалайзер"] = out


def points(rows):
    pts = []
    for l in rows:
        f, db, nz, h2, h3 = l.split()[1:6]
        if db == "nan":
            continue
        pts.append({"гц": int(f), "дб": float(db), "фон": float(nz),
                    "h2": None if h2 == "nan" else float(h2), "h3": None if h3 == "nan" else float(h3)})
    return pts


def crosstalk(rows, rows2, result):
    on = {x["гц"]: x["дб"] for x in points(rows)}
    off = points(rows2)
    diffs = [(x["гц"], on[x["гц"]] - x["дб"]) for x in off if x["гц"] in on]
    if not diffs:
        return
    result["наводка"] = [{"гц": f, "звук_над_наводкою": round(d, 1)} for f, d in diffs]
    worst = min(diffs, key=lambda t: t[1])
    txt = ", ".join(f"{f} Гц {d:+.0f}" for f, d in diffs)
    lvl = OK if worst[1] >= 20 else WARN
    mark(lvl, "чесність", f"з підсилювачем голосніше, ніж без нього, на: {txt} дБ" +
         ("" if worst[1] >= 20 else f" — на {worst[0]} Гц замір забиває наводка"))


def analyse(rows, state, result):
    pts = points(rows)
    result["звук"] = {"частота_тактів": state[3], "рівень_тону": state[4], "точки": pts}
    if not pts:
        mark(FAIL, "звук", "жодної точки")
        return
    # опора — середнє 500 Гц…2 кГц (там динамік і мікрофон точно чують)
    mid = [x["дб"] for x in pts if 500 <= x["гц"] <= 2000]
    ref = sum(mid) / len(mid) if mid else max(x["дб"] for x in pts)
    print(f"\n  АЧХ тракту (тон {state[4]} дБ, такти {state[3]} Гц; 0 — середнє 500 Гц…2 кГц = {ref:.1f} дБ повної шкали)")
    print("     Гц    дБ   над фоном  спотворення")
    for x in pts:
        rel = x["дб"] - ref
        snr = x["дб"] - x["фон"]
        bar_len = int(round(max(0, rel + 30) / 1.5))
        bar = "█" * bar_len
        thd = max([v for v in (x["h2"], x["h3"]) if v is not None], default=None)
        thd_s = f"{thd:6.1f}" if thd is not None else "     -"
        flag = "  ← шум" if snr < 10 else ("  ← хрипить" if thd is not None and thd > -15 else "")
        print(f"  {x['гц']:6} {rel:+6.1f}  {snr:6.1f}     {thd_s}  {bar}{flag}")
    usable = [x for x in pts if x["дб"] - ref > -10 and x["дб"] - x["фон"] >= 10]
    if usable:
        lo, hi = usable[0]["гц"], usable[-1]["гц"]
        result["звук"]["смуга"] = [lo, hi]
        quiet = sum(1 for x in pts if x["дб"] - x["фон"] < 10)
        lvl = OK if quiet <= 6 else WARN
        mark(lvl, "звук", f"тракт працює: смуга −10 дБ ≈ {lo}…{hi} Гц; точок у шумі {quiet} з {len(pts)}")
    else:
        mark(FAIL, "звук", "тонів у мікрофоні не чутно — динамік чи підсилювач мовчать")


if __name__ == "__main__":
    sys.exit(main())
