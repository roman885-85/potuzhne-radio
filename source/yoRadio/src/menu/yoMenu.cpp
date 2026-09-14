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
#include "../extras/yoSfx.h"
#include "../extras/yoExtras.h"
#include "../extras/yoSermons.h"
#include "../extras/yoRecorder.h"
#include "../extras/yoLogos.h"
#include "../extras/yoDsp.h"
#include "../extras/yoMic.h"
#include "../displays/fonts/yoUI11.h"
#include "../displays/fonts/yoUI8.h"
#include <WiFi.h>

extern DspCore dsp;
YoMenu yomenu;

/*  Меню малює в кадр у пам'яті (menu/uicanvas): на екран іде лише змінене, а
    текст — згладженими шрифтами Roboto замість 1-бітної Verdana. Далі в цьому
    файлі «dsp» — це кадр, а шрифти меню — їхні згладжені двійники тих самих
    розмірів, тож розкладка сторінок лишається.  */
#include "uicanvas.h"
#include "../displays/fonts/aa/aaUI8.h"
#include "../displays/fonts/aa/aaUI9.h"
#include "../displays/fonts/aa/aaUI9b.h"
#include "../displays/fonts/aa/aaUI11.h"
#include "../displays/fonts/aa/aaUI12b.h"
#define dsp     ui
#define yoUI8   aaUI8
#define yoUI9   aaUI9
#define yoUI9b  aaUI9b
#define yoUI11  aaUI11
#define yoUI12b aaUI12b
#include "../displays/fonts/aa/aaUI6.h"
#include "../displays/fonts/aa/aaUI26b.h"

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
#define WIFI_PAD   56          /* смуга праворуч у рядку мережі: замок і сигнал */
/*  Wi-Fi: хід підключення  */
#define WC_BTN_Y   170
#define WC_BTN_H   44
/*  Wi-Fi: знайома мережа — три дії  */
#define WP_Y(i)    (44 + (i) * 62)
#define WP_H       54
/*  Wi-Fi: відомі мережі  */
#define WS_TOP     44
#define WS_ROW     32
#define WS_H       28
/*  Плитки головного меню й параметрів: 3×3  */
#define HM_X(i)   (6 + ((i) % 3) * 104)
#define HM_Y(i)   (33 + ((i) / 3) * 69)
#define HM_W      100
#define HM_H      65
/*  Звук: еквалайзер  */
#define EQ_TOP_Y   32
#define EQ_TOP_H   28
#define EQ_COL0    62        /* верх області смуг */
#define EQ_VAL_Y   74        /* підпис значення */
#define EQ_T0      82
#define EQ_T1      182
#define EQ_ZERO    132
#define EQ_FRQ_Y   198
#define EQ_BTN_Y   206
#define EQ_BTN_H   30
#define C_ROOM     0x07FF    /* поправка під кімнату — бірюзова */
#define C_VOICE    0x07E0

/*  ---------- стиль: заокруглені плашки й згладжені значки ----------
    Усе малюється в кадр меню (uicanvas), тож краї фігур змішуються з тлом.
    Кути плашок рахуються над відомим тлом (bg), а не над тим, що в кадрі:
    кнопка, перемальована іншим кольором, не лишає на кутах слідів.  */
#define R_BTN   7            /* кнопки */
#define R_TILE  9            /* плитки */

static void icGear(float cx, float cy, float s, uint16_t c){
  dsp.arcAA(cx, cy, 5.2f * s, 2.3f * s, c);
  for(uint8_t k = 0; k < 8; k++){
    float a = k * 0.785398f;
    dsp.lineAA(cx + 7.4f * s * sinf(a), cy - 7.4f * s * cosf(a), cx + 9.4f * s * sinf(a), cy - 9.4f * s * cosf(a), 2.7f * s, c);
  }
}
static void icMenu(float cx, float cy, uint16_t c){
  for(uint8_t i = 0; i < 3; i++) dsp.lineAA(cx - 8, cy - 6 + i * 6, cx + 8, cy - 6 + i * 6, 2.4f, c);
}
static void icStar(float cx, float cy, float R, uint16_t c){
  float xy[20];
  for(uint8_t i = 0; i < 10; i++){
    float a = -1.5708f + i * 0.62832f;
    float r = (i % 2) ? R * 0.45f : R;
    xy[2 * i] = cx + r * cosf(a); xy[2 * i + 1] = cy + r * sinf(a);
  }
  dsp.polyAA(xy, 10, c);
}
/*  Хвилі мережі над точкою (cx, cy) — точка знизу.  */
static void icWifi(float cx, float cy, float s, uint16_t c){
  for(uint8_t k = 1; k <= 3; k++) dsp.arcAA(cx, cy, 5.5f * k * s, 2.4f * s, c, -45, 45);
  dsp.fillCircleAA(cx, cy, 2.6f * s, c);
}
static void icPower(float cx, float cy, float r, float wd, uint16_t c){
  dsp.arcAA(cx, cy, r, wd, c, 38, 322);
  dsp.lineAA(cx, cy - r - 2.5f, cx, cy - r * 0.25f, wd, c);
}
static void icCross(float cx, float cy, float r, float wd, uint16_t c){
  dsp.lineAA(cx - r, cy - r, cx + r, cy + r, wd, c);
  dsp.lineAA(cx - r, cy + r, cx + r, cy - r, wd, c);
}
/*  «›» — перехід далі.  */
static void icNext(float x, float cy, uint16_t c){
  dsp.lineAA(x, cy - 5.5f, x + 5.5f, cy, 2.3f, c);
  dsp.lineAA(x + 5.5f, cy, x, cy + 5.5f, 2.3f, c);
}
/*  Перша літера — велика: «Параметри», «Звук» (CP1251, як віддає utf8Rus).  */
static void capFirst(char* t){
  uint8_t c = (uint8_t)t[0];
  if(c >= 'a' && c <= 'z') t[0] = c - 0x20;
  else if(c >= 0xE0) t[0] = c - 0x20;
  else if(c == 0xB3) t[0] = (char)0xB2;          /* і → І */
  else if(c == 0xBF) t[0] = (char)0xAF;          /* ї → Ї */
  else if(c == 0xBA) t[0] = (char)0xAA;          /* є → Є */
}


static const uint16_t SLEEP_MIN[5] = { 0, 15, 30, 60, 90 };
static const char* const SLEEP_LBL[5] = { "вимк", "15", "30", "60", "90" };
static const char* const DAYS_LBL[2]  = { "щодня", "будні" };
static const char* const LED_LBL[3]   = { "вимк", "стан", "музика" };
static const char* const SAVE_LBL[5]  = { "вимк", "10 с", "15 с", "30 с", "60 с" };

static WidgetConfig wc(uint16_t l, uint16_t t, WidgetAlign a=WA_LEFT){ WidgetConfig c; c.left=l; c.top=t; c.textsize=1; c.align=a; return c; }

/*  ---------- побудова сторінок ---------- */

