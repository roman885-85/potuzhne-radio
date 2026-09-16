#include "options.h"
#if (TS_MODEL!=TS_MODEL_UNDEFINED) && (DSP_MODEL!=DSP_DUMMY)
#include "Arduino.h"
#include "touchscreen.h"
#include "../m2/m2player.h"
#include "config.h"
#include "display.h"
#include "network.h"
#include "../menu/yoMenu.h"
#include "../extras/yoSfx.h"
#include "../extras/yoExtras.h"

#ifndef TS_FT6336_ADDR
  #define TS_FT6336_ADDR        0x38
#endif
#ifndef TS_X_MIN
  #define TS_X_MIN              400
#endif
#ifndef TS_X_MAX
  #define TS_X_MAX              3800
#endif
#ifndef TS_Y_MIN
  #define TS_Y_MIN              260
#endif
#ifndef TS_Y_MAX
  #define TS_Y_MAX              3800
#endif

#if TS_MODEL==TS_MODEL_XPT2046
  #ifdef TS_SPIPINS
    SPIClass  TSSPI(HSPI);
  #endif
  #include <XPT2046_Touchscreen.h>
  XPT2046_Touchscreen ts(TS_CS);
#elif TS_MODEL==TS_MODEL_FT6336
  #include "../FT6336_Touchscreen/yoFT6336.h"
  YoFT6336 ts = YoFT6336(TS_SDA, TS_SCL, TS_INT, TS_RST);
#endif

void TouchScreen::init(uint16_t w, uint16_t h){
#if TS_MODEL==TS_MODEL_XPT2046
  #ifdef TS_SPIPINS
    TSSPI.begin(TS_SPIPINS);
    ts.begin(TSSPI);
  #else
    ts.begin();
  #endif
#elif TS_MODEL==TS_MODEL_FT6336
  /*  Тачскрину — той самий номер повороту, що й дисплею: він сам переведе
      портретні координати панелі в екранні.  */
  ts.begin(TS_FT6336_ADDR);
#endif
  _width  = w;
  _height = h;
  flip();
#if TS_MODEL==TS_MODEL_FT6336
  ts.setResolution(_width, _height);
#endif
}

void TouchScreen::flip(){
#if TS_MODEL==TS_MODEL_XPT2046
  ts.setRotation(config.store.fliptouch?3:1);
#elif TS_MODEL==TS_MODEL_FT6336
  ts.setRotation(config.store.fliptouch?1:3);
#endif
}

void TouchScreen::_point(uint16_t& x, uint16_t& y){
  if(_inject){ x = _injX; y = _injY; return; }
#if TS_MODEL==TS_MODEL_XPT2046
  TS_Point p = ts.getPoint();
  x = map(p.x, TS_X_MIN, TS_X_MAX, 0, _width);
  y = map(p.y, TS_Y_MIN, TS_Y_MAX, 0, _height);
#else
  x = ts.points[0].x;
  y = ts.points[0].y;
#endif
}

volatile bool yoTouchLog = false;   /* команда touchlog: друкувати координати кожного дотику */

void TouchScreen::loop(){
  static bool wastouched = true;
  if(millis() - _touchdelay <= 10) return;       /* 100 разів на секунду: швидкий тик інакше губиться */
  _touchdelay = millis();
#if TS_MODEL==TS_MODEL_FT6336
  ts.read();
#endif
  const bool istouched = _istouched();
#if TS_MODEL==TS_MODEL_FT6336
  if(yoTouchLog && istouched && !wastouched && !_inject){
    uint16_t lx = ts.points[0].x, ly = ts.points[0].y;
    Serial.printf("##TOUCH#\tсирий rx=%u ry=%u -> екран x=%u y=%u\n", ts.rawX, ts.rawY, lx, ly);
  }
#endif

  /*  Дотик до погаслого екрана (ніч, кінець таймера сну, заставка) лише будить
      його і далі, до відпускання, нічого не натискає: людина не бачить, куди
      влучила. Кожен інший дотик продовжує денну яскравість уночі.  */
  static bool wakeOnly = false;
  if(istouched && !wastouched){
    if(extras.touchWake()) wakeOnly = true;
    if(config.isScreensaver){ wakeOnly = true; display.putRequest(NEWMODE, PLAYER); }
  }
  if(istouched && !wastouched && !wakeOnly) sfx.play(SFX_CLICK);     /* клацання — лише коли ввімкнули */
  if(wakeOnly){
    if(!istouched) wakeOnly = false;
    wastouched = istouched;
    return;
  }

  uint16_t x = _x, y = _y;
  if(istouched) _point(x, y);

  /*  Відкрите меню забирає дотики собі цілком.  */
  if(yomenu.active()){
    if(istouched){
      if(!wastouched){ _x = x; _y = y; _t0 = millis(); yomenu.onPress(x, y); }
      else yomenu.onDrag(x, y);
    }else if(wastouched){
      yomenu.onRelease(_x, _y, millis() - _t0);
    }
    wastouched = istouched;
    return;
  }

  /*  Мережі немає чи зв'язок зник: на екрані «немає зв'язку», і дотик веде
      просто до списку мереж — там людина й вибирає свою.  */
  const bool lostLink = (display.mode() == LOST);
  const bool netReady = (network.status == CONNECTED || network.status == SDREADY) && !lostLink;
  if(!netReady){
    if(!istouched && wastouched) yomenu.openWifi(!lostLink);
    wastouched = istouched;
    return;
  }

  /*  Головний екран розбирає дотики сам.  */
  if(display.mode() == PLAYER && m2::P.shown()){
    if(istouched){
      _x = x; _y = y;
      if(!wastouched) m2::P.onPress(x, y);
      else m2::P.onDrag(x, y);
    }else if(wastouched){
      m2::P.onRelease(_x, _y);
    }
    wastouched = istouched;
    return;
  }

  /*  Будь-який інший стан (картка пам'яті читається, оновлення) — дотик нічого
      не робить, лише повертає на головний екран, коли відпустять.  */
  if(!istouched && wastouched && display.mode() != UPDATING && display.mode() != SDCHANGE)
    display.putRequest(NEWMODE, PLAYER);
  wastouched = istouched;
}

void TouchScreen::injectBegin(uint16_t x, uint16_t y){ _inject=true; _injTouched=true;  _injX=x; _injY=y; }
void TouchScreen::injectMove (uint16_t x, uint16_t y){ _inject=true; _injTouched=true;  _injX=x; _injY=y; }
/*  Знімаємо і сам прапорець підміни: інакше після відлагоджувального жесту
    справжня панель лишалася б відключеною до перезавантаження.  */
void TouchScreen::injectEnd  (){ _injTouched=false; _inject=false; }

void TouchScreen::dbgState(){
  Serial.printf("дотик: останній %u,%u режим=%d меню=%d плеєр=%d\n", _x, _y, (int)display.mode(),
                yomenu.active() ? 1 : 0, m2::P.shown() ? 1 : 0);
}

bool TouchScreen::_istouched(){
  if(_inject) return _injTouched;
#if TS_MODEL==TS_MODEL_XPT2046
  return ts.touched();
#else
  return ts.isTouched;
#endif
}

#endif  // TS_MODEL!=TS_MODEL_UNDEFINED
