#include "yoMenu.h"
#include "../extras/yoVersion.h"
#ifdef USE_YOMENU

#include "../core/config.h"
#include "../core/display.h"
#include "../core/player.h"
#include "../core/network.h"
#include "../core/timekeeper.h"
#include "../displays/dspcore.h"
#include "../displays/widgets/pages.h"
#include "../displays/tools/utf8Rus.h"
#include "../displays/fonts/yoUI9.h"
#include "../displays/fonts/yoUI9b.h"
#include "../displays/fonts/yoUI12b.h"
#include "../extras/yoExtras.h"
#include "../extras/yoSermons.h"
#include "../extras/yoRecorder.h"
#include "../extras/yoLogos.h"
#include "../displays/fonts/yoUI11.h"
#include "../displays/fonts/yoUI8.h"
#include <WiFi.h>

extern DspCore dsp;
YoMenu yomenu;

/*  Кольори зняті з Nextion: жовта шапка, темні панелі, білий текст.  */
#define C_BG    0x0000
#define C_PANEL 0x2124
#define C_PAN2  0x39E7
#define C_ACC   0xE68B
#define C_TXT   0xFFFF
#define C_DIM   0x8410
#define C_SIDE  0x18E3

#define SW      320
#define SH      240
#define HDR     28
#define SIDE    34
/*  Бічної колонки більше немає: налаштування — плитками, як головне меню,
    а сторінки на всю ширину з полями по 16 пікселів.  */
#define CX      16
#define CW      (SW-2*CX)

static const char* TITLES[7] = { "інформація", "еквалайзер", "Wi-Fi", "часовий пояс", "система",
                                 "будильник", "екран" };

/*  Ліва колонка: сім значків із кроком 30 — на п'ять колишніх по 41 уже
    не вистачало висоти.  */
#define SIDE_Y0    (HDR+1)
#define SIDE_STEP  30

/*  «Сон і будильник»: розкладка по вертикалі  */
#define SL_SEG_Y   52
#define AL_CHK_Y   96
#define AL_ROW_Y   120      /* лічильники годин і хвилин, висота 34 */
#define AL_DAYS_Y  164
/*  «Ніч і світло»  */
#define NT_BRI_Y   34       /* робоча яскравість */
#define NT_SAVE_Y  86       /* економія батареї, під підписом */
#define NT_CHK_Y   124
#define NT_ROW_Y   146      /* «з … до …», висота 30 */
#define NT_LVL_Y   188
#define SY_LED_Y   164      /* світлодіод — на сторінці «система» */

/*  Wi-Fi: знайдені мережі показує спільний список (plGenericDraw), той
    самий, що станції й проповіді.  */
/*  Wi-Fi: відомі мережі  */
#define WS_TOP     44
#define WS_ROW     32
#define WS_H       28

static const uint16_t SLEEP_MIN[5] = { 0, 15, 30, 60, 90 };
static const char* const SLEEP_LBL[5] = { "вимк", "15", "30", "60", "90" };
static const char* const DAYS_LBL[2]  = { "щодня", "будні" };
static const char* const LED_LBL[3]   = { "вимк", "стан", "музика" };
static const char* const SAVE_LBL[5]  = { "вимк", "10 с", "15 с", "30 с", "60 с" };

static WidgetConfig wc(uint16_t l, uint16_t t, WidgetAlign a=WA_LEFT){ WidgetConfig c; c.left=l; c.top=t; c.textsize=1; c.align=a; return c; }

/*  ---------- побудова сторінок ---------- */

void YoMenu::_build(){
  if(_built) return;
  for(uint8_t i=0;i<PG_N;i++) _pg[i] = new Page();

  /*  INFO: сім рядків «підпис / значення»  */
  /*  Тут же все, що прибрано з головного екрана: бітрейт, тиск і вологість,
      батарея цифрами.  */
  static const char* infoLbl[8] = { "версія", "мережа", "ip", "сигнал", "потік", "погода", "батарея", "пам'ять" };
  for(uint8_t i=0;i<8;i++){
    _info[i][0].init(wc(CX, 36+i*25), &yoUI9,  84, 16, C_DIM, C_BG);
    _info[i][1].init(wc(CX+86, 36+i*25), &yoUI9b, CW-86, 16, C_TXT, C_BG);
    _info[i][0].setText(infoLbl[i]);
    _pg[PG_INFO]->addWidget(&_info[i][0]);
    _pg[PG_INFO]->addWidget(&_info[i][1]);
  }

  /*  EQ: чотири повзунки -16..+16, як у Nextion  */
  static const char* eqLbl[4] = { "баланс", "високі", "середні", "низькі" };
  for(uint8_t i=0;i<4;i++){
    _eq[i].init(wc(CX, 44+i*44), &yoUI9, CW, -16, 16, C_TXT, C_BG, C_ACC);
    _eq[i].setLabel(eqLbl[i]);
    _pg[PG_EQ]->addWidget(&_eq[i]);
  }

  /*  WI-FI: п'ять слотів SSID / пароль  */
  for(uint8_t i=0;i<YOM_SSIDS;i++){
    _wifiS[i].init(wc(CX+14,  38+i*29), &yoUI9, 136, 22, C_TXT, C_PANEL);
    _wifiP[i].init(wc(CX+158, 38+i*29), &yoUI9, 106, 22, C_TXT, C_PANEL);
  }

  /*  TIMEZONE: поточний час і два лічильники  */
  _tmNow.init(wc(CX, 40, WA_CENTER), &yoUI12b, CW, 24, C_ACC, C_BG);
  _tmH.init(wc(CX+60,  110, WA_CENTER), &yoUI12b, 70, 30, C_TXT, C_PANEL);
  _tmM.init(wc(CX+160, 110, WA_CENTER), &yoUI12b, 70, 30, C_TXT, C_PANEL);
  _pg[PG_TIME]->addWidget(&_tmNow);
  _pg[PG_TIME]->addWidget(&_tmH);
  _pg[PG_TIME]->addWidget(&_tmM);

  /*  SYSTEM: два прапорці з Nextion, третій — перемикач джерела (моє
      доповнення), і яскравість, яка в Nextion робилась самою панеллю.  */
  _chkStart.init(wc(CX, 46),  &yoUI9, CW, C_TXT, C_BG, C_ACC); _chkStart.setLabel("автостарт");
  _chkInfo .init(wc(CX, 82),  &yoUI9, CW, C_TXT, C_BG, C_ACC); _chkInfo .setLabel("інфо про потік");
  _chkSrc  .init(wc(CX, 118), &yoUI9, CW, C_TXT, C_BG, C_ACC); _chkSrc  .setLabel("джерело: картка");
  _pg[PG_SYS]->addWidget(&_chkStart);
  _pg[PG_SYS]->addWidget(&_chkInfo);
  _pg[PG_SYS]->addWidget(&_chkSrc);

  /*  СОН І БУДИЛЬНИК  */
  _slStat.init(wc(CX+150, 32, WA_RIGHT), &yoUI9b, CW-150, 18, C_ACC, C_BG);
  _slSeg.init(wc(CX, SL_SEG_Y), &yoUI9b, CW, 30, C_TXT, C_BG, C_ACC, C_PAN2);
  _slSeg.setItems(5, SLEEP_LBL);
  _chkAlarm.init(wc(CX, AL_CHK_Y), &yoUI9, 110, C_TXT, C_BG, C_ACC); _chkAlarm.setLabel("будильник");
  _alStat.init(wc(CX+120, AL_CHK_Y-4, WA_RIGHT), &yoUI9, CW-120, 18, C_DIM, C_BG);
  _alH.init(wc(CX+38,  AL_ROW_Y, WA_CENTER), &yoUI12b, 48, 30, C_TXT, C_PANEL);
  _alM.init(wc(CX+178, AL_ROW_Y, WA_CENTER), &yoUI12b, 48, 30, C_TXT, C_PANEL);
  _alDays.init(wc(CX, AL_DAYS_Y), &yoUI9b, CW, 28, C_TXT, C_BG, C_ACC, C_PAN2);
  _alDays.setItems(2, DAYS_LBL);
  _alSta.init(wc(CX, 200), &yoUI9, CW, 20, C_DIM, C_BG);
  _pg[PG_SLEEP]->addWidget(&_slStat);
  _pg[PG_SLEEP]->addWidget(&_slSeg);
  _pg[PG_SLEEP]->addWidget(&_chkAlarm);
  _pg[PG_SLEEP]->addWidget(&_alStat);
  _pg[PG_SLEEP]->addWidget(&_alH);
  _pg[PG_SLEEP]->addWidget(&_alM);
  _pg[PG_SLEEP]->addWidget(&_alDays);
  _pg[PG_SLEEP]->addWidget(&_alSta);

  /*  ЕКРАН: денна яскравість, нічний режим «з … до …» і його яскравість,
      світлодіод. Усе, що стосується світла, — на одній сторінці.  */
  _bright.init(wc(CX, NT_BRI_Y), &yoUI9, CW, 5, 100, C_TXT, C_BG, C_ACC); _bright.setLabel("яскравість");
  _chkNight.init(wc(CX, NT_CHK_Y), &yoUI9, CW, C_TXT, C_BG, C_ACC); _chkNight.setLabel("нічний режим");
  _saveSeg.init(wc(CX, NT_SAVE_Y), &yoUI9b, CW, 26, C_TXT, C_BG, C_ACC, C_PAN2);
  _saveSeg.setItems(5, SAVE_LBL);
  _nFrom.init(wc(CX+34,  NT_ROW_Y, WA_CENTER), &yoUI11, 66, 26, C_TXT, C_PANEL);
  _nTo.init  (wc(CX+188, NT_ROW_Y, WA_CENTER), &yoUI11, 66, 26, C_TXT, C_PANEL);
  _nLevel.init(wc(CX, NT_LVL_Y), &yoUI9, CW, 0, 100, C_TXT, C_BG, C_ACC); _nLevel.setLabel("яскравість уночі");
  _pg[PG_NIGHT]->addWidget(&_bright);
  _pg[PG_NIGHT]->addWidget(&_saveSeg);
  _pg[PG_NIGHT]->addWidget(&_chkNight);
  _pg[PG_NIGHT]->addWidget(&_nFrom);
  _pg[PG_NIGHT]->addWidget(&_nTo);
  _pg[PG_NIGHT]->addWidget(&_nLevel);
  /*  світлодіод — на сторінці «система»  */
  _ledSeg.init(wc(CX, SY_LED_Y), &yoUI9b, CW, 30, C_TXT, C_BG, C_ACC, C_PAN2);
  _ledSeg.setItems(3, LED_LBL);
  _pg[PG_SYS]->addWidget(&_ledSeg);

  /*  РОЗРОБНИК: що показувати на екрані й чи працює картка  */
  _chkBat .init(wc(CX, 40),  &yoUI9, CW, C_TXT, C_BG, C_ACC); _chkBat .setLabel("батарея");
  _chkIp  .init(wc(CX, 70),  &yoUI9, CW, C_TXT, C_BG, C_ACC); _chkIp  .setLabel("IP-адреса на екрані");
  _chkSdEn.init(wc(CX, 100), &yoUI9, CW, C_TXT, C_BG, C_ACC); _chkSdEn.setLabel("картка пам'яті");
  _pg[PG_DEV]->addWidget(&_chkBat);
  _pg[PG_DEV]->addWidget(&_chkIp);
  _pg[PG_DEV]->addWidget(&_chkSdEn);

  /*  Клавіатура: єдиний віджет — рядок вводу. Самі клавіші статичні,
      їх досить намалювати один раз при відкритті.  */
  _kbdField.init(wc(8, 32), &yoUI12b, SW-16, 26, C_TXT, C_PANEL);
  _pg[PG_KBD]->addWidget(&_kbdField);

  _built = true;
}

/*  ---------- каркас сторінки ---------- */

void YoMenu::_sidebar(){
  dsp.fillRect(0, HDR, SIDE, SH-HDR, C_SIDE);
  for(uint8_t i=0;i<NSIDE;i++){
    int16_t y = SIDE_Y0 + i*SIDE_STEP;
    bool on = (_cur == (int8_t)i);
    uint16_t pb = on ? C_PAN2 : C_SIDE;          /* тло плашки — для вирізів у значках */
    if(on) dsp.fillRect(0, y, SIDE, SIDE_STEP-1, C_PAN2);
    uint16_t c = on ? C_ACC : C_DIM;
    int16_t x = 8, iy = y + 5;
    switch(i){
      case 0: dsp.drawCircle(x+9, iy+9, 9, c); dsp.drawCircle(x+9, iy+9, 8, c);
              dsp.fillRect(x+8, iy+7, 3, 8, c); dsp.fillRect(x+8, iy+3, 3, 3, c); break;
      case 1: dsp.fillRect(x+2, iy+11, 4, 7, c); dsp.fillRect(x+8, iy+5, 4, 13, c);
              dsp.fillRect(x+14, iy+1, 4, 17, c); break;
      case 2: for(int r=4;r<=12;r+=4) for(int a=-140;a<=-40;a+=4){
                float rad=a*3.14159f/180.0f;
                dsp.drawPixel(x+9+(int)(r*cosf(rad)), iy+16+(int)(r*sinf(rad)), c); }
              dsp.fillCircle(x+9, iy+16, 2, c); break;
      case 3: dsp.drawCircle(x+9, iy+9, 9, c); dsp.drawCircle(x+9, iy+9, 8, c);
              dsp.drawLine(x+9, iy+9, x+9, iy+4, c); dsp.drawLine(x+9, iy+9, x+13, iy+11, c); break;
      case 4: dsp.drawCircle(x+9, iy+9, 6, c); dsp.drawCircle(x+9, iy+9, 5, c);
              dsp.fillRect(x+8, iy+0, 3, 4, c); dsp.fillRect(x+8, iy+14, 3, 4, c);
              dsp.fillRect(x+0, iy+8, 4, 3, c); dsp.fillRect(x+14, iy+8, 4, 3, c); break;
      case 5: /* місяць — сон і будильник */
              dsp.fillCircle(x+9, iy+9, 8, c); dsp.fillCircle(x+13, iy+5, 7, pb); break;
      default: /* лампочка — ніч і світло */
              dsp.drawCircle(x+9, iy+7, 6, c); dsp.drawCircle(x+9, iy+7, 5, c);
              dsp.fillRect(x+6, iy+13, 7, 2, c); dsp.fillRect(x+6, iy+16, 7, 2, c); break;
    }
  }
  dsp.fillRect(SIDE, HDR, 1, SH-HDR, C_PAN2);
}

/*  П'ятикутна зірка з десяти трикутників від центру.  */
static void drawStar(int16_t cx, int16_t cy, int16_t R, uint16_t c){
  int16_t px[10], py[10];
  for(uint8_t i=0;i<10;i++){
    float a = -1.5708f + i * 0.62832f;
    float r = (i % 2) ? R * 0.42f : R;
    px[i] = cx + (int16_t)lroundf(r * cosf(a));
    py[i] = cy + (int16_t)lroundf(r * sinf(a));
  }
  for(uint8_t i=0;i<10;i++){ uint8_t j = (i+1) % 10; dsp.fillTriangle(cx, cy, px[i], py[i], px[j], py[j], c); }
}

