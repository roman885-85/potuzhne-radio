#!/bin/bash
# Стиснути файли веб-сторінки й залити на плату (звук на час заливки зупиниться).
#   ./webdeploy.sh [адреса плати, типово 192.168.1.32]
set -e
HOST="${1:-192.168.1.32}"
SRC="$(cd "$(dirname "$0")/../source/yoRadio" && pwd)"
node --check "$SRC/web/app.js"
gzip -9 -c "$SRC/web/app.js"  > "$SRC/data/www/app.js.gz"
gzip -9 -c "$SRC/web/app.css" > "$SRC/data/www/app.css.gz"
mkdir -p "$SRC/../../firmware/web" && cp "$SRC"/data/www/app.*.gz "$SRC/../../firmware/web/"   # поруч із прошивкою — для оновлення через сторінку
cd "$SRC/data/www"
code=$(curl -s -m 60 -o /dev/null -w "%{http_code}" -F "www=@app.js.gz" -F "www=@app.css.gz" "http://$HOST/webboard")
echo "залито на $HOST: HTTP $code"
