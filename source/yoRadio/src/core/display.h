#ifndef display_h
#define display_h
#include "common.h"

#if DSP_MODEL==DSP_DUMMY
#define DUMMYDISPLAY
#endif

#ifndef DUMMYDISPLAY
class ScrollWidget;
class PlayListWidget;
class BitrateWidget;
class FillWidget;
class SliderWidget;
class Pager;
class Page;
class VuWidget;
class WeatherIconWidget;
class SdCtlWidget;
class NumWidget;
class ClockWidget;
class TextWidget;
    
/*  Рядок обраного на головному (варіант А) — для сенсора теж  */
#define FM_TILE   45
#define FM_X(i)   (8 + (i) * 51)
#define FM_TY     151
#define FM_Y0     146
#define FM_Y1     202

class Display {
  public:
    uint16_t currentPlItem;
    uint16_t numOfNextStation;
    displayMode_e _mode;
  public:
    Display() {};
    ~Display();
    displayMode_e mode() { return _mode; }
    void mode(displayMode_e m) { _mode=m; }
    void init();
    void loop();
    void _start();
    void splashDemo(uint32_t ms){ _splashDemoMs = ms ? ms : 1; }   /* відладка: заставка поверх плеєра */
    bool ready() { return _bootStep==2; }
    void resetQueue();
    void putRequest(displayRequestType_e type, int payload=0);
    void flip();
    void invert();
    void forceRedraw();
    void fadeLoop();             /* плавна зміна екранів, із задачі дисплея */
    bool fading() const { return _fadeStep >= 0; }
    void openStationsNow() { _swichMode(STATIONS); }   /* із задачі дисплея, коли меню закривається в темряві */
    void forceLogo() { _logoCrc = 1; }                 /* перечитати логотип станції */
    void requestRedraw() { _redrawReq = true; }        /* з іншої задачі: перемалювати плеєр (веб змінив налаштування) */
    bool sermonPult() const { return _smLayout; }      /* пульт на плеєрі керує проповіддю */
    bool favMainOn() const { return _fmOn; }           /* на головному рядок обраного (варіант А) */
    void plButton(int8_t idx, bool on);  /* стан кнопки плейлиста; малює задача дисплея */
    void sdPress(int8_t which, bool on); /* пульт картки: натиснута кнопка треку */
    void sdPreview(float f);             /* пульт картки: куди веде палець, <0 — ніде */
    uint16_t volbarShown();      /* для перевірки анімації гучності */
    void dbgWeather();           /* стан віджетів погоди */
    void drawHeaderIcons();
    void plScrollTo(float pos);   /* стан, малює задача дисплея */
    void plScrollStop(float finalPos);
    void plSmooth(float pos);
    uint32_t plBenchmark(uint16_t frames);   /* скільки мс на кадр */      /* плавна прокрутка списку */
    void plSettle(uint16_t item);  /* повернення до звичайного вигляду */      /* список і шестерня у шапці плеєра */          /* полная перерисовка после выхода из меню */
    bool deepsleep();
    void wakeup();
    void setContrast();
    void lock()   { _locked=true; }
    void unlock() { _locked=false; }
    uint16_t width();
    uint16_t height();
  private:
    ScrollWidget *_meta, *_title1, *_plcurrent, *_weather, *_title2;
    WeatherIconWidget *_weathericon = nullptr;  /* картинка погоди з проєкту Nextion */
    SdCtlWidget *_sdctl = nullptr;       /* пульт відтворення з картки */
    void _applySdLayout();               /* погода чи пульт — залежно від режиму */
    int8_t   _fadeStep = -1;     /* крок плавної зміни екрана */
    uint32_t _fadeTick = 0;
    displayMode_e _fadeTo = PLAYER;
    void _beginFade(displayMode_e to);
    uint32_t _sbSig = 0xFFFFFFFF;        /* що зараз намальовано в рядку стану */
    uint32_t _sbTick = 0;
    void _statusBar();                   /* Wi-Fi, батарея, будильник, сон */
    int16_t  _sbBatX = -1;               /* де намальовано батарею — для анімації */
    uint8_t  _sbPh = 0xFF;
    void _lowBat();                      /* попередження про розряд */
    void _applyDev();                    /* перемикачі сторінки «розробник» */
    uint16_t* _logoPix = nullptr;        /* логотип станції 45x45 */
    uint32_t  _logoCrc = 0;
    volatile bool _redrawReq = false;
    bool      _logoOn = false;
    void _stationLogo();
    bool     _smLayout = false;          /* грає проповідь: обкладинка замість погоди */
    uint32_t _smShown = 0xFFFFFFFF;
    void _sermonLayout();
    void _sermonBlock();
    char     _smPr[80] = {0};            /* проповідник, бігучим рядком, якщо не влазить */
    int16_t  _smPrW = 0, _smPrShift = 0;
    uint32_t _smPrT = 0;
    void _sermonMarquee(bool force);
    /*  Обране на головному: компактний годинник угорі, під ним дата, далі
        рядок із шести логотипів — дотик по логотипу вмикає станцію.  */
    bool     _fmOn = false;
    uint32_t _fmSig = 0, _fmSigT = 0;
    int16_t  _fmMin = -1;
    int8_t   _fmSec = -1;
    uint16_t* _fmPix = nullptr;          /* 6 логотипів 45x45 */
    uint32_t _fmPixCrc[6] = {0};
    uint32_t _fmLogoVer = 0xFFFFFFFF;
    void _favMain();
    void _fmApply(bool on);
    void _fmClock(bool full);
    void _fmRow();
    volatile int8_t _plBtnIdx = -1;
    volatile bool   _plBtnOn = false, _plBtnDirty = false;
    volatile bool   _plFinal = false;  /* прокрутка закінчилась — дорисувати й продовжити таймер */
    PlayListWidget *_plwidget;
    BitrateWidget *_fullbitrate;
    FillWidget *_metabackground, *_plbackground;
    SliderWidget *_volbar, *_heapbar;
    Pager *_pager;
    Page *_footer;
    VuWidget *_vuwidget;
    NumWidget *_nums;
    ClockWidget *_clock;
    Page *_boot;
    Page *_m2empty = nullptr;          /* новий головний екран (src/m2): сторінка плеєра без старих віджетів */
    TextWidget *_bootstring, *_volip, *_voltxt, *_rssi, *_bitrate;
    bool _locked = false;
    bool  _plScroll = false;
    float _plScrollPos = 0.0f;
    uint8_t _bootStep;
    bool    _lostPending = false;   /* зв'язок зник ще на заставці — показати, щойно плеєр готовий */
    bool    _playerBuilt = false;   /* сторінки й віджети плеєра створено */
    volatile uint32_t _splashDemoMs = 0; /* відладка: показати заставку стільки мс */
    uint32_t _splashDemoUntil = 0;
    bool    _ensurePlayer();        /* стартували без мережі, а вона з'явилась — добудувати плеєр */
    void    _finishStart(bool draw);
    void _time(bool redraw = false);
    void _noNetScreen();          /* мережі немає: одразу список мереж */
    void _swichMode(displayMode_e newmode);
    void _drawPlaylist();
    void _volume();
    void _title();
    void _station();
    void _drawNextStationNum(uint16_t num);
    void _createDspTask();
    void _showDialog(const char *title);
    void _buildPager();
    void _bootScreen();
    void _layoutChange(bool played);
    void _setRSSI(int rssi);
};

#else

class Display {
  public:
    uint16_t currentPlItem;
    uint16_t numOfNextStation;
    displayMode_e _mode;
  public:
    Display() {};
    displayMode_e mode() { return _mode; }
    void mode(displayMode_e m) { _mode=m; }
    void init();
    void _start();
    void putRequest(displayRequestType_e type, int payload=0);
    void loop(){}
    bool ready() { return true; }
    void resetQueue(){}
    void centerText(const char* text, uint8_t y, uint16_t fg, uint16_t bg){}
    void rightText(const char* text, uint8_t y, uint16_t fg, uint16_t bg){}
    void flip(){}
    void invert(){}
    void setContrast(){}
    bool deepsleep(){return true;}
    void wakeup(){}
    void printPLitem(uint8_t pos, const char* item){}
    void lock()   {}
    void unlock() {}
    uint16_t width(){ return 0; }
    uint16_t height(){ return 0; }
  private:
    void _createDspTask();
};

#endif

extern Display display;


#endif