void YoMenu::_chrome(const char* title, uint8_t icon){
  dsp.fillRect(0, 0, SW, HDR, C_ACC);
  if(icon == 1) drawStar(14, 15, 10, C_BG);
  else if(icon == 2){ for(uint8_t i=0;i<3;i++) dsp.fillRect(5, 7 + i*6, 18, 3, C_BG); }
  else{
    dsp.drawCircle(14, 14, 6, C_BG); dsp.drawCircle(14, 14, 5, C_BG);
    dsp.fillRect(13, 5, 3, 4, C_BG); dsp.fillRect(13, 19, 3, 4, C_BG);
    dsp.fillRect(5, 13, 4, 3, C_BG); dsp.fillRect(19, 13, 4, 3, C_BG);
  }
  dsp.setFont(&yoUI12b); dsp.setTextSize(1); dsp.setTextColor(C_BG);
  dsp.setCursor(34, 20); dsp.print(utf8Rus(title, true));
  _titleEnd = dsp.getCursorX();              /* підказку в шапці ставимо лише правіше */
  dsp.setFont();
  /*  У режимі точки доступу виходу з налаштувань немає, доки мережу не
      задано, тож і стрілку не малюємо: намальована, але мертва кнопка
      виглядає поламаною.  */
  if(!(_apLock && _cur != PG_KBD && _cur != PG_WSAVED)){
    dsp.fillTriangle(SW-26, 14, SW-16, 8, SW-16, 20, C_BG);
    dsp.fillRect(SW-16, 12, 8, 5, C_BG);
  }
}

/*  ---------- перемикання сторінок ---------- */

/*  Перемикання сторінки лише запам'ятовується. Саме малювання — і наплив,
    і сама сторінка — виконує задача дисплея: у головному циклі, звідки
    приходить дотик, крутиться декодер звуку, і пауза там чутна.  */
void YoMenu::_show(int8_t p){
  _lastAct = millis();
  /*  Пошук мереж і спроби повернутися в мережу живуть в одному радіомодулі:
      поки людина вибирає мережу, спроби спинено — інакше ні пошуку, ні
      підключення не діждешся.  */
  network.pauseSta(p == PG_WIFI || p == PG_WSAVED ||
                   (p == PG_KBD && (_kbdBack == PG_WIFI || _kbdBack == PG_WSAVED)));
  if(_cur >= 0 && _pg[_cur]) _pg[_cur]->setActive(false);
  _cur = p;
  _fadeStep = 0;
  _fadeTick = 0;
}

/*  Плавна зміна: гасимо підсвітку, міняємо картинку вже в темряві й
    засвічуємо назад. Перемальовки не видно зовсім — жодних смуг і ривків.
    Усе відбувається в задачі дисплея, тож звук не переривається.  */
void YoMenu::_fade(){
  uint32_t now = millis();
  if(now - _fadeTick < 12) return;
  _fadeTick = now;
  const int8_t STEPS = 8;
#if BRIGHTNESS_PIN!=255
  uint16_t full = extras.pwmTarget();          /* уночі — нічна, не денна */
#else
  uint16_t full = 255;
#endif
  if(_fadeStep < STEPS){                       /* гасимо */
#if BRIGHTNESS_PIN!=255
    if(config.store.dspon) analogWrite(BRIGHTNESS_PIN, full - full*(_fadeStep+1)/STEPS);
#endif
    if(++_fadeStep == STEPS){
      if(_closeReq){                           /* вихід із меню — теж у темряві */
        _closeReq = false;
        network.pauseSta(false);
        if(_cur >= 0 && _pg[_cur]) _pg[_cur]->setActive(false);
        _cur = PG_OFF;
        dsp.setFont();
        display.forceRedraw();
        /*  «Станції» з меню: список малюємо ще в темряві, тож перехід один,
            а не два поспіль (меню → плеєр → список).  */
        if(_afterClose == 1) display.openStationsNow();
        _afterClose = 0;
      }else _paint();
    }
  }else{                                       /* засвічуємо */
    int8_t k = _fadeStep - STEPS + 1;
#if BRIGHTNESS_PIN!=255
    if(config.store.dspon) analogWrite(BRIGHTNESS_PIN, full*k/STEPS);
#endif
    if(++_fadeStep >= 2*STEPS){
      _fadeStep = -1;
#if BRIGHTNESS_PIN!=255
      if(config.store.dspon) extras.pwmSet(full);
#endif
    }
  }
}

void YoMenu::_paint(){
  int8_t p = _cur;
  if(p == PG_OFF) return;
  /*  Чистимо екран саме тут: підсвітка в цю мить погашена, тож повної
      перемальовки не видно зовсім. Раніше замість цього був наплив смугами,
      який заразом і стирав попередню картинку.  */
  dsp.fillScreen(C_BG);
  if(p == PG_KBD){
    _chrome(_kbdTitle);
    _drawKbdKeys();
  }else if(p == PG_HOME){
    _chrome("меню", 2);
    _drawHome();
    _msgDirty = false;
  }else if(p == PG_SETUP){
    _chrome("параметри", 0);
    _drawSetup();
  }else if(p == PG_POWER){
    _chrome("живлення", 0);
    _pwrArm = -1; _pwrGo = -1;
    _drawPower();
  }else if(p == PG_WSAVED){
    _chrome("відомі мережі", 2);
    _wsArm = -1;
    _drawSaved();
    _favDirty = false;
  }else if(p == PG_DEV){
    _chrome("розробник", 0);
    _chkBat.setValue(!extras.s.noBat);
    _chkIp.setValue(!extras.s.noIp);
    _chkSdEn.setValue(!extras.s.noSd);
    _drawDevBtn();
  }else if(p == PG_DAC){
    _chrome("аудіовихід", 0);
    _drawDacList();
  }else if(p == PG_DACINFO){
    static const char* nm[5] = { "ES8311", "PCM5102A", "UDA1334A", "MAX98357A", "VS1053B" };
    _chrome(nm[_dacSel], 0);
    _drawDacInfo();
  }else if(p == PG_WIFI){
    /*  Мережі — тим самим списком, що станції й проповіді: «лупа» на
        вибраному рядку, кнопки ▲ ▼ ▶ ↶ збоку, прокрутка пальцем.  */
    _smSel = 1; _smCur = 1.0f; _smShift = 0; _smMqW = 0; _smMqT = millis() + 600;
    _smAnim = false; _smFling = false; _dActive = false; _smHold = -1;
    plGenericChrome();
    _smDraw(false);
    _smDirty = false;
    if(!_scanning) _wifiScan();
  }else if(p == PG_SERM){
    /*  Проповіді — точнісінько як список станцій: той самий код малювання  */
    _smVer = sermons.version(); _smLoad = sermons.loading();
    _smShift = 0; _smMqT = millis() + 600; _smMqW = 0;
    _smCur = _smSel; _smAnim = false; _smFling = false; _dActive = false; _smHold = -1;
    plGenericChrome();
    _smDraw(false);
    _smDirty = false;
  }else if(p == PG_FAV){
    _chrome("обране", 1);
    _drawFav();
    _favDirty = _msgDirty = false;
  }else{
    /*  зі сторінок головного меню — «☰», з параметрів — шестерня  */
    _chrome(TITLES[p], _parent(p) == PG_SETUP ? 0 : 2);
    if(p == PG_TIME){
      for(uint8_t i=0;i<2;i++){
        int16_t x = CX+60 + i*100;
        dsp.fillTriangle(x+35, 74, x+20, 96, x+50, 96, C_ACC);    /* ▲ */
        dsp.fillTriangle(x+35, 178, x+20, 156, x+50, 156, C_ACC); /* ▼ */
        dsp.fillRect(x, 110, 70, 30, C_PANEL);
      }
      dsp.setFont(&yoUI9); dsp.setTextColor(C_DIM);
      dsp.setCursor(CX+72, 210); dsp.print(utf8Rus("години", false));
      dsp.setCursor(CX+170, 210); dsp.print(utf8Rus("хвилини", false)); dsp.setFont();
    }
    if(p == PG_SLEEP){
      dsp.setFont(&yoUI9); dsp.setTextColor(C_DIM);
      dsp.setCursor(CX, 46); dsp.print(utf8Rus("таймер сну, хв", false));
      dsp.setFont();
      /*  [−] гг [+]  :  [−] хх [+]  */
      _stepper(CX,     AL_ROW_Y, 34, 34, false);
      dsp.fillRect(CX+38, AL_ROW_Y, 48, 34, C_PANEL);
      _stepper(CX+90,  AL_ROW_Y, 34, 34, true);
      dsp.fillRect(CX+130, AL_ROW_Y+10, 4, 4, C_TXT);
      dsp.fillRect(CX+130, AL_ROW_Y+21, 4, 4, C_TXT);
      _stepper(CX+140, AL_ROW_Y, 34, 34, false);
      dsp.fillRect(CX+178, AL_ROW_Y, 48, 34, C_PANEL);
      _stepper(CX+230, AL_ROW_Y, 34, 34, true);
    }
    if(p == PG_NIGHT){
      dsp.setFont(&yoUI9); dsp.setTextSize(1); dsp.setTextColor(C_DIM);
      dsp.setCursor(CX, NT_SAVE_Y-6); dsp.print(utf8Rus("без зарядника пригасити через", false));
      dsp.setFont();
      /*  два однакові блоки часу з тире між ними: [−][22:00][+] — [−][07:00][+]  */
      _stepper(CX,     NT_ROW_Y, 32, 30, false);
      dsp.fillRect(CX+34,  NT_ROW_Y, 66, 30, C_PANEL);
      _stepper(CX+102, NT_ROW_Y, 32, 30, true);
      dsp.fillRect(CX+139, NT_ROW_Y+14, 10, 3, C_DIM);
      _stepper(CX+154, NT_ROW_Y, 32, 30, false);
      dsp.fillRect(CX+188, NT_ROW_Y, 66, 30, C_PANEL);
      _stepper(CX+256, NT_ROW_Y, 32, 30, true);
    }
    if(p == PG_SYS){
      dsp.setFont(&yoUI9); dsp.setTextSize(1); dsp.setTextColor(C_DIM);
      dsp.setCursor(CX, SY_LED_Y-6); dsp.print(utf8Rus("світлодіод", false));
      dsp.setFont();
    }
  }
  /*  Значення заповнюємо ДО активації: віджет ще не малює, а коли Page його
      увімкне — він намалює себе вже з правильним вмістом, одним проходом.  */
  char b[48];
  if(p == PG_INFO){
    { char v[32]; snprintf(v, sizeof(v), "%s, %.10s", prVersion(), prBuild()); _info[0][1].setText(v); }
    _infoLive();
  }else if(p == PG_EQ){
    _eq[0].setValue(config.store.balance);
    _eq[1].setValue(config.store.trebble);
    _eq[2].setValue(config.store.middle);
    _eq[3].setValue(config.store.bass);
  }else if(p == PG_TIME){
    snprintf(b, sizeof(b), "%02d", config.store.tzHour); _tmH.setText(b);
    snprintf(b, sizeof(b), "%02d", config.store.tzMin); _tmM.setText(b);
    strftime(b, sizeof(b), "%H:%M:%S", &network.timeinfo); _tmNow.setText(b);
  }else if(p == PG_SYS){
    _syncSys();
    _ledSeg.setSel(extras.s.ledMode);
  }else if(p == PG_SLEEP || p == PG_NIGHT){
    _syncExtras();
  }
  if(_pg[p]) _pg[p]->setActive(true);          /* віджети малюють себе самі */
}

/*  Кнопка лічильника: темна плашка з мінусом або плюсом.  */
void YoMenu::_stepper(int16_t x, int16_t y, int16_t w, int16_t h, bool plus){
  dsp.fillRect(x, y, w, h, C_PAN2);
  int16_t cx = x + w/2, cy = y + h/2;
  dsp.fillRect(cx-6, cy-1, 13, 3, C_ACC);
  if(plus) dsp.fillRect(cx-1, cy-6, 3, 13, C_ACC);
}

/*  Назву станції обрізаємо по літерах, а не по байтах: кирилиця в UTF-8
    займає два байти, і різати посеред літери не можна.  */
static void cutUtf8(char* s, uint8_t maxChars){
  uint8_t n = 0;
  for(char* q = s; *q; q++){
    if(((uint8_t)*q & 0xC0) != 0x80){ if(n == maxChars){ *q = '\0'; return; } n++; }
  }
}

void YoMenu::_syncExtras(){
  char b[64];
  /*  сон  */
  uint16_t sm = extras.sleepMinutes();
  int8_t si = -1;
  for(uint8_t i=0;i<5;i++) if(SLEEP_MIN[i] == sm) si = i;
  _slSeg.setSel(si);
  if(sm == 0) snprintf(b, sizeof(b), "вимкнено");
  else{ uint32_t s = extras.sleepLeftSec(); snprintf(b, sizeof(b), "ще %u:%02u", (unsigned)(s/60), (unsigned)(s%60)); }
  _slStat.setText(b);
  /*  будильник  */
  _chkAlarm.setValue(extras.s.alarmOn);
  snprintf(b, sizeof(b), "%02u", extras.s.alarmH); _alH.setText(b);
  snprintf(b, sizeof(b), "%02u", extras.s.alarmM); _alM.setText(b);
  _alDays.setSel(extras.s.alarmDays);
  int32_t m = extras.alarmInMin();
  if(!extras.s.alarmOn) snprintf(b, sizeof(b), "вимкнено");
  else if(m < 0)        snprintf(b, sizeof(b), "час ще не відомий");
  else                  snprintf(b, sizeof(b), "через %d год %02d хв", (int)(m/60), (int)(m%60));
  _alStat.setText(b);
  if(config.getMode() == PM_WEB && config.station.name[0]){
    char nm[64]; strlcpy(nm, config.station.name, sizeof(nm)); cutUtf8(nm, 36);
    snprintf(b, sizeof(b), "заграє: %s", nm);
  }else snprintf(b, sizeof(b), "заграє остання радіостанція");
  _alSta.setText(b);
  /*  ніч і світло  */
  _chkNight.setValue(extras.s.nightOn);
  snprintf(b, sizeof(b), "%02u:%02u", extras.s.nightFrom/2, (extras.s.nightFrom%2)*30); _nFrom.setText(b);
  snprintf(b, sizeof(b), "%02u:%02u", extras.s.nightTo/2,   (extras.s.nightTo%2)*30);   _nTo.setText(b);
  _bright.setValue(config.store.brightness);
  _nLevel.setValue(extras.s.nightLevel);
  _ledSeg.setSel(extras.s.ledMode);
  _saveSeg.setSel(extras.s.batSave);
  uint16_t mv = extras.batMv();
  if(mv == 0)           snprintf(b, sizeof(b), "батарея: вимірюю...");
  else if(mv < 2800)    snprintf(b, sizeof(b), "батарею не знайдено");
  else if(extras.onUsb()) snprintf(b, sizeof(b), "живлення від USB, %u.%02u В", mv/1000, (mv%1000)/10);
  else                  snprintf(b, sizeof(b), "батарея %d%%, %u.%02u В", extras.batPct(), mv/1000, (mv%1000)/10);
  _batTxt.setText(b);
}

/*  ---------- обране, проповіді, запис ---------- */

/*  Плитки обраного: два стовпці по три, під ними дві великі кнопки.  */
#define FV_X(i)   (6 + ((i) % 2) * 157)
#define FV_Y(i)   (34 + ((i) / 2) * 62)
#define FV_W      151
#define FV_H      58
#define FV_BOT_Y  176
#define FV_BOT_H  56
#define FV_CHK_X  182       /* прапорець «на головному» внизу сторінки */
#define SM_ROW_H  66
#define SM_TOP    32
#define SM_ROWS   3
#define SM_LIST_W 258

/*  Рядок у перекодуванні шрифту, що влазить у w пікселів; інакше з «..».
    Шрифт має бути вже вибраний.  */