void YoMenu::_build(){
  if(_built) return;
  ui.begin();
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


  /*  WI-FI: п'ять слотів SSID / пароль  */
  for(uint8_t i=0;i<YOM_SSIDS;i++){
    _wifiS[i].init(wc(CX+14,  38+i*29), &yoUI9, 136, 22, C_TXT, C_PANEL);
    _wifiP[i].init(wc(CX+158, 38+i*29), &yoUI9, 106, 22, C_TXT, C_PANEL);
  }

  /*  TIMEZONE: поточний час і два лічильники  */
  _tmNow.init(wc(CX, 40, WA_CENTER), &yoUI12b, CW, 24, C_ACC, C_BG);
  _tmH.init(wc(CX+60,  110, WA_CENTER), &yoUI12b, 70, 30, C_TXT, C_PANEL); _tmH.setInset(R_BTN);
  _tmM.init(wc(CX+160, 110, WA_CENTER), &yoUI12b, 70, 30, C_TXT, C_PANEL); _tmM.setInset(R_BTN);
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
  _alH.init(wc(CX+38,  AL_ROW_Y, WA_CENTER), &yoUI12b, 48, 30, C_TXT, C_PANEL); _alH.setInset(R_BTN);
  _alM.init(wc(CX+178, AL_ROW_Y, WA_CENTER), &yoUI12b, 48, 30, C_TXT, C_PANEL); _alM.setInset(R_BTN);
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
  _nFrom.init(wc(CX+34,  NT_ROW_Y, WA_CENTER), &yoUI11, 66, 26, C_TXT, C_PANEL); _nFrom.setInset(R_BTN);
  _nTo.init  (wc(CX+188, NT_ROW_Y, WA_CENTER), &yoUI11, 66, 26, C_TXT, C_PANEL); _nTo.setInset(R_BTN);
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
  _chkBat .init(wc(CX, 38),  &yoUI9, CW, C_TXT, C_BG, C_ACC); _chkBat .setLabel("батарея");
  _chkIp  .init(wc(CX, 64),  &yoUI9, CW, C_TXT, C_BG, C_ACC); _chkIp  .setLabel("IP-адреса на екрані");
  _chkSdEn.init(wc(CX, 90),  &yoUI9, CW, C_TXT, C_BG, C_ACC); _chkSdEn.setLabel("картка пам'яті");
  /*  РОЗРОБНИК → ЗВУКИ Й ЗАСТАВКА  */
  _chkSplash.init(wc(CX, 40),  &yoUI9, CW, C_TXT, C_BG, C_ACC); _chkSplash.setLabel("анімована заставка");
  _sldSplash.init(wc(CX, 64),  &yoUI9, CW, 0, 100, C_TXT, C_BG, C_ACC); _sldSplash.setLabel("звук заставки");
  _chkSfx   .init(wc(CX, 108), &yoUI9, CW, C_TXT, C_BG, C_ACC); _chkSfx.setLabel("звуки подій");
  _sldSfx   .init(wc(CX, 132), &yoUI9, CW, 0, 100, C_TXT, C_BG, C_ACC); _sldSfx.setLabel("гучність звуків подій");
  _pg[PG_DEVSND]->addWidget(&_chkSplash);
  _pg[PG_DEVSND]->addWidget(&_sldSplash);
  _pg[PG_DEVSND]->addWidget(&_chkSfx);
  _pg[PG_DEVSND]->addWidget(&_sldSfx);
  _pg[PG_DEV]->addWidget(&_chkBat);
  _pg[PG_DEV]->addWidget(&_chkIp);
  _pg[PG_DEV]->addWidget(&_chkSdEn);

  /*  Клавіатура: єдиний віджет — рядок вводу. Самі клавіші статичні,
      їх досить намалювати один раз при відкритті.  */
  _kbdField.init(wc(8, 32), &yoUI12b, SW-74, 26, C_TXT, C_PANEL);
  _kbdField.setInset(R_BTN);
  _pg[PG_KBD]->addWidget(&_kbdField);

  _buildSound();                     /* звук і мікрофон */
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

static void fitText(char* t, uint16_t w);      /* оголошення: сама вона нижче */

void YoMenu::_chrome(const char* title, uint8_t icon){
  /*  Шапка — темна: жовтий значок і назва, біла стрілка повернення, під
      ними тонка лінія, що гасне праворуч. Раніше — суцільна жовта смуга.  */
  dsp.fillRect(0, 0, SW, HDR, C_BG);
  if(icon == 1) icStar(14, 14, 10, C_ACC);
  else if(icon == 2) icMenu(14, 14, C_ACC);
  else icGear(14, 14, 1.0f, C_ACC);
  dsp.setFont(&yoUI12b); dsp.setTextSize(1); dsp.setTextColor(C_ACC);
  /*  Довга назва (а надто «пароль: <мережа>») наповзала на стрілку
      повернення праворуч — обрізаємо по місцю, що лишилось.  */
  char tt[64];
  snprintf(tt, sizeof(tt), "%s", utf8Rus(title, false));
  if(title != _wSsid) capFirst(tt);            /* назву мережі — як є */
  fitText(tt, SW - 34 - 46);
  dsp.setCursor(34, 20); dsp.print(tt);
  _titleEnd = dsp.getCursorX();              /* підказку в шапці ставимо лише правіше */
  dsp.setFont();
  /*  У режимі точки доступу виходу з налаштувань немає, доки мережу не
      задано, тож і стрілку не малюємо: намальована, але мертва кнопка
      виглядає поламаною.  */
  if(!(_apLock && _cur != PG_KBD && _cur != PG_WSAVED && _cur != PG_WPICK && _cur != PG_WCONN)){
    dsp.lineAA(SW - 11, 14, SW - 26, 14, 2.2f, C_TXT);
    dsp.lineAA(SW - 20, 8, SW - 26, 14, 2.2f, C_TXT);
    dsp.lineAA(SW - 26, 14, SW - 20, 20, 2.2f, C_TXT);
  }
  for(int16_t x = 0; x < SW; x++){
    float k = 1.0f - (float)x / SW;
    dsp.drawPixel(x, HDR - 1, dsp.blend(C_BG, C_ACC, (uint8_t)(170 * k * k)));
  }
}

/*  ---------- перемикання сторінок ---------- */

/*  Перемикання сторінки лише запам'ятовується. Саме малювання — і наплив,
    і сама сторінка — виконує задача дисплея: у головному циклі, звідки
    приходить дотик, крутиться декодер звуку, і пауза там чутна.  */
void YoMenu::_show(int8_t p){
  _lastAct = millis();
  if(_cur == PG_OFF) ui.setActive(true);          /* меню відкривається — малюємо в кадр */
  Serial.printf("##MENU#\tсторінка %d -> %d\n", (int)_cur, (int)p);
  /*  З екрана підсумку спроби пішли — спробу закриваємо тут же: інакше
      повернення в збережені мережі стоїть, доки її хтось не закриє.  */
  if(_cur == PG_WCONN && p != PG_WCONN && network.tryState() >= TRY_OK) network.tryClear();
  network.tryHeld = (p == PG_WCONN);
  /*  Пошук мереж і спроби повернутися в мережу живуть в одному радіомодулі:
      поки людина вибирає мережу, спроби спинено — інакше ні пошуку, ні
      підключення не діждешся.  */
  network.pauseSta(p == PG_WIFI || p == PG_WSAVED || p == PG_WPICK || p == PG_WCONN ||
                   (p == PG_KBD && (_kbdBack == PG_WIFI || _kbdBack == PG_WSAVED || _kbdBack == PG_WPICK)));
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
        if(_cur == PG_WCONN && network.tryState() >= TRY_OK) network.tryClear();
        network.tryHeld = false;
        if(_cur >= 0 && _pg[_cur]) _pg[_cur]->setActive(false);
        _cur = PG_OFF;
        dsp.setFont();
        ui.setActive(false);                     /* далі екран — плеєра, напряму */
        display.forceRedraw();
        /*  «Станції» з меню: список малюємо ще в темряві, тож перехід один,
            а не два поспіль (меню → плеєр → список).  */
        if(_afterClose == 1) display.openStationsNow();
        if(_afterClose == 2) display.splashDemo(7000);
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
    dsp.box(8, 32, SW-74, 26, R_BTN, C_PANEL, C_BG);   /* рядок вводу */
    _kbShown = -1; _kbPopX = -1; _kbDirty = 0;
    _drawKbdKeys();
    _kbdRefresh();                 /* текст — до активації: віджет намалює себе вже з ним */
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
  }else if(p == PG_WCONN){
    _chrome(_wSsid, 2);
    _wcShown = -1;
    _drawWconn();
    _wcShown = (int8_t)network.tryState();
  }else if(p == PG_WPICK){
    _chrome(_wSsid, 2);
    _wpArm = -1;
    _drawWpick();
    _favDirty = false;
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
  }else if(p == PG_DEVSND){
    _chrome("звуки", 0);
    _chkSplash.setValue(!extras.s.splashOff);
    _sldSplash.setValue(extras.s.splashVol);
    _chkSfx.setValue(extras.s.sfxOn);
    _sldSfx.setValue(extras.s.sfxVol);
    _drawDevSnd();
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
  }else if(_isSound(p)){
    _paintSound(p);
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
        dsp.triAA(x+35, 76, x+21, 96, x+49, 96, C_ACC);           /* ▲ */
        dsp.triAA(x+35, 176, x+21, 156, x+49, 156, C_ACC);        /* ▼ */
        dsp.box(x, 110, 70, 30, R_BTN, C_PANEL, C_BG);
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
      dsp.box(CX+38, AL_ROW_Y, 48, 34, R_BTN, C_PANEL, C_BG);
      _stepper(CX+90,  AL_ROW_Y, 34, 34, true);
      dsp.fillCircleAA(CX+132, AL_ROW_Y+12, 2.2f, C_TXT);
      dsp.fillCircleAA(CX+132, AL_ROW_Y+23, 2.2f, C_TXT);
      _stepper(CX+140, AL_ROW_Y, 34, 34, false);
      dsp.box(CX+178, AL_ROW_Y, 48, 34, R_BTN, C_PANEL, C_BG);
      _stepper(CX+230, AL_ROW_Y, 34, 34, true);
    }
    if(p == PG_NIGHT){
      dsp.setFont(&yoUI9); dsp.setTextSize(1); dsp.setTextColor(C_DIM);
      dsp.setCursor(CX, NT_SAVE_Y-6); dsp.print(utf8Rus("без зарядника пригасити через", false));
      dsp.setFont();
      /*  два однакові блоки часу з тире між ними: [−][22:00][+] — [−][07:00][+]  */
      _stepper(CX,     NT_ROW_Y, 32, 30, false);
      dsp.box(CX+34,  NT_ROW_Y, 66, 30, R_BTN, C_PANEL, C_BG);
      _stepper(CX+102, NT_ROW_Y, 32, 30, true);
      dsp.lineAA(CX+140, NT_ROW_Y+15, CX+148, NT_ROW_Y+15, 2.2f, C_DIM);
      _stepper(CX+154, NT_ROW_Y, 32, 30, false);
      dsp.box(CX+188, NT_ROW_Y, 66, 30, R_BTN, C_PANEL, C_BG);
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
  dsp.box(x, y, w, h, R_BTN, C_PAN2, C_BG);
  float cx = x + w/2.0f, cy = y + h/2.0f;
  dsp.lineAA(cx-6, cy, cx+6, cy, 2.6f, C_ACC);
  if(plus) dsp.lineAA(cx, cy-6, cx, cy+6, 2.6f, C_ACC);
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
  dsp.fillRect(x0, 0, SW-40-x0, HDR-1, C_BG);
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
  dsp.setFont(&yoUI9); dsp.setTextSize(1); dsp.setTextColor(C_DIM);
  fitText(t, SW-44-x0);
  dsp.setCursor(SW-44 - textW(t), 19); dsp.print(t);
  dsp.setFont();
}

void YoMenu::_drawTile(uint8_t i){
  int16_t x = FV_X(i), y = FV_Y(i);
  const FavItem& f = extras.fav[i];
  if(!f.url[0]){
    dsp.frame(x, y, FV_W, FV_H, R_TILE, 1.2f, C_PAN2, C_BG, C_BG);
    /*  «+» і підказка в два рядки: в один не влазило  */
    int16_t cx = x + FV_W / 2;
    dsp.lineAA(cx - 6.5f, y + 14, cx + 6.5f, y + 14, 2.2f, C_DIM); dsp.lineAA(cx, y + 7.5f, cx, y + 20.5f, 2.2f, C_DIM);
    dsp.setFont(&yoUI8); dsp.setTextSize(1); dsp.setTextColor(C_DIM);
    char t[40]; snprintf(t, sizeof(t), "%s", utf8Rus("додати станцію,", false));
    dsp.setCursor(cx - (int16_t)textW(t) / 2, y + 36); dsp.print(t);
    snprintf(t, sizeof(t), "%s", utf8Rus("що грає", false));
    dsp.setCursor(cx - (int16_t)textW(t) / 2, y + 51); dsp.print(t);
    dsp.setFont();
    return;
  }
  bool on = extras.favPlaying() == (int8_t)i;
  if(on) dsp.frame(x, y, FV_W, FV_H, R_TILE, 2.0f, C_ACC, C_PANEL, C_BG);
  else    dsp.box(x, y, FV_W, FV_H, R_TILE, C_PANEL, C_BG);
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
  dsp.box(6, y, FV_W, FV_BOT_H, R_TILE, C_PAN2, C_BG);
  dsp.lineAA(22, y+13, 22, y+43, 4.0f, C_ACC);
  dsp.lineAA(13.5f, y+22, 30.5f, y+22, 4.0f, C_ACC);
  dsp.setFont(&yoUI12b); dsp.setTextSize(1); dsp.setTextColor(C_TXT);
  snprintf(t, sizeof(t), "%s", utf8Rus("проповіді", false));
  dsp.setCursor(42, y+25); dsp.print(t);
  dsp.setFont(&yoUI9); dsp.setTextColor(C_DIM);
  snprintf(t, sizeof(t), "%s", utf8Rus("з сайту церкви", false));
  dsp.setCursor(42, y+44); dsp.print(t);
  /*  запис ефіру  */
  int16_t x = 6 + 157;
  bool rec = recorder.active();
  dsp.box(x, y, FV_W, FV_BOT_H, R_TILE, rec ? 0x8000 : C_PAN2, C_BG);
  if(rec) dsp.fillRoundRectAA(x+12, y+14, 16, 16, 3, C_TXT);
  else    dsp.fillCircleAA(x+20.5f, y+22.5f, 8, 0xF800);
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
  if(on){
    dsp.box(FV_CHK_X, 223, 13, 13, 3, C_ACC, C_BG);
    dsp.lineAA(FV_CHK_X + 3.2f, 229.8f, FV_CHK_X + 5.6f, 232.2f, 1.8f, C_BG);
    dsp.lineAA(FV_CHK_X + 5.6f, 232.2f, FV_CHK_X + 10.0f, 227.0f, 1.8f, C_BG);
  }else dsp.frame(FV_CHK_X, 223, 13, 13, 3, 1.3f, C_DIM, C_BG, C_BG);
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
    _info[2][1].setText("немає");
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
  if(p == PG_DAC || p == PG_DEVSND) return PG_DEV;
  if(p == PG_DACINFO) return PG_DAC;
  if(p == PG_POWER) return PG_SETUP;
  if(p == PG_WSAVED || p == PG_WPICK || p == PG_WCONN) return PG_WIFI;
  if(p == PG_SND || p == PG_ROOM) return PG_EQ;
  if(p == PG_MIC) return PG_SETUP;
  if(p == PG_MGEST || p == PG_MPRES) return PG_MIC;
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
  int16_t pad = (_cur == PG_WIFI) ? WIFI_PAD : 0;      /* місце під замок і сигнал */
  int16_t room = PL_LIST_W - 16 - ((_cur == PG_WIFI && _smSel <= _scanN) ? WIFI_PAD : 0);
  int16_t wrap = _smMqW > room ? _smMqW + 40 : 0;
  plGenericDraw((float)_smSel, n ? n : 1, smName, _smShift, bandOnly, play, wrap, pad, _scanN);
  _wifiBars((float)_smSel, bandOnly);
}

/*  Бігучий рядок у жовтій смузі — довгі назви проповідей видно цілком.  */
void YoMenu::_smMarquee(){
  uint32_t now = millis();
  if(!_smMqW){ _smMqW = plTextWidth(smName(_smSel)); if(!_smMqW) _smMqW = 1; }
  const int16_t room = PL_LIST_W - 16 - ((_cur == PG_WIFI && _smSel <= _scanN) ? WIFI_PAD : 0);
  if(_smMqW <= room) return;
  if((int32_t)(now - _smMqT) < 0) return;
  _smMqT = now + 25;                         /* по колу, без зупинок — як і в станцій */
  _smShift++;
  if(_smShift >= (int16_t)(_smMqW + 40)) _smShift = 0;
  _smDraw(true);
}

/*  ---------- сітка «параметри» ---------- */

void YoMenu::_drawSetup(){
  /*  Сітка 3×3, як у головному меню: сім розділів.  */
  static const char* lbl[7] = { "інформація", "Wi-Fi", "час", "система", "мікрофон", "розробник", "живлення" };
  for(uint8_t i=0;i<7;i++){
    int16_t x = HM_X(i), y = HM_Y(i), w = HM_W, h = HM_H;
    float cx = x + w / 2.0f, cy = y + 24;
    dsp.box(x, y, w, h, R_TILE, C_PANEL, C_BG);
    const uint16_t c = C_ACC;
    switch(i){
      case 0: /*  «i» у колі  */
              dsp.arcAA(cx, cy, 12, 2.3f, c);
              dsp.fillCircleAA(cx, cy - 6, 1.9f, c);
              dsp.lineAA(cx, cy - 1.5f, cx, cy + 6.5f, 3.0f, c); break;
      case 1: icWifi(cx, cy + 10, 1.0f, c); break;
      case 2: /*  годинник  */
              dsp.arcAA(cx, cy, 12, 2.3f, c);
              dsp.lineAA(cx, cy, cx, cy - 7, 2.4f, c);
              dsp.lineAA(cx, cy, cx + 5.5f, cy + 1.5f, 2.4f, c); break;
      case 3: /*  повзунки  */
              for(uint8_t k = 0; k < 3; k++){
                float yy = cy - 9 + k * 9, kx = cx + (k == 0 ? -6 : k == 1 ? 5 : -1);
                dsp.lineAA(cx - 13, yy, cx + 13, yy, 2.0f, c);
                dsp.fillCircleAA(kx, yy, 4.6f, C_PANEL);
                dsp.fillCircleAA(kx, yy, 3.4f, c);
              }
              break;
      case 4: /*  мікрофон: капсула, дужка й ніжка  */
              dsp.fillRoundRectAA((int16_t)cx - 5, (int16_t)cy - 15, 11, 19, 5.5f, c);
              dsp.arcAA(cx + 0.5f, cy - 4, 9.5f, 2.2f, c, 95, 265);
              dsp.lineAA(cx + 0.5f, cy + 6, cx + 0.5f, cy + 11, 2.2f, c);
              dsp.lineAA(cx - 5, cy + 12, cx + 6, cy + 12, 2.2f, c);
              break;
      case 6: icPower(cx, cy + 1, 12.5f, 2.6f, c); break;
      default: dsp.setFont(&yoUI12b); dsp.setTextSize(1); dsp.setTextColor(c);
              dsp.setCursor((int16_t)cx - 16, (int16_t)cy + 6); dsp.print("</>"); dsp.setFont();
              break;
    }
    dsp.setFont(&yoUI8); dsp.setTextSize(1); dsp.setTextColor(C_TXT);
    char t[32]; snprintf(t, sizeof(t), "%s", utf8Rus(lbl[i], false));
    fitText(t, w - 6);
    dsp.setCursor(x + (w - (int16_t)textW(t)) / 2, y + h - 9); dsp.print(t);
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
    dsp.box(CX, y, CW, PW_H, R_TILE + 1, bg, C_BG);
    float cx = CX + 26.5f, cy = y + PW_H / 2.0f;
    if(i == 0){
      /*  коло зі стрілкою  */
      dsp.arcAA(cx, cy, 12, 2.6f, ic, 25, 320);
      dsp.triAA(cx + 1, cy - 17.5f, cx + 1, cy - 6.5f, cx + 9, cy - 12, ic);
    }else icPower(cx, cy, 12, 2.6f, ic);
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
  dsp.box(CX, 114, CW, 30, R_BTN, C_PAN2, C_BG);
  dsp.setFont(&yoUI9); dsp.setTextSize(1); dsp.setTextColor(C_TXT);
  snprintf(t, sizeof(t), "%s", utf8Rus(_msg[0] ? _msg : "оновити логотипи станцій", false));
  fitText(t, CW - 16);
  dsp.setCursor(CX + (CW - (int16_t)textW(t)) / 2, 134); dsp.print(t);
  /*  звуки подій і заставка — окремою сторінкою (власник: усе це — у розробнику)  */
  dsp.box(CX, 148, CW, 32, R_BTN, C_PAN2, C_BG);
  dsp.setTextColor(C_TXT);
  snprintf(t, sizeof(t), "%s", utf8Rus("звуки й заставка", false));
  dsp.setCursor(CX + 10, 169); dsp.print(t);
  icNext(CX + CW - 16, 164, C_ACC);                                                  /* › */
  dsp.box(CX, 184, CW, 44, R_BTN, C_PAN2, C_BG);
  dsp.setFont(&yoUI8); dsp.setTextColor(C_DIM);
  snprintf(t, sizeof(t), "%s", utf8Rus("аудіовихід", false));
  dsp.setCursor(CX + 10, 197); dsp.print(t);
  dsp.setFont(&yoUI11); dsp.setTextColor(C_TXT);
  dsp.setCursor(CX + 10, 219); dsp.print(DAC_NAME[extras.s.dac]);
  dsp.setFont(&yoUI8); dsp.setTextColor(C_DIM);
  snprintf(t, sizeof(t), "%s", utf8Rus(DAC_KIND[extras.s.dac], false));
  fitText(t, CW - 130);
  dsp.setCursor(CX + 120, 218); dsp.print(t);
  icNext(CX + CW - 16, 206, C_ACC);                                                  /* › */
  dsp.setFont();
}

/*  Сторінка «звуки й заставка»: вимикачі й повзунки — віджети; кнопка показу
    й підказка — тут.  */
void YoMenu::_drawDevSnd(){
  char t[64];
  dsp.box(CX, 172, CW, 32, R_BTN, C_PAN2, C_BG);
  dsp.setFont(&yoUI9); dsp.setTextSize(1); dsp.setTextColor(C_TXT);
  snprintf(t, sizeof(t), "%s", utf8Rus("показати заставку", false));
  dsp.setCursor(CX + (CW - (int16_t)textW(t)) / 2, 193); dsp.print(t);
  dsp.setFont(&yoUI8); dsp.setTextColor(C_DIM);
  snprintf(t, sizeof(t), "%s", utf8Rus("свої звуки - зі сторінки радіо", false));
  fitText(t, CW);
  dsp.setCursor(CX, 224); dsp.print(t);
  dsp.setFont();
}

void YoMenu::_drawDacList(){
  for(uint8_t i = 0; i < 5; i++){
    int16_t y = 32 + i * 41;
    bool cur = (extras.s.dac == i);
    dsp.box(CX - 6, y, CW + 12, 38, R_BTN, cur ? C_PAN2 : C_PANEL, C_BG);
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
        dsp.box(x, y + 15, 72, 18, 4, on ? C_ACC : C_PAN2, C_BG);
        dsp.setFont(&yoUI9b); dsp.setTextColor(on ? C_BG : C_DIM);
        dsp.setCursor(x + (72 - (int16_t)textW(H[h].pin[k])) / 2, y + 29); dsp.print(H[h].pin[k]);
        if(on && H[h].to[k][0]){
          dsp.setFont(&yoUI8); dsp.setTextColor(C_TXT);
          snprintf(t, sizeof(t), "-> %s", H[h].to[k]);
          dsp.setCursor(x + (72 - (int16_t)textW(t)) / 2, y + 47); dsp.print(t);
        }
      }
    }
    dsp.box(8, 212, SW - 16, 26, R_BTN, C_PAN2, C_BG);
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
      dsp.box(8, y + 1, 72, rh - 2, 4, C_PAN2, C_BG);
      dsp.box(SW - 68, y + 1, 60, rh - 2, 4, C_PAN2, C_BG);
      dsp.lineAA(80, cy + 0.5f, SW - 68, cy + 0.5f, 2.4f, wc);          /* провід */
      dsp.fillCircleAA(80, cy + 0.5f, 3.2f, wc); dsp.fillCircleAA(SW - 68, cy + 0.5f, 3.2f, wc);
      dsp.setFont(&yoUI8); dsp.setTextColor(C_TXT);
      dsp.setCursor(13, cy + 5); dsp.print(pins[k].esp);
      dsp.setCursor(SW - 63, cy + 5); dsp.print(pins[k].mod);
    }
  }else{
    /*  вбудований: плата -> ES8311 -> SC8002B -> динамік  */
    const char* blk[4] = { "ESP32-S3", "ES8311", "SC8002B", "динамік" };
    for(uint8_t k = 0; k < 4; k++){
      int16_t x = 8 + k * 78;
      dsp.box(x, 70, 70, 40, R_BTN, C_PAN2, C_BG);
      dsp.setFont(&yoUI8); dsp.setTextColor(C_TXT);
      snprintf(t, sizeof(t), "%s", utf8Rus(blk[k], false));
      dsp.setCursor(x + (70 - (int16_t)textW(t)) / 2, 95); dsp.print(t);
      if(k < 3) dsp.lineAA(x + 70, 89.5f, x + 78, 89.5f, 2.2f, C_ACC);
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
    dsp.box(8, 212, SW - 16, 26, R_BTN, C_PAN2, C_BG);
    dsp.setFont(&yoUI9b); dsp.setTextColor(C_TXT);
    snprintf(t, sizeof(t), "%s", utf8Rus("де ці роз'єми на платі", false));
    dsp.setCursor((SW - (int16_t)textW(t)) / 2, 230); dsp.print(t);
  }else{
    bool cur = (extras.s.dac == _dacSel);
    dsp.box(8, 212, SW - 16, 26, R_BTN, cur ? C_PAN2 : C_ACC, C_BG);
    dsp.setFont(&yoUI9b); dsp.setTextColor(cur ? C_DIM : C_BG);
    snprintf(t, sizeof(t), "%s", utf8Rus(cur ? "зараз звук іде сюди" : "увімкнути цей вихід", false));
    dsp.setCursor((SW - (int16_t)textW(t)) / 2, 230); dsp.print(t);
  }
  dsp.setFont();
}

