#include "options.h"
#if (TS_MODEL!=TS_MODEL_UNDEFINED) && (DSP_MODEL!=DSP_DUMMY)
#include "Arduino.h"
#include <math.h>
#include "touchscreen.h"
#include "sdmanager.h"
#include "config.h"
#include "controls.h"
#include "display.h"
#include "player.h"
#include "network.h"
#include "../menu/yoMenu.h"
#include "../extras/yoSfx.h"
#include "../extras/yoExtras.h"
#include "../extras/yoSermons.h"

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
#ifndef TS_STEPS
  #define TS_STEPS              40
#endif
#ifndef TS_FT6336_ADDR
  #define TS_FT6336_ADDR        0x38
#endif
/*  Розкладка сторінки плеєра: шапка з іконками та смуга гучності внизу.  */
#ifndef TS_HEADER_H
  #define TS_HEADER_H           38
#endif
#ifndef TS_GEAR_X
  /*  Значок списку намальовано на 254..276, шестерню на 291..311. Межа на
      274 відрізала правий край списку до шестерні; ставимо її посередині
      проміжку між ними.  */
  #define TS_GEAR_X             284
#endif
/*  Висота рядка списку: playlistConf має textsize 2, а віджет рахує
    textsize*(CHARHEIGHT-1) + textsize*4 = 22 пікселі.  */
/*  Кожна перемальовка списку відкриває playlist.csv та index.dat і читає їх
    з накопичувача. Тому крокуємо скільки завгодно швидко, а перемальовку
    просимо не частіше, ніж вона встигає — інакше список захлинається.  */
/*  Список НЕ закільцьований: перша й остання станції — краї, далі він
    не їде. Раніше тут був штатний перенос через край, і з останньої
    станції прокрутка перескакувала на першу.  */
float TouchScreen::_plClamp(float p){
  int cs = config.playlistLength();
  if(cs < 1) return 1.0f;
  if(p < 1.0f)        p = 1.0f;
  if(p > (float)cs)   p = (float)cs;
  return p;
}

static void plApply(int delta){
  if(delta == 0) return;
  int cs = config.playlistLength();
  if(cs < 1) return;
  int p = (int)display.currentPlItem + delta;
  if(p < 1)  p = 1;
  if(p > cs) p = cs;
  display.currentPlItem = p;
  display.resetQueue();
  display.putRequest(DRAWPLAYLIST, p);
}

#ifndef TS_PL_REDRAW_MS
  #define TS_PL_REDRAW_MS       90
#endif
#ifndef TS_PL_ITEM_H
  #define TS_PL_ITEM_H          32    /* має збігатися з PL_ROW_H у widgets.h */
#endif
#ifndef TS_PL_FLING_MIN
  #define TS_PL_FLING_MIN       2.0f    /* нижче цієї швидкості розгін не починаємо */
#endif
/*  Розмітка сторінки плейлиста — та сама, що PL_* у widgets.h.  */
#define TS_PL_LIST_X    258      /* правіше — стовпчик кнопок */
#define TS_PL_TOP       8
#define TS_PL_ROWS      7
#define TS_PL_CUR       3
enum { PLB_UP_T = 0, PLB_DOWN_T, PLB_PLAY_T, PLB_BACK_T };
/*  Пульт картки на плеєрі — розмітка та сама, що SD_* у widgets.h.  */
#define TS_SD_Y0        80
#define TS_SD_Y1        126
#define TS_SD_PREV_X    106      /* лівіше — попередній трек */
#define TS_SD_NEXT_X    274      /* правіше — наступний */
#define TS_SD_BAR_X0    110
#define TS_SD_BAR_X1    270
static float sdFracAt(int x){
  float f = (float)(x - TS_SD_BAR_X0) / (TS_SD_BAR_X1 - TS_SD_BAR_X0);
  return f < 0 ? 0 : (f > 1 ? 1 : f);
}
#ifndef TS_VOLBAR_Y
  #define TS_VOLBAR_Y           212   /* смуга 224..238, зона з запасом під палець */
#endif