static void fitText(char* t, uint16_t w){
  int16_t x1, y1; uint16_t tw, th;
  dsp.getTextBounds(t, 0, 40, &x1, &y1, &tw, &th);
  if(tw <= w) return;
  size_t n = strlen(t);
  char tmp[72];
  while(n > 1){
    t[--n] = 0;
    while(n && t[n-1] == ' ') t[--n] = 0;
    snprintf(tmp, sizeof(tmp), "%s..", t);
    dsp.getTextBounds(tmp, 0, 40, &x1, &y1, &tw, &th);
    if(tw <= w){ strlcpy(t, tmp, 72); return; }
  }
}

static uint16_t textW(const char* t){
  int16_t x1, y1; uint16_t tw, th;
  dsp.getTextBounds(t, 0, 40, &x1, &y1, &tw, &th);
  return tw;
}

void YoMenu::_setMsg(const char* m){
  strlcpy(_msg, m, sizeof(_msg));
  _msgUntil = millis() + 3500;
  _msgDirty = true;
}

/*  Права частина жовтої шапки: підказка або відповідь на дотик.  */
void YoMenu::_drawHdrMsg(){
  int16_t x0 = _titleEnd + 8;
  if(x0 > SW-60) return;
  dsp.fillRect(x0, 0, SW-40-x0, HDR, C_ACC);
  char t[72];
  if(_cur == PG_FAV){
    snprintf(t, sizeof(t), "%s", utf8Rus(_msg, false));   /* підказка — під плитками */
  }else if(_cur == PG_HOME){
    snprintf(t, sizeof(t), "%s", utf8Rus(_msg, false));
  }else{
    uint16_t n = sermons.count();
    if(n) snprintf(t, sizeof(t), "%u-%u / %u", _smTop + 1, (_smTop + SM_ROWS < n ? _smTop + SM_ROWS : n), n);
    else t[0] = 0;
  }
  dsp.setFont(&yoUI9); dsp.setTextSize(1); dsp.setTextColor(C_BG);
  fitText(t, SW-44-x0);
  dsp.setCursor(SW-44 - textW(t), 19); dsp.print(t);
  dsp.setFont();
}

void YoMenu::_drawTile(uint8_t i){
  int16_t x = FV_X(i), y = FV_Y(i);
  const FavItem& f = extras.fav[i];
  if(!f.url[0]){
    dsp.fillRect(x, y, FV_W, FV_H, C_BG);
    dsp.drawRect(x, y, FV_W, FV_H, C_PAN2);
    /*  «+» і підказка в два рядки: в один не влазило  */
    int16_t cx = x + FV_W / 2;
    dsp.fillRect(cx - 7, y + 13, 15, 3, C_DIM); dsp.fillRect(cx - 1, y + 7, 3, 15, C_DIM);
    dsp.setFont(&yoUI8); dsp.setTextSize(1); dsp.setTextColor(C_DIM);
    char t[40]; snprintf(t, sizeof(t), "%s", utf8Rus("додати станцію,", false));
    dsp.setCursor(cx - (int16_t)textW(t) / 2, y + 36); dsp.print(t);
    snprintf(t, sizeof(t), "%s", utf8Rus("що грає", false));
    dsp.setCursor(cx - (int16_t)textW(t) / 2, y + 51); dsp.print(t);
    dsp.setFont();
    return;
  }
  bool on = extras.favPlaying() == (int8_t)i;
  dsp.fillRect(x, y, FV_W, FV_H, C_PANEL);
  if(on){ dsp.drawRect(x, y, FV_W, FV_H, C_ACC); dsp.drawRect(x+1, y+1, FV_W-2, FV_H-2, C_ACC); }
  dsp.setFont(&yoUI11); dsp.setTextSize(1); dsp.setTextColor(on ? C_ACC : C_TXT);
  char t[72]; snprintf(t, sizeof(t), "%s", utf8Rus(f.name, false));
  const uint16_t W = FV_W - 10;
  if(textW(t) <= W){
    dsp.setCursor(x + (FV_W - (int16_t)textW(t)) / 2, y + FV_H/2 + 5); dsp.print(t);
  }else{
    /*  Два рядки: ділимо на пробілі, найближчому до середини, з якого
        перша половина ще влазить.  */
    size_t n = strlen(t); int cut = -1;
    for(size_t k = 1; k < n; k++){
      if(t[k] != ' ') continue;
      char a[72]; memcpy(a, t, k); a[k] = 0;
      if(textW(a) <= W && (cut < 0 || abs((int)k - (int)n/2) < abs(cut - (int)n/2))) cut = k;
    }
    char a[72], b[72];
    if(cut > 0){ memcpy(a, t, cut); a[cut] = 0; strlcpy(b, t + cut + 1, sizeof(b)); }
    else{ strlcpy(a, t, sizeof(a)); fitText(a, W); b[0] = 0; }
    fitText(b, W);
    dsp.setCursor(x + (FV_W - (int16_t)textW(a)) / 2, y + 24); dsp.print(a);
    dsp.setCursor(x + (FV_W - (int16_t)textW(b)) / 2, y + 43); dsp.print(b);
  }
  dsp.setFont();
}

void YoMenu::_drawBottom(){
  int16_t y = FV_BOT_Y;
  char t[48];
  /*  проповіді: хрест і підпис  */
  dsp.fillRect(6, y, FV_W, FV_BOT_H, C_PAN2);
  dsp.fillRect(20, y+12, 4, 32, C_ACC);
  dsp.fillRect(12, y+20, 20, 4, C_ACC);
  dsp.setFont(&yoUI12b); dsp.setTextSize(1); dsp.setTextColor(C_TXT);
  snprintf(t, sizeof(t), "%s", utf8Rus("проповіді", false));
  dsp.setCursor(42, y+25); dsp.print(t);
  dsp.setFont(&yoUI9); dsp.setTextColor(C_DIM);
  snprintf(t, sizeof(t), "%s", utf8Rus("з сайту церкви", false));
  dsp.setCursor(42, y+44); dsp.print(t);
  /*  запис ефіру  */
  int16_t x = 6 + 157;
  bool rec = recorder.active();
  dsp.fillRect(x, y, FV_W, FV_BOT_H, rec ? 0x8000 : C_PAN2);
  if(rec) dsp.fillRect(x+12, y+14, 16, 16, C_TXT);
  else    dsp.fillCircle(x+20, y+22, 8, 0xF800);
  dsp.setFont(&yoUI12b); dsp.setTextColor(C_TXT);
  snprintf(t, sizeof(t), "%s", utf8Rus(rec ? "зупинити" : "запис ефіру", false));
  dsp.setCursor(x+36, y+27); dsp.print(t);
  dsp.setFont(&yoUI9); dsp.setTextColor(rec ? C_TXT : C_DIM);
  if(rec){
    uint32_t s = recorder.seconds();
    snprintf(t, sizeof(t), "%u:%02u, %.1f %s", (unsigned)(s/60), (unsigned)(s%60), recorder.bytes() / 1048576.0f, utf8Rus("МБ", false));
  }else snprintf(t, sizeof(t), "%s", utf8Rus(config.getMode() == PM_WEB ? "на картку пам'яті" : "лише для радіо", false));
  dsp.setCursor(x+12, y+46); dsp.print(t);
  dsp.setFont();
}

void YoMenu::_drawFav(){
  _drawHdrMsg();
  for(uint8_t i=0;i<FAV_N;i++) _drawTile(i);
  /*  Підказка окремим рядком унизу: у шапці після назви вона не влазила
      жодним шрифтом.  */
  dsp.fillRect(0, 220, SW, 20, C_BG);
  dsp.setFont(&yoUI8); dsp.setTextSize(1); dsp.setTextColor(C_DIM);
  char t[64]; snprintf(t, sizeof(t), "%s", utf8Rus("утримати - прибрати", false));
  dsp.setCursor(6, 234); dsp.print(t);
  /*  праворуч — чи показувати обране рядком логотипів на головному екрані  */
  bool on = !extras.s.favHide;
  dsp.drawRect(FV_CHK_X, 223, 13, 13, on ? C_ACC : C_DIM);
  if(on) dsp.fillRect(FV_CHK_X + 3, 226, 7, 7, C_ACC);
  dsp.setTextColor(on ? C_TXT : C_DIM);
  snprintf(t, sizeof(t), "%s", utf8Rus("на головному", false));
  dsp.setCursor(FV_CHK_X + 19, 234); dsp.print(t);
  dsp.setFont();
}

void YoMenu::_drawSerm(){
  _drawHdrMsg();
  dsp.fillRect(0, HDR, SM_LIST_W+4, SH-HDR, C_BG);
  uint16_t n = sermons.count();
  /*  кнопки гортання праворуч, як у списку станцій  */
  bool up = _smTop > 0, dn = _smTop + SM_ROWS < n;
  dsp.fillRect(266, SM_TOP, 48, 98, C_PAN2);
  dsp.fillTriangle(290, SM_TOP+34, 276, SM_TOP+60, 304, SM_TOP+60, up ? C_ACC : C_PANEL);
  dsp.fillRect(266, SM_TOP+102, 48, 98, C_PAN2);
  dsp.fillTriangle(290, SM_TOP+166, 276, SM_TOP+140, 304, SM_TOP+140, dn ? C_ACC : C_PANEL);

  char t[72];
  if(sermons.loading() || n == 0){
    dsp.setFont(&yoUI11); dsp.setTextSize(1); dsp.setTextColor(sermons.loading() ? C_TXT : C_ACC);
    snprintf(t, sizeof(t), "%s", utf8Rus(sermons.loading() ? "завантажую з сайту..." : (sermons.error()[0] ? sermons.error() : "список порожній"), false));
    dsp.setCursor((SM_LIST_W - (int16_t)textW(t)) / 2, 120); dsp.print(t);
    if(!sermons.loading()){
      dsp.setFont(&yoUI9); dsp.setTextColor(C_DIM);
      snprintf(t, sizeof(t), "%s", utf8Rus("торкніться, щоб спробувати ще", false));
      dsp.setCursor((SM_LIST_W - (int16_t)textW(t)) / 2, 142); dsp.print(t);
    }
    dsp.setFont();
    return;
  }
  for(uint8_t r=0;r<SM_ROWS;r++){
    uint16_t idx = _smTop + r;
    if(idx >= n) break;
    const Sermon* s = sermons.at(idx);
    int16_t y = SM_TOP + r * SM_ROW_H;
    bool on = sermons.playing() == (int16_t)idx && player.remoteStationName && player.status() == PLAYING;
    /*  Назва — до двох рядків: ділимо на пробілі, з якого перший рядок ще
        влазить; що не влізло в другий — обрізаємо з «..».  */
    dsp.setFont(&yoUI9b); dsp.setTextSize(1); dsp.setTextColor(on ? C_ACC : C_TXT);
    snprintf(t, sizeof(t), "%s", utf8Rus(s->title, false));
    const uint16_t TW = SM_LIST_W - 8;
    if(textW(t) <= TW){
      dsp.setCursor(6, y + 17); dsp.print(t);
    }else{
      size_t n = strlen(t); int cut = -1;
      for(size_t k = 1; k < n; k++){
        if(t[k] != ' ') continue;
        char a[72]; memcpy(a, t, k); a[k] = 0;
        if(textW(a) <= TW) cut = k;
      }
      char a[72], b[72];
      if(cut > 0){ memcpy(a, t, cut); a[cut] = 0; strlcpy(b, t + cut + 1, sizeof(b)); }
      else{ strlcpy(a, t, sizeof(a)); fitText(a, TW); b[0] = 0; }
      fitText(b, TW);
      dsp.setCursor(6, y + 17); dsp.print(a);
      dsp.setCursor(6, y + 34); dsp.print(b);
    }
    /*  третій рядок: хто і коли, праворуч — тривалість  */
    dsp.setFont(&yoUI9);
    char d[24];
    if(strlen(s->date) >= 10) snprintf(d, sizeof(d), "%.2s.%.2s, %u %s", s->date + 8, s->date + 5, (unsigned)((s->dur + 30) / 60), utf8Rus("хв", false));
    else snprintf(d, sizeof(d), "%u %s", (unsigned)((s->dur + 30) / 60), utf8Rus("хв", false));
    uint16_t dw = textW(d);
    dsp.setTextColor(C_DIM);
    dsp.setCursor(SM_LIST_W - (int16_t)dw, y + 55); dsp.print(d);
    snprintf(t, sizeof(t), "%s", utf8Rus(s->preacher, false));
    fitText(t, SM_LIST_W - 16 - dw);
    dsp.setCursor(6, y + 55); dsp.print(t);
    dsp.drawFastHLine(4, y + SM_ROW_H - 2, SM_LIST_W - 2, C_PAN2);
  }
  dsp.setFont();
}

/*  Рядки «інформації», що міняються: мережа, сигнал, потік, погода,
    батарея, пам'ять. Сетери мовчать, якщо текст той самий.  */
void YoMenu::_infoLive(){
  char b[48];
  if(WiFi.status() == WL_CONNECTED){
    _info[1][1].setText(WiFi.SSID().c_str());
    _info[2][1].setText(WiFi.localIP().toString().c_str());
    snprintf(b, sizeof(b), "%d dBm", (int)WiFi.RSSI());
  }else{
    _info[1][1].setText("точка доступу");
    _info[2][1].setText(WiFi.softAPIP().toString().c_str());
    snprintf(b, sizeof(b), "-");
  }
  _info[3][1].setText(b);
  if(player.status() == PLAYING && config.station.bitrate)
    snprintf(b, sizeof(b), "%d кбіт/с, %s", config.station.bitrate, player.getCodecname());
  else snprintf(b, sizeof(b), "-");
  _info[4][1].setText(b);
  /*  «°» — байт 0xB0 кодування шрифту; utf8Rus() пропускає його як є  */
  if(timekeeper.weatherHave)
    snprintf(b, sizeof(b), "%.1f" "\xB0" "  %d мм  %d%%", (float)timekeeper.weatherTemp, (int)timekeeper.weatherPress, (int)timekeeper.weatherHum);
  else snprintf(b, sizeof(b), "-");
  _info[5][1].setText(b);
  uint16_t mv = extras.batMv();
  if(extras.s.noBat) snprintf(b, sizeof(b), "вимкнено");
  else if(mv < 2800) snprintf(b, sizeof(b), "-");
  else snprintf(b, sizeof(b), "%d%%, %u.%02u В%s", extras.batPct(), mv/1000, (mv%1000)/10,
                extras.charged() ? ", заряджено" : (extras.charging() ? ", заряджається" : ""));
  _info[6][1].setText(b);
  snprintf(b, sizeof(b), "%u КБ", (unsigned)(ESP.getFreeHeap()/1024));
  _info[7][1].setText(b);
}

/*  Куди веде стрілка «назад»: сторінки параметрів — у сітку параметрів,
    решта — у головне меню.  */
int8_t YoMenu::_parent(int8_t p) const {
  if(p == PG_INFO || p == PG_WIFI || p == PG_TIME || p == PG_SYS || p == PG_DEV) return PG_SETUP;
  if(p == PG_DAC) return PG_DEV;
  if(p == PG_DACINFO) return PG_DAC;
  if(p == PG_POWER) return PG_SETUP;
  if(p == PG_WSAVED) return PG_WIFI;
  return PG_HOME;
}

/*  ---------- список проповідей у вигляді списку станцій ---------- */

/*  Один список на дві сторінки: рядки бере в того, хто зараз відкритий.  */
static const char* smName(int idx){ return yomenu.rowName(idx); }

