#!/bin/bash
#
# Збірка Android-застосунку «ПОТУЖНЕ РАДІО» в один .apk
#
# Без Android Studio і без Gradle — так само, як «Панель сервера». Жодної
# сторонньої бібліотеки (ані androidx), тож потрібні лише інструменти
# Android SDK і Java: aapt2 → javac → d8 → zipalign → apksigner.
#
#   ./build.sh
#
# Результат — «Програми/ПОТУЖНЕ РАДІО.apk» у корені проєкту радіо.
# Працює на Android 5.0 і новіших (minSdk 21).

set -euo pipefail
cd "$(dirname "$0")"
HERE="$(pwd)"

SDK="${ANDROID_SDK:-$HOME/Library/Android/sdk}"
MIN_SDK=21
TARGET_SDK=36
WORK="Службові"
B="$WORK/build"
OUT_DIR="$(cd "$HERE/../.." && pwd)/Програми"
OUT="$OUT_DIR/ПОТУЖНЕ РАДІО.apk"
PKG_DIR="ua/potuzhne/radio"

say() { printf '\033[36m›\033[0m %s\n' "$1"; }
die() { printf '\033[31m✗ %s\033[0m\n' "$1" >&2; exit 1; }

# ---- номер версії -----------------------------------------------------------
# Той самий, що в прошивки: за ним застосунок розуміє, чи є у випуску на
# GitHub щось свіжіше за нього (Update.java). versionCode має лише зростати,
# тож складаємо його з тих самих трьох чисел.
MAN="app/src/main/AndroidManifest.xml"
FW="$(cd "$HERE/../.." && pwd)/firmware/VERSION"
if [ -f "$FW" ]; then
  V=$(tr -d '[:space:]' < "$FW")
  CODE=$(echo "$V" | awk -F. '{printf "%d", $1*10000 + $2*100 + $3}')
  if [ -n "$V" ] && [ "$CODE" -gt 0 ] 2>/dev/null; then
    sed -i '' -e "s/android:versionCode=\"[^\"]*\"/android:versionCode=\"$CODE\"/" \
              -e "s/android:versionName=\"[^\"]*\"/android:versionName=\"$V\"/" "$MAN"
    say "версія $V (код $CODE)"
  fi
fi

# ---- інструменти ------------------------------------------------------------

[ -d "$SDK" ] || die "Не знайдено Android SDK у $SDK. Задайте ANDROID_SDK=шлях"

# Найсвіжіші build-tools і платформа: прив'язка до конкретного номера
# ламала б збірку від кожного оновлення SDK. minSdk від цього не залежить —
# його задають прапорці нижче.
BT=$(ls -d "$SDK"/build-tools/*/ 2>/dev/null | sort -V | tail -1)
[ -n "$BT" ] || die "У SDK немає build-tools"
PLATFORM=$(ls -d "$SDK"/platforms/android-*/ 2>/dev/null | sort -V | tail -1)
[ -n "$PLATFORM" ] || die "У SDK немає жодної платформи (platforms/android-NN)"
JAR="$PLATFORM/android.jar"
[ -f "$JAR" ] || die "Немає $JAR"

if [ -z "${JAVA_HOME:-}" ]; then
  for candidate in /usr/local/opt/openjdk /opt/homebrew/opt/openjdk \
                   "$(/usr/libexec/java_home 2>/dev/null || true)"; do
    [ -x "${candidate:-}/bin/javac" ] && { JAVA_HOME="$candidate"; break; }
  done
fi
[ -x "${JAVA_HOME:-}/bin/javac" ] || die "Не знайдено Java. Поставте: brew install openjdk"
export JAVA_HOME
PATH="$JAVA_HOME/bin:$PATH"

say "SDK         $SDK"
say "build-tools $(basename "$BT")"
say "платформа   $(basename "$PLATFORM") (minSdk $MIN_SDK)"
say "Java        $(java -version 2>&1 | head -1)"

rm -rf "$B"
mkdir -p "$B/classes" "$B/gen" "$B/dex" "$B/icons"

# ---- ключ підпису -------------------------------------------------------------
#
# Свій ключ, не спільний з іншими застосунками. Створюється один раз; пароль —
# випадковий, лежить поруч у «Службові/пароль.txt» і в git не йде.
# Ключ втрачати не можна: зібране іншим ключем телефон вважає чужим
# застосунком і не ставить поверх — спершу доведеться видалити старе.