/*  ---------- головне меню «☰» ---------- */

/*  Дев'ять плиток 3x3: великий жовтий значок і підпис. Тут усе, що раніше
    розсипалось значками по шапці плеєра, плюс входи в налаштування.  */
enum { HM_STATIONS = 0, HM_FAV, HM_SERM, HM_SRC, HM_REC, HM_ALARM, HM_SCREEN, HM_SOUND, HM_SETUP };

void YoMenu::_drawHomeTile(uint8_t i){
  int16_t x = HM_X(i), y = HM_Y(i);
  float cx = x + HM_W / 2.0f, cy = y + 24;
  bool rec = recorder.active();
  const uint16_t tb = (i == HM_REC && rec) ? 0x8000 : C_PANEL;      /* тло плитки — для вирізів */
  dsp.box(x, y, HM_W, HM_H, R_TILE, tb, C_BG);
  const uint16_t c = C_ACC;
  const char* lbl = "";
  char buf[24];
  switch(i){
    case HM_STATIONS:
      for(uint8_t k=0;k<3;k++){
        dsp.fillCircleAA(cx - 11, cy - 8 + k * 8, 2.3f, c);
        dsp.lineAA(cx - 5, cy - 8 + k * 8, cx + 12, cy - 8 + k * 8, 3.2f, c);
      }
      lbl = "станції"; break;
    case HM_FAV:
      icStar(cx, cy + 1, 13.5f, c); lbl = "обране"; break;
    case HM_SERM:
      dsp.lineAA(cx, cy - 13, cx, cy + 13, 4.4f, c);
      dsp.lineAA(cx - 9, cy - 5, cx + 9, cy - 5, 4.4f, c);
      lbl = "проповіді"; break;
    case HM_SRC:
      if(config.getMode() == PM_SDCARD){          /* куди перемкне: на радіо */
        dsp.lineAA(cx, cy - 3, cx, cy + 12, 2.6f, c);
        dsp.fillCircleAA(cx, cy - 5, 3.2f, c);
        for(uint8_t k = 1; k <= 2; k++){
          dsp.arcAA(cx, cy - 5, 4 + 5 * k, 2.2f, c, 55, 125);
          dsp.arcAA(cx, cy - 5, 4 + 5 * k, 2.2f, c, 235, 305);
        }
        lbl = "радіо";
      }else{                                      /* на картку */
        const float xy[10] = { cx - 9, cy - 13, cx + 4, cy - 13, cx + 9, cy - 8, cx + 9, cy + 13, cx - 9, cy + 13 };
        dsp.polyAA(xy, 5, c);
        for(uint8_t k=0;k<3;k++) dsp.lineAA(cx - 5 + k * 4, cy - 10.5f, cx - 5 + k * 4, cy - 6, 1.8f, tb);
        lbl = "картка";
      }
      break;
    case HM_REC:
      if(rec){
        dsp.fillRoundRectAA((int16_t)cx - 8, (int16_t)cy - 8, 16, 16, 3, C_TXT);
        uint32_t sec = recorder.seconds();
        snprintf(buf, sizeof(buf), "%u:%02u", (unsigned)(sec/60), (unsigned)(sec%60));
        lbl = buf;
      }else{ dsp.fillCircleAA(cx, cy, 10, 0xF800); lbl = "запис"; }
      break;
    case HM_ALARM: {
      /*  дзвоник: купол, розширення донизу, обідок і язичок  */
      dsp.fillCircleAA(cx, cy - 4, 8.5f, c);
      const float xy[8] = { cx - 8.5f, cy - 4, cx + 8.5f, cy - 4, cx + 12, cy + 7, cx - 12, cy + 7 };
      dsp.polyAA(xy, 4, c);
      dsp.lineAA(cx - 12.5f, cy + 7.5f, cx + 12.5f, cy + 7.5f, 2.6f, c);
      dsp.fillCircleAA(cx, cy + 11.5f, 3, c);
      lbl = "будильник"; break; }
    case HM_SCREEN:
      dsp.arcAA(cx, cy - 4, 8.5f, 2.3f, c);
      dsp.lineAA(cx - 4.5f, cy + 8, cx + 4.5f, cy + 8, 2.4f, c);
      dsp.lineAA(cx - 3, cy + 12, cx + 3, cy + 12, 2.4f, c);
      lbl = "екран"; break;
    case HM_SOUND:
      dsp.fillRoundRectAA((int16_t)cx - 12, (int16_t)cy - 2, 5, 14, 2.5f, c);
      dsp.fillRoundRectAA((int16_t)cx - 3, (int16_t)cy - 12, 5, 24, 2.5f, c);
      dsp.fillRoundRectAA((int16_t)cx + 6, (int16_t)cy - 6, 5, 18, 2.5f, c);
      lbl = "звук"; break;
    default:
      icGear(cx, cy, 1.35f, c);
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
/*  Сторінка лише просить пошук. Саму роботу з радіомодулем робить головний
    цикл (wifiTick): виклики Wi-Fi іноді тривають секунди, а в задачі дисплея
    це зупиняє малювання, забиває чергу запитів — і сторожовий таймер
    перезавантажує радіо.  */
void YoMenu::_wifiScan(){
  _scanReq = true;
  _wlDirty = true;
}

void YoMenu::wifiTick(){
  if(_cur == PG_OFF) return;
  /*  Раз на 200 мс запам'ятовуємо, у якій ми мережі: далі малювання бере
      готове, а не смикає радіомодуль по десять разів на кадр.  */
  static uint32_t ask = 0;
  if(millis() - ask >= 200){
    ask = millis();
    bool up = (WiFi.status() == WL_CONNECTED);
    char now[33] = {0};
    if(up) strlcpy(now, WiFi.SSID().c_str(), sizeof(now));
    if(up != _staUp || strcmp(now, _curSsid)){ _staUp = up; strlcpy(_curSsid, now, sizeof(_curSsid)); _wlDirty = true; }
  }
  if(_cur != PG_WIFI && !_scanning && !_scanReq) return;
  if(_scanReq){
    _scanReq = false;
    if(WiFi.getMode() != WIFI_STA) WiFi.mode(WIFI_STA);   /* точки доступу в нас немає */
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
  _wifiPoll();
}

void YoMenu::_wifiPoll(){
  /*  Питати радіомодуль щооберта не можна: поки триває пошук, він зайнятий
      перебором каналів, і кожне таке питання відбирає в циклу мілісекунди.  */
  static uint32_t askT = 0;
  if(millis() - askT < 200) return;
  askT = millis();
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

/*  ---------- підключення: одразу й з відповіддю ---------- */

/*  Раніше вибір мережі означав перезавантаження наосліп: не підійшов пароль —
    радіо поверталось у точку доступу, і людина гадала, що сталось. Тепер
    підключення йде тут-таки, а сторінка каже, чим воно скінчилось.  */
void YoMenu::_drawWconn(){
  dsp.fillRect(0, HDR, SW, SH - HDR, C_BG);
  n_Try_e st = network.tryState();
  const char* big =
      st == TRY_OK       ? "готово" :
      st == TRY_BADPASS  ? "невірний пароль" :
      st == TRY_NOTFOUND ? "мережі не видно" :
      st == TRY_FAIL     ? "не вдалося" : "підключаюсь...";
  const char* sub =
      st == TRY_OK       ? "" :
      st == TRY_BADPASS  ? "перевірте й введіть ще раз" :
      st == TRY_NOTFOUND ? "вона зникла з ефіру" :
      st == TRY_FAIL     ? "мережа не відповідає" : "це займає кілька секунд";
  char t[72];
  dsp.setFont(&yoUI12b); dsp.setTextSize(1);
  dsp.setTextColor(st == TRY_OK || st == TRY_RUN || st == TRY_NONE ? C_ACC : C_TXT);
  snprintf(t, sizeof(t), "%s", utf8Rus(big, false));
  fitText(t, SW - 20);
  dsp.setCursor((SW - (int16_t)textW(t)) / 2, 92); dsp.print(t);
  dsp.setFont(&yoUI9); dsp.setTextColor(C_DIM);
  if(st == TRY_OK) snprintf(t, sizeof(t), "%s", utf8Rus(config.ipToStr(WiFi.localIP()), false));
  else             snprintf(t, sizeof(t), "%s", utf8Rus(sub, false));
  fitText(t, SW - 20);
  dsp.setCursor((SW - (int16_t)textW(t)) / 2, 118); dsp.print(t);
  /*  кнопки з'являються, лише коли є що робити  */
  if(st == TRY_BADPASS || st == TRY_NOTFOUND || st == TRY_FAIL){
    static const char* lb[2] = { "ще раз", "до списку" };
    for(uint8_t i = 0; i < 2; i++){
      int16_t x = CX + i * 148, w = 140;
      dsp.box(x, WC_BTN_Y, w, WC_BTN_H, R_TILE, i ? C_PANEL : C_ACC, C_BG);
      dsp.setFont(&yoUI11); dsp.setTextColor(i ? C_TXT : C_BG);
      snprintf(t, sizeof(t), "%s", utf8Rus(lb[i], false));
      fitText(t, w - 10);
      dsp.setCursor(x + (w - (int16_t)textW(t)) / 2, WC_BTN_Y + 28); dsp.print(t);
    }
  }
  dsp.setFont();
}

/*  ---------- знайома мережа: що з нею зробити ---------- */

/*  Мережу, яку радіо вже знає, не треба щоразу набирати наново: одразу
    видно, що можна підключитись, змінити пароль або забути її тут-таки —
    саме там, де людина її й вибрала.  */
void YoMenu::_drawWpick(){
  dsp.fillRect(0, HDR, SW, SH - HDR, C_BG);
  static const char* t1[3] = { "підключитись", "змінити пароль", "забути мережу" };
  static const char* t2[3] = { "радіо перезавантажиться", "якщо його змінили", "разом із паролем" };
  char t[64];
  for(uint8_t i = 0; i < 3; i++){
    int16_t y = WP_Y(i);
    bool armed = (i == 2 && _wpArm == 0);
    uint16_t bg = armed ? C_ACC : C_PANEL, fg = armed ? C_BG : C_TXT,
             sub = armed ? C_BG : C_DIM, ic = armed ? C_BG : C_ACC;
    dsp.box(CX, y, CW, WP_H, R_TILE, bg, C_BG);
    float cx = CX + 28, cy = y + WP_H / 2.0f;
    if(i == 0) icWifi(cx, cy + 8, 0.95f, ic);    /* значок мережі: три дуги */
    else if(i == 1){                             /* олівець */
      dsp.lineAA(cx - 4, cy + 4, cx + 7, cy - 7, 4.6f, ic);
      dsp.triAA(cx - 9.5f, cy + 9.5f, cx - 7.5f, cy + 2.5f, cx - 2.5f, cy + 7.5f, ic);
    }else icCross(cx, cy, 7.5f, 2.6f, ic);       /* × */
    dsp.setFont(&yoUI11); dsp.setTextSize(1); dsp.setTextColor(fg);
    snprintf(t, sizeof(t), "%s", utf8Rus(armed ? "ще раз - і забуду" : t1[i], false));
    fitText(t, CW - 60);
    dsp.setCursor(CX + 52, y + 24); dsp.print(t);
    dsp.setFont(&yoUI8); dsp.setTextColor(sub);
    snprintf(t, sizeof(t), "%s", utf8Rus(armed ? "" : t2[i], false));
    fitText(t, CW - 60);
    dsp.setCursor(CX + 52, y + 42); dsp.print(t);
  }
  dsp.setFont();
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
    snprintf(t, sizeof(t), "%s", utf8Rus("виберіть мережу зі списку —", false));
    dsp.setCursor((SW - (int16_t)textW(t)) / 2, 140); dsp.print(t);
    snprintf(t, sizeof(t), "%s", utf8Rus("радіо запам'ятає її саме", false));
    dsp.setCursor((SW - (int16_t)textW(t)) / 2, 156); dsp.print(t);
    dsp.setFont();
    return;
  }
  bool sta = _staUp;
  for(uint8_t i = 0; i < n; i++){
    int16_t y = WS_TOP + i * WS_ROW;
    bool armed = (_wsArm == (int8_t)i);
    bool cur = sta && !strcmp(_curSsid, _ssid[i]);
    dsp.box(CX, y, CW, WS_H, R_BTN, armed ? C_ACC : (cur ? C_PAN2 : C_PANEL), C_BG);
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
      float ax = CX + CW - 72 + 17.5f, ay = y + WS_H / 2.0f;
      dsp.triAA(ax, ay - 8, ax - 7, ay + 0.5f, ax + 7, ay + 0.5f, ic);
      dsp.lineAA(ax, ay, ax, ay + 7, 3.6f, ic);
    }
    /*  × — забути  */
    icCross(CX + CW - 36 + 17.5f, y + WS_H / 2.0f, 6.0f, 2.4f, ic);
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
    bool cur = _staUp && !strcmp(_curSsid, w.ssid);
    /*  У рядку — сама назва. Мережу, у якій радіо зараз, позначає той самий
        значок, що й станцію, яка грає; знайому — крапка коло замка. Словами
        цього не пишемо: довга назва тоді лізла під значки й бігла рядком, а
        щоб зрушити один піксель, довелось би малювати рядок сорок разів
        на секунду.  */
    (void)cur;
    snprintf(buf, sizeof(buf), "%s", w.ssid);
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
  if(_cur == PG_WIFI && _staUp){
    for(uint8_t i = 0; i < _scanN; i++) if(!strcmp(_curSsid, _scan[i].ssid)) return i + 1;
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
    /*  крапка — мережа вже знайома, пароль питати не будемо  */
    if(wifiSaved(_scan[idx-1].ssid))
      dsp.fillCircle(PL_X0 + PL_LIST_W - 48, y + PL_ROW_H/2, 3, band ? C_BG : C_ACC);
  }
}

/*  Вибрали мережу: відкрита — одразу підключаємось, закрита — пароль.
    Якщо мережу вже збережено, пароль підставляємо з пам'яті.  */
void YoMenu::_wifiPick(uint8_t i){
  /*  Та сама мережа, що й щойно? Тоді лишаємо набраний пароль: людина
      щойно його вводила, спроба не вдалася — і повертатись до старого,
      негодящого, безглуздо. Інша мережа — починаємо з чистого.  */
  bool again = !strcmp(_wSsid, _scan[i].ssid) && _wPass[0];
  strlcpy(_wSsid, _scan[i].ssid, sizeof(_wSsid));
  bool known = false;
  for(uint8_t k = 0; k < config.ssidsCount && k < YOM_SSIDS; k++)
    if(!strcmp(config.ssids[k].ssid, _wSsid)) known = true;
  if(!again){
    _wPass[0] = 0;
    for(uint8_t k = 0; k < config.ssidsCount && k < YOM_SSIDS; k++)
      if(!strcmp(config.ssids[k].ssid, _wSsid)) strlcpy(_wPass, config.ssids[k].password, sizeof(_wPass));
  }
  /*  Знайому мережу не треба набирати наново: питаємо, що з нею зробити —
      підключитись, змінити пароль чи забути.  */
  if(known){ _loadWifi(); _wpArm = -1; _show(PG_WPICK); return; }
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
  _scanReq = false; _scanning = false;      /* пошук більше не потрібен — він лише заважає */
  /*  Пробуємо просто зараз. У список мережу запишемо, лише коли вийде —
      невірний пароль там ні до чого.  */
  network.tryClear();
  network.connectTo(_wSsid, _wPass);
  _wcShown = -1; _wcOkAt = 0;
  _show(PG_WCONN);
}

/*  Вийшло — тепер мережа перша в списку, і радіо почне з неї наступного разу. */
void YoMenu::_wifiSaveCurrent(){
  _loadWifi();
  String out = String(_wSsid) + "\t" + String(_wPass) + "\n";
  uint8_t n = 1;
  for(uint8_t i = 0; i < YOM_SSIDS && n < YOM_SSIDS; i++){
    if(!_ssid[i][0] || !strcmp(_ssid[i], _wSsid)) continue;
    out += String(_ssid[i]) + "\t" + String(_pass[i]) + "\n";
    n++;
  }
  config.saveWifiList(out.c_str());
  config.setLastSSID(1);
  _loadWifi();
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

void YoMenu::openPage(int8_t p){
  if(p < 0 || p >= PG_N) return;
  if(_cur == PG_OFF){ _build(); _apLock = false; _loadWifi(); _syncSys(); }
  _show(p);
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
  /*  Мережі не було, і меню не мало виходу; радіо саме повернулось у
      збережену мережу — замок знімаємо й повертаємось на плеєр (набір
      пароля не перериваємо).  */
  if(_apLock && network.status == CONNECTED && _cur != PG_WCONN){
    _apLock = false;
    if(_cur != PG_KBD){ close(); return; }
  }
  /*  Хвилину без дотиків — назад на плеєр, тим самим плавним переходом.
      Клавіатуру й налаштування мережі без неї не чіпаємо: там людина
      може думати над паролем.  */
  if(!_apLock && _cur != PG_KBD && _cur != PG_WIFI && !(_cur == PG_ROOM && (yoDsp.roomState() == 1 || yoDsp.roomState() == 2))
     && millis() - _lastAct > 60000UL){ close(); return; }
  /*  Крок анімації смуг — щоразу, коли задача дисплея проходить повз, а не
      раз на секунду разом із оновленням даних.  */
  if(_cur >= 0 && _pg[_cur]) _pg[_cur]->loop();
  if(_cur == PG_DACINFO && _favDirty){ _favDirty = false; _drawDacInfo(); return; }
  if(_cur == PG_WSAVED){
    if(_wsArm >= 0 && millis() - _wsArmT > 4000){ _wsArm = -1; _favDirty = true; }   /* не підтвердили */
    if(_favDirty){ _favDirty = false; _drawSaved(); }
    return;
  }
  if(_cur == PG_KBD){ _kbRender(); return; }
  if(_cur == PG_WCONN){
    int8_t st = (int8_t)network.tryState();
    if(st != _wcShown){
      _wcShown = st;
      if(st == (int8_t)TRY_OK){ _wifiSaveCurrent(); _wcOkAt = millis(); }
      _drawWconn();
    }
    /*  вийшло — трохи показуємо адресу й повертаємось на плеєр  */
    if(_wcOkAt && millis() - _wcOkAt > 1800){ _wcOkAt = 0; _apLock = false; network.tryClear(); close(); }
    return;
  }
  if(_cur == PG_WPICK){
    if(_wpArm >= 0 && millis() - _wpArmT > 4000){ _wpArm = -1; _favDirty = true; }
    if(_favDirty){ _favDirty = false; _drawWpick(); }
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
  if(_isSound(_cur)){ _renderSound(); return; }
  if(_isList(_cur)){
    if(_cur == PG_WIFI){
      /*  пошук веде головний цикл; тут лише перемальовуємо, коли є що  */
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
      if(_smDragDirty){ _smDragDirty = false; plGenericDraw(_smCur, nn ? nn : 1, smName, 0, false, play, 0, (_cur == PG_WIFI) ? WIFI_PAD : 0, _scanN); }
      return;
    }
    if(_smFling){
      static uint32_t ft = 0;
      if(now - ft < 10) return;          /* кадр накату — під темп екрана (100/с) */
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
        plGenericDraw(_smCur, n, smName, 0, false, play, 0, (_cur == PG_WIFI) ? WIFI_PAD : 0, _scanN);
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
        plGenericDraw(_smCur, nn ? nn : 1, smName, 0, false, play, 0, (_cur == PG_WIFI) ? WIFI_PAD : 0, _scanN);
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
#define KY0   62
#define KROWH 35
#define KKW   32

/*  Рядок вводу: пароль видно, доки його не сховали оком. Набирати наосліп
    по зірочках, ще й пальцем по дрібних клавішах, — марна справа.  */
/*  Рядок вводу: пароль видно, доки його не сховали оком. Довгий — показуємо
    кінець, щоб видно було саме те, що набирається, а не обрізаний початок.  */
void YoMenu::_kbdRefresh(){
  if(!_kbdTarget) return;
  char shown[YOM_PASS_LEN+2];
  if(_kbdIsPass && !_kbdShow){ size_t n=strlen(_kbdTarget); if(n>YOM_PASS_LEN) n=YOM_PASS_LEN; memset(shown,'*',n); shown[n]='\0'; }
  else snprintf(shown, sizeof(shown), "%s", _kbdTarget);
  dsp.setFont(&yoUI12b); dsp.setTextSize(1);
  const uint16_t room = SW - 74 - 8;
  char* v = shown;
  char t[YOM_PASS_LEN+4];
  bool cut = false;
  for(;;){
    snprintf(t, sizeof(t), "%s%s", cut ? ".." : "", utf8Rus(v, false));
    if(!*v || textW(t) <= room) break;
    v++; while((*v & 0xC0) == 0x80) v++;          /* не різати посеред літери */
    cut = true;
  }
  dsp.setFont();
  snprintf(t, sizeof(t), "%s%s", cut ? ".." : "", v);
  _kbdField.setText(t);
}

/*  Око праворуч від рядка: показати чи сховати набране.  */
static bool s_eyeHl = false;                     /* палець на оці — жовта рамка */
void YoMenu::_drawKbdEye(){
  int16_t x = SW-62, y = 32, w = 54, h = 26;
  if(s_eyeHl) dsp.frame(x, y, w, h, R_BTN, 1.5f, C_ACC, C_PAN2, C_BG);
  else        dsp.box(x, y, w, h, R_BTN, C_PAN2, C_BG);
  float cx = x + w/2.0f, cy = y + h/2.0f;
  uint16_t c = _kbdShow ? C_ACC : C_DIM;
  dsp.arcAA(cx, cy + 10, 15, 1.8f, c, -50, 50);  /* дві дуги — контур ока */
  dsp.arcAA(cx, cy - 10, 15, 1.8f, c, 130, 230);
  dsp.fillCircleAA(cx, cy, 3.3f, c);
  if(!_kbdShow) dsp.lineAA(cx - 9, cy + 8, cx + 9, cy - 8, 1.8f, C_TXT);   /* перекреслене */
}

#define K_SHIFT  64
#define K_BACK   65
#define K_PAGE   66
#define K_SPACE  67
#define K_OK     68
#define K_CANCEL 69
#define K_EYE    70
#define K_ROW3X  46                 /* між «ABC» і «<-» */
#define K_ROW3W  (SW - 92)

/*  Ряд клавіш: де починається й яка ширина клавіші. Нижній ряд знаків
    стоїть між «ABC» і «<-»; у знаках там 8 клавіш — вужчих, щоб не
    налізали на «стерти», як було.  */
static void kbRow(uint8_t page, uint8_t r, int16_t& x0, int16_t& kw, uint8_t& len){
  const char* row = KROWS[page][r];
  len = strlen(row);
  if(r == 3){
    kw = (int16_t)(K_ROW3W / len); if(kw > KKW) kw = KKW;
    x0 = K_ROW3X + (K_ROW3W - kw * len) / 2;
  }else{
    kw = KKW;
    x0 = (SW - len * KKW) / 2;
  }
}

bool YoMenu::_kbKeyRect(int8_t k, int16_t& x, int16_t& y, int16_t& w, int16_t& h) const {
  const int16_t by = KY0 + 4 * KROWH;
  h = KROWH - 2;
  switch(k){
    case K_SHIFT:  x = 0;       y = KY0 + 3 * KROWH; w = 44; return true;
    case K_BACK:   x = SW - 44; y = KY0 + 3 * KROWH; w = 44; return true;
    case K_PAGE:   x = 0;   y = by; w = 58;  return true;
    case K_SPACE:  x = 60;  y = by; w = 118; return true;
    case K_OK:     x = 180; y = by; w = 66;  return true;
    case K_CANCEL: x = 248; y = by; w = 72;  return true;
    case K_EYE:    x = SW - 62; y = 32; w = 54; h = 26; return true;
  }
  if(k < 0 || k >= 64) return false;
  uint8_t r = k >> 4, c = k & 15, len; int16_t x0, kw;
  kbRow(_kbdPage, r, x0, kw, len);
  if(c >= len) return false;
  x = x0 + c * kw; y = KY0 + r * KROWH; w = kw - 2;
  return true;
}

/*  Клавіша під пальцем — найближча в ряду: проміжків, де дотик пропадає,
    немає.  */
int8_t YoMenu::_kbKeyAt(int16_t x, int16_t y) const {
  if(y < KY0){
    if(x >= SW - 64 && y >= HDR + 10) return K_EYE;       /* вище — зона стрілки «назад» */
    return -1;
  }
  int r = (y - KY0) / KROWH; if(r > 4) r = 4;
  if(r == 4) return x < 59 ? K_PAGE : x < 179 ? K_SPACE : x < 247 ? K_OK : K_CANCEL;
  if(r == 3 && x < K_ROW3X) return K_SHIFT;
  if(r == 3 && x >= SW - K_ROW3X) return K_BACK;
  uint8_t len; int16_t x0, kw;
  kbRow(_kbdPage, r, x0, kw, len);
  int c = (x - x0) / kw;
  if(x < x0) c = 0;
  if(c >= len) c = len - 1;
  return (int8_t)(r * 16 + c);
}

void YoMenu::_kbDrawKey(int8_t k, bool hl){
  int16_t x, y, w, h;
  if(!_kbKeyRect(k, x, y, w, h)) return;
  if(k == K_EYE){ s_eyeHl = hl; _drawKbdEye(); s_eyeHl = false; return; }
  bool ok = k == K_OK;
  uint16_t bg = hl ? (ok ? C_TXT : C_ACC) : (ok ? C_ACC : C_PAN2);
  uint16_t fg = (hl || ok) ? C_BG : C_TXT;
  dsp.box(x, y, w, h, 5, bg, C_BG);
  char t[20];
  const GFXfont* f = &yoUI12b;
  int16_t base = y + 23;
  if(k < 64){ t[0] = KROWS[_kbdPage][k >> 4][k & 15]; t[1] = 0; }
  else{
    f = (k == K_SHIFT || k == K_BACK) ? &yoUI9b : &yoUI8;
    base = y + 21;
    const char* l = k == K_SHIFT ? (_kbdPage == 1 ? "abc" : "ABC") : k == K_BACK ? "<-" :
                    k == K_PAGE ? (_kbdPage == 2 ? "абв" : "?123") : k == K_SPACE ? "пробіл" : k == K_OK ? "OK" : "відміна";
    snprintf(t, sizeof(t), "%s", utf8Rus(l, false));
  }
  dsp.setFont(f); dsp.setTextSize(1); dsp.setTextColor(fg);
  dsp.setCursor(x + (w - (int16_t)textW(t)) / 2, base); dsp.print(t);
  dsp.setFont();
}

void YoMenu::_drawKbdKeys(){
  dsp.fillRect(0, KY0, SW, SH - KY0, C_BG);
  for(uint8_t r = 0; r < 4; r++){
    const char* row = KROWS[_kbdPage][r];
    for(uint8_t c = 0; c < strlen(row); c++) _kbDrawKey((int8_t)(r * 16 + c), false);
  }
  for(int8_t k = K_SHIFT; k <= K_EYE; k++) _kbDrawKey(k, false);
}

/*  Прибрати збільшену клавішу: перемалювати все, що вона накривала.  */
void YoMenu::_kbRestore(int16_t px, int16_t py, int16_t pw, int16_t ph){
  dsp.fillRect(px, py, pw, ph, C_BG);
  for(int8_t k = 0; k <= K_EYE; k++){
    int16_t x, y, w, h;
    if(!_kbKeyRect(k, x, y, w, h)) continue;
    if(x < px + pw && x + w > px && y < py + ph && y + h > py) _kbDrawKey(k, k == _kbShown);
  }
  if(py < KY0){ _kbdField.redraw(); }
}

/*  Такт клавіатури в задачі дисплея.  */
void YoMenu::_kbRender(){
  uint8_t d = _kbDirty;
  if(!d) return;
  _kbDirty &= ~d;
  if(d & 2){ _drawKbdKeys(); _kbShown = -1; _kbPopX = -1; d |= 8; }
  if(d & 1) _kbdRefresh();
  if(d & 4) _drawKbdEye();
  if(d & 8){
    int8_t want = _kbDown ? _kbKey : -1;
    if(want != _kbShown){
      int8_t old = _kbShown;
      _kbShown = want;
      if(_kbPopX >= 0){ int16_t px = _kbPopX; _kbPopX = -1; _kbRestore(px, _kbPopY, 44, 50); }
      if(old >= 0) _kbDrawKey(old, false);
      if(want >= 0){
        _kbDrawKey(want, true);
        int16_t x, y, w, h;
        if(want < 64 && _kbKeyRect(want, x, y, w, h)){
          /*  Збільшена клавіша над пальцем — видно, що саме введеться, ще
              до відпускання; палець можна довести до потрібної.  */
          int16_t px = x + w / 2 - 22, py = y - 52;
          if(px < 0) px = 0;
          if(px > SW - 44) px = SW - 44;
          if(py < HDR + 2) py = HDR + 2;
          dsp.frame(px, py, 44, 50, 8, 2.0f, C_ACC, C_BG, C_BG);
          char t[2] = { KROWS[_kbdPage][want >> 4][want & 15], 0 };
          dsp.setFont(&aaUI26b); dsp.setTextSize(1); dsp.setTextColor(C_ACC);
          int16_t x1, y1; uint16_t tw, th;
          dsp.getTextBounds(t, 0, 40, &x1, &y1, &tw, &th);
          dsp.setCursor(px + (44 - (int16_t)tw) / 2 - x1, py + 37);
          dsp.print(t);
          dsp.setTextSize(1); dsp.setFont();
          _kbPopX = px; _kbPopY = py;
        }
      }
    }
  }
}

/*  Дія клавіші — при відпусканні пальця, з тієї клавіші, де він був
    наостанок (а не де торкнувся).  */
void YoMenu::_kbAction(int8_t k){
  if(!_kbdTarget) return;
  size_t l = strlen(_kbdTarget);
  if(k >= 0 && k < 64){
    char ch = KROWS[_kbdPage][k >> 4][k & 15];
    if(ch && l + 1 < _kbdMax){ _kbdTarget[l] = ch; _kbdTarget[l+1] = '\0'; }
    _kbDirty |= 1;
    return;
  }
  switch(k){
    case K_BACK:  if(l > 0) _kbdTarget[l-1] = '\0'; _kbDirty |= 1; return;
    case K_SPACE: if(l + 1 < _kbdMax){ _kbdTarget[l] = ' '; _kbdTarget[l+1] = '\0'; } _kbDirty |= 1; return;
    case K_SHIFT: _kbdPage = (_kbdPage == 1) ? 0 : 1; _kbDirty |= 2; return;
    case K_PAGE:  _kbdPage = (_kbdPage == 2) ? 0 : 2; _kbDirty |= 2; return;
    case K_EYE:   _kbdShow = !_kbdShow; _kbDirty |= 1 | 4; return;
    case K_OK:
      Serial.printf("##KBD#\tOK: крок=%d мережа='%s' пароль %u симв.\n", (int)_kbdNext, _wSsid, (unsigned)strlen(_wPass));
      /*  OK: лишаємо набране. Раніше тут, як і у відміні, викликався
          _loadWifi(), який перечитує поля з налаштувань — тобто набраний
          пароль щоразу губився, і ввести мережу з екрана було неможливо. */
      if(_kbdNext == 1 && _wSsid[0]){            /* назву є — тепер пароль */
        _kbdNext = 2;
        snprintf(_kbdTitleBuf, sizeof(_kbdTitleBuf), "пароль: %s", _wSsid);
        _openKbd(_wPass, YOM_PASS_LEN, true, _kbdTitleBuf);
        return;
      }
      if(_kbdNext == 2){ _kbdNext = 0; _wifiConnect(); return; }
      _kbdNext = 0;
      if(_kbdBack == PG_WIFI) _showWifi();
      _show(_kbdBack);
      return;
    case K_CANCEL:
      /*  Відміна: повертаємо те, що було до правки.  */
      _kbdNext = 0;
      strlcpy(_kbdTarget, _kbdUndo, _kbdMax);
      if(_kbdBack == PG_WIFI) _showWifi();
      _show(_kbdBack);
      return;
  }
}

void YoMenu::_openKbd(char* target, size_t max, bool isPass, const char* title){
  _kbdTarget = target; _kbdMax = max; _kbdIsPass = isPass;
  _kbdTitle = title; _kbdPage = 0;
  if(_cur != PG_KBD) _kbdBack = _cur;   /* з клавіатури на клавіатуру — назад туди ж, звідки прийшли */
  strlcpy(_kbdUndo, target, sizeof(_kbdUndo));   /* щоб відміна справді відміняла */
  _kbdShow = true;                 /* набирати наосліп по зірочках — мука */
  _kbDown = false; _kbKey = -1; _kbShown = -1; _kbPopX = -1;
  _show(PG_KBD);                   /* рядок і клавіші намалює задача дисплея */
}

/*  ---------- дотики ---------- */

void YoMenu::onRelease(uint16_t x, uint16_t y, uint32_t held){
  if(_fadeStep >= 0) return;               /* сторінка ще набігає */
  _held = held;
  _lastAct = millis();
  /*  координати тут — місця натискання, а не відпускання: значення вже взяте з руху  */
  if(_cur == PG_EQ && _eqBand >= 0){ _eqBand = -1; _eqApply(true); return; }
  if(_cur == PG_KBD){
    int8_t k = _kbKey;
    bool was = _kbDown, rep = _kbRep;
    _kbDown = false; _kbKey = -1; _kbRep = false;
    _kbDirty |= 8;
    if(was && k >= 0){ if(!(k == K_BACK && rep)) _kbAction(k); return; }
  }
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
  /*  Відгук під пальцем — хвиля світла по кнопці. Клавіатура підсвічує клавішу
      сама, списки й смуги еквалайзера ведуться пальцем — там хвилі не треба.  */
  if(_fadeStep < 0 && _cur != PG_OFF && _cur != PG_KBD && !_isList(_cur) &&
     !(_cur == PG_EQ && y >= EQ_COL0 && y < EQ_T1 + 12))
    ui.press(x, y, C_ACC);
  if(_cur == PG_KBD && _fadeStep < 0){
    _kbKey = _kbKeyAt(x, y);
    _kbDown = _kbKey >= 0;
    _kbDownT = millis(); _kbRep = false;
    _kbDirty |= 8;
    return;
  }
  if(_cur == PG_EQ && _fadeStep < 0 && y >= EQ_COL0 && y < EQ_T1 + 12){
    int b = ((int)x - CX) * 10 / CW;
    if(b < 0) b = 0; if(b > 9) b = 9;
    _eqBand = b;                               /* палець тягне саме цю смугу, хоч би куди зсунувся */
    _eqTouch(y);
    return;
  }
  if(_fadeStep >= 0 || !_isList(_cur)) return;
  if(x >= PL_BTN_X - 7){
    int b = ((int)y - 3) / 58; if(b < 0) b = 0; if(b > 3) b = 3;
    _smHold = b; _smHoldT = millis(); _smRep = false; _smBtnDraw = b;
    return;
  }
  /*  палець зупиняє накат чи пружину й бере список там, де він є. Такий
      дотик нічого не вибирає: інакше палець, що ловить список на ходу,
      вмикав випадковий сусідній рядок.  */
  _dCaught = (_smFling || _smAnim);
  _smFling = false; _smAnim = false;
  _dActive = true; _dMoved = false;
  _dY0 = _dLastY = y; _dPos0 = _smCur; _dVel = 0.0f; _dLastT = millis();
}

void YoMenu::onDrag(uint16_t x, uint16_t y){
  (void)x;
  if(_cur == PG_EQ && _eqBand >= 0){ _eqTouch(y); return; }
  if(_cur == PG_KBD && _kbDown){
    /*  Палець поїхав: клавіша міняється, лише коли він вийшов за межі
        поточної на 5 пікселів, — дрож на межі не перемикає сусідню.  */
    int16_t kx, ky, kw, kh;
    int8_t cur = _kbKey;
    bool inside = cur >= 0 && _kbKeyRect(cur, kx, ky, kw, kh) &&
                  (int)x >= kx - 5 && (int)x < kx + kw + 5 && (int)y >= ky - 5 && (int)y < ky + kh + 5;
    if(!inside){
      int8_t k = _kbKeyAt(x, y);
      if(k != cur){ _kbKey = k; _kbDownT = millis(); _kbRep = false; _kbDirty |= 8; }
    }
    /*  «стерти», якщо тримати, стирає далі  */
    uint32_t now = millis();
    if(_kbKey == K_BACK && now - _kbDownT > 500 && now - _kbRepT > 110){ _kbRepT = now; _kbRep = true; _kbAction(K_BACK); }
    return;
  }
  if(!_dActive || !_isList(_cur)) return;
  int16_t dy = (int16_t)y - _dY0;
  if(!_dMoved){
    if(abs(dy) <= 8) return;               /* поки це дотик, а не прокрутка */
    /*  Рушаємо від цієї точки, а не від місця натискання: інакше список
        стрибав одразу на весь поріг — саме це відчувалось як ривок.  */
    _dMoved = true; _dY0 = y; _dLastY = y; _dLastT = millis(); dy = 0;
  }
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
  /*  Палець веде нерівно, і список від того смикається. Трохи згладжуємо:
      рух лишається миттєвим, а дрібне тремтіння з'їдається.  */
  _smCur += (pos - _smCur) * 0.5f;
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
    if(_dCaught){                                  /* спіймали список на ходу */
      _dCaught = false;
      int16_t to = (int16_t)lroundf(_smCur);
      if(to < 1) to = 1; if(to > n) to = n;
      _smGo(to);                                   /* дотягуємо до рядка, нічого не вмикаючи */
      return;
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
    if(_apLock && _cur != PG_WSAVED && _cur != PG_WPICK && _cur != PG_WCONN) return;   /* звідси вихід є завжди */
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


  if(_isSound(_cur) && _hitSound(x, y)) return;
  switch(_cur){
    case PG_WCONN: {
      n_Try_e st = network.tryState();
      if(st != TRY_BADPASS && st != TRY_NOTFOUND && st != TRY_FAIL) return;
      if(y < WC_BTN_Y || y >= WC_BTN_Y + WC_BTN_H) return;
      network.tryClear();
      if((int)x < CX + 148){                      /* ще раз: той самий пароль правимо */
        _kbdNext = 2;
        snprintf(_kbdTitleBuf, sizeof(_kbdTitleBuf), "пароль: %s", _wSsid);
        _openKbd(_wPass, YOM_PASS_LEN, true, _kbdTitleBuf);
      }else _show(PG_WIFI);
      return;
    }
    case PG_WPICK: {
      if(y < WP_Y(0)) return;
      uint8_t i = (y - WP_Y(0)) / 62;
      if(i > 2 || y >= WP_Y(i) + WP_H){ if(_wpArm >= 0){ _wpArm = -1; _favDirty = true; } return; }
      if(i == 0){ _wifiConnect(); return; }                     /* підключитись */
      if(i == 1){                                               /* змінити пароль */
        _wpArm = -1; _kbdNext = 2;
        snprintf(_kbdTitleBuf, sizeof(_kbdTitleBuf), "пароль: %s", _wSsid);
        _openKbd(_wPass, YOM_PASS_LEN, true, _kbdTitleBuf);
        return;
      }
      /*  забути: перше торкання питає, друге виконує  */
      /*  Підтвердження має бути окремим дотиком, а не тим самим: між
          питанням і виконанням мусить пройти хоч трохи часу.  */
      if(_wpArm == 0 && millis() - _wpArmT > 400){
        _wpArm = -1;
        uint8_t k = 0;
        while(k < YOM_SSIDS && strcmp(_ssid[k], _wSsid)) k++;
        if(k < YOM_SSIDS){
          for(uint8_t j = k; j + 1 < YOM_SSIDS; j++){
            strlcpy(_ssid[j], _ssid[j+1], YOM_SSID_LEN);
            strlcpy(_pass[j], _pass[j+1], YOM_PASS_LEN);
          }
          _ssid[YOM_SSIDS-1][0] = 0; _pass[YOM_SSIDS-1][0] = 0;
          _savedWrite();
        }
        /*  Забули — значить і в пам'яті меню пароля більше нема: наступного
            разу поле має бути порожнім, а не зі старим, негодящим.  */
        _wPass[0] = 0; _wSsid[0] = 0;
        _show(PG_WIFI);
      }else{ _wpArm = 0; _wpArmT = millis(); _favDirty = true; }
      return;
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
        if(_wsArm == (int8_t)i && millis() - _wsArmT > 400){
          _wsArm = -1;
          for(uint8_t k = i; k + 1 < YOM_SSIDS; k++){
            strlcpy(_ssid[k], _ssid[k+1], YOM_SSID_LEN);
            strlcpy(_pass[k], _pass[k+1], YOM_PASS_LEN);
          }
          _ssid[YOM_SSIDS-1][0] = 0; _pass[YOM_SSIDS-1][0] = 0;
          _savedWrite();
          _wPass[0] = 0; _wSsid[0] = 0;
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
      if(y >= 32 && y < 60){ s.noBat = !s.noBat; _chkBat.setValue(!s.noBat); extras.changed(); }
      else if(y >= 60 && y < 86){ s.noIp = !s.noIp; _chkIp.setValue(!s.noIp); extras.changed(); }
      else if(y >= 86 && y < 112){
        s.noSd = !s.noSd; _chkSdEn.setValue(!s.noSd); extras.changed();
        if(s.noSd){ recorder.stop(); if(config.getMode() == PM_SDCARD) config.changeMode(PM_WEB); }
      }
      else if(y >= 112 && y < 146){
        uint16_t n = logos.forget();
        char m[48]; snprintf(m, sizeof(m), "шукатиму знову: %u", n);
        _setMsg(m); _favDirty = true;
        display.forceLogo();
      }
      else if(y >= 146 && y < 182) _show(PG_DEVSND);
      else if(y >= 182) _show(PG_DAC);
      break;
    }
    case PG_DEVSND: {
      ExtStore& s = extras.s;
      if(y >= 34 && y < 62){ s.splashOff = !s.splashOff; _chkSplash.setValue(!s.splashOff); extras.changed(); }
      else if(y >= 62 && y < 100){ int v = _sldSplash.valueAt(x); _sldSplash.setValue(v); s.splashVol = v; extras.changed(); }
      else if(y >= 102 && y < 128){ s.sfxOn = !s.sfxOn; _chkSfx.setValue(s.sfxOn); extras.changed(); }
      else if(y >= 128 && y < 166){
        int v = _sldSfx.valueAt(x); _sldSfx.setValue(v); s.sfxVol = v; extras.changed();
        sfx.test(SFX_GESTURE);                        /* почути нову гучність */
      }
      else if(y >= 168 && y < 208){ _afterClose = 2; close(); }   /* заставка — коли меню вже закрилось */
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
      int r = ((int)y - HM_Y(0)) / 69; if(r > 2) r = 2;
      static const int8_t go[9] = { PG_INFO, PG_WIFI, PG_TIME, PG_SYS, PG_MIC, PG_DEV, PG_POWER, -1, -1 };
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


/*  =====================================================================
 *  Звук і мікрофон
 *
 *  «звук» — еквалайзер на 10 смуг: пресет ◀ ▶, увімк/вимк, повзунки
 *  пальцем, внизу — «під кімнату» і «налаштування» (обробка: захист
 *  динаміка, віртуальний бас, тонкомпенсація, баланс).
 *  «мікрофон» (у параметрах) — слухати, чутливість, під час звуку, живий
 *  рівень; далі «хлопки й стук» і «присутність».
 *  ===================================================================== */


static inline int16_t eqColX(uint8_t b){ return CX + (int16_t)(b * CW / 10); }
static const int16_t EQ_COLW = CW / 10;
static inline int16_t eqY(float db){
  if(db > 12) db = 12;
  if(db < -12) db = -12;
  return EQ_ZERO - (int16_t)lroundf(db * (EQ_ZERO - EQ_T0) / 12.0f);
}
static const char* const EQ_FRQ[10] = { "31", "62", "125", "250", "500", "1к", "2к", "4к", "8к", "16к" };

static void uiText(int16_t x, int16_t y, const char* t, uint16_t c, const GFXfont* f){
  dsp.setFont(f); dsp.setTextSize(1); dsp.setTextColor(c);
  char b[72]; snprintf(b, sizeof(b), "%s", utf8Rus(t, false));
  fitText(b, SW - x - 4);
  dsp.setCursor(x, y); dsp.print(b);
  dsp.setFont();
}

/*  Дрібні підписи (частоти, значення смуг) — вбудованим шрифтом 5×7: у
    колонку 28 пікселів гладкий шрифт «125» уже не вміщує.  */
static void uiTiny(int16_t x, int16_t w, int16_t y, const char* t, uint16_t c){
  /*  y — верх рядка, як у вбудованого шрифту; вузький згладжений шрифт  */
  dsp.setFont(&aaUI6); dsp.setTextSize(1); dsp.setTextColor(c);
  char b[16]; snprintf(b, sizeof(b), "%s", utf8Rus(t, false));
  dsp.setCursor(x + (w - (int16_t)textW(b)) / 2, y + 7); dsp.print(b);
  dsp.setFont();
}

static void uiTextC(int16_t x, int16_t w, int16_t y, const char* t, uint16_t c, const GFXfont* f){
  dsp.setFont(f); dsp.setTextSize(1); dsp.setTextColor(c);
  char b[72]; snprintf(b, sizeof(b), "%s", utf8Rus(t, false));
  fitText(b, w - 4);
  dsp.setCursor(x + (w - (int16_t)textW(b)) / 2, y); dsp.print(b);
  dsp.setFont();
}

static void uiBtn(int16_t x, int16_t y, int16_t w, int16_t h, const char* t, bool on, const GFXfont* f){
  dsp.box(x, y, w, h, R_BTN, on ? C_ACC : C_PANEL, C_BG);
  uiTextC(x, w, y + h / 2 + (f == &yoUI8 ? 4 : 5), t, on ? C_BG : C_TXT, f);
}

static void uiArrow(int16_t x, int16_t y, int16_t w, int16_t h, bool right){
  dsp.box(x, y, w, h, R_BTN, C_PAN2, C_BG);
  float cx = x + w / 2.0f, cy = y + h / 2.0f;
  if(right) dsp.triAA(cx - 3.5f, cy - 6.5f, cx - 3.5f, cy + 6.5f, cx + 5, cy, C_ACC);
  else      dsp.triAA(cx + 3.5f, cy - 6.5f, cx + 3.5f, cy + 6.5f, cx - 5, cy, C_ACC);
}

static const char* const GUARD_LBL[3]  = { "вимк", "м'який", "сильний" };
static const char* const VB_LBL[4]     = { "вимк", "1", "2", "3" };
static const char* const LOUD_LBL[3]   = { "вимк", "м'яка", "сильна" };
static const char* const GAIN_LBL[4]   = { "низька", "середня", "висока", "макс" };
static const uint8_t     GAIN_VAL[4]   = { 3, 0, 7, 8 };          /* extras.s.micGain: 0 — типове (24 дБ) */
static const char* const GTAB_LBL[2]   = { "хлопки", "стук" };
static const char* const SENS_LBL[3]   = { "низька", "середня", "висока" };
static const char* const EARMIN_LBL[4] = { "5", "10", "15", "30" };
static const uint8_t     EARMIN_VAL[4] = { 5, 10, 15, 30 };
static const char* const POFF_LBL[4]   = { "ні", "5", "15", "30" };
static const uint8_t     POFF_VAL[4]   = { 0, 5, 15, 30 };
static const uint8_t     ACT_ORDER[8]  = { MA_TOGGLE, MA_NEXT, MA_PREV, MA_VOLUP, MA_VOLDN, MA_SCREEN, MA_FAV1, MA_NONE };

void YoMenu::_buildSound(){
  /*  обробка  */
  _sGuard.init(wc(CX, 50),  &yoUI9b, CW, 26, C_TXT, C_BG, C_ACC, C_PAN2); _sGuard.setItems(3, GUARD_LBL);
  _sVb   .init(wc(CX, 98),  &yoUI9b, CW, 26, C_TXT, C_BG, C_ACC, C_PAN2); _sVb.setItems(4, VB_LBL);
  _sLoud .init(wc(CX, 146), &yoUI9b, CW, 26, C_TXT, C_BG, C_ACC, C_PAN2); _sLoud.setItems(3, LOUD_LBL);
  _sBal.init(wc(CX, 186), &yoUI9, CW, -16, 16, C_TXT, C_BG, C_ACC); _sBal.setLabel("баланс");
  _pg[PG_SND]->addWidget(&_sGuard);
  _pg[PG_SND]->addWidget(&_sVb);
  _pg[PG_SND]->addWidget(&_sLoud);
  _pg[PG_SND]->addWidget(&_sBal);
  /*  мікрофон  */
  _mOn.init(wc(CX, 36), &yoUI9, 150, C_TXT, C_BG, C_ACC); _mOn.setLabel("слухати");
  _mGain.init(wc(CX, 94), &yoUI8, CW, 26, C_TXT, C_BG, C_ACC, C_PAN2); _mGain.setItems(4, GAIN_LBL);
  _mPlay.init(wc(CX, 128), &yoUI9, CW, C_TXT, C_BG, C_ACC); _mPlay.setLabel("слухати й під час звуку");
  _pg[PG_MIC]->addWidget(&_mOn);
  _pg[PG_MIC]->addWidget(&_mGain);
  _pg[PG_MIC]->addWidget(&_mPlay);
  /*  хлопки й стук  */
  _gTab.init(wc(CX, 32), &yoUI9b, CW, 26, C_TXT, C_BG, C_ACC, C_PAN2); _gTab.setItems(2, GTAB_LBL);
  _gOn.init(wc(CX, 68), &yoUI9, CW, C_TXT, C_BG, C_ACC); _gOn.setLabel("увімкнено");
  _gSens.init(wc(CX, 116), &yoUI8, CW, 26, C_TXT, C_BG, C_ACC, C_PAN2); _gSens.setItems(3, SENS_LBL);
  _pg[PG_MGEST]->addWidget(&_gTab);
  _pg[PG_MGEST]->addWidget(&_gOn);
  _pg[PG_MGEST]->addWidget(&_gSens);
  /*  присутність  */
  _pEar.init(wc(CX, 34), &yoUI9, CW, C_TXT, C_BG, C_ACC); _pEar.setLabel("таймер сну слухає");
  _pEarMin.init(wc(CX, 100), &yoUI9b, CW, 26, C_TXT, C_BG, C_ACC, C_PAN2); _pEarMin.setItems(4, EARMIN_LBL);
  _pWake.init(wc(CX, 140), &yoUI9, CW, C_TXT, C_BG, C_ACC); _pWake.setLabel("голос будить екран");
  _pOff.init(wc(CX, 196), &yoUI9b, CW, 26, C_TXT, C_BG, C_ACC, C_PAN2); _pOff.setItems(4, POFF_LBL);
  _pg[PG_MPRES]->addWidget(&_pEar);
  _pg[PG_MPRES]->addWidget(&_pEarMin);
  _pg[PG_MPRES]->addWidget(&_pWake);
  _pg[PG_MPRES]->addWidget(&_pOff);
}

/*  ---------- еквалайзер ---------- */

void YoMenu::_drawEqTop(){
  const ExtStore& e = extras.s;
  uiArrow(CX, EQ_TOP_Y, 34, EQ_TOP_H, false);
  dsp.box(CX + 36, EQ_TOP_Y, 116, EQ_TOP_H, R_BTN, C_PANEL, C_BG);
  uiTextC(CX + 36, 116, EQ_TOP_Y + 19, YoDsp::PRESET_NAME[e.eqPreset < EQ_PRESETS ? e.eqPreset : 0], e.eqOn ? C_TXT : C_DIM, &yoUI9b);
  uiArrow(CX + 154, EQ_TOP_Y, 34, EQ_TOP_H, true);
  uiBtn(CX + 196, EQ_TOP_Y, CW - 196, EQ_TOP_H, e.eqOn ? "увімкнено" : "вимкнено", e.eqOn, &yoUI8);
}

void YoMenu::_drawEqBand(uint8_t b){
  const ExtStore& e = extras.s;
  int16_t x0 = eqColX(b), cx = x0 + EQ_COLW / 2;
  bool on = e.eqOn;
  int v = e.eq[b];
  dsp.fillRect(x0, EQ_COL0, EQ_COLW, EQ_T1 - EQ_COL0 + 6, C_BG);
  char t[8];
  if(v) snprintf(t, sizeof(t), "%+d", v); else snprintf(t, sizeof(t), "0");
  uiTiny(x0, EQ_COLW, EQ_VAL_Y - 7, t, (on && v) ? C_ACC : C_DIM);
  dsp.box(cx - 1, EQ_T0, 3, EQ_T1 - EQ_T0, 1.5f, C_PAN2, C_BG);
  dsp.fillRect(cx - 6, EQ_ZERO, 13, 1, C_DIM);
  int16_t yv = eqY(v);
  if(v) dsp.fillRect(cx - 2, v > 0 ? yv : EQ_ZERO, 5, abs(yv - EQ_ZERO), on ? C_ACC : C_DIM);
  /*  поправка під кімнату — бірюзовою позначкою там, де смуга звучить разом із нею  */
  if(e.eqRoomOn && e.eqRoom[b]){
    int16_t yr = eqY((float)(on ? v : 0) + e.eqRoom[b]);
    dsp.fillRect(cx + 5, yr - 2, 5, 5, C_ROOM);
  }
  dsp.fillRoundRectAA(cx - 9, yv - 3, 19, 7, 3.5f, on ? C_TXT : C_DIM);
}

void YoMenu::_drawEqBottom(){
  const ExtStore& e = extras.s;
  dsp.fillRect(CX, EQ_BTN_Y, CW, EQ_BTN_H, C_BG);
  uiBtn(CX, EQ_BTN_Y, 140, EQ_BTN_H, e.eqRoomOn ? "кімната: є" : "під кімнату", false, &yoUI9);
  uiBtn(CX + 146, EQ_BTN_Y, CW - 146, EQ_BTN_H, "налаштування", false, &yoUI9);
}

/*  Палець на повзунку: значення міняється одразу на екрані, а ланцюг звуку
    перераховується не частіше ніж раз на 150 мс (це сотні синусів).  */
void YoMenu::_eqTouch(uint16_t y){
  if(_eqBand < 0) return;
  ExtStore& e = extras.s;
  int db = (int)lroundf((float)(EQ_ZERO - (int)y) * 12.0f / (EQ_ZERO - EQ_T0));
  if(db > 12) db = 12;
  if(db < -12) db = -12;
  uint8_t b = (uint8_t)_eqBand;
  if(db == e.eq[b] && e.eqOn && e.eqPreset == 0) return;
  uint16_t m = 1 << b;
  if(e.eqPreset){ e.eqPreset = 0; m |= 1 << 10; }
  if(!e.eqOn){ e.eqOn = 1; m |= 0x3FF | (1 << 10); }
  e.eq[b] = (int8_t)db;
  _sndMask |= m;
  _eqPend = true;
  _eqApply(false);
}

void YoMenu::_eqApply(bool force){
  if(!_eqPend) return;
  if(!force && millis() - _eqApplyT < 150) return;
  _eqApplyT = millis();
  _eqPend = false;
  extras.changed();
  yoDsp.changed();
}

/*  ---------- під кімнату ---------- */

void YoMenu::_drawRoom(bool full){
  const ExtStore& e = extras.s;
  if(full){
    uiText(CX, 46, "радіо грає тони й слухає себе", C_DIM, &yoUI8);
    uiText(CX, 60, "у кімнаті має бути тихо", C_DIM, &yoUI8);
    for(uint8_t b = 0; b < 10; b++) uiTiny(eqColX(b), EQ_COLW, 154, EQ_FRQ[b], C_DIM);
  }
  bool has = false;
  for(uint8_t b = 0; b < 10; b++) if(e.eqRoom[b]) has = true;
  dsp.fillRect(CX, 66, CW, 84, C_BG);
  dsp.fillRect(CX, 108, CW, 1, C_PAN2);
  for(uint8_t b = 0; b < 10; b++){
    int v = e.eqRoom[b];
    int16_t cx = eqColX(b) + EQ_COLW / 2, yv = 108 - v * 4;
    if(v) dsp.fillRect(cx - 6, v > 0 ? yv : 108, 12, abs(v * 4), e.eqRoomOn ? C_ROOM : C_DIM);
  }
  if(!has) uiTextC(CX, CW, 100, "ще не міряли", C_DIM, &yoUI8);
  /*  хід і результат  */
  uint8_t st = yoDsp.roomState(), pr = yoDsp.roomProgress();
  dsp.fillRect(CX, 168, CW, 28, C_BG);
  char t[64];
  if(_rmErr[0])                 snprintf(t, sizeof(t), "%s", _rmErr);
  else if(st == 1 || st == 2)   snprintf(t, sizeof(t), "%s... %u%%", yoDsp.roomMsg(), (unsigned)pr);
  else if(st == 3)              snprintf(t, sizeof(t), "%s", yoDsp.roomMsg());
  else if(st == 4)              snprintf(t, sizeof(t), "не вийшло: %s", yoDsp.roomMsg());
  else                          snprintf(t, sizeof(t), "%s", has ? "поправку зміряно" : "");
  uiText(CX, 182, t, st == 4 || _rmErr[0] ? 0xFB00 : C_TXT, &yoUI8);
  if(st == 1 || st == 2){
    dsp.box(CX, 188, CW, 4, 2, C_PANEL, C_BG);
    if(pr) dsp.box(CX, 188, (int16_t)(CW * pr / 100) < 4 ? 4 : (int16_t)(CW * pr / 100), 4, 2, C_ACC, C_PANEL);
  }
  bool busy = st == 1 || st == 2;
  uiBtn(CX, 202, 140, 34, busy ? "зупинити" : "зміряти", !busy, &yoUI9b);
  uiBtn(CX + 146, 202, CW - 146, 34, e.eqRoomOn ? "поправка увімк" : "поправка вимк", e.eqRoomOn && has, &yoUI8);
}

/*  ---------- мікрофон ---------- */

void YoMenu::_drawMicLive(){
  const int16_t mx = CX + 160, mw = CW - 160;
  bool on = mic.listening();
  if(_cur == PG_MIC){
    dsp.fillRect(mx, 36, mw, 28, C_BG);
    dsp.box(mx, 44, mw, 12, 6, C_PANEL, C_BG);
  }
  if(on && _cur == PG_MIC){
    float lv = mic.levelDb(), nz = mic.noiseDb();
    int16_t w = (int16_t)((lv + 80.0f) * mw / 60.0f);
    if(w < 0) w = 0; if(w > mw) w = mw;
    if(w >= 12) dsp.box(mx, 44, w, 12, 6, mic.speech() ? C_VOICE : C_ACC, C_PANEL);
    int16_t nx = (int16_t)((nz + 80.0f) * mw / 60.0f);
    if(nx >= 0 && nx < mw - 1) dsp.box(mx + nx - 1, 39, 3, 22, 1.5f, C_TXT, C_BG);
  }
  char t[64];
  uint32_t now = millis();
  if(!extras.s.micOn)                                   snprintf(t, sizeof(t), "мікрофон вимкнено");
  else if(mic.heard() && now - mic.heardMs() < 8000)    snprintf(t, sizeof(t), "почуто: %s, %s", YoMic::gestureName(mic.heard()), YoMic::actionName(YoMic::actionFor((MicGesture)mic.heard())));
  else if(mic.speech())                                 snprintf(t, sizeof(t), "чую голос");
  else if(mic.aecActive())                              snprintf(t, sizeof(t), "віднімаю власний звук");
  else                                                  snprintf(t, sizeof(t), "слухаю");
  if(_cur == PG_MIC){
    dsp.fillRect(CX, 208, CW, 20, C_BG);
    uiText(CX, 222, t, C_DIM, &yoUI8);
  }else if(_cur == PG_MGEST){
    dsp.fillRect(CX, 222, CW, 18, C_BG);
    if(mic.heard() && now - mic.heardMs() < 8000) uiText(CX, 234, t, C_ACC, &yoUI8);
    else uiText(CX, 234, extras.s.micOn ? "плесніть чи постукайте двічі" : "мікрофон вимкнено", C_DIM, &yoUI8);
  }
}

static uint8_t gestIdx(uint8_t kind){
  const ExtStore& e = extras.s;
  uint8_t sens = kind ? e.knockSens : e.clapSens;       /* 0 середня, 1 низька, 2 висока */
  return sens == 1 ? 0 : sens == 2 ? 2 : 1;
}

void YoMenu::_syncGest(){
  const ExtStore& e = extras.s;
  _gTab.setSel(_gKind);
  _gOn.setValue(_gKind ? e.knockOn : e.clapOn);
  _gSens.setSel(gestIdx(_gKind));
}

void YoMenu::_drawGestRows(){
  for(uint8_t r = 0; r < 2; r++){
    int16_t y = 150 + r * 36;
    MicGesture g = _gKind ? (r ? MG_KNOCK3 : MG_KNOCK2) : (r ? MG_CLAP3 : MG_CLAP2);
    dsp.fillRect(CX, y, CW, 32, C_BG);
    uiText(CX, y + 21, r ? "3 рази" : "2 рази", C_TXT, &yoUI9);
    uiArrow(CX + 70, y, 30, 30, false);
    dsp.box(CX + 102, y, 154, 30, R_BTN, C_PANEL, C_BG);
    uiTextC(CX + 102, 154, y + 20, YoMic::actionName(YoMic::actionFor(g)), C_TXT, &yoUI9);
    uiArrow(CX + 258, y, 30, 30, true);
  }
}

void YoMenu::_syncPres(){
  const ExtStore& e = extras.s;
  _pEar.setValue(e.sleepEar);
  uint8_t em = e.sleepEarMin ? e.sleepEarMin : 10;
  int8_t ei = 1;
  for(uint8_t i = 0; i < 4; i++) if(EARMIN_VAL[i] == em) ei = i;
  _pEarMin.setSel(ei);
  _pWake.setValue(e.presWake);
  int8_t pi = 0;
  for(uint8_t i = 0; i < 4; i++) if(POFF_VAL[i] == e.presOff) pi = i;
  _pOff.setSel(pi);
}

/*  ---------- сторінки: малювання й такт ---------- */

void YoMenu::_paintSound(int8_t p){
  const ExtStore& e = extras.s;
  _sndMask = 0;
  if(p == PG_EQ){
    _chrome("звук", 2);
    _drawEqTop();
    for(uint8_t b = 0; b < 10; b++){ _drawEqBand(b); uiTiny(eqColX(b), EQ_COLW, EQ_FRQ_Y - 7, EQ_FRQ[b], C_DIM); }
    _drawEqBottom();
  }else if(p == PG_SND){
    _chrome("обробка", 0);
    uiText(CX, 46, "захист динаміка", C_DIM, &yoUI9);
    uiText(CX, 94, "віртуальний бас", C_DIM, &yoUI9);
    uiText(CX, 142, "тонкомпенсація, коли тихо", C_DIM, &yoUI9);
    _sGuard.setSel(e.eqGuard);
    _sVb.setSel(e.vbass);
    _sLoud.setSel(e.eqLoud);
    _sBal.setValue(config.store.balance);
  }else if(p == PG_ROOM){
    _chrome("кімната", 0);
    _rmErr[0] = 0;
    _drawRoom(true);
    _rmShown = yoDsp.roomState(); _rmProg = yoDsp.roomProgress();
  }else if(p == PG_MIC){
    _chrome("мікрофон", 0);
    uiText(CX, 88, "чутливість", C_DIM, &yoUI9);
    uiBtn(CX, 164, 130, 34, "хлопки й стук", false, &yoUI8);
    uiBtn(CX + 136, 164, CW - 136, 34, "сон і присутність", false, &yoUI8);
    _mOn.setValue(e.micOn);
    int8_t gi = 1;
    for(uint8_t i = 0; i < 4; i++) if(GAIN_VAL[i] == e.micGain) gi = i;
    _mGain.setSel(gi);
    _mPlay.setValue(e.micPlay);
    _drawMicLive();
  }else if(p == PG_MGEST){
    _chrome("хлопки й стук", 0);
    uiText(CX, 112, "чутливість", C_DIM, &yoUI9);
    _syncGest();
    _drawGestRows();
    _drawMicLive();
  }else if(p == PG_MPRES){
    _chrome("присутність", 0);
    uiText(CX, 94, "у кімнаті тихо, хв", C_DIM, &yoUI9);
    uiText(CX, 190, "гасити екран, коли тихо, хв", C_DIM, &yoUI9);
    _syncPres();
  }
}

void YoMenu::_renderSound(){
  uint16_t m = _sndMask;
  if(m) _sndMask &= ~m;
  uint32_t now = millis();
  if(_cur == PG_EQ){
    if(m & (1 << 10)) _drawEqTop();
    for(uint8_t b = 0; b < 10; b++) if(m & (1 << b)) _drawEqBand(b);
    if(m & (1 << 11)) _drawEqBottom();
  }else if(_cur == PG_ROOM){
    uint8_t st = yoDsp.roomState(), pr = yoDsp.roomProgress();
    if(m || st != _rmShown || pr != _rmProg){ _rmShown = st; _rmProg = pr; _drawRoom(false); }
  }else if(_cur == PG_MIC || _cur == PG_MGEST){
    if(m & (1 << 12)) _drawGestRows();
    if(now - _micLiveT > 150){ _micLiveT = now; _drawMicLive(); }
  }
}

/*  ---------- дотики ---------- */

bool YoMenu::_hitSound(uint16_t x, uint16_t y){
  ExtStore& e = extras.s;
  switch(_cur){
    case PG_EQ: {
      if(y >= EQ_TOP_Y - 2 && y < EQ_TOP_Y + EQ_TOP_H + 4){
        uint8_t p = e.eqPreset;
        if((int)x < CX + 40)                            yoDsp.applyPreset(p <= 1 ? EQ_PRESETS - 1 : p - 1);
        else if((int)x >= CX + 150 && (int)x < CX + 192) yoDsp.applyPreset(p >= EQ_PRESETS - 1 ? 1 : p + 1);
        else if((int)x >= CX + 196){ e.eqOn = !e.eqOn; extras.changed(); yoDsp.changed(); }
        else return true;
        _sndMask |= 0x7FF;
      }else if(y >= EQ_BTN_Y - 4){
        _show((int)x < CX + 143 ? PG_ROOM : PG_SND);
      }
      return true;
    }
    case PG_SND: {
      int8_t i;
      if(y >= 46 && y < 80)       { if((i = _sGuard.indexAt(x)) < 0) return true; e.eqGuard = i; _sGuard.setSel(i); }
      else if(y >= 94 && y < 128) { if((i = _sVb.indexAt(x)) < 0) return true;    e.vbass = i;   _sVb.setSel(i); }
      else if(y >= 142 && y < 176){ if((i = _sLoud.indexAt(x)) < 0) return true;  e.eqLoud = i;  _sLoud.setSel(i); }
      else if(y >= 180){
        int v = _sBal.valueAt(x);
        _sBal.setValue(v);
        config.setBalance((int8_t)v);
        return true;
      }else return true;
      extras.changed(); yoDsp.changed();
      return true;
    }
    case PG_ROOM: {
      if(y < 198) return true;
      uint8_t st = yoDsp.roomState();
      _rmErr[0] = 0;
      if((int)x < CX + 143){
        if(st == 1 || st == 2) mic.sweepAbort();
        else {
          if(!e.micOn){ e.micOn = 1; extras.changed(); mic.apply(); }
          const char* w = yoDsp.roomTuneStart();
          if(w) snprintf(_rmErr, sizeof(_rmErr), "%s", w);
        }
      }else{
        bool has = false;
        for(uint8_t b = 0; b < 10; b++) if(e.eqRoom[b]) has = true;
        if(!has){ snprintf(_rmErr, sizeof(_rmErr), "спершу зміряйте"); }
        else { e.eqRoomOn = !e.eqRoomOn; extras.changed(); yoDsp.changed(); }
      }
      _sndMask |= 1;
      return true;
    }
    case PG_MIC: {
      if(y >= 30 && y < 68 && (int)x < CX + 156){
        e.micOn = !e.micOn; _mOn.setValue(e.micOn); extras.changed(); mic.apply();
      }else if(y >= 90 && y < 124){
        int8_t i = _mGain.indexAt(x); if(i < 0) return true;
        e.micGain = GAIN_VAL[i]; _mGain.setSel(i); extras.changed(); mic.apply();
      }else if(y >= 124 && y < 160){
        e.micPlay = !e.micPlay; _mPlay.setValue(e.micPlay); extras.changed();
      }else if(y >= 160 && y < 204){
        _show((int)x < CX + 133 ? PG_MGEST : PG_MPRES);
      }
      return true;
    }
    case PG_MGEST: {
      if(y >= 28 && y < 62){
        int8_t i = _gTab.indexAt(x); if(i < 0) return true;
        _gKind = i; _syncGest(); _sndMask |= 1 << 12;
      }else if(y >= 62 && y < 100){
        uint8_t& on = _gKind ? e.knockOn : e.clapOn;
        on = !on; _gOn.setValue(on);
        if(on && !e.micOn){ e.micOn = 1; mic.apply(); }
        extras.changed();
      }else if(y >= 112 && y < 146){
        int8_t i = _gSens.indexAt(x); if(i < 0) return true;
        uint8_t v = i == 0 ? 1 : i == 2 ? 2 : 0;
        (_gKind ? e.knockSens : e.clapSens) = v;
        _gSens.setSel(i); extras.changed();
      }else if(y >= 148 && y < 220){
        uint8_t r = y < 184 ? 0 : 1;
        MicGesture g = _gKind ? (r ? MG_KNOCK3 : MG_KNOCK2) : (r ? MG_CLAP3 : MG_CLAP2);
        uint8_t cur = YoMic::actionFor(g), k = 0;
        for(uint8_t i = 0; i < 8; i++) if(ACT_ORDER[i] == cur) k = i;
        k = (int)x < CX + 102 ? (k + 7) % 8 : (k + 1) % 8;
        uint8_t& slot = _gKind ? (r ? e.knock3 : e.knock2) : (r ? e.clap3 : e.clap2);
        slot = ACT_ORDER[k];
        extras.changed();
        _sndMask |= 1 << 12;
      }
      return true;
    }
    case PG_MPRES: {
      if(y >= 30 && y < 68){
        e.sleepEar = !e.sleepEar;
        if(e.sleepEar && !e.micOn){ e.micOn = 1; mic.apply(); }
      }else if(y >= 96 && y < 132){
        int8_t i = _pEarMin.indexAt(x); if(i < 0) return true;
        e.sleepEarMin = EARMIN_VAL[i];
      }else if(y >= 136 && y < 174){
        e.presWake = !e.presWake;
        if(e.presWake && !e.micOn){ e.micOn = 1; mic.apply(); }
      }else if(y >= 192 && y < 228){
        int8_t i = _pOff.indexAt(x); if(i < 0) return true;
        e.presOff = POFF_VAL[i];
        if(e.presOff && !e.micOn){ e.micOn = 1; mic.apply(); }
      }else return true;
      extras.changed();
      _syncPres();
      return true;
    }
  }
  return false;
}

#endif
