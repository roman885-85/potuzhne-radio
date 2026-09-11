/*  ---------------------------------------------------------------------------
 *  Логотипи станцій — самі, без комп'ютера.
 *
 *  Заграла станція, а логотипа для неї ще немає — шукаємо її в каталозі
 *  radio-browser.info за адресою потоку (запасний шлях — точний збіг назви),
 *  качаємо значок, розпаковуємо (PNG, ICO, JPEG), зменшуємо до 45x45 і
 *  кладемо в SPIFFS готовими пікселями: /logo/<crc32 адреси>.565.
 *  Не знайшли — ставимо позначку .no, щоб не питати щоразу; мережеві збої
 *  позначки не ставлять — спробуємо іншим разом.
 *
 *  PNG розбираємо самі: розпаковувач zlib (tinfl) уже є в ПЗУ ESP32-S3.
 *  ------------------------------------------------------------------------- */
#ifndef yoLogos_h
#define yoLogos_h

#include <Arduino.h>

#define LOGO_S   45

class YoLogos {
  public:
    void     want(const char* url, const char* name);   /* з задачі дисплея */
    uint32_t version() const { return _ver; }          /* змінюється, коли з'явився новий логотип */
    bool     busy() const { return _busy; }
    uint16_t forget();                                  /* стерти позначки «нема» — шукати знову */
    void     bump() { _ver++; }                         /* логотип змінили ззовні (веб) — перечитати */
    static uint32_t crc(const char* url);
  private:
    volatile bool     _busy = false;
    volatile uint32_t _ver = 0;
    char     _url[200] = {0};
    char     _name[64] = {0};
    uint32_t _crc = 0;
    static void _task(void* arg);
};

extern YoLogos logos;

/*  Розпакувати зображення (PNG / ICO / JPEG) у W x H RGB565, прозоре — на білому.  */
bool yoImageFit(const uint8_t* data, size_t len, uint16_t* out, int W, int H);

#endif