const char* YoMenu::rowName(int idx){
  if(_cur == PG_WIFI) return _wifiRowName(idx);
  return _sermRowName(idx);
}

const char* YoMenu::_sermRowName(int idx){
  static char msg[48];
  if(sermons.loading()){
    if(sermons.loadedSoFar()) snprintf(msg, sizeof(msg), "завантажую з сайту... %u", sermons.loadedSoFar());
    else strlcpy(msg, "завантажую з сайту...", sizeof(msg));
    return msg;
  }
  if(sermons.count() == 0){
    snprintf(msg, sizeof(msg), "%s - торкніться", sermons.error()[0] ? sermons.error() : "порожньо");
    return msg;
  }
  const Sermon* s = sermons.at(idx - 1);
  return s ? s->title : "";
}

void YoMenu::_smGo(int16_t sel){
  _smFrom = _smCur;
  _smSel = sel; _smT0 = millis(); _smAnim = true;
  _smShift = 0; _smMqT = millis() + 600; _smMqW = 0;
}

void YoMenu::_smDraw(bool bandOnly){
  int n = _listCount();
  int play = _listPlay();
  int16_t wrap = _smMqW > PL_LIST_W - 16 ? _smMqW + 40 : 0;
  plGenericDraw((float)_smSel, n ? n : 1, smName, _smShift, bandOnly, play, wrap);
  _wifiBars((float)_smSel, bandOnly);
}

/*  Бігучий рядок у жовтій смузі — довгі назви проповідей видно цілком.  */
void YoMenu::_smMarquee(){
  uint32_t now = millis();
  if(!_smMqW){ _smMqW = plTextWidth(smName(_smSel)); if(!_smMqW) _smMqW = 1; }
  const int16_t room = PL_LIST_W - 16;
  if(_smMqW <= room) return;
  if((int32_t)(now - _smMqT) < 0) return;
  _smMqT = now + 25;                         /* по колу, без зупинок — як і в станцій */
  _smShift++;
  if(_smShift >= (int16_t)(_smMqW + 40)) _smShift = 0;
  _smDraw(true);
}

/*  ---------- сітка «параметри» ---------- */

void YoMenu::_drawSetup(){
  static const char* lbl[6] = { "інформація", "Wi-Fi", "час", "система", "розробник", "живлення" };
  for(uint8_t i=0;i<6;i++){
    int16_t x = 6 + (i % 3) * 104, y = 33 + (i / 3) * 104, w = 100, h = 100;
    int16_t cx = x + w / 2, cy = y + 38;
    dsp.fillRect(x, y, w, h, C_PANEL);
    const uint16_t c = C_ACC;
    switch(i){
      case 0: dsp.drawCircle(cx, cy, 14, c); dsp.drawCircle(cx, cy, 13, c);
              dsp.fillRect(cx-2, cy-3, 5, 12, c); dsp.fillRect(cx-2, cy-9, 5, 4, c); break;
      case 1: for(int r = 6; r <= 18; r += 6) for(int a = -140; a <= -40; a += 3){
                float rad = a * 3.14159f / 180.0f;
                dsp.drawPixel(cx + (int)(r * cosf(rad)), cy + 10 + (int)(r * sinf(rad)), c);
                dsp.drawPixel(cx + (int)((r+1) * cosf(rad)), cy + 10 + (int)((r+1) * sinf(rad)), c); }
              dsp.fillCircle(cx, cy + 10, 3, c); break;
      case 2: dsp.drawCircle(cx, cy, 14, c); dsp.drawCircle(cx, cy, 13, c);
              dsp.fillRect(cx-1, cy-9, 3, 10, c); dsp.fillRect(cx, cy-1, 8, 3, c); break;
      case 3:
              /*  система: три повзунки  */
              for(uint8_t k=0;k<3;k++){ dsp.fillRect(cx-14, cy-9 + k*9, 28, 2, c); dsp.fillRect(cx-8 + k*7, cy-12 + k*9, 5, 8, c); }
              break;
      case 5:
              /*  живлення: кільце з розривом угорі й риска  */
              for(int a = -55; a <= 235; a += 2){
                float rad = (a - 90) * 3.14159f / 180.0f;
                for(int r = 13; r <= 15; r++) dsp.drawPixel(cx + (int)(r * cosf(rad)), cy + (int)(r * sinf(rad)), c);
              }
              dsp.fillRect(cx-1, cy-17, 3, 15, c);
              break;
      default:
              /*  розробник: «</>»  */
              dsp.setFont(&yoUI12b); dsp.setTextSize(1); dsp.setTextColor(c);
              dsp.setCursor(cx - 16, cy + 6); dsp.print("</>"); dsp.setFont();
              break;
    }
    dsp.setFont(&yoUI8); dsp.setTextSize(1); dsp.setTextColor(C_TXT);
    char t[32]; snprintf(t, sizeof(t), "%s", utf8Rus(lbl[i], false));
    fitText(t, w - 6);
    dsp.setCursor(x + (w - (int16_t)textW(t)) / 2, y + h - 16); dsp.print(t);
    dsp.setFont();
  }
}

/*  ---------- живлення ----------
    Дві великі кнопки. Перше торкання «зводить» кнопку (жовта, «торкніться ще
    раз»), друге за 4 с — виконує: випадково вимкнути радіо не вийде.  */
#define PW_Y(i)  (40 + (i) * 80)
#define PW_H     70
void YoMenu::_drawPower(){
  if(_pwrGo >= 0){
    /*  останній кадр: що відбувається — і далі екран уже не малює  */
    dsp.fillRect(0, 0, SW, SH, C_BG);
    dsp.setFont(&yoUI11); dsp.setTextSize(1); dsp.setTextColor(C_ACC);
    char t[40]; snprintf(t, sizeof(t), "%s", utf8Rus(_pwrGo == 0 ? "Перезавантажую..." : "Вимикаюсь...", false));
    dsp.setCursor((SW - (int16_t)textW(t)) / 2, 116); dsp.print(t);
    if(_pwrGo == 1){
      dsp.setFont(&yoUI8); dsp.setTextColor(C_DIM);
      snprintf(t, sizeof(t), "%s", utf8Rus("увімкнеться дотиком", false));
      dsp.setCursor((SW - (int16_t)textW(t)) / 2, 142); dsp.print(t);
    }
    dsp.setFont();
    if(_pwrGo == 1){
      delay(900);                          /* напис видно, потім екран спить */
      extras.pwmSet(0);
      display.deepsleep();                 /* ILI9341 — у режим сну, з цієї ж задачі */
    }
    extras.requestPower(_pwrGo == 0 ? 1 : 2);
    return;
  }
  dsp.fillRect(0, HDR, SW, SH - HDR, C_BG);
  static const char* t1[2] = { "перезавантажити", "вимкнути" };
  static const char* t2[2] = { "звук стихне на 10 с", "увімкнеться дотиком" };
  for(uint8_t i = 0; i < 2; i++){
    int16_t y = PW_Y(i);
    bool armed = (_pwrArm == (int8_t)i);
    uint16_t bg = armed ? C_ACC : C_PANEL, fg = armed ? C_BG : C_TXT, sub = armed ? C_BG : C_DIM, ic = armed ? C_BG : C_ACC;
    dsp.fillRect(CX, y, CW, PW_H, bg);
    int16_t cx = CX + 26, cy = y + PW_H / 2;
    if(i == 0){
      /*  коло зі стрілкою  */
      for(int a = 30; a <= 330; a += 2){
        float rad = (a - 90) * 3.14159f / 180.0f;
        for(int r = 11; r <= 13; r++) dsp.drawPixel(cx + (int)(r * cosf(rad)), cy + (int)(r * sinf(rad)), ic);
      }
      dsp.fillTriangle(cx + 4, cy - 17, cx + 4, cy - 7, cx + 11, cy - 12, ic);
    }else{
      for(int a = -55; a <= 235; a += 2){
        float rad = (a - 90) * 3.14159f / 180.0f;
        for(int r = 11; r <= 13; r++) dsp.drawPixel(cx + (int)(r * cosf(rad)), cy + (int)(r * sinf(rad)), ic);
      }
      dsp.fillRect(cx - 1, cy - 15, 3, 13, ic);
    }
    char t[40];
    dsp.setFont(&yoUI11); dsp.setTextSize(1); dsp.setTextColor(fg);
    snprintf(t, sizeof(t), "%s", utf8Rus(t1[i], false));
    dsp.setCursor(CX + 52, y + 30); dsp.print(t);
    dsp.setFont(&yoUI8); dsp.setTextColor(sub);
    snprintf(t, sizeof(t), "%s", utf8Rus(armed ? "торкніться ще раз" : t2[i], false));
    dsp.setCursor(CX + 52, y + 54); dsp.print(t);
  }
  dsp.setFont(&yoUI8); dsp.setTextColor(C_DIM);
  char t[40]; snprintf(t, sizeof(t), "%s", utf8Rus("у сні майже не бере заряд", false));
  dsp.setCursor((SW - (int16_t)textW(t)) / 2, 222); dsp.print(t);
  dsp.setFont();
}

/*  ---------- розробник: аудіовихід ---------- */

static const char* const DAC_NAME[5] = { "ES8311", "PCM5102A", "UDA1334A", "MAX98357A", "VS1053B" };
static const char* const DAC_KIND[5] = { "вбудований кодек і підсилювач", "стерео ЦАП, лінійний вихід",
                                         "стерео ЦАП, навушники й лінія", "моно підсилювач 3 Вт на динамік",
                                         "окремий декодер, інша прошивка" };

void YoMenu::_drawDevBtn(){
  char t[64];
  /*  логотипи станцій: знайдені плата шукає й сама; кнопка — шукати знову
      для тих, де минулого разу не знайшлось  */
  dsp.fillRect(CX, 128, CW, 30, C_PAN2);
  dsp.setFont(&yoUI9); dsp.setTextSize(1); dsp.setTextColor(C_TXT);
  snprintf(t, sizeof(t), "%s", utf8Rus(_msg[0] ? _msg : "оновити логотипи станцій", false));
  fitText(t, CW - 16);
  dsp.setCursor(CX + (CW - (int16_t)textW(t)) / 2, 148); dsp.print(t);
  dsp.setFont(&yoUI9); dsp.setTextColor(C_DIM);
  snprintf(t, sizeof(t), "%s", utf8Rus("аудіовихід", false));
  dsp.setCursor(CX, 178); dsp.print(t);
  dsp.fillRect(CX, 184, CW, 44, C_PAN2);
  dsp.setFont(&yoUI11); dsp.setTextColor(C_TXT);
  dsp.setCursor(CX + 10, 212); dsp.print(DAC_NAME[extras.s.dac]);
  dsp.setFont(&yoUI8); dsp.setTextColor(C_DIM);
  snprintf(t, sizeof(t), "%s", utf8Rus(DAC_KIND[extras.s.dac], false));
  fitText(t, CW - 130);
  dsp.setCursor(CX + 120, 211); dsp.print(t);
  dsp.fillTriangle(CX + CW - 16, 198, CX + CW - 16, 214, CX + CW - 8, 206, C_ACC);   /* › */
  dsp.setFont();
}

void YoMenu::_drawDacList(){
  for(uint8_t i = 0; i < 5; i++){
    int16_t y = 32 + i * 41;
    bool cur = (extras.s.dac == i);
    dsp.fillRect(CX - 6, y, CW + 12, 38, cur ? C_PAN2 : C_PANEL);
    /*  назва — зверху, опис — другим рядком під нею: в один рядок
        вони наповзали одне на одне  */
    dsp.setFont(&yoUI9b); dsp.setTextSize(1); dsp.setTextColor(cur ? C_ACC : C_TXT);
    dsp.setCursor(CX + 4, y + 16); dsp.print(DAC_NAME[i]);
    dsp.setFont(&yoUI8); dsp.setTextColor(cur ? C_ACC : C_DIM);
    char t[64]; snprintf(t, sizeof(t), "%s", utf8Rus(cur ? "зараз звук іде сюди" : DAC_KIND[i], false));
    fitText(t, CW - 16);
    dsp.setCursor(CX + 4, y + 32); dsp.print(t);
  }
  dsp.setFont();
}

/*  Схема підключення: ліворуч виводи роз'єму плати, праворуч — модуля,
    між ними проводи: живлення червоні, земля сіра, сигнали жовті.  */
