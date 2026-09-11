/*  ---------------------------------------------------------------------------
 *  Запис ефіру на картку — як є, без перекодування.
 *
 *  Байти потоку беремо в Audio::processWebStream() одразу після читання з
 *  мережі, коли ICY-метадані вже вирізано: у файл іде рівно те, що йде в
 *  декодер. MP3 так і лишається MP3, AAC — ADTS-файлом, який грає будь-що.
 *
 *  Вся робота — у головному циклі, там само, де декодер: відвід лише
 *  дописує в буфер у PSRAM, а на картку він скидається шматками по 8 КБ.
 *  Запис шматка на SDMMC триває кілька мілісекунд, звук цього не помічає.
 *
 *  Картка в режимі радіо штатно відмонтована; для запису монтуємо її самі
 *  й лишаємо змонтованою — yoRadio при переході на картку це влаштовує.
 *  ------------------------------------------------------------------------- */
#ifndef yoRecorder_h
#define yoRecorder_h

#include <Arduino.h>
#include <FS.h>

class YoRecorder {
  public:
    bool active() const { return _on; }
    bool start();                        /* false — нема картки, місця чи звуку */
    void stop();
    void loop();
    void tap(const uint8_t* p, size_t n);   /* з Audio, у тому ж потоці */
    uint32_t seconds() const { return _on ? (millis() - _t0) / 1000 : 0; }
    uint32_t bytes() const { return _written + _len; }
    const char* fileName() const { return _name; }
    const char* lastError() const { return _err; }
  private:
    bool     _on = false;
    File     _f;
    uint8_t* _buf = nullptr;
    size_t   _len = 0;
    uint32_t _written = 0, _dropped = 0, _t0 = 0;
    uint16_t _station = 0;
    char     _name[80] = {0};
    const char* _err = "";
    void _flush();
};

extern YoRecorder recorder;

#endif
