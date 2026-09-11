#!/bin/bash
# =============================================================================
#  build.sh — збирає «Збірка ПОТУЖНОГО РАДІО» для Windows разом із поставкою
# =============================================================================
#  Результат: ../../Програми/Збірка ПОТУЖНОГО РАДІО (Windows)/
#     Збірка ПОТУЖНОГО РАДІО.exe      — програма (один самодостатній файл)
#     Проєкт/source/yoRadio/          — скетч (тека мусить зватися як .ino)
#     Проєкт/build/sketchbook/libraries/ — бібліотеки
#     Проєкт/firmware/VERSION         — номер версії
#  Разом близько 11 МБ проєкту + сама програма. Інструменти збірки (arduino-cli,
#  ядро ESP32) програма ставить на Windows сама при першому запуску.
#
#  .NET SDK — той самий, що в клієнта (див. ../windows-client/build.sh).
#  Вихідний код і прошивка проєкту тут лише КОПІЮЮТЬСЯ, нічого не змінюється.
# =============================================================================
set -euo pipefail

cd "$(dirname "$0")"
HERE="$PWD"
ROOT="$(cd ../.. && pwd)"

APP_NAME="Збірка ПОТУЖНОГО РАДІО"
OUT="${POTUZHNE_BUILDER_OUT:-$ROOT/Програми/Збірка ПОТУЖНОГО РАДІО (Windows)}"
RID="win-x64"
TOOLCHAIN="${POTUZHNE_TOOLCHAIN:-$HOME/Documents/cloude_work/tools/nas-migrator-win/toolchain}"

RED='\033[0;31m'; GREEN='\033[0;32m'; BLUE='\033[0;34m'; NC='\033[0m'
step() { echo -e "${BLUE}▶ $1${NC}"; }
ok()   { echo -e "${GREEN}✓ $1${NC}"; }
fail() { echo -e "${RED}✗ $1${NC}"; exit 1; }

export DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1 DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1
# Пакети — спільна з клієнтом тека (WinForms для win-x64 уже там).
export NUGET_PACKAGES="$ROOT/apps/windows-client/.nuget"

FALLBACK=()
if [ -x "$TOOLCHAIN/dotnet/dotnet" ]; then
  export DOTNET_ROOT="$TOOLCHAIN/dotnet"
  export PATH="$DOTNET_ROOT:$PATH"
  [ -d "$TOOLCHAIN/nuget" ] && FALLBACK=(-p:RestoreAdditionalProjectFallbackFolders="$TOOLCHAIN/nuget")
else
  export PATH="$HOME/.dotnet:$PATH"
  command -v dotnet >/dev/null || fail "Не знайдено .NET SDK 10 (див. ../windows-client/build.sh)"
fi
echo "  dotnet $(dotnet --version)"

step "Іконка"
# Та сама антена, що в клієнта, плюс шестерня в куті — як у Mac-програми збірки.
if command -v swiftc >/dev/null; then
  TMPBIN="${TMPDIR:-/tmp}/potuzhne-make-ico-$$"
  swiftc -O ../windows-client/make-ico.swift -o "$TMPBIN" 2>/dev/null \
    && "$TMPBIN" Resources/AppIcon.ico - build >/dev/null \
    && ok "Resources/AppIcon.ico" \
    || echo "  Іконку зібрати не вдалося — беру готову"
  rm -f "$TMPBIN"
fi
[ -f Resources/AppIcon.ico ] || fail "Немає Resources/AppIcon.ico"

step "Збірка $RID (один самодостатній файл)"
PUB="$HERE/obj/publish-$RID"
rm -rf "$PUB"
dotnet publish PotuzhneRadioBuilder.csproj -c Release -r "$RID" --self-contained true \
  -p:PublishSingleFile=true \
  -p:IncludeNativeLibrariesForSelfExtract=true \
  -p:EnableCompressionInSingleFile=true \
  -p:DebugType=none \
  ${FALLBACK[@]+"${FALLBACK[@]}"} \
  -o "$PUB" \
  -v q --nologo || fail "Збірка не вдалася"
[ -f "$PUB/PotuzhneRadioBuilder.exe" ] || fail "PotuzhneRadioBuilder.exe не з'явився"
mkdir -p "$OUT"
cp "$PUB/PotuzhneRadioBuilder.exe" "$OUT/$APP_NAME.exe"
rm -rf "$PUB"
ok "$(du -h "$OUT/$APP_NAME.exe" | cut -f1 | tr -d ' ') — $APP_NAME.exe"

step "Проєкт поруч із програмою"
# Лише копіювання. Службові файли macOS («._*», .DS_Store) не беремо: на
# флешці FAT/exFAT «._main.cpp» компілятор узяв би за код.
# Скетч і бібліотеки — дзеркалом (--delete), а з firmware/ — лише VERSION:
# зібране вже на Windows (образи, web) повторна поставка не стирає.
P="$OUT/Проєкт"
mkdir -p "$P/source" "$P/build/sketchbook" "$P/firmware"
export COPYFILE_DISABLE=1
rsync -a --delete --exclude '.DS_Store' --exclude '._*' "$ROOT/source/yoRadio/" "$P/source/yoRadio/"
rsync -a --delete --exclude '.DS_Store' --exclude '._*' "$ROOT/build/sketchbook/libraries/" "$P/build/sketchbook/libraries/"
cp "$ROOT/firmware/VERSION" "$P/firmware/VERSION"
[ -f "$P/source/yoRadio/yoRadio.ino" ] || fail "У копії немає yoRadio.ino"
ok "$(du -sh "$P" | cut -f1 | tr -d ' ') — Проєкт (версія $(tr -d ' \n\r' < "$P/firmware/VERSION"))"

echo
ok "Готово: $OUT"