void YoMenu::_drawDacInfo(){
  if(_dacSel == 4 && _dacAlt){
    /*  Де роз'єми: усі три — на звороті плати, 1.25 мм, 4 контакти. Пайка
        не потрібна: SPI для VS1053 — окрема шина на вільних GPIO, а не
        шина екрана (його шлейф припаяно під приклеєною панеллю).  */
    dsp.fillRect(0, HDR, SW, SH - HDR, C_BG);
    char t[72];
    struct Hdr { const char* name; const char* pin[4]; uint8_t used; const char* to[4]; };
    static const Hdr H[3] = {
      { "роз'єм розширення (Expand)", { "IO2", "IO3", "IO14", "IO21" }, 0x0F, { "XCS", "MISO", "SCK", "MOSI" } },
      { "роз'єм UART",                { "TXD0", "RXD0", "GND", "5V" },   0x0F, { "XDCS", "DREQ", "GND", "5V" } },
      { "роз'єм I2C",                 { "3.3V", "GND", "SCL", "SDA" },   0x01, { "XRST", "", "", "" } },
    };
    for(uint8_t h = 0; h < 3; h++){
      int16_t y = 34 + h * 58;
      dsp.setFont(&yoUI8); dsp.setTextSize(1); dsp.setTextColor(C_DIM);
      snprintf(t, sizeof(t), "%s", utf8Rus(H[h].name, false));
      dsp.setCursor(8, y + 10); dsp.print(t);
      for(uint8_t k = 0; k < 4; k++){
        int16_t x = 8 + k * 76;
        bool on = H[h].used & (1 << k);
        dsp.fillRect(x, y + 15, 72, 18, on ? C_ACC : C_PAN2);
        dsp.setFont(&yoUI9b); dsp.setTextColor(on ? C_BG : C_DIM);
        dsp.setCursor(x + (72 - (int16_t)textW(H[h].pin[k])) / 2, y + 29); dsp.print(H[h].pin[k]);
        if(on && H[h].to[k][0]){
          dsp.setFont(&yoUI8); dsp.setTextColor(C_TXT);
          snprintf(t, sizeof(t), "-> %s", H[h].to[k]);
          dsp.setCursor(x + (72 - (int16_t)textW(t)) / 2, y + 47); dsp.print(t);
        }
      }
    }
    dsp.fillRect(8, 212, SW - 16, 26, C_PAN2);
    dsp.setFont(&yoUI9b); dsp.setTextColor(C_TXT);
    snprintf(t, sizeof(t), "%s", utf8Rus("назад до схеми", false));
    dsp.setCursor((SW - (int16_t)textW(t)) / 2, 230); dsp.print(t);
    dsp.setFont();
    return;
  }
  dsp.fillRect(0, HDR, SW, SH - HDR, C_BG);     /* після виду «де підпаятись» — з чистого */
  struct Pin { const char* esp; const char* mod; };
  /*  За специфікацією плати: роз'єм розширення — лише чотири GPIO (2, 3,
      14, 21), без живлення; 5V і GND — з роз'єму UART, 3.3V — з I2C.  */
  static const Pin P1[] = { {"5V UART","VIN"}, {"GND UART","GND"}, {"IO14","BCK"}, {"IO21","LCK"}, {"IO2","DIN"}, {"GND I2C","SCK"}, {"3V3 I2C","XSMT"} };
  static const Pin P2[] = { {"5V UART","VIN"}, {"GND UART","GND"}, {"IO14","BCLK"}, {"IO21","WSEL"}, {"IO2","DIN"} };
  static const Pin P3[] = { {"5V UART","VIN"}, {"GND UART","GND"}, {"IO14","BCLK"}, {"IO21","LRC"}, {"IO2","DIN"} };
  /*  VS1053 — окрема шина SPI на вільних виводах, SPI екрана не потрібна  */
  static const Pin P4[] = { {"5V UART","5V"}, {"GND UART","GND"}, {"IO14","SCK"}, {"IO21","MOSI"}, {"IO3","MISO"},
                            {"IO2","XCS"}, {"TXD0","XDCS"}, {"RXD0","DREQ"}, {"3V3 I2C","XRST"} };
  static const char* D1[] = { "Кодек ES8311 + підсилювач SC8002B на динамік.", "(ES8388 на цій платі немає)" };
  static const char* D2[] = { "Стерео ЦАП, лінійний вихід 3.5 мм.", "SCK на GND, XSMT на 3V3." };
  static const char* D3[] = { "Стерео ЦАП: навушники й лінійний вихід.", "Живлення 3-5 В." };
  static const char* D4[] = { "Моно підсилювач 3 Вт на динамік 4-8 Ом.", "GAIN і SD не під'єднувати." };
  static const char* D5[] = { "Без пайки: роз'єми на звороті.", "Потрібна окрема прошивка." };
  const Pin* pins = nullptr; uint8_t np = 0; const char** d = D1;
  switch(_dacSel){
    case 1: pins = P1; np = 7; d = D2; break;
    case 2: pins = P2; np = 5; d = D3; break;
    case 3: pins = P3; np = 5; d = D4; break;
    case 4: pins = P4; np = 9; d = D5; break;
    default: break;
  }
  char t[64];
  if(np){
    const int16_t rh = np > 7 ? 14 : (np > 5 ? 17 : 19), y0 = 46;
    dsp.setFont(&yoUI8); dsp.setTextSize(1); dsp.setTextColor(C_DIM);
    snprintf(t, sizeof(t), "%s", utf8Rus("плата (роз'єм)", false)); dsp.setCursor(8, 41); dsp.print(t);
    snprintf(t, sizeof(t), "%s", DAC_NAME[_dacSel]); dsp.setCursor(SW - 8 - (int16_t)textW(t), 41); dsp.print(t);
    for(uint8_t k = 0; k < np; k++){
      int16_t y = y0 + k * rh, cy = y + rh / 2;
      bool pwr = !strncmp(pins[k].esp, "5V", 2) || !strncmp(pins[k].esp, "3V3", 3);
      bool gnd = !strncmp(pins[k].esp, "GND", 3);
      uint16_t wc = pwr ? 0xF800 : (gnd ? 0x8410 : C_ACC);
      dsp.fillRect(8, y + 1, 72, rh - 2, C_PAN2);
      dsp.fillRect(SW - 68, y + 1, 60, rh - 2, C_PAN2);
      dsp.fillRect(80, cy - 1, SW - 148, 3, wc);                    /* провід */
      dsp.fillCircle(80, cy, 3, wc); dsp.fillCircle(SW - 68, cy, 3, wc);
      dsp.setFont(&yoUI8); dsp.setTextColor(C_TXT);
      dsp.setCursor(13, cy + 5); dsp.print(pins[k].esp);
      dsp.setCursor(SW - 63, cy + 5); dsp.print(pins[k].mod);
    }
  }else{
    /*  вбудований: плата -> ES8311 -> SC8002B -> динамік  */
    const char* blk[4] = { "ESP32-S3", "ES8311", "SC8002B", "динамік" };
    for(uint8_t k = 0; k < 4; k++){
      int16_t x = 8 + k * 78;
      dsp.fillRect(x, 70, 70, 40, C_PAN2);
      dsp.setFont(&yoUI8); dsp.setTextColor(C_TXT);
      snprintf(t, sizeof(t), "%s", utf8Rus(blk[k], false));
      dsp.setCursor(x + (70 - (int16_t)textW(t)) / 2, 95); dsp.print(t);
      if(k < 3) dsp.fillRect(x + 70, 88, 8, 3, C_ACC);
    }
  }
  /*  опис  */
  dsp.setFont(&yoUI8); dsp.setTextColor(C_DIM);
  int16_t ty = np ? 46 + np * (np > 7 ? 14 : (np > 5 ? 17 : 19)) + 13 : 150;
  for(uint8_t k = 0; k < 2; k++){
    snprintf(t, sizeof(t), "%s", utf8Rus(d[k], false)); fitText(t, SW - 16);
    dsp.setCursor(8, ty + k * 14); dsp.print(t);
  }
  /*  кнопка  */
  if(_dacSel == 4){
    dsp.fillRect(8, 212, SW - 16, 26, C_PAN2);
    dsp.setFont(&yoUI9b); dsp.setTextColor(C_TXT);
    snprintf(t, sizeof(t), "%s", utf8Rus("де ці роз'єми на платі", false));
    dsp.setCursor((SW - (int16_t)textW(t)) / 2, 230); dsp.print(t);
  }else{
    bool cur = (extras.s.dac == _dacSel);
    dsp.fillRect(8, 212, SW - 16, 26, cur ? C_PAN2 : C_ACC);
    dsp.setFont(&yoUI9b); dsp.setTextColor(cur ? C_DIM : C_BG);
    snprintf(t, sizeof(t), "%s", utf8Rus(cur ? "зараз звук іде сюди" : "увімкнути цей вихід", false));
    dsp.setCursor((SW - (int16_t)textW(t)) / 2, 230); dsp.print(t);
  }
  dsp.setFont();
}

/*  ---------- головне меню «☰» ---------- */

/*  Дев'ять плиток 3x3: великий жовтий значок і підпис. Тут усе, що раніше
    розсипалось значками по шапці плеєра, плюс входи в налаштування.  */
#define HM_X(i)   (6 + ((i) % 3) * 104)
#define HM_Y(i)   (33 + ((i) / 3) * 69)
#define HM_W      100
#define HM_H      65
enum { HM_STATIONS = 0, HM_FAV, HM_SERM, HM_SRC, HM_REC, HM_ALARM, HM_SCREEN, HM_SOUND, HM_SETUP };

void YoMenu::_drawHomeTile(uint8_t i){
  int16_t x = HM_X(i), y = HM_Y(i), cx = x + HM_W / 2, cy = y + 24;
  bool rec = recorder.active();
  dsp.fillRect(x, y, HM_W, HM_H, (i == HM_REC && rec) ? 0x8000 : C_PANEL);
  const uint16_t c = C_ACC;
  const char* lbl = "";
  char buf[24];
  switch(i){
    case HM_STATIONS:
      for(uint8_t k=0;k<3;k++){ dsp.fillRect(cx-13, cy-9 + k*8, 4, 4, c); dsp.fillRect(cx-6, cy-9 + k*8, 19, 4, c); }
      lbl = "станції"; break;
    case HM_FAV:
      drawStar(cx, cy, 13, c); lbl = "обране"; break;
    case HM_SERM:
      dsp.fillRect(cx-2, cy-14, 5, 28, c); dsp.fillRect(cx-10, cy-7, 21, 5, c);
      lbl = "проповіді"; break;
    case HM_SRC:
      if(config.getMode() == PM_SDCARD){          /* куди перемкне: на радіо */
        dsp.fillRect(cx-1, cy-3, 3, 16, c); dsp.fillCircle(cx, cy-5, 3, c);
        for(int8_t r = 8; r <= 13; r += 5)
          for(int16_t a = -55; a <= 55; a += 6){
            float rad = a * 3.14159f / 180.0f;
            dsp.drawPixel(cx - (int16_t)(r * cosf(rad)), cy-5 + (int16_t)(r * sinf(rad)), c);
            dsp.drawPixel(cx + (int16_t)(r * cosf(rad)), cy-5 + (int16_t)(r * sinf(rad)), c);
          }
        lbl = "радіо";
      }else{                                      /* на картку */
        dsp.fillRect(cx-9, cy-13, 12, 26, c); dsp.fillRect(cx+3, cy-8, 6, 21, c);
        dsp.fillTriangle(cx+3, cy-13, cx+3, cy-8, cx+8, cy-8, c);
        for(uint8_t k=0;k<3;k++) dsp.fillRect(cx-7 + k*4, cy-11, 2, 6, C_PANEL);
        lbl = "картка";
      }
      break;
    case HM_REC:
      if(rec){
        dsp.fillRect(cx-8, cy-8, 16, 16, C_TXT);
        uint32_t sec = recorder.seconds();
        snprintf(buf, sizeof(buf), "%u:%02u", (unsigned)(sec/60), (unsigned)(sec%60));
        lbl = buf;
      }else{ dsp.fillCircle(cx, cy, 10, 0xF800); lbl = "запис"; }
      break;
    case HM_ALARM:
      dsp.fillCircle(cx, cy-3, 9, c); dsp.fillRect(cx-9, cy-3, 19, 10, c);
      dsp.fillRect(cx-12, cy+6, 25, 3, c); dsp.fillCircle(cx, cy+11, 3, c);
      lbl = "будильник"; break;
    case HM_SCREEN:
      dsp.drawCircle(cx, cy-4, 9, c); dsp.drawCircle(cx, cy-4, 8, c);
      dsp.fillRect(cx-5, cy+6, 11, 3, c); dsp.fillRect(cx-5, cy+10, 11, 3, c);
      lbl = "екран"; break;
    case HM_SOUND:
      dsp.fillRect(cx-12, cy-2, 5, 14, c); dsp.fillRect(cx-3, cy-12, 5, 24, c); dsp.fillRect(cx+6, cy-6, 5, 18, c);
      lbl = "звук"; break;
    default:
      dsp.drawCircle(cx, cy, 9, c); dsp.drawCircle(cx, cy, 8, c); dsp.fillCircle(cx, cy, 3, c);
      dsp.fillRect(cx-2, cy-14, 5, 5, c); dsp.fillRect(cx-2, cy+10, 5, 5, c);
      dsp.fillRect(cx-14, cy-2, 5, 5, c); dsp.fillRect(cx+10, cy-2, 5, 5, c);
      lbl = "параметри"; break;
  }
  /*  Verdana 8: у 9 «будильник» і «параметри» не влазили в плитку  */
  dsp.setFont(&yoUI8); dsp.setTextSize(1); dsp.setTextColor(C_TXT);
  char t[32]; snprintf(t, sizeof(t), "%s", utf8Rus(lbl, false));
  fitText(t, HM_W - 6);
  dsp.setCursor(x + (HM_W - (int16_t)textW(t)) / 2, y + HM_H - 9); dsp.print(t);
  dsp.setFont();
}

void YoMenu::_drawHome(){
  _drawHdrMsg();
  for(uint8_t i=0;i<9;i++) _drawHomeTile(i);
}

/*  ---------- Wi-Fi: пошук мереж ---------- */

/*  Пошук асинхронний: WiFi сам обходить канали, а ми лише питаємо, чи
    готово. У режимі точки доступу для пошуку потрібна ще й станція.  */
void YoMenu::_wifiScan(){
  if(WiFi.getMode() == WIFI_AP) WiFi.mode(WIFI_AP_STA);
  /*  Поки радіо намагається повернутися в мережу, пошук не стартує зовсім:
      радіомодуль один. Тому спершу спиняємо спроби.  */
  network.pauseSta(true);
  WiFi.scanDelete();
  _scanAgain = 0;
  if(WiFi.scanNetworks(true, false) == WIFI_SCAN_FAILED){
    /*  модуль ще зайнятий попередньою спробою — за мить повторимо  */
    _scanning = false;
    if(_scanFails < 6){ _scanFails++; _scanAgain = millis() + 800; }
  }else{
    _scanning = true; _scanFails = 0; _scanT0 = millis();
  }
  _wlDirty = true;
}

void YoMenu::_wifiPoll(){
  if(!_scanning){
    if(_scanAgain && (int32_t)(millis() - _scanAgain) >= 0){ _scanAgain = 0; _wifiScan(); }
    return;
  }
  int16_t n = WiFi.scanComplete();
  /*  пошук завис — не тримаємо напис «шукаю мережі...» довіку  */
  if(n == WIFI_SCAN_RUNNING && millis() - _scanT0 > 20000UL){
    WiFi.scanDelete(); _scanning = false; _scanN = 0; _wlDirty = true; return;
  }
  if(n == WIFI_SCAN_RUNNING) return;
  _scanning = false;
  _scanN = 0;
  for(int16_t i = 0; i < n; i++){
    String id = WiFi.SSID(i);
    if(id.length() == 0) continue;                 /* прихована — лише вручну */
    int8_t rs = (int8_t)WiFi.RSSI(i);
    /*  Одна мережа на кількох точках доступу — показуємо раз, найсильнішу  */
    int8_t dup = -1;
    for(uint8_t k = 0; k < _scanN; k++) if(id == _scan[k].ssid){ dup = k; break; }
    if(dup >= 0){ if(rs > _scan[dup].rssi){ _scan[dup].rssi = rs; _scan[dup].enc = WiFi.encryptionType(i); } continue; }
    if(_scanN >= WS_MAX) continue;
    strlcpy(_scan[_scanN].ssid, id.c_str(), sizeof(_scan[0].ssid));
    _scan[_scanN].rssi = rs;
    _scan[_scanN].enc = (uint8_t)WiFi.encryptionType(i);
    _scanN++;
  }
  WiFi.scanDelete();
  /*  найсильніші вгорі  */
  for(uint8_t a = 1; a < _scanN; a++)
    for(uint8_t b = a; b > 0 && _scan[b].rssi > _scan[b-1].rssi; b--){ WScan t = _scan[b]; _scan[b] = _scan[b-1]; _scan[b-1] = t; }
  _wlDirty = true;
}

static bool wifiSaved(const char* ssid){
  for(uint8_t i = 0; i < config.ssidsCount && i < YOM_SSIDS; i++)
    if(!strcmp(config.ssids[i].ssid, ssid)) return true;
  return false;
}

/*  ---------- Wi-Fi: відомі мережі ---------- */

/*  Список мереж, які радіо пам'ятає. Перша — та, з якої воно починає; коли
    її немає в ефірі, радіо саме перебирає решту. Тут мережу можна підняти
    першою або забути зовсім — без перезавантаження.  */
