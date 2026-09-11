#!/bin/bash
# Перевірка збірки 4: фавікон і кеш, таймер списку, кнопка джерела, плейлист
# картки без службових файлів, M4A без повзунка, перемотка FLAC, краї плейлиста.
cd "$(dirname "$0")"
say(){ echo; echo "=== $*"; }
info(){ W=3 python3 cmd.py "info" 2>&1 | grep -oE "режим=[A-Z]+ станція=[0-9]+ гучність=[0-9]+ грає=[01]"; }
waitmode(){ for i in $(seq 1 40); do info | grep -q "режим=$1" && return 0; sleep 2; done; echo "  !! режим $1 не настав"; return 1; }
for i in $(seq 1 10); do BEFORE=$(info); [ -n "$BEFORE" ] && break; sleep 2; done   # одразу після старту info може не відповісти
say "до перевірки: $BEFORE"
ST=$(echo "$BEFORE" | sed -nE 's/.*станція=([0-9]+).*/\1/p'); [ -z "$ST" -o "$ST" = "0" ] && ST=1   # ніколи не шлемо «play» без номера

say "фавікон і кеш"
curl -s -o /dev/null -D - --max-time 8 http://192.168.1.32/favicon.ico | grep -iE "^HTTP|content-type|content-length|cache-control" | tr -d '\r' | sed 's/^/  /'
curl -s -o /dev/null -D - --max-time 8 http://192.168.1.32/ | grep -i "cache-control" | tr -d '\r' | sed 's/^/  головна: /'

say "список не закривається сам (7 натискань за ~35 с)"
W=3 python3 cmd.py "list" >/dev/null 2>&1; sleep 1
for i in 1 2 3 4 5 6 7; do
  [ $((i % 2)) -eq 1 ] && B="289 90" || B="289 32"
  W=2 python3 cmd.py "stap $B" >/dev/null 2>&1; sleep 3
  echo "  крок $i: $(W=2 python3 cmd.py "plstate" 2>&1 | grep -oE "режим=[0-9]+ станція=[0-9]+")"
done
W=3 python3 cmd.py "stap 289 206" >/dev/null 2>&1; sleep 1

say "кнопка джерела в шапці -> картка"
W=3 python3 cmd.py "stap 230 19" >/dev/null 2>&1; waitmode SD && info
say "перебудова плейлиста картки"; W=20 python3 cmd.py "sdindex" 2>&1 | grep -E "sdindex"
say "M4A (трек 1): смуга без повзунка"; W=6 python3 cmd.py "play 1" >/dev/null 2>&1; sleep 3
python3 screenshot.py scr b4_m4a.png 2>&1 | tail -1
say "FLAC (трек 2): перемотка на середину"; W=6 python3 cmd.py "play 2" >/dev/null 2>&1; sleep 4
W=3 python3 cmd.py "stap 190 108" >/dev/null 2>&1; sleep 3
python3 screenshot.py scr b4_flac_seek.png 2>&1 | tail -1
say "краї: ⏭ з треку 4 і ⏮ з треку 1 не мають перескакувати"
W=6 python3 cmd.py "play 4" >/dev/null 2>&1; sleep 2; W=4 python3 cmd.py "stap 296 103" >/dev/null 2>&1; sleep 2; echo "  після ⏭ на 4-му: $(info)"
W=6 python3 cmd.py "play 1" >/dev/null 2>&1; sleep 2; W=4 python3 cmd.py "stap 84 103"  >/dev/null 2>&1; sleep 2; echo "  після ⏮ на 1-му: $(info)"
W=4 python3 cmd.py "stap 296 103" >/dev/null 2>&1; sleep 2; echo "  ⏭ з 1-го: $(info)"
say "кнопка джерела -> радіо, станція $ST"
W=3 python3 cmd.py "stap 230 19" >/dev/null 2>&1; waitmode WEB
W=6 python3 cmd.py "play $ST" >/dev/null 2>&1; echo "  $(info)"
