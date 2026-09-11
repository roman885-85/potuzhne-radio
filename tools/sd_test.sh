#!/bin/bash
# Перевірка пульта картки: попередній / наступний трек і перемотка.
# Потім повертає режим радіо й станцію, що грала до перевірки.
cd "$(dirname "$0")"
say(){ echo; echo "=== $*"; }
BEFORE=$(W=3 python3 cmd.py "info" 2>&1 | grep -oE "режим=[A-Z]+ станція=[0-9]+")
say "до перевірки: $BEFORE"
MODE=$(echo "$BEFORE" | sed -E 's/режим=([A-Z]+).*/\1/'); ST=$(echo "$BEFORE" | sed -E 's/.*станція=([0-9]+)/\1/')
if [ "$MODE" = "WEB" ]; then W=3 python3 cmd.py "mode" >/dev/null 2>&1; fi
until W=3 python3 cmd.py "info" 2>&1 | grep -q "режим=SD.*грає=1"; do sleep 2; done; sleep 3
W=3 python3 cmd.py "info" 2>&1 | grep режим
python3 screenshot.py scr sd_1.png 2>&1 | tail -1
say "наступний трек";  W=5 python3 cmd.py "stap 296 103" "info" 2>&1 | grep -E "дотик|режим"
say "попередній трек"; W=5 python3 cmd.py "stap 84 103" "info" 2>&1 | grep -E "дотик|режим"
sleep 3
say "перемотка на середину смуги"; W=4 python3 cmd.py "stap 190 108" 2>&1 | grep дотик; sleep 2
python3 screenshot.py scr sd_2_seek.png 2>&1 | tail -1
say "повертаю як було: $MODE, станція $ST"
if [ "$MODE" = "WEB" ]; then W=3 python3 cmd.py "mode" >/dev/null 2>&1; until W=3 python3 cmd.py "info" 2>&1 | grep -q "режим=WEB"; do sleep 2; done; fi
W=6 python3 cmd.py "play $ST" "info" 2>&1 | grep режим