void YoMenu::_drawSaved(){
  dsp.fillRect(0, HDR, SW, SH-HDR, C_BG);
  char t[72];
  uint8_t n = 0;
  for(uint8_t i = 0; i < YOM_SSIDS; i++) if(_ssid[i][0]) n++;
  if(!n){
    dsp.setFont(&yoUI11); dsp.setTextSize(1); dsp.setTextColor(C_DIM);
    snprintf(t, sizeof(t), "%s", utf8Rus("жодної мережі не збережено", false));
    dsp.setCursor((SW - (int16_t)textW(t)) / 2, 110); dsp.print(t);
    dsp.setFont(&yoUI8); dsp.setTextColor(C_ACC);
    snprintf(t, sizeof(t), "%s", utf8Rus("після перезавантаження радіо", false));
    dsp.setCursor((SW - (int16_t)textW(t)) / 2, 140); dsp.print(t);
    snprintf(t, sizeof(t), "%s", utf8Rus("підніме точку доступу PotuzhneRadio", false));
    dsp.setCursor((SW - (int16_t)textW(t)) / 2, 156); dsp.print(t);
    dsp.setFont();
    return;
  }
  bool sta = WiFi.status() == WL_CONNECTED;
  for(uint8_t i = 0; i < n; i++){
    int16_t y = WS_TOP + i * WS_ROW;
    bool armed = (_wsArm == (int8_t)i);
    bool cur = sta && WiFi.SSID() == _ssid[i];
    dsp.fillRect(CX, y, CW, WS_H, armed ? C_ACC : (cur ? C_PAN2 : C_PANEL));
    dsp.setFont(&yoUI9b); dsp.setTextSize(1);
    dsp.setTextColor(armed ? C_BG : (cur ? C_ACC : C_TXT));
    if(armed) snprintf(t, sizeof(t), "%s", utf8Rus("ще раз - і забуду", false));
    else      snprintf(t, sizeof(t), "%s", utf8Rus(_ssid[i], false));
    fitText(t, CW - 92);
    dsp.setCursor(CX + 26, y + 19); dsp.print(t);
    /*  номер у черзі: видно, хто перший  */
    dsp.setFont(&yoUI8); dsp.setTextColor(armed ? C_BG : C_DIM);
    snprintf(t, sizeof(t), "%u", (unsigned)(i + 1));
    dsp.setCursor(CX + 9, y + 18); dsp.print(t);
    uint16_t ic = armed ? C_BG : C_ACC;
    /*  ↑ — підняти першою (у першого рядка нема куди)  */
    if(i > 0){
      int16_t ax = CX + CW - 72 + 17, ay = y + WS_H / 2;
      dsp.fillTriangle(ax, ay - 8, ax - 7, ay + 1, ax + 7, ay + 1, ic);
      dsp.fillRect(ax - 3, ay + 1, 6, 7, ic);
    }
    /*  × — забути  */
    int16_t bx = CX + CW - 36 + 17, by2 = y + WS_H / 2;
    for(int8_t d = -7; d <= 7; d++){
      dsp.drawPixel(bx + d, by2 + d, ic); dsp.drawPixel(bx + d + 1, by2 + d, ic);
      dsp.drawPixel(bx + d, by2 - d, ic); dsp.drawPixel(bx + d + 1, by2 - d, ic);
    }
  }
  dsp.setFont(&yoUI8); dsp.setTextColor(C_DIM);
  /*  Довге тире перетворювач у CP1251 не знає — на екрані з нього виходить
      «вЂ”», тому в написах лише дефіс. І рядки вище краю: на SH-8 хвостики
      літер зрізало.  */
  dsp.setCursor(CX, SH - 26); dsp.print(utf8Rus("перша - з неї радіо починає;", false));
  dsp.setCursor(CX, SH - 12); dsp.print(utf8Rus("немає її в ефірі - перебере решту", false));
  dsp.setFont();
}

/*  Записуємо список і одразу перечитуємо його в пам'ять: перезавантаження
    заради забутої мережі — зайве.  */
void YoMenu::_savedWrite(){
  String out;
  for(uint8_t i = 0; i < YOM_SSIDS; i++)
    if(_ssid[i][0]) out += String(_ssid[i]) + "\t" + String(_pass[i]) + "\n";
  config.saveWifiList(out.c_str());
  _loadWifi();
}

/*  Рядки списку мереж: спершу знайдені, далі три дії. Дії живуть у самому
    списку, бо кнопок збоку лише чотири й усі зайняті: ▲ ▼ ▶ і назад.  */
const char* YoMenu::_wifiRowName(int idx){
  static char buf[72];
  if(idx >= 1 && idx <= _scanN){
    const WScan& w = _scan[idx-1];
    bool cur = WiFi.status() == WL_CONNECTED && WiFi.SSID() == w.ssid;
    /*  Мережу, у якій радіо зараз, позначає той самий значок, що й станцію,
        яка грає, — інакше слово в назві розганяло її бігти рядком.  */
    snprintf(buf, sizeof(buf), "%s%s", w.ssid,
             (!cur && wifiSaved(w.ssid)) ? "  (збережена)" : "");
    return buf;
  }
  switch(idx - _scanN){
    case 1:  return _scanning ? "шукаю мережі" : "шукати ще раз";
    case 2:  return "додати вручну";
    default: return "відомі мережі";
  }
}

int YoMenu::_listCount() const {
  if(_cur == PG_WIFI) return _scanN + WIFI_ACTS;
  return sermons.count();
}

/*  Рядок, позначений як «зараз»: проповідь, що грає, або мережа, у якій радіо.  */
int YoMenu::_listPlay() const {
  if(_cur == PG_SERM)
    return (player.remoteStationName && player.status() == PLAYING) ? sermons.playing() + 1 : -1;
  if(_cur == PG_WIFI && WiFi.status() == WL_CONNECTED){
    String cur = WiFi.SSID();
    for(uint8_t i = 0; i < _scanN; i++) if(cur == _scan[i].ssid) return i + 1;
  }
  return -1;
}

/*  Вибір рядка: у проповідях — грати, у мережах — підключитись або дія.  */
void YoMenu::_listPick(int idx){
  if(_cur == PG_SERM){
    if(sermons.count() && sermons.play(idx - 1)) close();
    return;
  }
  if(idx >= 1 && idx <= _scanN){ _wifiPick(idx - 1); return; }
  switch(idx - _scanN){
    case 1: if(!_scanning){ _scanFails = 0; _wifiScan(); _smDirty = true; } break;
    case 2: _wSsid[0] = 0; _wPass[0] = 0; _kbdNext = 1;
            _openKbd(_wSsid, YOM_SSID_LEN, false, "назва мережі"); break;
    default: _loadWifi(); _wsArm = -1; _show(PG_WSAVED); break;
  }
}

/*  Рівень сигналу малюємо поверх рядків: сам список уміє лише текст, а без
    смуг не видно, яка мережа ближча. Лише коли список стоїть — під час
    прокрутки рядки й так летять.  */
void YoMenu::_wifiBars(float pos, bool bandOnly){
  if(_cur != PG_WIFI) return;
  int base = (int)floorf(pos);
  int16_t off = (int16_t)((pos - base) * PL_ROW_H);
  const int16_t top = PL_TOP, bot = PL_TOP + PL_ROWS * PL_ROW_H;
  for(int r = -1; r <= PL_ROWS; r++){
    if(bandOnly && r != PL_CUR) continue;
    int idx = base - PL_CUR + r;
    if(idx < 1 || idx > _scanN) continue;
    int16_t y = top + r * PL_ROW_H - off;
    if(y < top || y + PL_ROW_H > bot) continue;          /* напіврядки не чіпаємо */
    bool band = (r == PL_CUR && off == 0);
    uint8_t lv = _scan[idx-1].rssi > -55 ? 4 : _scan[idx-1].rssi > -65 ? 3 :
                 _scan[idx-1].rssi > -75 ? 2 : _scan[idx-1].rssi > -85 ? 1 : 0;
    for(uint8_t b = 0; b < 4; b++){
      int16_t h = 4 + b * 3;
      int16_t bx = PL_X0 + PL_LIST_W - 24 + b * 5, by = y + PL_ROW_H/2 + 7 - h;
      dsp.fillRect(bx, by, 3, h, b < lv ? (band ? C_BG : C_TXT) : (band ? C_ACC : C_PAN2));
    }
    /*  замок — мережа з паролем  */
    if(_scan[idx-1].enc != WIFI_AUTH_OPEN){
      int16_t lx = PL_X0 + PL_LIST_W - 40, ly = y + PL_ROW_H/2 - 6;
      uint16_t c = band ? C_BG : C_DIM;
      dsp.drawRoundRect(lx+2, ly, 7, 8, 3, c);
      dsp.fillRect(lx, ly+5, 11, 8, c);
    }
  }
}

/*  Вибрали мережу: відкрита — одразу підключаємось, закрита — пароль.
    Якщо мережу вже збережено, пароль підставляємо з пам'яті.  */
void YoMenu::_wifiPick(uint8_t i){
  strlcpy(_wSsid, _scan[i].ssid, sizeof(_wSsid));
  _wPass[0] = 0;
  for(uint8_t k = 0; k < config.ssidsCount && k < YOM_SSIDS; k++)
    if(!strcmp(config.ssids[k].ssid, _wSsid)) strlcpy(_wPass, config.ssids[k].password, sizeof(_wPass));
  if(_scan[i].enc == WIFI_AUTH_OPEN){ _wPass[0] = 0; _wifiConnect(); return; }
  _kbdNext = 2;
  snprintf(_kbdTitleBuf, sizeof(_kbdTitleBuf), "пароль: %s", _wSsid);
  _openKbd(_wPass, YOM_PASS_LEN, true, _kbdTitleBuf);   /* пароль — зірочками: його видно з-за плеча */
}

/*  До п'яти мереж: вибрана стає першою, решта збережених іде за нею,
    найстаріша п'ята випадає. Підключення — штатне yoRadio після
    перезавантаження, починаючи з першої; не вийде — плата сама перейде
    до наступної збереженої, а ми після старту скажемо, що не вдалося.  */
void YoMenu::_wifiConnect(){
  if(!_wSsid[0]) return;
  _loadWifi();
  String out = String(_wSsid) + "\t" + String(_wPass) + "\n";
  uint8_t n = 1;
  for(uint8_t i = 0; i < YOM_SSIDS && n < YOM_SSIDS; i++){
    if(!_ssid[i][0] || !strcmp(_ssid[i], _wSsid)) continue;
    out += String(_ssid[i]) + "\t" + String(_pass[i]) + "\n";
    n++;
  }
  extras.wifiPending(_wSsid);
  config.setLastSSID(1);
  dsp.fillRect(0, HDR, SW, SH-HDR, C_BG);
  dsp.setFont(&yoUI11); dsp.setTextSize(1); dsp.setTextColor(C_ACC);
  char t[72];
  snprintf(t, sizeof(t), "%s", utf8Rus("Підключаюсь до мережі", false));
  dsp.setCursor((SW - (int16_t)textW(t)) / 2, 100); dsp.print(t);
  dsp.setTextColor(C_TXT);
  snprintf(t, sizeof(t), "%s", utf8Rus(_wSsid, false)); fitText(t, SW - 20);
  dsp.setCursor((SW - (int16_t)textW(t)) / 2, 124); dsp.print(t);
  dsp.setFont(&yoUI9); dsp.setTextColor(C_DIM);
  snprintf(t, sizeof(t), "%s", utf8Rus("радіо перезавантажиться", false));
  dsp.setCursor((SW - (int16_t)textW(t)) / 2, 150); dsp.print(t);
  dsp.setFont();
  delay(600);
  config.saveWifiFromNextion(out.c_str());
}

/*  ---------- життєвий цикл ---------- */

void YoMenu::open(){
  if(_cur != PG_OFF) return;
  _build(); _apLock=false; _loadWifi(); _syncSys();
  _show(PG_INFO);
}

/*  Кнопка «☰» у шапці плеєра: усе, що раніше було окремими значками.  */
void YoMenu::openHome(){
  if(_cur != PG_OFF) return;
  _build(); _apLock = false; _loadWifi(); _syncSys();
  _show(PG_HOME);
}

void YoMenu::openFav(){
  if(_cur != PG_OFF) return;
  _build(); _apLock = false;
  _show(PG_FAV);
}

/*  Як apScreen() у Nextion: без мережі одразу Wi-Fi і без переходів далі.
    Коли зв'язок просто зник (мережа ще збережена), виходу не замикаємо:
    мережа може повернутися сама, і людина має змогу піти на плеєр.  */
void YoMenu::openWifi(bool lock){
  if(_cur != PG_OFF) return;
  _build(); _apLock = lock; _loadWifi();
  _show(PG_WIFI);
}

/*  Закриття теж іде через затемнення, і саму перемальовку робить задача
    дисплея: раніше вона виконувалась просто в потоці дотику.  */
void YoMenu::close(){
  if(_cur == PG_OFF || _fadeStep >= 0) return;
  if(_apLock){ return; }
  _closeReq = true;
  _fadeStep = 0;
  _fadeTick = 0;
}

void YoMenu::tick(){ if(_cur==PG_INFO || _cur==PG_TIME || _cur==PG_SLEEP || _cur==PG_NIGHT || _cur==PG_FAV || _cur==PG_HOME) _live = true; }

/*  Жодних перемальовок «про всяк випадок»: сетери мовчать, якщо значення
    не змінилось, тому цей виклик здебільшого не чіпає екран узагалі.  */
