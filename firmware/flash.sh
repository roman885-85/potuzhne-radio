#!/bin/bash
# ---------------------------------------------------------------------------
#  Прошивка «ПОТУЖНЕ РАДІО» в плату ES3C28P (ESP32-S3, 16 МБ flash).
#
#  У платы НЕТ микросхемы USB-UART: Type-C идёт напрямую в ESP32-S3.
#  Поэтому перед прошивкой её надо вручную перевести в режим загрузки:
#     1. отсоедините кабель;
#     2. зажмите и держите кнопку BOOT;
#     3. вставьте кабель;
#     4. отпустите BOOT.
#  В системе появится новый порт /dev/cu.usbmodemXXXX — его и берём.
#
#  Использование:
#     ./flash.sh                 — залить всё одним образом (по умолчанию)
#     ./flash.sh /dev/cu.usbmodem1101
#     ./flash.sh --app-only      — обновить только программу, настройки и
#                                  список станций в SPIFFS не трогать
# ---------------------------------------------------------------------------
set -e
cd "$(dirname "$0")"

APP_ONLY=0
PORT=""
for a in "$@"; do
  case "$a" in
    --app-only) APP_ONLY=1 ;;
    *)          PORT="$a" ;;
  esac
done

ESPTOOL=$(ls -d "$HOME"/Library/Arduino15/packages/esp32/tools/esptool_py/*/esptool 2>/dev/null | sort -V | tail -1)
if [ ! -x "$ESPTOOL" ]; then
  if command -v esptool.py >/dev/null; then ESPTOOL=esptool.py
  elif command -v esptool >/dev/null; then ESPTOOL=esptool
  else echo "esptool не найден. Поставьте: pip3 install esptool"; exit 1; fi
fi

if [ -z "$PORT" ]; then
  PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)
fi
if [ -z "$PORT" ]; then
  echo "Порт не найден. Переведите плату в режим загрузки — см. шапку файла."
  exit 1
fi
echo "esptool: $ESPTOOL"
echo "порт:    $PORT"
echo

#  Разом із програмою пишемо й boot_app0.bin у розділ otadata: після оновлення
#  через сторінку радіо вантажиться з другого розділу (app1), і нова програма
#  в app0 без цього так і лежала б невживана.
if [ "$APP_ONLY" = "1" ]; then
  "$ESPTOOL" --chip esp32s3 --port "$PORT" --baud 921600 \
    --before default-reset --after hard-reset write-flash -z \
    --flash-mode dio --flash-freq 80m --flash-size 16MB \
    0xe000  boot_app0.bin \
    0x10000 PotuzhneRadio-ES3C28P-update.bin
else
  "$ESPTOOL" --chip esp32s3 --port "$PORT" --baud 921600 \
    --before default-reset --after hard-reset write-flash -z \
    --flash-mode dio --flash-freq 80m --flash-size 16MB \
    0x0 PotuzhneRadio-ES3C28P-full.bin
fi

echo
echo "Готово. Отсоедините и снова подключите кабель — плата стартует сама."
echo "Если Wi-Fi ещё не задан: подключитесь к точке доступа PotuzhneRadio (пароль 12345987)"
echo "и откройте http://192.168.4.1/ , чтобы задать свой Wi-Fi."