#if TS_MODEL==TS_MODEL_XPT2046
  #ifdef TS_SPIPINS
    SPIClass  TSSPI(HSPI);
  #endif
  #include <XPT2046_Touchscreen.h>
  XPT2046_Touchscreen ts(TS_CS);
  typedef TS_Point TSPoint;
#elif TS_MODEL==TS_MODEL_GT911
  #include "../GT911_Touchscreen/TAMC_GT911.h"
  TAMC_GT911 ts = TAMC_GT911(TS_SDA, TS_SCL, TS_INT, TS_RST, 0, 0);
  typedef TP_Point TSPoint;
#elif TS_MODEL==TS_MODEL_FT6336
  #include "../FT6336_Touchscreen/yoFT6336.h"
  YoFT6336 ts = YoFT6336(TS_SDA, TS_SCL, TS_INT, TS_RST);
  typedef FT_Point TSPoint;
#endif

void TouchScreen::init(uint16_t w, uint16_t h){
  
#if TS_MODEL==TS_MODEL_XPT2046
  #ifdef TS_SPIPINS
    TSSPI.begin(TS_SPIPINS);
    ts.begin(TSSPI);
  #else
    #if TS_HSPI
      ts.begin(SPI2);
    #else
      ts.begin();
    #endif
  #endif
  ts.setRotation(config.store.fliptouch?3:1);
#endif
#if TS_MODEL==TS_MODEL_GT911
  ts.begin();
  ts.setRotation(config.store.fliptouch?0:2);
#endif
#if TS_MODEL==TS_MODEL_FT6336
  ts.begin(TS_FT6336_ADDR);
  /*  Тачскрину отдаём тот же номер поворота, что и дисплею: он сам переведёт
      портретные координаты панели в экранные.                                */
  ts.setRotation(config.store.fliptouch?1:3);
#endif
  _width  = w;
  _height = h;
#if TS_MODEL==TS_MODEL_GT911 || TS_MODEL==TS_MODEL_FT6336
  ts.setResolution(_width, _height);
#endif
}

tsDirection_e TouchScreen::_tsDirection(uint16_t x, uint16_t y) {
  int16_t dX = x - _oldTouchX;
  int16_t dY = y - _oldTouchY;
  if (abs(dX) > 20 || abs(dY) > 20) {
    if (abs(dX) > abs(dY)) {
      if (dX > 0) {
        return TSD_RIGHT;
      } else {
        return TSD_LEFT;
      }
    } else {
      if (dY > 0) {
        return TSD_DOWN;
      } else {
        return TSD_UP;
      }
    }
  } else {
    return TDS_REQUEST;
  }
}

void TouchScreen::flip(){
#if TS_MODEL==TS_MODEL_XPT2046
  ts.setRotation(config.store.fliptouch?3:1);
#endif
#if TS_MODEL==TS_MODEL_GT911
  ts.setRotation(config.store.fliptouch?0:2);
#endif
#if TS_MODEL==TS_MODEL_FT6336
  ts.setRotation(config.store.fliptouch?1:3);
#endif
}