void YoMenu::render(){
  if(_cur == PG_OFF && _fadeStep < 0) return;
  if(_fadeStep >= 0){ _fade(); return; }   /* поки триває зміна — тільки вона */
  if(_cur == PG_OFF) return;
  /*  Хвилину без дотиків — назад на плеєр, тим самим плавним переходом.
      Клавіатуру й налаштування мережі без неї не чіпаємо: там людина
      може думати над паролем.  */
  if(!_apLock && _cur != PG_KBD && _cur != PG_WIFI && millis() - _lastAct > 60000UL){ close(); return; }
  /*  Крок анімації смуг — щоразу, коли задача дисплея проходить повз, а не
      раз на секунду разом із оновленням даних.  */
  if(_cur >= 0 && _pg[_cur]) _pg[_cur]->loop();
  if(_cur == PG_DACINFO && _favDirty){ _favDirty = false; _drawDacInfo(); return; }
  if(_cur == PG_WSAVED){
    if(_wsArm >= 0 && millis() - _wsArmT > 4000){ _wsArm = -1; _favDirty = true; }   /* не підтвердили */
    if(_favDirty){ _favDirty = false; _drawSaved(); }
    return;
  }
  if(_cur == PG_POWER){
    if(_pwrArm >= 0 && _pwrGo < 0 && millis() - _pwrArmT > 4000){ _pwrArm = -1; _favDirty = true; }   /* не підтвердили — знімаємо */
    if(_favDirty){ _favDirty = false; _drawPower(); }
    return;
  }
  if(_cur == PG_DEV){
    if(_msgUntil && (int32_t)(millis() - _msgUntil) >= 0){ _msgUntil = 0; _msg[0] = 0; _favDirty = true; }
    if(_favDirty){ _favDirty = false; _msgDirty = false; _drawDevBtn(); }
  }
  if(_cur == PG_HOME){
    if(_msgUntil && (int32_t)(millis() - _msgUntil) >= 0){ _msgUntil = 0; _msg[0] = 0; _msgDirty = true; }
    if(_favDirty){ _favDirty = false; _drawHome(); _msgDirty = false; }
    if(_msgDirty){ _msgDirty = false; _drawHdrMsg(); }
    if(_live){ _live = false; if(recorder.active()) _drawHomeTile(4); }
    return;
  }
  if(_isList(_cur)){
    if(_cur == PG_WIFI){
      /*  пошук іде своїм ходом: список перемальовуємо, коли він скінчився  */
      _wifiPoll();
      if(_wlDirty){
        _wlDirty = false;
        if(_smSel > _listCount()) _smSel = _listCount();
        _smShift = 0; _smMqW = 0; _smMqT = millis() + 600; _smDirty = true;
      }
    }
    if(_cur == PG_SERM && (sermons.version() != _smVer || sermons.loading() != _smLoad)){
      _smVer = sermons.version(); _smLoad = sermons.loading();
      if(_smSel > sermons.count()) _smSel = sermons.count() ? sermons.count() : 1;
      _smShift = 0; _smMqT = millis() + 600; _smDirty = true;
    }
    /*  поки вантажиться — оновлюємо лічильник у рядку, не частіше ніж раз на 0,4 с  */
    static uint16_t shownGot = 0; static uint32_t gotT = 0;
    if(_cur == PG_SERM && sermons.loading() && sermons.loadedSoFar() != shownGot && millis() - gotT > 400){
      shownGot = sermons.loadedSoFar(); gotT = millis(); _smDirty = true;
    }
    if(_smBtn >= 0 && millis() - _smBtnT > 150){ plGenericButton(_smBtn, false); _smBtn = -1; }
    if(_smBtnDraw >= 0){ int8_t b = _smBtnDraw; _smBtnDraw = -1; plGenericButton(b & 3, b < 4); }
    uint32_t now = millis();
    /*  утримання ▲▼: після 420 мс — крок кожні 140 мс, як у списку станцій  */
    if(_smHold == 0 || _smHold == 1){
      if(now - _smHoldT > 420 && now - _smRepT > 140){ _smRepT = now; _smRep = true; _smStep(_smHold ? 1 : -1); }
    }
    int nn = _listCount();
    int play = _listPlay();
    if(_dActive){
      if(_smDragDirty){ _smDragDirty = false; plGenericDraw(_smCur, nn ? nn : 1, smName, 0, false, play); }
      return;
    }
    if(_smFling){
      static uint32_t ft = 0;
      if(now - ft < 15) return;
      float dt = (now - (ft ? ft : now - 16)) / 1000.0f; ft = now;
      if(dt > 0.1f) dt = 0.1f;
      _smCur += _dVel * dt;
      _dVel *= powf(0.06f, dt);
      int n = nn ? nn : 1;
      if(_smCur <= 1.0f || _smCur >= (float)n) _dVel = 0.0f;
      if(fabsf(_dVel) < 0.6f){
        _smFling = false; ft = 0;
        float to = roundf(_smCur); if(to < 1) to = 1; if(to > n) to = n;
        _smFrom = _smCur; _smSel = (int16_t)to; _smT0 = now; _smAnim = true;   /* дотягуємо пружиною */
        _smShift = 0; _smMqT = now + 600; _smMqW = 0;
      }else{
        plGenericDraw(_smCur, n, smName, 0, false, play);
        return;
      }
    }
    if(_smAnim){
      /*  та сама згасаюча пружина, що й у списку станцій  */
      float ts = (millis() - _smT0) / 1000.0f;
      if(ts >= 0.45f){ _smAnim = false; _smCur = _smSel; _smDirty = true; }
      else{
        const float zw = 11.0f, wd = 16.7f;
        float e = 1.0f - expf(-zw * ts) * (cosf(wd * ts) + (zw / wd) * sinf(wd * ts));
        _smCur = _smFrom + ((float)_smSel - _smFrom) * e;
        plGenericDraw(_smCur, nn ? nn : 1, smName, 0, false, play);
        return;
      }
    }
    if(_smDirty){ _smDirty = false; _smMqW = 0; _smCur = _smSel; _smDraw(false); }
    else _smMarquee();
    return;
  }
  /*  Обране: перемальовуємо лише те, що позначено.  */
  if(_cur == PG_FAV){
    if(_msgUntil && (int32_t)(millis() - _msgUntil) >= 0){ _msgUntil = 0; _msg[0] = 0; _msgDirty = true; }
    if(_cur == PG_SERM && (sermons.version() != _smVer || sermons.loading() != _smLoad)){
      _smVer = sermons.version(); _smLoad = sermons.loading(); _smDirty = true;
    }
    if(_favDirty){ _favDirty = false; _msgDirty = false; if(_cur == PG_FAV) _drawFav(); }
    if(_smDirty){ _smDirty = false; _msgDirty = false; if(_cur == PG_SERM) _drawSerm(); }
    if(_msgDirty){ _msgDirty = false; _drawHdrMsg(); }
    if(_live){ _live = false; if(_cur == PG_FAV && recorder.active()) _drawBottom(); }
    return;
  }
  if(!_live) return;
  _live = false;
  char v[40];
  if(_cur == PG_INFO){
    _infoLive();
  }else if(_cur == PG_TIME){
    strftime(v, sizeof(v), "%H:%M:%S", &network.timeinfo);   _tmNow.setText(v);
    snprintf(v, sizeof(v), "%02d", config.store.tzHour);     _tmH.setText(v);
    snprintf(v, sizeof(v), "%02d", config.store.tzMin);      _tmM.setText(v);
  }else if(_cur == PG_SLEEP || _cur == PG_NIGHT){
    _syncExtras();
  }
}

/*  ---------- дані сторінок ---------- */

/*  Тільки оновити написи в полях із того, що вже набрано в пам'яті.
    Раніше це вміла лише _loadWifi(), яка заразом перечитує налаштування —
    через це після виходу з клавіатури набране губилось.  */
void YoMenu::_showWifi(){
  for(uint8_t i=0;i<YOM_SSIDS;i++){
    char stars[16]; size_t n=strlen(_pass[i]); if(n>13) n=13;
    memset(stars, '*', n); stars[n]='\0';
    _wifiS[i].setText(_ssid[i]);
    _wifiP[i].setText(stars);
  }
}

void YoMenu::_loadWifi(){
  for(uint8_t i=0;i<YOM_SSIDS;i++){
    _ssid[i][0]='\0'; _pass[i][0]='\0';
    if(i < config.ssidsCount){
      snprintf(_ssid[i], YOM_SSID_LEN, "%s", config.ssids[i].ssid);
      snprintf(_pass[i], YOM_PASS_LEN, "%s", config.ssids[i].password);
    }
  }
  _showWifi();
}

void YoMenu::_syncSys(){
  _chkStart.setValue(config.store.smartstart != 2);
  _chkInfo .setValue(config.store.audioinfo);
  _chkSrc  .setValue(config.getMode() == PM_SDCARD);
  _bright  .setValue(config.store.brightness);
}

void YoMenu::_saveWifi(){
  String out;
  for(uint8_t i=0;i<YOM_SSIDS;i++)
    if(_ssid[i][0]) out += String(_ssid[i]) + "\t" + String(_pass[i]) + "\n";
  if(out.length()==0) return;
  dsp.fillRect(0, HDR, SW, SH-HDR, C_BG);
  dsp.setFont(&yoUI9b); dsp.setTextColor(C_ACC);
  dsp.setCursor(CX, 120); dsp.print(utf8Rus("Зберігаю та перезавантажуюсь...", false));
  dsp.setFont();
  config.saveWifiFromNextion(out.c_str());
}

/*  ---------- екранна клавіатура ---------- */

static const char* KROWS[3][4] = {
  { "1234567890", "qwertyuiop", "asdfghjkl",  "zxcvbnm"  },
  { "1234567890", "QWERTYUIOP", "ASDFGHJKL",  "ZXCVBNM"  },
  { "1234567890", "!@#$%^&*()", "-_=+[]{}|?", "'\",.;:/" },
};
#define KY0   64
#define KROWH 34
#define KKW   32

void YoMenu::_drawKbdKeys(){
  for(uint8_t r=0;r<4;r++){
    const char* row = KROWS[_kbdPage][r];
    uint8_t len = strlen(row);
    int16_t y = KY0 + r*KROWH;
    int16_t x0 = (r==2) ? (SW - len*KKW)/2 : (r==3 ? 48 : 0);
    if(r==3){
      dsp.fillRect(0, y, 46, KROWH-2, C_PAN2);
      dsp.setFont(&yoUI9b); dsp.setTextColor(C_TXT);
      dsp.setCursor(6, y+22); dsp.print(_kbdPage==1?"abc":"ABC"); dsp.setFont();
    }
    for(uint8_t c=0;c<len;c++){
      int16_t x = x0 + c*KKW;
      dsp.fillRect(x, y, KKW-2, KROWH-2, C_PAN2);
      char k[2] = { row[c], 0 };
      dsp.setFont(&yoUI12b); dsp.setTextColor(C_TXT);
      dsp.setCursor(x+9, y+23); dsp.print(k); dsp.setFont();
    }
    if(r==3){
      dsp.fillRect(SW-46, y, 46, KROWH-2, C_PAN2);
      dsp.setFont(&yoUI9b); dsp.setTextColor(C_TXT);
      dsp.setCursor(SW-34, y+22); dsp.print("<-"); dsp.setFont();
    }
  }
  int16_t y = KY0 + 4*KROWH;
  struct { int16_t x,w; const char* s; uint16_t bg; } ab[4] = {
    { 0,   58, _kbdPage==2?"абв":"?123", C_PAN2 },
    { 60, 118, "пробіл",                 C_PAN2 },
    { 180, 66, "OK",                     C_ACC  },
    { 248, 72, "відміна",                C_PAN2 } };
  for(uint8_t i=0;i<4;i++){
    dsp.fillRect(ab[i].x, y, ab[i].w, KROWH-2, ab[i].bg);
    dsp.setFont(&yoUI8); dsp.setTextColor(ab[i].bg==C_ACC?C_BG:C_TXT);
    char t[20]; snprintf(t, sizeof(t), "%s", utf8Rus(ab[i].s, false));
    int16_t x1,y1; uint16_t tw,th; dsp.getTextBounds(t,0,40,&x1,&y1,&tw,&th);
    dsp.setCursor(ab[i].x + (ab[i].w-(int16_t)tw)/2, y+21); dsp.print(t); dsp.setFont();
  }
}

void YoMenu::_openKbd(char* target, size_t max, bool isPass, const char* title){
  _kbdTarget = target; _kbdMax = max; _kbdIsPass = isPass;
  _kbdTitle = title; _kbdPage = 0;
  if(_cur != PG_KBD) _kbdBack = _cur;   /* з клавіатури на клавіатуру — назад туди ж, звідки прийшли */
  strlcpy(_kbdUndo, target, sizeof(_kbdUndo));   /* щоб відміна справді відміняла */
  _show(PG_KBD);
  char shown[YOM_PASS_LEN+2];
  if(_kbdIsPass){ size_t n=strlen(target); if(n>26) n=26; memset(shown,'*',n); shown[n]='\0'; }
  else snprintf(shown, sizeof(shown), "%s", target);
  _kbdField.setText(shown);
}

/*  ---------- дотики ---------- */

void YoMenu::onRelease(uint16_t x, uint16_t y, uint32_t held){
  if(_fadeStep >= 0) return;               /* сторінка ще набігає */
  _held = held;
  _lastAct = millis();
  if(_isList(_cur)){
    if(_smHold >= 0){                      /* відпустили кнопку списку */
      int8_t b = _smHold; _smHold = -1; _smBtnDraw = 4 + b;   /* 4+ — намалювати відпущеною */
      if(!_smRep){
        if(b == 0) _smStep(-1);
        else if(b == 1) _smStep(1);
        else if(b == 2) _listPick(_smSel);
        /*  назад: у проповідей — меню, у мереж — «параметри»; у режимі
            точки доступу виходу немає, доки мережу не задано  */
        else if(!_apLock) _show(_parent(_cur));
      }
      return;
    }
    if(_dActive){
      _dActive = false;
      if(_dMoved){ _smFling = true; _smFlingT = millis(); return; }   /* вели пальцем — накат, не дотик */
    }
  }
  if(_cur != PG_OFF) _hit(x, y);
}

void YoMenu::onPress(uint16_t x, uint16_t y){
  _lastAct = millis();
  if(_fadeStep >= 0 || !_isList(_cur)) return;
  if(x >= PL_BTN_X - 7){
    int b = ((int)y - 3) / 58; if(b < 0) b = 0; if(b > 3) b = 3;
    _smHold = b; _smHoldT = millis(); _smRep = false; _smBtnDraw = b;
    return;
  }
  /*  палець зупиняє накат чи пружину й бере список там, де він є  */
  _smFling = false; _smAnim = false;
  _dActive = true; _dMoved = false;
  _dY0 = _dLastY = y; _dPos0 = _smCur; _dVel = 0.0f; _dLastT = millis();
}

void YoMenu::onDrag(uint16_t x, uint16_t y){
  (void)x;
  if(!_dActive || !_isList(_cur)) return;
  int16_t dy = (int16_t)y - _dY0;
  if(!_dMoved && abs(dy) > 8) _dMoved = true;
  if(!_dMoved) return;
  int n = _listCount(); if(n < 1) n = 1;
  float pos = _dPos0 - dy / (float)PL_ROW_H;
  /*  за краями список пружинить, а не їде далі  */
  if(pos < 1.0f) pos = 1.0f - (1.0f - pos) * 0.35f;
  if(pos > n)    pos = n + (pos - n) * 0.35f;
  uint32_t now = millis();
  float dt = (now - _dLastT) / 1000.0f;
  if(dt > 0.004f){
    float v = -((int16_t)y - _dLastY) / (float)PL_ROW_H / dt;
    _dVel = _dVel * 0.6f + v * 0.4f;
    _dLastY = y; _dLastT = now;
  }
  _smCur = pos;
  _smDragDirty = true;
}

void YoMenu::_smStep(int8_t d){
  int n = _listCount();
  int16_t to = (int16_t)roundf(_smAnim ? (float)_smSel : _smCur) + d;
  if(to < 1 || to > n) return;
  _smGo(to);
}

