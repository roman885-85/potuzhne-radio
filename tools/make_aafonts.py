#!/usr/bin/env python3
"""Згладжені шрифти для меню радіо → source/yoRadio/src/displays/fonts/aa/*.h

Та сама структура GFXfont, що й у звичайних шрифтів Adafruit GFX (метрики
гліфів однакові за змістом, тож розкладка меню й getTextBounds працюють без
змін), але бітмапи — 4 біти на піксель (16 рівнів прозорості). Малює їх
UiCanvas (menu/uicanvas.cpp), змішуючи з тим, що вже лежить у кадрі.

Коди символів — CP1251, як у utf8Rus(): 0x20..0xFF.
Розміри підібрано під висоту великих літер нинішніх шрифтів Verdana, щоб
розкладка сторінок не поїхала. Roboto — Apache 2.0; Oswald — OFL.

  scratchpad/imgenv/bin/python3 tools/make_aafonts.py
"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "source", "yoRadio", "src", "displays", "fonts", "aa")
FONTS = os.path.expanduser("~/Library/Fonts")

# ім'я, файл шрифту, висота великої літери «H» у пікселях (як у старого шрифту), рядок yAdvance
SET = [
    ("aaUI8",   "Roboto-Regular.ttf", 11, 19),
    ("aaUI9",   "Roboto-Regular.ttf", 12, 21),
    ("aaUI9b",  "Roboto-Bold.ttf",    12, 21),
    ("aaUI11",  "Roboto-Regular.ttf", 15, 26),
    ("aaUI12b", "Roboto-Bold.ttf",    16, 29),
    ("aaUI13",  "Roboto-Regular.ttf", 17, 31),
    ("aaUI14b", "Oswald-Regular.ttf", 18, 33),
    ("aaUI15",  "Roboto-Regular.ttf", 20, 36),
]
GAMMA = 0.85          # трохи густіші краї: дрібний світлий текст на темному інакше «худне»


def cap_size(path, cap_px):
    """Розмір шрифту (px), за якого «H» має задану висоту."""
    lo, hi = 4.0, 80.0
    for _ in range(30):
        mid = (lo + hi) / 2
        f = ImageFont.truetype(path, mid)
        l, t, r, b = f.getbbox("H", anchor="ls")
        if (b - t) < cap_px:
            lo = mid
        else:
            hi = mid
    return (lo + hi) / 2


def build(name, file, cap, yadv):
    path = os.path.join(FONTS, file)
    size = cap_size(path, cap)
    font = ImageFont.truetype(path, size)
    bitmaps = bytearray()
    glyphs = []
    for code in range(0x20, 0x100):
        try:
            ch = bytes([code]).decode("cp1251")
        except UnicodeDecodeError:
            ch = " "
        if code == 0x98:
            ch = " "
        adv = int(round(font.getlength(ch)))
        l, t, r, b = font.getbbox(ch, anchor="ls")
        w, h = max(0, r - l), max(0, b - t)
        off = len(bitmaps)
        if w and h and ch.strip():
            img = Image.new("L", (w, h), 0)
            ImageDraw.Draw(img).text((-l, -t), ch, font=font, fill=255, anchor="ls")
            px = img.load()
            nib = []
            for y in range(h):
                for x in range(w):
                    a = (px[x, y] / 255.0) ** GAMMA
                    nib.append(min(15, int(round(a * 15))))
            if len(nib) & 1:
                nib.append(0)
            for i in range(0, len(nib), 2):
                bitmaps.append((nib[i] << 4) | nib[i + 1])
        else:
            w = h = 0
        glyphs.append((off, min(255, w), min(255, h), max(0, min(255, adv)), max(-128, min(127, l)), max(-128, min(127, t))))
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, name + ".h"), "w") as f:
        f.write(f"/* Створює tools/make_aafonts.py — вручну не правити. {file}, «H» {cap} px (розмір {size:.2f}), 4 біти на піксель. */\n")
        f.write("#pragma once\n#include <Adafruit_GFX.h>\n\n")
        f.write(f"inline const uint8_t {name}Bitmaps[] PROGMEM = {{\n")
        for i in range(0, len(bitmaps), 20):
            f.write("  " + ", ".join(f"0x{v:02X}" for v in bitmaps[i:i + 20]) + ",\n")
        f.write("  0x00 };\n\n")
        f.write(f"inline const GFXglyph {name}Glyphs[] PROGMEM = {{\n")
        for i, g in enumerate(glyphs):
            f.write(f"  {{ {g[0]:6d}, {g[1]:3d}, {g[2]:3d}, {g[3]:3d}, {g[4]:4d}, {g[5]:4d} }},   // 0x{0x20 + i:02X}\n")
        f.write("};\n\n")
        f.write(f"inline const GFXfont {name} PROGMEM = {{ (uint8_t*){name}Bitmaps, (GFXglyph*){name}Glyphs, 0x20, 0xFF, {yadv} }};\n")
    print(f"{name}: {file} {size:.1f}px, бітмапи {len(bitmaps)} байт")


for args in SET:
    build(*args)