KEYSTORE="$WORK/ключ.jks"
PASSFILE="$WORK/пароль.txt"
ALIAS="potuzhne"
if [ ! -f "$KEYSTORE" ]; then
  say "Створюю ключ підпису (один раз)"
  # Без «tr < /dev/urandom | head»: при pipefail tr падає від SIGPIPE,
  # і set -e мовчки обриває збірку.
  (umask 077; openssl rand -hex 16 | tr -d '\n' > "$PASSFILE")
  keytool -genkeypair -keystore "$KEYSTORE" -storetype PKCS12 -alias "$ALIAS" \
    -keyalg RSA -keysize 2048 -validity 10000 \
    -storepass:file "$PASSFILE" -keypass:file "$PASSFILE" \
    -dname "CN=Potuzhne Radio, O=Potuzhne Radio, C=UA" >/dev/null 2>&1 \
    || die "keytool не створив ключ"
fi
[ -f "$PASSFILE" ] || die "Є ключ, але немає $PASSFILE — без пароля підписати не вийде"

# ---- значок -------------------------------------------------------------------
#
# PNG для старих Android малюються щоразу з опису антени (scripts/MakeIcons.java),
# а не лежать готовими — так значок не розійдеться з логотипом на сторінці радіо.

say "Малюю значок"
java -Djava.awt.headless=true scripts/MakeIcons.java "$B/icons"

# ---- ресурси ------------------------------------------------------------------

say "Збираю ресурси"
"$BT/aapt2" compile --dir app/src/main/res -o "$B/res.zip"
"$BT/aapt2" compile --dir "$B/icons" -o "$B/icons.zip"

say "Складаю ресурси з маніфестом"
"$BT/aapt2" link \
  -o "$B/base.apk" \
  -I "$JAR" \
  --manifest app/src/main/AndroidManifest.xml \
  --java "$B/gen" \
  --min-sdk-version "$MIN_SDK" \
  --target-sdk-version "$TARGET_SDK" \
  "$B/res.zip" "$B/icons.zip"

# ---- код ----------------------------------------------------------------------

say "Компілюю код"
find app/src/main/java "$B/gen" -name '*.java' > "$B/sources.txt"
javac --release 17 -nowarn -encoding UTF-8 \
  -classpath "$JAR" \
  -d "$B/classes" \
  @"$B/sources.txt" 2>&1 | grep -v '^Note:' || true
[ -f "$B/classes/$PKG_DIR/MainActivity.class" ] || die "Компіляція не вдалася"

# Лямбди, рядки через «+» і try-with-resources d8 сам переписує так, щоб
# вони йшли на Android 5. А от виклики новіших API він не чіпає — їх
# показує перевірка нижче, і кожен має стояти за Build.VERSION.SDK_INT.
say "Виклики, новіші за Android 5 (API $MIN_SDK) — кожен має бути за перевіркою версії:"
python3 scripts/api-check.py "$B/classes" "$PLATFORM/data/api-versions.xml" "$MIN_SDK"

say "Перетворюю на dex"
"$BT/d8" --release --min-api "$MIN_SDK" --lib "$JAR" \
  --output "$B/dex" \
  $(find "$B/classes" -name '*.class')

# ---- пакунок ------------------------------------------------------------------

say "Складаю пакунок"
cp "$B/base.apk" "$B/unsigned.apk"
(cd "$B/dex" && zip -q -X ../unsigned.apk classes*.dex)
"$BT/zipalign" -f -p 4 "$B/unsigned.apk" "$B/aligned.apk"

# Android 5–6 перевіряють лише старий підпис (v1, JAR); apksigner ставить
# його сам, коли minSdk нижчий за 24, — разом із новими v2/v3.
say "Підписую"
mkdir -p "$OUT_DIR"
# Пароль ключа окремо не задаємо: у PKCS12 він той самий, а два «file:» на
# один файл apksigner читає як два рядки — і на другому натрапляє на кінець.
# -J-enable…: JDK 23+ інакше засипає попередженнями про native access у conscrypt
# (обгортка apksigner сама дописує один дефіс, тому тут він один).
"$BT/apksigner" -J-enable-native-access=ALL-UNNAMED sign \
  --ks "$KEYSTORE" --ks-key-alias "$ALIAS" \
  --ks-pass "file:$PASSFILE" \
  --min-sdk-version "$MIN_SDK" \
  --v4-signing-enabled false \
  --out "$OUT" \
  "$B/aligned.apk"
"$BT/apksigner" -J-enable-native-access=ALL-UNNAMED verify --min-sdk-version "$MIN_SDK" "$OUT" >/dev/null

VERSION=$(sed -n 's/.*android:versionName="\([^"]*\)".*/\1/p' app/src/main/AndroidManifest.xml | head -1)
SIZE=$(ls -lh "$OUT" | awk '{print $5}')
printf '\033[32m✓ Готово:\033[0m %s (%s, версія %s, Android 5.0+)\n' "$OUT" "$SIZE" "$VERSION"
