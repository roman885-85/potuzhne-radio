#!/bin/bash
# ---------------------------------------------------------------------------
#  Пересборка прошивки «ПОТУЖНЕ РАДІО» для плати ES3C28P.
#  Версія — у firmware/VERSION; для оновлення через браузер — PotuzhneRadio-ES3C28P-update.bin.
#
#  Збирає ../source/yoRadio власним набором бібліотек із ../build/sketchbook —
#  глобальні бібліотеки Arduino не чіпаються. Готові образи кладе поруч.
# ---------------------------------------------------------------------------
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$HERE")"
B="$ROOT/build"
SKETCH="$ROOT/source/yoRadio"

#  Беремо саме шлях, а не коротке ім'я: перевірка -x нижче дивиться на
#  файл, і коротке ім'я вона шукала б у поточній теці, а не в PATH.
CLI="$HOME/bin/arduino-cli"
_p="$(command -v arduino-cli || true)"; [ -n "$_p" ] && CLI="$_p"
[ -x "$CLI" ] || { echo "arduino-cli не знайдено (очікувався в ~/bin)"; exit 1; }
[ -d "$SKETCH" ] || { echo "не знайдено $SKETCH"; exit 1; }

#  Налаштування arduino-cli — під цей комп'ютер. У git їх немає (там абсолютні
#  шляхи), тож на новому Mac створюємо самі.
if [ ! -f "$B/arduino-cli.yaml" ]; then
  mkdir -p "$B/sketchbook"
  cat > "$B/arduino-cli.yaml" <<YAML
board_manager:
  additional_urls:
    - https://espressif.github.io/arduino-esp32/package_esp32_index.json
directories:
  data: $HOME/Library/Arduino15
  downloads: $HOME/Library/Arduino15/staging
  user: $B/sketchbook
library:
  enable_unsafe_install: true
logging:
  level: warn
YAML
fi

FQBN="esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=16M,FlashMode=qio,PSRAM=opi,CPUFreq=240,PartitionScheme=huge_app,LoopCore=1,EventsCore=1,DebugLevel=none"

#  Версія й час збірки — у прошивку: сторінка «Оновлення» читає їх із файлу
#  й показує, що саме заливається і чи воно новіше за те, що на радіо.
VER="$(tr -d ' \n\r' < "$HERE/VERSION" 2>/dev/null)"; [ -n "$VER" ] || VER="1.0"
BUILD="$(date '+%d.%m.%Y %H:%M')"
printf '/* Створює firmware/rebuild.sh під час кожної збірки — вручну не правити. */\n#define PR_VERSION "%s"\n#define PR_BUILD   "%s"\n' "$VER" "$BUILD" > "$SKETCH/src/extras/yoBuild.h"
echo ">>> версія $VER, збірка $BUILD"

#  Сторінка радіо: свіжі app.js / app.css — стиснуті в образ файлової системи
if [ -f "$SKETCH/web/app.js" ]; then
  gzip -9 -c "$SKETCH/web/app.js"  > "$SKETCH/data/www/app.js.gz"
  gzip -9 -c "$SKETCH/web/app.css" > "$SKETCH/data/www/app.css.gz"
fi

echo ">>> компіляція (перший запуск — близько 13 хвилин)"
"$CLI" --config-file "$B/arduino-cli.yaml" compile \
  --fqbn "$FQBN" --build-path "$B/bp" --output-dir "$B/out" "$SKETCH"

echo ">>> образ файлової системи"
MKSPIFFS=$(ls -d "$HOME"/Library/Arduino15/packages/esp32/tools/mkspiffs/*/mkspiffs | head -1)
"$MKSPIFFS" -c "$SKETCH/data" -b 4096 -p 256 -s 0x200000 "$B/out/yoRadio.spiffs.bin"

echo ">>> образ ресурсів (звуки подій, заставка) — LittleFS у вільних 7,9 МБ"
MKLFS=$(ls -d "$HOME"/Library/Arduino15/packages/esp32/tools/mklittlefs/*/mklittlefs | sort -V | tail -1)
"$MKLFS" -c "$SKETCH/assets" -b 4096 -p 256 -s 0x7E0000 "$B/out/yoRadio.assets.bin" > /dev/null

echo ">>> склейка одного образу"
ESPTOOL=$(ls -d "$HOME"/Library/Arduino15/packages/esp32/tools/esptool_py/*/esptool | sort -V | tail -1)
cp "$HOME"/Library/Arduino15/packages/esp32/hardware/esp32/3.3.3/tools/partitions/boot_app0.bin "$B/out/"
cd "$B/out"
"$ESPTOOL" --chip esp32s3 merge-bin -o PotuzhneRadio-ES3C28P-full.bin \
  --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0      yoRadio.ino.bootloader.bin \
  0x8000   yoRadio.ino.partitions.bin \
  0xe000   boot_app0.bin \
  0x10000  yoRadio.ino.bin \
  0x610000 yoRadio.spiffs.bin \
  0x820000 yoRadio.assets.bin

#  Імена — нашого проєкту: те, що бачить власник, не має нагадувати yoRadio.
cp PotuzhneRadio-ES3C28P-full.bin                  "$HERE/PotuzhneRadio-ES3C28P-full.bin"
cp yoRadio.ino.bin                                 "$HERE/PotuzhneRadio-ES3C28P-update.bin"
cp yoRadio.ino.bootloader.bin                      "$HERE/PotuzhneRadio-ES3C28P-bootloader.bin"
cp yoRadio.ino.partitions.bin                      "$HERE/PotuzhneRadio-ES3C28P-partitions.bin"
cp yoRadio.spiffs.bin                              "$HERE/PotuzhneRadio-ES3C28P-files.bin"
cp yoRadio.assets.bin                              "$HERE/PotuzhneRadio-ES3C28P-assets.bin"
cp boot_app0.bin                                   "$HERE/boot_app0.bin"
rm -f "$HERE"/yoRadio.ino.bin "$HERE"/yoRadio.ino.bootloader.bin "$HERE"/yoRadio.ino.partitions.bin "$HERE"/yoRadio.spiffs.bin "$HERE"/yoRadio.assets.bin
mkdir -p "$HERE/web" && cp "$SKETCH"/data/www/app.*.gz "$HERE/web/" 2>/dev/null || true   # файли сторінки — поруч із прошивкою
# ELF цієї збірки — щоб потім було чим розшифрувати дамп падіння (команда
# coredump друкує адреси, а вони без ELF тієї самої збірки нічого не варті).
# Тримаємо останні 10: файл важить близько 28 МБ.
if [ -f "$B/bp/yoRadio.ino.elf" ]; then
  mkdir -p "$HERE/elf"
  cp "$B/bp/yoRadio.ino.elf" "$HERE/elf/$VER.elf"
  ls -1t "$HERE"/elf/*.elf 2>/dev/null | tail -n +11 | xargs -r rm -f
  echo ">>> ELF збережено: elf/$VER.elf"
fi
echo ">>> готово: версія $VER від $BUILD, образи в $HERE"