void TouchScreen::loop(){
  uint16_t touchX, touchY;
  static bool wastouched = true;
  static uint32_t touchLongPress;
  static tsDirection_e direct;
  static uint16_t touchVol, touchStation;
  if (!_checklpdelay(10, _touchdelay)) return;   /* 100 разів на секунду: швидкий тик інакше губиться */
#if TS_MODEL==TS_MODEL_GT911 || TS_MODEL==TS_MODEL_FT6336
  ts.read();
#endif
  bool istouched = _istouched();

  /*  Дотик до погаслого екрана (ніч або кінець таймера сну) лише будить
      його і далі, до відпускання, нічого не натискає: людина не бачить,
      куди влучила. Кожен інший дотик продовжує денну яскравість уночі.  */
  static bool wakeOnly = false;
  if(istouched && !wastouched && extras.touchWake()) wakeOnly = true;
  if(istouched && !wastouched && !wakeOnly) sfx.play(SFX_CLICK);     /* клацання — лише коли ввімкнули */
  if(wakeOnly){
    if(!istouched) wakeOnly = false;
    wastouched = istouched;
    return;
  }

  /*  Плавний підвід списку до вибраного рядка: 180 мс, зі сповільненням
      наприкінці. Малює задача дисплея, тут лише рахуємо положення.  */
  if(_plAnim){
    if(display.mode() != STATIONS) _plAnim = false;
    else{
      /*  Згасаюча пружина: список трохи перелітає ціль (на 3-4 пікселі) і
          м'яко повертається. Коефіцієнт згасання 0.55, власна частота
          20 рад/с — рух займає менше півсекунди.  */
      float ts = (millis() - _plAnimT0) / 1000.0f;
      if(ts >= 0.45f){ _plAnim = false; _plPos = _plAnimTo; display.plScrollStop(_plAnimTo); }
      else{
        const float zw = 11.0f, wd = 16.7f;
        float e = 1.0f - expf(-zw * ts) * (cosf(wd * ts) + (zw / wd) * sinf(wd * ts));
        _plPos = _plAnimFrom + (_plAnimTo - _plAnimFrom) * e;
        display.plScrollTo(_plPos);
      }
    }
  }

  /*  Накат після відриву пальця і завершення жесту. Малює задача дисплея,
      сюди лише рахуємо положення. Швидкість гасне приблизно вдвічі за
      чверть секунди, потім список дотягується до найближчого рядка.  */
  /*  лише з наступного такту після відпускання: спершу відпускання рахує
      швидкість кидка, інакше накат бачив нуль і одразу дотягував пружиною  */
  if(_plActive && !istouched && !wastouched){
    uint32_t now = millis();
    float dt = (now - _plLastMs) / 1000.0f;
    if(dt <= 0.0f) dt = 0.001f;
    _plLastMs = now;
    if(display.mode() != STATIONS){          /* список закрився — прибираємо стан */
      _plActive = false; _plVel = 0.0f; _plCarry = 0.0f;
    }else{
      _plPos = _plClamp(_plPos - _plVel * dt);
      _plVel *= powf(0.12f, dt);                  /* м'якше: удвічі за третину секунди */
      /*  накат уперся в край списку — зупиняємось, а не «б'ємося» об нього  */
      if(_plPos <= 1.0f || _plPos >= (float)config.playlistLength()) _plVel = 0.0f;
      if(fabsf(_plVel) < 0.6f || dt > 1.0f){
        _plSnap();                                /* дотягуємо пружиною, а не стрибком */
      }else{
        display.plScrollTo(_plPos);
      }
    }
  }

#ifdef USE_YOMENU
  /*  Открытое меню забирает касания себе целиком.  */
  if(yomenu.active()){
    if(istouched){
      if(!wastouched){
      #if TS_MODEL==TS_MODEL_XPT2046
        TSPoint p = ts.getPoint();
        _oldTouchX = map(p.x, TS_X_MIN, TS_X_MAX, 0, _width);
        _oldTouchY = map(p.y, TS_Y_MIN, TS_Y_MAX, 0, _height);
      #else
        _oldTouchX = ts.points[0].x;
        _oldTouchY = ts.points[0].y;
      #endif
        if(_inject){ _oldTouchX = _injX; _oldTouchY = _injY; }
        _menuT0 = millis();
        yomenu.onPress(_oldTouchX, _oldTouchY);
      }else{
        /*  палець ведуть — для прокрутки списку проповідей  */
        uint16_t mx, my;
      #if TS_MODEL==TS_MODEL_XPT2046
        TSPoint p = ts.getPoint();
        mx = map(p.x, TS_X_MIN, TS_X_MAX, 0, _width); my = map(p.y, TS_Y_MIN, TS_Y_MAX, 0, _height);
      #else
        mx = ts.points[0].x; my = ts.points[0].y;
      #endif
        if(_inject){ mx = _injX; my = _injY; }
        yomenu.onDrag(mx, my);
      }
    }else if(wastouched){
      /*  Тривалість дотику — для «утримати, щоб прибрати» в обраному.  */
      yomenu.onRelease(_oldTouchX, _oldTouchY, millis() - _menuT0);
    }
    wastouched = istouched;
    return;
  }
  /*  Без сети управлять плеером нечем, но в меню попасть надо — там как раз
      и настраивают Wi-Fi. Поэтому обрабатываем только долгое нажатие.  */
  /*  Зв'язок зник: на екрані діалог «немає зв'язку», сторінки плеєра немає,
      і дотик раніше не робив геть нічого — вибрати іншу мережу з радіо було
      неможливо. Тепер цей екран сам веде до списку мереж.  */
  bool lostLink = (display.mode() == LOST);
  bool netReady = (network.status == CONNECTED || network.status == SDREADY) && !lostLink;
  if(!netReady){
    if(istouched){
      if(!wastouched) touchLongPress = millis();
    }else if(wastouched){
      uint32_t t = millis() - touchLongPress;
      /*  без мережі є сенс лише в Wi-Fi, як apScreen() у Nextion; при втраті
          зв'язку меню не замикаємо — мережа може повернутись сама  */
      if(t > 50) yomenu.openWifi(!lostLink);
    }
    wastouched = istouched;
    return;
  }
#endif

  if(istouched){
  #if TS_MODEL==TS_MODEL_XPT2046
    TSPoint p = ts.getPoint();
    touchX = map(p.x, TS_X_MIN, TS_X_MAX, 0, _width);
    touchY = map(p.y, TS_Y_MIN, TS_Y_MAX, 0, _height);
  #elif TS_MODEL==TS_MODEL_GT911 || TS_MODEL==TS_MODEL_FT6336
    TSPoint p = ts.points[0];
    touchX = p.x;
    touchY = p.y;
  #endif
    if(_inject){ touchX = _injX; touchY = _injY; }
  if (!wastouched) { /*     START TOUCH     */
      _oldTouchX = touchX;
      _oldTouchY = touchY;
      touchVol = touchX;
      touchStation = touchY;
      direct = TDS_REQUEST;
      touchLongPress=millis();
      _volSlide = (display.mode()==PLAYER && touchY >= TS_VOLBAR_Y);
      /*  Пульт картки: кнопки треків і смуга позиції. Діє лише на плеєрі
          й лише тоді, коли грає картка.  */
      _sdZone = -1;
      /*  Той самий пульт і для проповіді з сайту.  */
      if(!_volSlide && display.mode()==PLAYER && (config.getMode()==PM_SDCARD || display.sermonPult()) &&
         touchY >= TS_SD_Y0 && touchY < TS_SD_Y1){
        if(touchX < TS_SD_PREV_X)       { _sdZone = 0; display.sdPress(0, true); }
        else if(touchX >= TS_SD_NEXT_X) { _sdZone = 1; display.sdPress(1, true); }
        else                            { _sdZone = 2; _sdFrac = sdFracAt(touchX); display.sdPreview(_sdFrac); }   /* M4A теж: за таблицями stbl */
      }
      _plPrevY = touchY; _plVel = 0.0f;
      _plLastMs = millis();
      _plBtn = -1; _plTap = false; _plRepeated = false;
      if(display.mode()==STATIONS){
        if(touchX >= TS_PL_LIST_X){               /* стовпчик кнопок */
          int b = ((int)touchY - 3) / 58;
          if(b < 0) b = 0; if(b > 3) b = 3;
          _plBtn = b; _plBtnMs = millis(); _plRepMs = 0;
          display.plButton(_plBtn, true);
        }else{                                    /* сам список */
          _plTap = true; _plTapX = touchX; _plTapY = touchY; _plTapMs = millis();
          _plCaught = _plActive || _plAnim;       /* палець зупинив рух, що ще тривав */
          /*  Швидкий накат дотик лише зупиняє; пружину, що дотягує рядок, —
              ні: там список майже стоїть, і дотик має вибирати.  */
          _plTapFling = _plActive && fabsf(_plVel) > 3.0f;
          _plAnim = false;                        /* _plPos уже там, де зупинилось */
          _plPrevY = touchY;                      /* рахуємо зсув від пальця, без стрибка */
          if(!_plActive){ _plActive = true; if(!_plCaught) _plPos = display.currentPlItem; }
          _plVel = 0.0f;
          _plTapPos = _plPos;                     /* саме так список зараз намальовано */
          _plTarget = _plPos;
          _plHistN = 0; _plHist(touchY, millis());
        }
      }
    }
    /*  Список тягнеться за пальцем: накопичуємо зсув у пікселях і крокуємо
        по рядку, а не по кожному русі. Заразом рахуємо швидкість, щоб після
        відриву список ще трохи проїхав.  */
    /*  Кнопку тримають: ▲ і ▼ після паузи повторюються, щоб довгий список
        можна було прогорнути, не натискаючи щоразу.  */
    if(display.mode()==STATIONS && wastouched && _plBtn >= 0){
      uint32_t now = millis();
      if((_plBtn == PLB_UP_T || _plBtn == PLB_DOWN_T) && now - _plBtnMs > 420 && now - _plRepMs > 140){
        _plRepMs = now; _plRepeated = true; _plAction(_plBtn);
      }
      direct = TSD_STAY;
    }
    if(display.mode()==STATIONS && wastouched && _plBtn < 0){
      /*  Якщо жест устиг завершитись посеред ведення, поновлюємо його:
          інакше рух пальця міняв би швидкість, не вмикаючи накат, і після
          відпускання лишався б хвіст.  */
      /*  Поки палець не відійшов далі кількох пікселів, це дотик, а не
          прокрутка: список стоїть на місці. Раніше будь-яке тремтіння пальця
          вже рахувалось рухом, і дотик по станції майже ніколи її не вмикав.  */
      /*  Поки палець не відійшов на 14 пікселів, це дотик, а не прокрутка:
          FT6336 на відриві пальця часто «стрибає» на кілька пікселів, і з
          порогом 8 дотик по станції перетворювався на крихітну прокрутку.  */
      if(_plTap && abs((int)touchY - _plTapY) <= 14 && abs((int)touchX - _plTapX) <= 16){
        /* ще дотик */
      }else{
      if(_plTap){                               /* перший рух після дотику */
        _plTap = false;
        _plPrevY = touchY;                      /* без ривка на весь поріг */
        _plTarget = _plPos;
      }
      if(!_plActive){ _plActive = true; _plPos = display.currentPlItem; _plTarget = _plPos; _plVel = 0.0f; _plPrevY = touchY; }
      uint32_t now = millis();
      int16_t dy = (int16_t)touchY - _plPrevY;
      if(dy != 0){
        _plPrevY = touchY;
        _plTarget = _plClamp(_plTarget - dy / (float)TS_PL_ITEM_H);
        _plHist(touchY, now);
      }
      /*  Список іде за пальцем згладжено (половина відстані за такт у 10 мс):
          нерівні кроки сенсора не смикають рядки, а відставання непомітне.  */
      float d = _plTarget - _plPos;
      if(fabsf(d) > 0.002f){
        _plPos += d * 0.5f;
        display.plScrollTo(_plPos);               /* малює задача дисплея */
      }
      _plLastMs = now;
      }
      direct = TSD_STAY;                          /* свайп тут не потрібен */
    }
    /*  Смуга гучності працює як повзунки еквалайзера: значення береться
        просто з координати пальця, і оновлюється поки палець ведуть.  */
    if(_volSlide){
      /*  Краї липкі: сенсор біля рамки майже не видає крайніх координат,
          і по самій смузі (8..312) палець не діставав ні нуля, ні максимуму.
          Тепер усе лівіше 32 — нуль, усе правіше 288 — максимум.  */
      int v = map((int)touchX, 32, 288, 0, 254);
      if(v<0) v=0; if(v>254) v=254;
      _pendVol = v;
      /*  Слати команду на кожен рух пальця не можна: черга плеєра має п'ять
          місць, а xQueueSend чекає до секунди — головний цикл стає, звідси
          гличі й спрацювання сторожового таймера. Кожна команда ще й будить
          перемальовку та веб-інтерфейс. Тому не частіше ніж раз на 150 мс,
          а кінцеве значення обов'язково шлемо на відпусканні.
          Викликаємо саме setVol(): він зводить штатний таймер, який запише
          гучність у пам'ять один раз через три секунди після останньої зміни,
          а не на кожен дотик.  */
      uint32_t now = millis();
      if(v != _lastVol && now - _volSent >= 150){
        _lastVol = v; _volSent = now;
        player.setVol((uint8_t)v);
      }
    } else if (_sdZone >= 0) {
      /*  Смугою ведуть — показуємо, куди перемотає; саму перемотку робимо
          на відпусканні, щоб не смикати файл на кожен рух пальця.  */
      if(_sdZone == 2){ _sdFrac = sdFracAt(touchX); display.sdPreview(_sdFrac); }
      direct = TSD_STAY;
    } else if (wastouched && display.mode()!=STATIONS) { /*     SWIPE TOUCH     */
      /*  У списку свайп не діє: він перемикав станцію й наново відкривав
          сторінку прямо посеред прокрутки.  */
      direct = _tsDirection(touchX, touchY);
      switch (direct) {
        case TSD_LEFT:
        case TSD_RIGHT: {
            /*  Гучність жестом більше не змінюється — тільки повзунком унизу.
                Проведення пальцем упоперек екрана легко виходило випадково,
                і гучність стрибала сама собою.  */
            touchLongPress=millis();
            break;
          }
        case TSD_UP:
        case TSD_DOWN: {
            touchLongPress=millis();
            if(display.mode()==PLAYER || display.mode()==STATIONS){
              int16_t yDelta = map(abs(touchStation - touchY), 0, _height, 0, TS_STEPS);
              display.putRequest(NEWMODE, STATIONS);
              if (yDelta>1) {
                controlsEvent((touchStation - touchY)<0);
                touchStation = touchY;
              }
            }
            break;
          }
        default:
            break;
      }
    }
    if (config.store.dbgtouch) {
      Serial.print(", x = ");
      Serial.print(p.x);
      Serial.print(", y = ");
      Serial.println(p.y);
    }
  }else{
    if (wastouched) {/*     END TOUCH     */
      if(display.mode()==STATIONS){
        if(_plBtn >= 0){                          /* відпустили кнопку */
          display.plButton(_plBtn, false);
          if(!_plRepeated) _plAction(_plBtn);
          _plBtn = -1;
        }else if(_plTap && millis() - _plTapMs < 600){
          _plTap = false; _plVel = 0.0f;
          if(_plTapFling){                        /* дотик лише зупинив швидкий накат */
            _plSnap();
          }else{
            /*  Станція під пальцем — за тим, як список був намальований у мить
                дотику (він міг ще на частку рядка доїжджати пружиною), а не за
                номером у смузі: інакше біля межі рядків вибиралась сусідня.  */
            float centre = TS_PL_TOP + TS_PL_CUR * TS_PL_ITEM_H + TS_PL_ITEM_H / 2.0f;
            float item = roundf(_plTapPos + ((float)_plTapY - centre) / TS_PL_ITEM_H);
            item = _plClamp(item);
            bool band = fabsf((float)_plTapY - centre) < TS_PL_ITEM_H / 2.0f + 2.0f;
#ifdef YO_DEBUG
            Serial.printf("##PL#\tдотик: список=%.2f палець=%d станція=%.0f смуга=%d\n", _plTapPos, (int)_plTapY, item, band?1:0);
#endif
            _plActive = false;
            if(band && fabsf(item - _plTapPos) < 0.5f){
              display.plScrollStop(item);         /* рівно на ній — і грати */
              _plAction(PLB_PLAY_T);
            }else _plAnimateTo(item);
          }
        }else if(_plActive){
          _plPos = _plTarget;
          _plVel = _plFlingVel();
#ifdef YO_DEBUG
          Serial.printf("##PL#\tвідпустили: позиція=%.2f кидок=%.2f рядків/с точок=%u\n", _plPos, _plVel, (unsigned)_plHistN);
#endif
          if(fabsf(_plVel) < TS_PL_FLING_MIN){    /* повільно вели — дотягуємо пружиною */
            _plSnap();
          }else _plLastMs = millis();             /* пускаємо накат */
        }
        _plTap = false;
        direct = TSD_STAY;
        wastouched = istouched;
        return;
      }
      if(_sdZone >= 0){                           /* пульт картки */
        bool sm = display.sermonPult();
        if(_sdZone == 0){ display.sdPress(0, false); if(sm) sermons.playRel(-1); else player.prev(); }
        else if(_sdZone == 1){ display.sdPress(1, false); if(sm) sermons.playRel(1); else player.next(); }
        else if(_sdZone == 2){
          uint32_t dur = player.durSec();
          if(dur){
            /*  Проповідь — новий запит до сервера з потрібного байта;
                картка — перемотка у файлі.  */
            if(sm) player.burlSeek((uint32_t)(_sdFrac * dur));
            else   player.setAudioPlayPosition((uint16_t)(_sdFrac * dur));
          }
          display.sdPreview(-1.0f);
        }
        _sdZone = -1;
        direct = TSD_STAY;
        wastouched = istouched;
        return;
      }
      if(display.mode()==STATIONS && _plActive){
        _plPos = _plTarget;
        _plVel = _plFlingVel();
        if(fabsf(_plVel) < TS_PL_FLING_MIN){      /* повільно вели — дотягуємо пружиною */
          _plSnap();
        }else _plLastMs = millis();               /* пускаємо накат */
      }
      if(_volSlide){
        _volSlide = false;
        if(_pendVol >= 0 && _pendVol != _lastVol){    /* кінцеве значення */
          _lastVol = _pendVol; _volSent = millis();
          player.setVol((uint8_t)_pendVol);
        }
      }else if (direct == TDS_REQUEST) {
        uint32_t pressTicks = millis()-touchLongPress;
#ifdef USE_YOMENU
        /*  У шапці плеєра, як на сторінці плеєра Nextion: праворуч шестерня
            налаштувань, лівіше — список станцій (в оригіналі він відкривався
            дотиком по назві станції).  */
        if(display.mode()==PLAYER && _oldTouchY < TS_HEADER_H){
          /*  Між назвою станції і значком списку — кнопка джерела (радіо чи
              картка). Діє, лише коли її видно: без картки на радіо її немає.  */
          /*  Лаконічна шапка: праворуч одна кнопка «☰» — усе меню, решта
              шапки (назва станції) відкриває список, як і раніше.  */
          /*  Лівий кут — значок джерела: він же кнопка «радіо ↔ картка»,
              як просив власник (перемикач — на головному екрані).  */
          if(_oldTouchX >= 272)    yomenu.openHome();
          else if(_oldTouchX < 32){ if(!extras.s.noSd) config.changeMode(); }
          else                     display.putRequest(NEWMODE, STATIONS);
          direct = TSD_STAY;
          wastouched = istouched;
          return;
        }
        /*  Рядок обраного на головному: дотик по логотипу — одразу грати;
            по порожній клітинці — сторінка «обране», щоб її заповнити.  */
        if(display.mode()==PLAYER && display.favMainOn() && _oldTouchY >= FM_Y0 && _oldTouchY < FM_Y1){
          int i = ((int)_oldTouchX - 5) / 51;
          if(i < 0) i = 0;
          if(i > FAV_N - 1) i = FAV_N - 1;
          if(extras.fav[i].url[0]) extras.favPlay((uint8_t)i);
          else yomenu.openFav();
          direct = TSD_STAY;
          wastouched = istouched;
          return;
        }
#endif
        if( pressTicks < BTN_PRESS_TICKS*2){
          /*  Тривалість не фільтруємо, але не даємо одному дотику
              спрацювати двічі від тремтіння панелі.  */
          if(millis() - _lastTap > 250){ _lastTap = millis(); onBtnClick(EVT_BTNCENTER); }
        }else{
          display.putRequest(NEWMODE, display.mode() == PLAYER ? STATIONS : PLAYER);
        }
      }
      direct = TSD_STAY;
    }
  }
  wastouched = istouched;
}

