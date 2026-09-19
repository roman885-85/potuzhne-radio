#!/bin/bash
# =============================================================================
#  build.sh — збирає Windows-клієнт «ПОТУЖНЕ РАДІО» в один .exe
# =============================================================================
#  Збирається на Mac, працює у Windows 10/11 (x64): .NET кладе всю потрібну
#  обслугу всередину файлу, тож на цільовому комп'ютері нічого ставити не
#  треба. Сторінку радіо показує WebView2 — він у Windows уже є.
#
#  Результат:  ../../Програми/ПОТУЖНЕ РАДІО.exe
#  Інше місце: POTUZHNE_WIN_OUT=/шлях/до/теки ./build.sh
#
#  .NET SDK 10 береться з уже наявного інструменту поруч, у проєкті
#  «Перенесення сайту» (1,5 ГБ — не копіюємо). Інше місце:
#  POTUZHNE_TOOLCHAIN=/шлях/до/toolchain ./build.sh
#  Бібліотеки, яких там немає (WinForms, WebView2), докачуються в .nuget/ поруч.
# =============================================================================
set -euo pipefail

cd "$(dirname "$0")"
HERE="$PWD"

APP_NAME="ПОТУЖНЕ РАДІО"
OUT="${POTUZHNE_WIN_OUT:-$(cd ../.. && pwd)/Програми}"
RID="win-x64"
TOOLCHAIN="${POTUZHNE_TOOLCHAIN:-$HOME/Documents/cloude_work/tools/nas-migrator-win/toolchain}"

RED='\033[0;31m'; GREEN='\033[0;32m'; BLUE='\033[0;34m'; NC='\033[0m'
step() { echo -e "${BLUE}▶ $1${NC}"; }
ok()   { echo -e "${GREEN}✓ $1${NC}"; }
fail() { echo -e "${RED}✗ $1${NC}"; exit 1; }

export DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1 DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1
# Свої пакети — у своїй теці; чужий кеш лише читаємо (див. нижче).
export NUGET_PACKAGES="$HERE/.nuget"

FALLBACK=()
if [ -x "$TOOLCHAIN/dotnet/dotnet" ]; then
  export DOTNET_ROOT="$TOOLCHAIN/dotnet"
  export PATH="$DOTNET_ROOT:$PATH"
  # Уже завантажені там пакети (зокрема середовище .NET для win-x64 на
  # десятки мегабайт) беремо звідти без копіювання — як резервну теку NuGet.
  [ -d "$TOOLCHAIN/nuget" ] && FALLBACK=(-p:RestoreAdditionalProjectFallbackFolders="$TOOLCHAIN/nuget")
else
  export PATH="$HOME/.dotnet:$PATH"
  command -v dotnet >/dev/null || fail "Не знайдено .NET SDK 10. Отримати його:
  curl -fsSL https://dot.net/v1/dotnet-install.sh -o /tmp/dotnet-install.sh
  bash /tmp/dotnet-install.sh --channel 10.0 --install-dir \"\$HOME/.dotnet\""
  echo "  toolchain не знайдено — беру dotnet із системи ($(command -v dotnet))"
fi
echo "  dotnet $(dotnet --version) з $(dirname "$(command -v dotnet)")"

step "Іконка програми"
# Малюється з геометрії значка антени. Без swiftc береться вже готова з Resources.
if command -v swiftc >/dev/null; then
  TMPBIN="${TMPDIR:-/tmp}/potuzhne-make-ico-$$"
  swiftc -O make-ico.swift -o "$TMPBIN" 2>/dev/null \
    && "$TMPBIN" Resources/AppIcon.ico >/dev/null \
    && ok "Resources/AppIcon.ico" \
    || echo "  Іконку зібрати не вдалося — беру готову з Resources"
  rm -f "$TMPBIN"
fi
[ -f Resources/AppIcon.ico ] || fail "Немає Resources/AppIcon.ico"

step "Збірка $RID (один самодостатній файл)"
PUB="$HERE/obj/publish-$RID"
rm -rf "$PUB"
# Номер версії — той самий, що в прошивки: за ним програма розуміє, чи є
# у випуску на GitHub щось свіжіше за неї (Sources/Net/Updater.cs).
VER=$(cat "$(cd ../.. && pwd)/firmware/VERSION" 2>/dev/null || echo 1.0.0)
dotnet publish PotuzhneRadio.csproj -c Release -r "$RID" --self-contained true \
  -p:Version="$VER" -p:FileVersion="$VER.0" -p:AssemblyVersion="$VER.0" \
  -p:PublishSingleFile=true \
  -p:IncludeNativeLibrariesForSelfExtract=true \
  -p:EnableCompressionInSingleFile=true \
  -p:DebugType=none \
  ${FALLBACK[@]+"${FALLBACK[@]}"} \
  -o "$PUB" \
  -v q --nologo || fail "Збірка не вдалася"

[ -f "$PUB/PotuzhneRadio.exe" ] || fail "PotuzhneRadio.exe не з'явився"
# Поруч не повинно лишитися нічого, без чого .exe не запуститься.
EXTRA=$(cd "$PUB" && ls | grep -v '^PotuzhneRadio.exe$' || true)
[ -n "$EXTRA" ] && echo "  Увага: поруч із .exe лишились файли: $EXTRA"

mkdir -p "$OUT"
cp "$PUB/PotuzhneRadio.exe" "$OUT/$APP_NAME.exe"
# Номер версії поруч: tools/release.sh пише в маніфест саме його, бо з самого
# .exe на Mac його не прочитати, а версія прошивки може вже піти вперед.
printf '%s' "$VER" > "$OUT/.windows-version"
rm -rf "$PUB"
ok "$(du -h "$OUT/$APP_NAME.exe" | cut -f1 | tr -d ' ') — $OUT/$APP_NAME.exe"
