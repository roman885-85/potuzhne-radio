/*  ---------------------------------------------------------------------------
 *  Кадр меню в пам'яті (PSRAM, 320×240 RGB565).
 *
 *  Меню малює не прямо в дисплей, а сюди (у yoMenu.cpp і menuwidgets.cpp
 *  «dsp» — це цей кадр), а на екран іде лише те, що змінилось: квадрати
 *  16×16, зведені в смуги, через DMA. Це дає:
 *    - згладжений текст на будь-якому тлі: гліф змішується з тим, що вже
 *      лежить у кадрі (шрифти fonts/aa/*, 4 біти на піксель);
 *    - анімації поверх сторінки (підсвітка під пальцем) без перемальовки
 *      віджетів: є що відновити;
 *    - менше миготіння: сторінку спершу збирають, а потім показують.
 *  Списки меню (Wi-Fi, проповіді) виводяться рядками прямо в дисплей —
 *  їхні пікселі дзеркаляться сюди, щоб кадр їх не затер.
 *  ------------------------------------------------------------------------- */
#ifndef uicanvas_h
#define uicanvas_h
#include <Arduino.h>
#include <Adafruit_GFX.h>

class UiCanvas : public GFXcanvas16 {
  public:
    UiCanvas();
    bool begin();                                  /* пам'ять під кадр; false — не вийшло (малюємо як раніше) */
    bool ok() const { return buffer != nullptr; }
    void setActive(bool on);                       /* меню відкрите: кадр живий */
    bool active() const { return _active; }

    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
    void fillScreen(uint16_t color) override;
    size_t write(uint8_t c) override;              /* згладжені шрифти */
    using Print::write;

    /*  Згладжене: коло й прямокутник із заокругленими кутами (край — змішуванням).  */
    void fillCircleAA(float cx, float cy, float r, uint16_t color);
    void fillRoundRectAA(int16_t x, int16_t y, int16_t w, int16_t h, float r, uint16_t color);
    uint16_t blend(uint16_t bg, uint16_t fg, uint8_t a) const;   /* a 0..255 */
    uint16_t pixelAt(int16_t x, int16_t y) const { return getPixel(x, y); }

    void mark(int16_t x, int16_t y, int16_t w, int16_t h);        /* позначити змінене */
    void markAll();
    void flush();                                  /* змінене — на екран (задача дисплея) */

    /*  Дзеркало того, що хтось вивів у дисплей напряму (списки меню).
        be — байти в порядку дисплея (старший першим).  */
    void mirror(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* px, bool be);
    void mirrorFill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

    static bool isAA(const GFXfont* f);

  private:
    static const int TS = 16, TW = 20, TH = 15;    /* квадрати змін */
    uint32_t _dirty[TH] = {0};
    bool     _active = false;
    uint8_t* _dma = nullptr;                       /* смуга на вивід, внутрішня пам'ять для DMA */
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
    void _markRaw(int16_t x, int16_t y, int16_t w, int16_t h);
    void _drawAAGlyph(int16_t x, int16_t y, uint8_t c);
};

extern UiCanvas ui;

/*  Для widgets.cpp: списки меню пишуть і сюди, якщо меню відкрите.  */
void uiMirror(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* px, bool be);
void uiMirrorFill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

#endif