/*  Плавно підвести список до рядка. Якщо підвід уже йде, починаємо з того
    місця, де він зараз, — тоді повтор ▲/▼ дає суцільний рух без ривків.  */
void TouchScreen::_plSnap(){
  float to = _plClamp(roundf(_plPos));
  _plActive = false; _plVel = 0.0f;
  if(fabsf(to - _plPos) < 0.02f){ _plPos = to; _plAnim = false; display.plScrollStop(to); return; }
  _plAnimFrom = _plPos; _plAnimTo = to; _plAnimT0 = millis(); _plAnim = true;
}

void TouchScreen::_plAnimateTo(float target){
  /*  Якщо список ще котився за інерцією, інерцію гасимо й підводимо з того
      місця, де він зараз, — інакше обидва рухи тягнули б його одночасно.  */
  float from = (_plAnim || _plActive) ? _plPos : (float)display.currentPlItem;
  _plActive = false; _plVel = 0.0f;
  _plAnimFrom = from; _plAnimTo = target; _plAnimT0 = millis();
  _plPos = from; _plAnim = true;
}

void TouchScreen::_plAction(int8_t b){
  switch(b){
    case PLB_UP_T:   { float base = _plAnim ? _plAnimTo : (_plActive ? roundf(_plPos) : (float)display.currentPlItem);
                       _plAnimateTo(_plClamp(base - 1)); break; }
    case PLB_DOWN_T: { float base = _plAnim ? _plAnimTo : (_plActive ? roundf(_plPos) : (float)display.currentPlItem);
                       _plAnimateTo(_plClamp(base + 1)); break; }
    case PLB_PLAY_T: {
      /*  Спершу доводимо рух до кінця, щоб грала саме та станція, що в смузі.  */
      if(_plAnim){ _plAnim = false; display.plScrollStop(_plAnimTo); }
      if(_plActive){ _plActive = false; _plVel = 0.0f; display.plScrollStop(_plClamp(roundf(_plPos))); }
      onBtnClick(EVT_BTNCENTER);                 /* як центральна кнопка: грати й на плеєр */
      break; }
    case PLB_BACK_T: display.putRequest(NEWMODE, PLAYER); break;
    default: break;
  }
}