void YoMenu::_hit(uint16_t x, uint16_t y){
  /*  Список проповідей — на весь екран, як список станцій: шапки немає,
      тож дотики розбираємо до перевірок шапки.  */
  if(_isList(_cur)){
    int16_t n = _listCount();
    if(x >= PL_BTN_X - 7) return;              /* кнопки — у onPress/onRelease */
    if(_cur == PG_SERM){
      if(sermons.loading()) return;
      if(n == 0){ sermons.fetch(); _smDirty = true; return; }
    }
    if(y < PL_TOP) return;
    int r = ((int)y - PL_TOP) / PL_ROW_H;
    if(r >= PL_ROWS) return;
    int idx = _smSel - PL_CUR + r;
    if(r == PL_CUR){ _listPick(_smSel); return; }        /* дотик по смузі — вибрати */
    if(idx >= 1 && idx <= n) _smGo(idx);
    return;
  }
  /*  шапка: шестерня ліворуч, стрілка повернення праворуч  */
  /*  Зона стрілки повернення на десять пікселів вища за саму шапку. На
      плеєрі шестерня має зону 36x38 і натискається легко, а тут було 39x28 —
      палець, що лягав трохи нижче стрілки, у меню заходив, але не виходив.
      Смуга 28..37 праворуч вільна на всіх сторінках, тож віддаємо її стрілці. */
  if(y < HDR + 10 && x > SW-40){
    if(_cur == PG_KBD){
      _kbdNext = 0;
      if(_kbdTarget) strlcpy(_kbdTarget, _kbdUndo, _kbdMax);
      if(_kbdBack==PG_WIFI) _showWifi();
      _show(_kbdBack);
      return;
    }
    /*  Стрілка — на рівень вгору: зі сторінки в її меню, з меню на плеєр.  */
    if(_apLock && _cur != PG_WSAVED) return;    /* із відомих мереж вихід є завжди */
    if(_cur == PG_HOME) close();
    else _show(_parent(_cur));
    return;
  }
  if(y < HDR){
    if(_cur == PG_KBD){
      /*  На екрані клавіатури стрілка теж малюється, але дотик у шапці не
          оброблявся зовсім — виходило, що кнопка є, а не працює. Тепер
          вона повертає назад, як і кнопка скасування знизу.  */
      if(x > SW-40){
        if(_kbdTarget) strlcpy(_kbdTarget, _kbdUndo, _kbdMax);
        if(_kbdBack==PG_WIFI) _showWifi();
        _show(_kbdBack);
      }
      return;
    }
    return;                                    /* значок ліворуч — лише значок */
  }

  if(_cur == PG_KBD){
    size_t l = strlen(_kbdTarget);
    int16_t ay = KY0 + 4*KROWH;
    if(y >= ay){
      if(x < 58){ _kbdPage = (_kbdPage==2)?0:2; _drawKbdKeys(); return; }
      if(x < 180){ if(l+1 < _kbdMax){ _kbdTarget[l]=' '; _kbdTarget[l+1]='\0'; } }
      else if(x < 248){
        /*  OK: лишаємо набране. Раніше тут, як і у відміні, викликався
            _loadWifi(), який перечитує поля з налаштувань — тобто набраний
            пароль щоразу губився, і ввести мережу з екрана було неможливо. */
        if(_kbdNext == 1 && _wSsid[0]){            /* назву є — тепер пароль */
          _kbdNext = 2;
          snprintf(_kbdTitleBuf, sizeof(_kbdTitleBuf), "пароль: %s", _wSsid);
          _openKbd(_wPass, YOM_PASS_LEN, true, _kbdTitleBuf);   /* пароль — зірочками: його видно з-за плеча */
          return;
        }
        if(_kbdNext == 2){ _kbdNext = 0; _wifiConnect(); return; }
        _kbdNext = 0;
        if(_kbdBack==PG_WIFI) _showWifi();
        _show(_kbdBack); return;
      }
      else {
        /*  Відміна: повертаємо те, що було до правки.  */
        _kbdNext = 0;
        if(_kbdTarget) strlcpy(_kbdTarget, _kbdUndo, _kbdMax);
        if(_kbdBack==PG_WIFI) _showWifi();
        _show(_kbdBack); return;
      }
    }else if(y >= KY0){
      uint8_t r = (y - KY0)/KROWH;
      if(r > 3) return;
      const char* row = KROWS[_kbdPage][r];
      uint8_t len = strlen(row);
      int16_t x0 = (r==2) ? (SW - len*KKW)/2 : (r==3 ? 48 : 0);
      if(r==3 && x < 46){ _kbdPage = (_kbdPage==1)?0:1; _drawKbdKeys(); return; }
      if(r==3 && x >= SW-46){ if(l>0) _kbdTarget[l-1]='\0'; }
      else{
        if((int)x < x0) return;
        uint8_t c = (x - x0)/KKW;
        if(c >= len) return;
        if(l+1 < _kbdMax){ _kbdTarget[l]=row[c]; _kbdTarget[l+1]='\0'; }
      }
    }else return;
    /*  оновлюється лише рядок вводу  */
    char shown[YOM_PASS_LEN+2];
    if(_kbdIsPass){ size_t n=strlen(_kbdTarget); if(n>26) n=26; memset(shown,'*',n); shown[n]='\0'; }
    else snprintf(shown, sizeof(shown), "%s", _kbdTarget);
    _kbdField.setText(shown);
    return;
  }

  switch(_cur){
    case PG_EQ: {
      /*  Без нижньої межі смуга під шапкою (28..39) потрапляла в перший
          повзунок, і дотик поруч із заголовком міняв баланс.  */
      if((int)y < 40) return;
      uint8_t i = (y - 40)/44;
      if(i > 3) return;
      int v = _eq[i].valueAt(x);
      _eq[i].setValue(v);
      int8_t b=config.store.bass, m=config.store.middle, t=config.store.trebble;
      if(i==0) config.setBalance((int8_t)v);
      if(i==1) config.setTone(b, m, (int8_t)v);
      if(i==2) config.setTone(b, (int8_t)v, t);
      if(i==3) config.setTone((int8_t)v, m, t);
      break;
    }
    case PG_WSAVED: {
      uint8_t n = 0;
      for(uint8_t k = 0; k < YOM_SSIDS; k++) if(_ssid[k][0]) n++;
      uint8_t i = (y >= WS_TOP) ? (y - WS_TOP) / WS_ROW : 255;
      /*  повз рядки або в проміжок між ними — просто знімаємо питання  */
      if(i >= n || y >= WS_TOP + i * WS_ROW + WS_H){
        if(_wsArm >= 0){ _wsArm = -1; _favDirty = true; }
        return;
      }
      if((int)x >= CX + CW - 36){                       /* × — забути */
        if(_wsArm == (int8_t)i){
          _wsArm = -1;
          for(uint8_t k = i; k + 1 < YOM_SSIDS; k++){
            strlcpy(_ssid[k], _ssid[k+1], YOM_SSID_LEN);
            strlcpy(_pass[k], _pass[k+1], YOM_PASS_LEN);
          }
          _ssid[YOM_SSIDS-1][0] = 0; _pass[YOM_SSIDS-1][0] = 0;
          _savedWrite();
        }else{ _wsArm = i; _wsArmT = millis(); }
        _favDirty = true;
        return;
      }
      if((int)x >= CX + CW - 72 && i > 0){              /* ↑ — підняти першою */
        char a[YOM_SSID_LEN], b[YOM_PASS_LEN];
        strlcpy(a, _ssid[i], sizeof(a)); strlcpy(b, _pass[i], sizeof(b));
        for(int8_t k = i; k > 0; k--){
          strlcpy(_ssid[k], _ssid[k-1], YOM_SSID_LEN);
          strlcpy(_pass[k], _pass[k-1], YOM_PASS_LEN);
        }
        strlcpy(_ssid[0], a, YOM_SSID_LEN); strlcpy(_pass[0], b, YOM_PASS_LEN);
        _wsArm = -1;
        config.setLastSSID(1);
        _savedWrite();
        _favDirty = true;
        return;
      }
      if(_wsArm >= 0){ _wsArm = -1; _favDirty = true; }
      return;
    }
    case PG_TIME: {
      int8_t h = config.store.tzHour, m = config.store.tzMin;
      /*  Стрілки намальовані на 100..130 і 200..230. Раніше зона тягнулась
          на всю ширину, і дотик деінде на цьому рівні міняв час.  */
      bool hour;
      if((int)x >= CX+70 && (int)x <= CX+120)       hour = true;
      else if((int)x >= CX+170 && (int)x <= CX+220) hour = false;
      else return;
      if(y>=74 && y<100)       { if(hour) h++; else m+=15; }
      else if(y>=150 && y<180) { if(hour) h--; else m-=15; }
      else return;
      if(h>14) h=-12; if(h<-12) h=14;
      if(m>45) m=0;   if(m<0)   m=45;
      config.setTimezone(h, m);
      if(strlen(config.store.sntp1)>0)
        configTime(h*3600+m*60, config.getTimezoneOffset(), config.store.sntp1,
                   strlen(config.store.sntp2)>0?config.store.sntp2:nullptr);
      timekeeper.forceTimeSync = true;
      _live = true;             /* напис оновить задача дисплея, не цей цикл */
      break;
    }
    case PG_SYS: {
      if(y >= SY_LED_Y-4 && y < SY_LED_Y+34){
        int8_t i = _ledSeg.indexAt(x);
        if(i < 0) return;
        extras.s.ledMode = i; extras.changed();
        _ledSeg.setSel(i);
        return;
      }
      if(y>=42 && y<74){
        bool on = !_chkStart.value();
        _chkStart.setValue(on);
        config.saveValue(&config.store.smartstart, static_cast<uint8_t>(on?1:2));
      }else if(y>=78 && y<110){
        bool on = !_chkInfo.value();
        _chkInfo.setValue(on);
        config.saveValue(&config.store.audioinfo, static_cast<bool>(on));
      }else if(y>=114 && y<150){
        /*  перемикач джерела — доданий мною, у Nextion його немає  */
        if(extras.s.noSd) return;              /* картку вимкнено в «розробнику» */
        config.changeMode();
        _chkSrc.setValue(config.getMode() == PM_SDCARD);
      }
      break;
    }
    /*  Нові сторінки лише міняють значення й піднімають _live: написи
        оновить задача дисплея, як і всюди в меню.  */
    case PG_HOME: {
      if(y < HM_Y(0)) return;
      int c = ((int)x - 6) / 104; if(c < 0) c = 0; if(c > 2) c = 2;
      int r = ((int)y - HM_Y(0)) / 69; if(r > 2) r = 2;
      switch(r * 3 + c){
        case HM_STATIONS: _afterClose = 1; close(); break;
        case HM_FAV:      _show(PG_FAV); break;
        case HM_SERM:
          if(!sermons.loading() && sermons.count() == 0) sermons.fetch();
          _smSel = sermons.playing() >= 0 ? sermons.playing() + 1 : 1;
          _show(PG_SERM); break;
        case HM_SRC:
          if(extras.s.noSd){ _setMsg("картку вимкнено"); break; }
          config.changeMode(); close(); break;
        case HM_REC:
          if(recorder.active()) recorder.stop();
          else if(!recorder.start()) _setMsg(recorder.lastError());
          _favDirty = true; break;
        case HM_ALARM:    _show(PG_SLEEP); break;
        case HM_SCREEN:   _show(PG_NIGHT); break;
        case HM_SOUND:    _show(PG_EQ); break;
        default:          _show(PG_SETUP); break;
      }
      break;
    }
    case PG_FAV: {
      if(y >= 218 && x >= FV_CHK_X - 12){
        extras.s.favHide = !extras.s.favHide; extras.changed();
        _setMsg(extras.s.favHide ? "вимкнено" : "увімкнено");
        _favDirty = true;
        break;
      }
      if(y >= FV_Y(0) && y < FV_Y(4) + FV_H + 2){
        uint8_t c = x < 160 ? 0 : 1;
        uint8_t r = (y - FV_Y(0)) / 62;
        if(r > 2) return;
        uint8_t i = r * 2 + c;
        if(extras.fav[i].url[0]){
          if(_held >= 700){ extras.favClear(i); _setMsg("прибрано з обраного"); _favDirty = true; }
          else if(extras.favPlay(i)) close();
          else _setMsg("цієї станції вже нема в списку");
        }else{
          if(extras.favSetCurrent(i)){ _setMsg("додано"); _favDirty = true; }
          else _setMsg("спершу увімкніть радіостанцію");
        }
      }
      break;
    }
    case PG_DEV: {
      ExtStore& s = extras.s;
      if(y >= 34 && y < 64){ s.noBat = !s.noBat; _chkBat.setValue(!s.noBat); extras.changed(); }
      else if(y >= 64 && y < 94){ s.noIp = !s.noIp; _chkIp.setValue(!s.noIp); extras.changed(); }
      else if(y >= 94 && y < 124){
        s.noSd = !s.noSd; _chkSdEn.setValue(!s.noSd); extras.changed();
        if(s.noSd){ recorder.stop(); if(config.getMode() == PM_SDCARD) config.changeMode(PM_WEB); }
      }
      else if(y >= 124 && y < 162){
        uint16_t n = logos.forget();
        char m[48]; snprintf(m, sizeof(m), "шукатиму знову: %u", n);
        _setMsg(m); _favDirty = true;
        display.forceLogo();
      }
      else if(y >= 180) _show(PG_DAC);
      break;
    }
    case PG_DAC: {
      if(y < 32) return;
      int i = ((int)y - 32) / 41;
      if(i > 4) return;
      _dacSel = i; _dacAlt = false; _show(PG_DACINFO);
      break;
    }
    case PG_DACINFO: {
      if(y >= 208 && _dacSel == 4){ _dacAlt = !_dacAlt; _favDirty = true; break; }
      if(y >= 208 && _dacSel != 4 && extras.s.dac != _dacSel){
        extras.s.dac = _dacSel; extras.changed(); extras.applyDac();
        _favDirty = true;                        /* перемалювати кнопку */
      }
      break;
    }
    case PG_POWER: {
      if(_pwrGo >= 0) return;
      int8_t i = -1;
      for(uint8_t k = 0; k < 2; k++) if(y >= PW_Y(k) && y < PW_Y(k) + PW_H) i = k;
      if(i < 0) return;
      if(_pwrArm == i && millis() - _pwrArmT < 4000) _pwrGo = i;
      else { _pwrArm = i; _pwrArmT = millis(); }
      _favDirty = true;
      break;
    }
    case PG_SETUP: {
      if(y < HM_Y(0)) return;
      int c = ((int)x - 6) / 104; if(c < 0) c = 0; if(c > 2) c = 2;
      int r = y < 136 ? 0 : 1;
      static const int8_t go[6] = { PG_INFO, PG_WIFI, PG_TIME, PG_SYS, PG_DEV, PG_POWER };
      if(go[r * 3 + c] >= 0) _show(go[r * 3 + c]);
      break;
    }
    case PG_SLEEP: {
      ExtStore& s = extras.s;
      if(y >= SL_SEG_Y-4 && y < SL_SEG_Y+34){
        int8_t i = _slSeg.indexAt(x);
        if(i < 0) return;
        extras.setSleep(SLEEP_MIN[i]);
      }else if(y >= AL_CHK_Y-8 && y < AL_ROW_Y-2){
        s.alarmOn = !s.alarmOn; extras.changed();
      }else if(y >= AL_ROW_Y-2 && y < AL_ROW_Y+38){
        if((int)x < CX+38)                        { s.alarmH = (s.alarmH + 23) % 24; }
        else if((int)x >= CX+86  && (int)x < CX+130) { s.alarmH = (s.alarmH + 1) % 24; }
        else if((int)x >= CX+134 && (int)x < CX+178) { s.alarmM = (s.alarmM + 55) % 60; }
        else if((int)x >= CX+226)                 { s.alarmM = (s.alarmM + 5) % 60; }
        else return;
        s.alarmM -= s.alarmM % 5;
        s.alarmOn = 1;             /* виставили час — отже, будильник потрібен */
        extras.changed();
      }else if(y >= AL_DAYS_Y-4 && y < AL_DAYS_Y+32){
        int8_t i = _alDays.indexAt(x);
        if(i < 0) return;
        s.alarmDays = i; extras.changed();
      }else return;
      _live = true;
      break;
    }
    case PG_NIGHT: {
      ExtStore& s = extras.s;
      if(y >= NT_BRI_Y-4 && y < NT_SAVE_Y-14){              /* робоча яскравість */
        int v = _bright.valueAt(x);
        _bright.setValue(v);
        config.store.brightness = (uint8_t)v;
        config.setBrightness(true);
        return;
      }else if(y >= NT_SAVE_Y-6 && y < NT_SAVE_Y+30){
        int8_t i = _saveSeg.indexAt(x);
        if(i < 0) return;
        s.batSave = i; extras.changed();
      }else if(y >= NT_CHK_Y-6 && y < NT_ROW_Y-4){
        s.nightOn = !s.nightOn; extras.changed();
      }else if(y >= NT_ROW_Y-4 && y < NT_ROW_Y+34){
        if((int)x < CX+34)                          s.nightFrom = (s.nightFrom + 47) % 48;
        else if((int)x >= CX+100 && (int)x < CX+144) s.nightFrom = (s.nightFrom + 1) % 48;
        else if((int)x >= CX+144 && (int)x < CX+188) s.nightTo = (s.nightTo + 47) % 48;
        else if((int)x >= CX+254)                   s.nightTo = (s.nightTo + 1) % 48;
        else return;
        extras.changed();
      }else if(y >= NT_LVL_Y-4 && y < NT_LVL_Y+34){
        int v = _nLevel.valueAt(x);
        _nLevel.setValue(v);
        s.nightLevel = (uint8_t)v; extras.changed();
      }else return;
      _live = true;
      break;
    }
    default: break;
  }
}

#endif
