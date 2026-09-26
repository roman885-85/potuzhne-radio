#!/usr/bin/env python3
"""Складає таблицю перекладу для прошивки з tools/lang/*.json.

Правити треба json, а не .h: тут рядки лише сортуються (побайтово, як
порівнює strcmp у прошивці) і перетворюються на масив C.

    python3 tools/mklang.py
"""
import json, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "tools", "lang", "en.json")
OUT = os.path.join(ROOT, "source", "yoRadio", "src", "m2", "m2lang_en.h")


#  Рядки в json записані так само, як у коді прошивки: \n там — це два знаки,
#  зворотна похила й «n», бо саме так їх бачить компілятор. Тому в .h вони
#  йдуть як є, а от сортувати треба за тим, що з них вийде після компіляції,
#  — інакше пошук навпіл шукатиме не те.
ESC = {"n": "\n", "t": "\t", "r": "\r", "0": "\0", "\\": "\\", '"': '"', "'": "'"}


def unescaped(s: str) -> str:
    out, i = [], 0
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s):
            out.append(ESC.get(s[i + 1], s[i + 1]))
            i += 2
        else:
            out.append(s[i])
            i += 1
    return "".join(out)


def main() -> int:
    with open(SRC, encoding="utf-8") as f:
        tab = json.load(f)

    bad = [k for k, v in tab.items() if not isinstance(v, str) or not v]
    if bad:
        print("порожній переклад:", bad[:5], file=sys.stderr)
        return 1
    #  Переноси рядків у json мають бути ДВОМА знаками (\ і n), як у коді
    #  прошивки. Справжній перенос розірве рядок C і збірка впаде на
    #  «missing terminating character» — перевіряємо тут, а не компілятором.
    raw = [k for k, v in tab.items() if "\n" in k or "\n" in v or "\r" in k or "\r" in v]
    if raw:
        print("справжній перенос рядка замість \\n:", raw[:3], file=sys.stderr)
        return 1
    for k, v in tab.items():
        if k.count("%") != v.count("%"):
            print(f"не збігаються підстановки:\n  {k!r}\n  {v!r}", file=sys.stderr)
            return 1

    rows = sorted(tab.items(), key=lambda kv: unescaped(kv[0]).encode("utf-8"))
    n = len(rows)
    size = sum(len(k.encode()) + len(v.encode()) + 2 for k, v in rows)

    with open(OUT, "w", encoding="utf-8") as f:
        f.write("/*  Складено автоматично: python3 tools/mklang.py\n")
        f.write("    Джерело — tools/lang/en.json. Руками не правити.  */\n")
        f.write("static const TrRow TR_EN[] = {\n")
        for k, v in rows:
            f.write(f'  {{ "{k}", "{v}" }},\n')
        f.write("};\n")

    print(f"{OUT}: {n} рядків, ~{size} байт")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
