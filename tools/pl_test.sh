#!/bin/bash
# Перевірка сторінки плейлиста на пристрої після прошивки.
# Кнопки праворуч: ▲ y≈32, ▼ y≈90, ▶ y≈148, ↶ y≈206 (x≈289). Рядки по 32 px від y=8.
# Знімки — лише наприкінці: знімок триває 25–40 с, а список сам повертається на
# плеєр через 30 с, тож натискання після знімка потрапляли б уже на плеєр.
cd "$(dirname "$0")"
st(){ W=2 python3 cmd.py "plstate" 2>&1 | grep -oE "станція=[0-9]+"; }
echo "=== відкриваю список";  W=3 python3 cmd.py "list" >/dev/null 2>&1; sleep 1; echo "  $(st)"
echo "=== швидкість прокрутки"; W=6 python3 cmd.py "sfps" 2>&1 | grep -E "плавна|з них"
W=3 python3 cmd.py "list" >/dev/null 2>&1; sleep 1; A=$(st); echo "  знову відкрито: $A"
W=2 python3 cmd.py "stap 289 90" >/dev/null 2>&1; sleep 0.5; echo "=== ▼        -> $(st)   (очікую +1)"
W=2 python3 cmd.py "stap 289 32" >/dev/null 2>&1; sleep 0.5; echo "=== ▲        -> $(st)   (очікую -1)"
W=2 python3 cmd.py "stap 289 32" >/dev/null 2>&1; sleep 0.5; echo "=== ▲        -> $(st)   (очікую -1)"
W=2 python3 cmd.py "stap 100 24" >/dev/null 2>&1; sleep 0.5; echo "=== тап ряд0 -> $(st)   (очікую -3)"
echo "=== знімок списку"; python3 screenshot.py scr pl_final.png 2>&1 | tail -1
echo "=== ↶ назад на плеєр";  W=3 python3 cmd.py "list" >/dev/null 2>&1; sleep 1
W=3 python3 cmd.py "stap 289 206" "info" 2>&1 | grep -E "режим"
