#!/bin/bash
# Випуск нової версії на GitHub — звідти радіо оновлюються самі (extras/yoOta).
#
#   tools/release.sh <версія> <файл з описом>          — одразу для всіх (Latest)
#   tools/release.sh <версія> <файл з описом> --beta   — попередній випуск: бачать лише радіо,
#                                                        яким сказали перевірити бета (команда «ota beta»)
#   tools/release.sh --promote <версія>                — перевірений попередній випуск → Latest для всіх
#
# Перед цим: firmware/VERSION = <версія>, firmware/rebuild.sh зібрано.
# Перший рядок опису радіо показує в пропозиції оновитись — пишіть його для людини.
#
# Що робить:
#   1. перевіряє, що зібрана прошивка — саме цієї версії;
#   2. пушить main і створює чернетку випуску v<версія>;
#   3. завантажує файли (з повторами: GitHub інколи відповідає 502 і лишає чернетку):
#      PotuzhneRadio-ES3C28P-update.bin — за ним ідуть радіо;
#      PotuzhneRadio-ES3C28P-app.js.gz, -app.css.gz — сторінка радіо (теж для радіо);
#      PotuzhneRadio-ES3C28P-full.bin, PotuzhneRadio-web.zip — для прошивання вручну;
#      програми (Android, Windows, збирачі) — з попереднього випуску; Mac — з Програми/, якщо зібрана під цю версію;
#   4. публікує (Latest або попередній) і звіряє завантажений update.bin із зібраним.
set -e
REPO="roman885-85/potuzhne-radio"

if [ "$1" = "--promote" ]; then
  TAG="v$2"
  [ -n "$2" ] || { echo "використання: tools/release.sh --promote <версія>"; exit 1; }
  gh release edit "$TAG" -R "$REPO" --prerelease=false --latest
  LATEST="$(gh api "repos/$REPO/releases/latest" --jq .tag_name)"
  [ "$LATEST" = "$TAG" ] || { echo "releases/latest показує $LATEST, а не $TAG"; exit 1; }
  echo ">>> $TAG тепер Latest — радіо всіх побачать його при наступній перевірці"
  exit 0
fi

VER="$1"; NOTES="$2"; BETA=""
[ "$3" = "--beta" ] && BETA=1
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FW="$ROOT/firmware"
TAG="v$VER"
[ -n "$VER" ] && [ -f "$NOTES" ] || { echo "використання: tools/release.sh <версія> <файл з описом> [--beta]"; exit 1; }
[ "$(tr -d ' \n\r' < "$FW/VERSION")" = "$VER" ] || { echo "firmware/VERSION не $VER"; exit 1; }
grep -a -q "POTUZHNE-RADIO-FW|ES3C28P|$VER|" "$FW/PotuzhneRadio-ES3C28P-update.bin" || { echo "update.bin зібрано не для $VER — спершу firmware/rebuild.sh"; exit 1; }

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
cp "$FW/PotuzhneRadio-ES3C28P-update.bin" "$FW/PotuzhneRadio-ES3C28P-full.bin" "$TMP/"
cp "$FW/web/app.js.gz"  "$TMP/PotuzhneRadio-ES3C28P-app.js.gz"
cp "$FW/web/app.css.gz" "$TMP/PotuzhneRadio-ES3C28P-app.css.gz"
(cd "$FW/web" && zip -q -j "$TMP/PotuzhneRadio-web.zip" app.js.gz app.css.gz)

# програми — з попереднього Latest (не змінювались)
PREV="$(gh release view -R "$REPO" --json tagName --jq .tagName 2>/dev/null || true)"
for f in PotuzhneRadio-Android.apk PotuzhneRadio-Windows.exe PotuzhneRadio-Mac.zip PotuzhneRadio-Builder-Mac.zip PotuzhneRadio-Builder-Windows.zip; do
  if [ -n "$PREV" ] && [ "$PREV" != "$TAG" ]; then gh release download "$PREV" -R "$REPO" -p "$f" -D "$TMP" 2>/dev/null || true; fi
done
# ---- маніфест версій програм ----------------------------------------------
# Програми оновлюються самі: кожна дивиться у цей файл останнього випуску й
# порівнює зі своєю версією. Тут пишемо, ЯКОЇ версії бінарник реально лежить у
# випуску: якщо програму не перезбирали, вона переноситься з минулого разом зі
# своїм номером — і тоді оновлення нікому не пропонується даремно.
MAN="$TMP/PotuzhneRadio-clients.json"
if [ -n "$PREV" ] && [ "$PREV" != "$TAG" ]; then
  gh release download "$PREV" -R "$REPO" -p PotuzhneRadio-clients.json -D "$TMP" 2>/dev/null || true