/*  Швидкість кидка — за останні ~80 мс руху пальця, від найстаршої точки
    в цьому вікні до найновішої: один нерівний крок сенсора її вже не
    перекручує, як було з ковзним середнім.  */
void TouchScreen::_plHist(int16_t y, uint32_t t){
  if(_plHistN == 8){ for(uint8_t i = 0; i < 7; i++){ _plHistY[i] = _plHistY[i+1]; _plHistT[i] = _plHistT[i+1]; } _plHistN = 7; }
  _plHistY[_plHistN] = y; _plHistT[_plHistN] = t; _plHistN++;
}

float TouchScreen::_plFlingVel(){
  if(_plHistN < 2) return 0.0f;
  uint32_t now = millis();
  uint8_t last = _plHistN - 1;
  if(now - _plHistT[last] > 60) return 0.0f;         /* палець уже стояв — кидка нема */
  int8_t first = last;
  while(first > 0 && _plHistT[last] - _plHistT[first - 1] <= 80) first--;
  if(first == last) first = last - 1;
  float dt = (_plHistT[last] - _plHistT[first]) / 1000.0f;
  if(dt < 0.008f) return 0.0f;
  return ((_plHistY[last] - _plHistY[first]) / (float)TS_PL_ITEM_H) / dt;
}

