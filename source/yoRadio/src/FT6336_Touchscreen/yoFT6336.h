/*  ---------------------------------------------------------------------------
 *  FT6336 — ёмкостный тачскрин платы ES3C28P (I2C 0x38, общая шина с ES8311).
 *
 *  Контроллер всегда отдаёт координаты в «портретной» системе панели:
 *  x = 0..239 поперёк, y = 0..319 вдоль. yoRadio же держит ILI9341 в ландшафте,
 *  поэтому read() сразу пересчитывает их в экранные — по той же матрице MADCTL,
 *  которой Adafruit_ILI9341 разворачивает картинку.
 *  ------------------------------------------------------------------------- */
#ifndef yoFT6336_h
#define yoFT6336_h

#include <Arduino.h>

class FT_Point {
  public:
    FT_Point() : id(0), x(0), y(0), size(0) {}
    uint8_t  id;
    uint16_t x;
    uint16_t y;
    uint8_t  size;
};

class YoFT6336 {
  public:
    YoFT6336(int8_t sda, int8_t scl, int8_t intPin, int8_t rstPin);
    bool begin(uint8_t addr = 0x38);
    uint16_t rawX = 0, rawY = 0;      /* останній сирий дотик панелі (для калібрування) */
  void setRotation(uint8_t displayRotation);   /* 1 или 3, как у Adafruit_GFX */
    void setResolution(uint16_t w, uint16_t h);
    void read();

    bool     isTouched = false;
    uint8_t  touches   = 0;
    FT_Point points[2];

  private:
    bool     _rd(uint8_t reg, uint8_t *buf, uint8_t len);
    void     _map(uint16_t rx, uint16_t ry, FT_Point &p);
    int8_t   _sda, _scl, _int, _rst;
    uint8_t  _addr     = 0x38;
    uint8_t  _rotation = 3;
    uint16_t _w = 320, _h = 240;
    bool     _present = false;
};

#endif