fi
manget(){ [ -f "$MAN" ] && python3 -c "import json,sys;print(json.load(open(sys.argv[1])).get(sys.argv[2],''))" "$MAN" "$1" 2>/dev/null || true; }
M_AND="$(manget android)"; M_MAC="$(manget mac)"; M_WIN="$(manget windows)"

# свіжа програма з «Програми/» замінює перенесену, якщо файл інший — тоді це нова збірка
newer(){ # $1 локальний файл, $2 назва у випуску
  [ -f "$1" ] || return 1
  if [ -f "$TMP/$2" ] && cmp -s "$1" "$TMP/$2"; then return 1; fi
  cp "$1" "$TMP/$2"; return 0
}
if newer "$ROOT/Програми/ПОТУЖНЕ РАДІО.apk" PotuzhneRadio-Android.apk; then
  M_AND="$(sed -n 's/.*android:versionName="\([^"]*\)".*/\1/p' "$ROOT/apps/android-client/app/src/main/AndroidManifest.xml" | head -1)"
  echo ">>> програма для Android $M_AND — з Програми/"
fi
if newer "$ROOT/Програми/ПОТУЖНЕ РАДІО.exe" PotuzhneRadio-Windows.exe; then
  M_WIN="$VER"; echo ">>> програма для Windows $VER — з Програми/"
fi

# програма для Mac — свіжа з «Програми/», якщо її зібрано під цю версію (apps/mac-client/build.sh)
MACAPP="$ROOT/Програми/ПОТУЖНЕ РАДІО.app"
if [ -d "$MACAPP" ] && [ "$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' "$MACAPP/Contents/Info.plist" 2>/dev/null)" = "$VER" ]; then
  echo ">>> програма для Mac $VER — з Програми/"
  rm -f "$TMP/PotuzhneRadio-Mac.zip"
  ditto -c -k --sequesterRsrc --keepParent "$MACAPP" "$TMP/PotuzhneRadio-Mac.zip"
  M_MAC="$VER"
fi
python3 - "$MAN" "$M_AND" "$M_MAC" "$M_WIN" <<'PY'
import json,sys
out={}
for k,v in (("android",sys.argv[2]),("mac",sys.argv[3]),("windows",sys.argv[4])):
    if v: out[k]=v
json.dump(out, open(sys.argv[1],"w"), ensure_ascii=False, indent=1)
PY
echo ">>> версії програм: $(cat "$MAN" | tr -d '\n ')"

echo ">>> push main"
git -C "$ROOT" push origin main

if ! gh release view "$TAG" -R "$REPO" >/dev/null 2>&1; then
  echo ">>> чернетка $TAG"
  for i in 1 2 3; do
    gh release create "$TAG" -R "$REPO" --draft --target main --title "ПОТУЖНЕ РАДІО $VER" --notes-file "$NOTES" && break || sleep 5
    gh release view "$TAG" -R "$REPO" >/dev/null 2>&1 && break
  done
fi
echo ">>> файли"
for f in "$TMP"/*; do
  for i in 1 2 3 4; do gh release upload "$TAG" -R "$REPO" "$f" --clobber && break || sleep 5; done
done
if [ -n "$BETA" ]; then
  echo ">>> публікую як попередній"
  gh release edit "$TAG" -R "$REPO" --draft=false --prerelease --latest=false --notes-file "$NOTES"
else
  echo ">>> публікую"
  gh release edit "$TAG" -R "$REPO" --draft=false --prerelease=false --latest --notes-file "$NOTES"
  LATEST="$(gh api "repos/$REPO/releases/latest" --jq .tag_name)"
  [ "$LATEST" = "$TAG" ] || { echo "releases/latest показує $LATEST, а не $TAG"; exit 1; }
fi
gh release download "$TAG" -R "$REPO" -p PotuzhneRadio-ES3C28P-update.bin -D "$TMP/check"
cmp "$TMP/check/PotuzhneRadio-ES3C28P-update.bin" "$FW/PotuzhneRadio-ES3C28P-update.bin" && echo ">>> $TAG опубліковано${BETA:+ (попередній)}, прошивка на GitHub збігається із зібраною"