bool TouchScreen::_checklpdelay(int m, uint32_t &tstamp) {
  if (millis() - tstamp > m) {
    tstamp = millis();
    return true;
  } else {
    return false;
  }
}

void TouchScreen::injectBegin(uint16_t x, uint16_t y){ _inject=true; _injTouched=true;  _injX=x; _injY=y; }
void TouchScreen::injectMove (uint16_t x, uint16_t y){ _inject=true; _injTouched=true;  _injX=x; _injY=y; }
/*  Знімаємо і сам прапорець підміни: інакше після відлагоджувального жесту
    справжня панель лишалася б відключеною до перезавантаження.  */
void TouchScreen::injectEnd  (){ _injTouched=false; _inject=false; }

void TouchScreen::dbgState(){
  Serial.printf("жест: активний=%d позиція=%.2f швидкість=%.2f режим=%d станція=%d\n",
                _plActive?1:0, _plPos, _plVel, (int)display.mode(), display.currentPlItem);
}

bool TouchScreen::_istouched(){
  if(_inject) return _injTouched;
#if TS_MODEL==TS_MODEL_XPT2046
  return ts.touched();
#elif TS_MODEL==TS_MODEL_GT911 || TS_MODEL==TS_MODEL_FT6336
  return ts.isTouched;
#endif
}

#endif  // TS_MODEL!=TS_MODEL_UNDEFINED
