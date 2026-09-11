#!/bin/bash
# Перемикання радіо <-> картка кнопкою в шапці: на радіо має бути погода й значок
# картки, на картці — пульт і значок радіо. Потім повертає станцію, що грала.
cd "$(dirname "$0")"
info(){ W=3 python3 cmd.py "info" 2>&1 | grep -oE "режим=[A-Z]+ станція=[0-9]+ гучність=[0-9]+ грає=[01]"; }
waitmode(){ for i in $(seq 1 40); do info | grep -q "режим=$1" && return 0; sleep 2; done; echo "  !! режим $1 не настав"; return 1; }
for i in $(seq 1 10); do B=$(info); [ -n "$B" ] && break; sleep 2; done
echo "=== до перевірки: $B"
ST=$(echo "$B" | sed -nE 's/.*станція=([0-9]+).*/\1/p'); [ -z "$ST" -o "$ST" = "0" ] && ST=1
echo "$B" | grep -q "режим=SD" && { W=3 python3 cmd.py "stap 230 19" >/dev/null 2>&1; waitmode WEB; W=6 python3 cmd.py "play $ST" >/dev/null 2>&1; }
sleep 3; python3 screenshot.py scr sw_1_radio.png 2>&1 | tail -1
echo "=== -> картка"; W=3 python3 cmd.py "stap 230 19" >/dev/null 2>&1; waitmode SD; sleep 4
python3 screenshot.py scr sw_2_sd.png 2>&1 | tail -1
echo "=== -> радіо"; W=3 python3 cmd.py "stap 230 19" >/dev/null 2>&1; waitmode WEB; sleep 4
python3 screenshot.py scr sw_3_radio.png 2>&1 | tail -1
W=6 python3 cmd.py "play $ST" >/dev/null 2>&1; echo "=== повернуто: $(info)"
