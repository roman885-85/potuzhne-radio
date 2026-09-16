#include "../core/options.h"
#if TS_MODEL==TS_MODEL_FT6336

#include "yoFT6336.h"
#include <Wire.h>

#define FT6336_TD_STATUS   0x02
#define FT6336_TOUCH_1     0x03
#define FT6336_CHIP_ID     0xA8   /* FocalTech ID, должен быть 0x11 */

/*  Физический размер матрицы панели в её собственной, портретной системе  */
#define FT_PANEL_W  240
#define FT_PANEL_H  320

YoFT6336::YoFT6336(int8_t sda, int8_t scl, int8_t intPin, int8_t rstPin)
  : _sda(sda), _scl(scl), _int(intPin), _rst(rstPin) {}

bool YoFT6336::_rd(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(_addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(_addr, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

bool YoFT6336::begin(uint8_t addr) {
  _addr = addr;
  Wire.begin(_sda, _scl, 400000);

  if (_int >= 0) pinMode(_int, INPUT);
  if (_rst >= 0) {
    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, HIGH);
    delay(10);
    digitalWrite(_rst, LOW);
    delay(20);
    digitalWrite(_rst, HIGH);
    delay(300);
  }

  uint8_t id = 0;
  _present = _rd(FT6336_CHIP_ID, &id, 1) && id == 0x11;
  if (!_present) Serial.println("##[ERROR]#\tFT6336 не отвечает по I2C");
  return _present;
}

void YoFT6336::setRotation(uint8_t displayRotation) { _rotation = displayRotation; }
void YoFT6336::setResolution(uint16_t w, uint16_t h) { _w = w; _h = h; }

void YoFT6336::_map(uint16_t rx, uint16_t ry, FT_Point &p) {
  if (rx >= FT_PANEL_W) rx = FT_PANEL_W - 1;
  if (ry >= FT_PANEL_H) ry = FT_PANEL_H - 1;
  switch (_rotation) {
    case 0: p.x = rx;                p.y = ry;                break;
    case 1: p.x = ry;                p.y = FT_PANEL_W - 1 - rx; break;
    case 2: p.x = FT_PANEL_W - 1 - rx; p.y = FT_PANEL_H - 1 - ry; break;
    default: /* 3 */
            p.x = FT_PANEL_H - 1 - ry; p.y = rx;                break;
  }
}

void YoFT6336::read() {
  if (!_present) { isTouched = false; touches = 0; return; }

  uint8_t st = 0;
  if (!_rd(FT6336_TD_STATUS, &st, 1)) { isTouched = false; touches = 0; return; }
  touches   = st & 0x0F;
  isTouched = (touches > 0 && touches < 3);
  if (!isTouched) return;

  for (uint8_t i = 0; i < touches && i < 2; i++) {
    uint8_t d[4];
    if (!_rd(FT6336_TOUCH_1 + i * 6, d, 4)) { isTouched = false; return; }
    uint16_t rx = (uint16_t)((d[0] & 0x0F) << 8) | d[1];
    uint16_t ry = (uint16_t)((d[2] & 0x0F) << 8) | d[3];
    points[i].id = d[2] >> 4;
    if(i == 0){ rawX = rx; rawY = ry; }
    _map(rx, ry, points[i]);
  }
}

#endif  // TS_MODEL_FT6336
