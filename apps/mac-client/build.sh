#!/bin/bash
# =============================================================================
#  build.sh — збирає клієнт «ПОТУЖНЕ РАДІО» для Mac у готовий .app
#  Потрібні лише Command Line Tools (swiftc). Результат: ../../Програми/ПОТУЖНЕ РАДІО.app
# =============================================================================
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(cd ../.. && pwd)"
APP_NAME="ПОТУЖНЕ РАДІО"
BUNDLE_ID="ua.potuzhne.macos.radio"
VERSION="$(tr -d ' \n\r' < "$ROOT/firmware/VERSION" 2>/dev/null || echo 1.0)"
MIN_MACOS="13.0"
OUT="${OUT:-$ROOT/Програми}"             # OUT=<тека> — зібрати деінде (перевірки)
APP="$OUT/$APP_NAME.app"
OBJ="$(mktemp -d)"
trap 'rm -rf "$OBJ"' EXIT

command -v swiftc >/dev/null || { echo "swiftc не знайдено: xcode-select --install"; exit 1; }
rm -rf "$APP"; mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources/uk.lproj"

SOURCES=(Sources/*.swift ../shared/Discovery.swift ../shared/Style.swift)
echo "▶ компіляція (${#SOURCES[@]} файлів)"
swiftc -O -swift-version 5 -target "arm64-apple-macos$MIN_MACOS" "${SOURCES[@]}" -o "$OBJ/arm64" 2>&1 | grep -v "warning:" | grep -E "error" && exit 1 || true
swiftc -O -swift-version 5 -target "x86_64-apple-macos$MIN_MACOS" "${SOURCES[@]}" -o "$OBJ/x86_64" 2>&1 | grep -E "error" && exit 1 || true
[ -f "$OBJ/arm64" ] && [ -f "$OBJ/x86_64" ] || { echo "✗ компіляція не вдалася"; exit 1; }
lipo -create "$OBJ/arm64" "$OBJ/x86_64" -output "$APP/Contents/MacOS/radio"

echo "▶ значок"
swiftc -O ../shared/make-icon.swift -o "$OBJ/make-icon" 2>/dev/null
"$OBJ/make-icon" "$OBJ/AppIcon.iconset" >/dev/null
iconutil -c icns "$OBJ/AppIcon.iconset" -o "$APP/Contents/Resources/AppIcon.icns"

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleName</key><string>$APP_NAME</string>
  <key>CFBundleDisplayName</key><string>$APP_NAME</string>
  <key>CFBundleIdentifier</key><string>$BUNDLE_ID</string>
  <key>CFBundleExecutable</key><string>radio</string>
  <key>CFBundleIconFile</key><string>AppIcon</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>$VERSION</string>
  <key>CFBundleVersion</key><string>$VERSION</string>
  <key>CFBundleDevelopmentRegion</key><string>uk</string>
  <key>CFBundleLocalizations</key><array><string>uk</string></array>
  <key>LSMinimumSystemVersion</key><string>$MIN_MACOS</string>
  <key>LSApplicationCategoryType</key><string>public.app-category.music</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>NSLocalNetworkUsageDescription</key><string>Щоб знайти ПОТУЖНЕ РАДІО у вашій мережі Wi-Fi і підключитися до нього.</string>
  <key>NSMicrophoneUsageDescription</key><string>Щоб слухати голосові команди для радіо: «наступна», «гучність 30», «грай Хіт FM».</string>
  <key>NSSpeechRecognitionUsageDescription</key><string>Щоб перетворити голосову команду на текст і передати її радіо.</string>
  <key>NSBonjourServices</key><array><string>_potuzhne._tcp</string><string>_http._tcp</string></array>
  <key>NSAppTransportSecurity</key><dict><key>NSAllowsArbitraryLoads</key><true/><key>NSAllowsLocalNetworking</key><true/></dict>
</dict></plist>
PLIST
#  Підпис. Є власний сертифікат у зв'язці ключів («Slovo», зроблений для «Слова») —
#  підписуємо ним: підпис однаковий від збірки до збірки, і дозвіл «Локальна
#  мережа», даний раз, macOS не питає знову. Немає — локальний (ad hoc).
if security find-identity -v -p codesigning 2>/dev/null | grep -q '"Slovo"'; then
  echo "▶ підпис (сертифікат Slovo)"; codesign --force --deep -s "Slovo" "$APP"
else
  echo "▶ підпис (локальний)"; codesign --force --deep -s - "$APP" 2>/dev/null
fi
echo "✓ готово: $APP"
