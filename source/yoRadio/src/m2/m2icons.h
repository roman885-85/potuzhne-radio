/*  Нове меню: значки. Малюються згладженими лініями в квадраті ~20×20 з
    центром (cx, cy); bg — колір під значком (для вирізів).  */
#ifndef m2icons_h
#define m2icons_h
#include "m2gfx.h"

namespace m2 {

enum Icon : uint8_t {
  IC_NONE = 0, IC_BACK, IC_CHEV, IC_MOON, IC_ALARM, IC_REC, IC_CARD, IC_RADIO, IC_STAR, IC_CROSS,
  IC_EQ, IC_SUN, IC_GEAR, IC_LIST, IC_WIFI, IC_MIC, IC_CLOCK, IC_INFO, IC_POWER, IC_CODE,
  IC_SPEAKER, IC_HAND, IC_EYE, IC_LOCK, IC_PLUS, IC_CLOSE, IC_UP, IC_CHECK, IC_REFRESH, IC_KEYS,
  IC_BATTERY, IC_LED, IC_NOTE, IC_ROOM, IC_PERSON, IC_WAVE, IC_CHIP, IC_PLAY, IC_STOP, IC_PENCIL,
  IC_TRASH, IC_RESTART, IC_MENU, IC_SPLASH, IC_BELL, IC_GLOBE, IC_START, IC_DOWN, IC_BACKSPACE, IC_SHIFT,
  IC_N
};

void icon(Gfx& g, uint8_t id, float cx, float cy, uint16_t c, uint16_t bg);
/*  рівень сигналу: 0..4 рисок  */
void signalBars(Gfx& g, float x, float bottom, uint8_t level, uint16_t on, uint16_t off);

}  // namespace m2
#endif
