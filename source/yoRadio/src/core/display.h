#ifndef display_h
#define display_h
#include "common.h"

/*  Екран радіо. Малюють його головний екран і меню (src/m2); цей клас — задача
    дисплея й черга запитів від решти радіо: старт і заставка, які з режимів
    (плеєр, «немає зв'язку», картка, оновлення, згаслий екран) зараз показати,
    плавна зміна в темряві, сон і пробудження матриці.

    Старий вигляд yoRadio (сторінки й віджети плеєра, діалоги з жовтою смугою,
    старий список станцій і годинник-заставка) прибрано: він малював поверх
    нового екрана навіть тоді, коли його не показували.  */
class Display {
  public:
    uint16_t currentPlItem = 1;      /* станція, вибрана кнопками чи енкодером (controls.cpp) */
    uint16_t numOfNextStation = 0;   /* набір номера станції цифрами */
    displayMode_e _mode = PLAYER;
  public:
    Display() {}
    displayMode_e mode() { return _mode; }
    void mode(displayMode_e m) { _mode = m; }
    void init();
    void loop();
    void _start();
    void splashDemo(uint32_t ms){ _splashDemoMs = ms ? ms : 1; }   /* заставка поверх плеєра (кнопка в меню, консоль) */
    bool ready() { return _bootStep == 2; }
    void resetQueue();
    void putRequest(displayRequestType_e type, int payload = 0);
    void flip();
    void invert();
    void setContrast() {}
    void forceRedraw();
    void fadeLoop();                 /* плавна зміна екранів, із задачі дисплея */
    bool fading() const { return _fadeStep >= 0; }
    void openStationsNow();          /* список станцій (сторінка меню) */
    void forceLogo();                /* перечитати логотип станції */
    void requestRedraw() { _redrawReq = true; }        /* з іншої задачі: перемалювати плеєр (веб змінив налаштування) */
    bool deepsleep();
    void wakeup();
    void lock()   { _locked = true; }
    void unlock() { _locked = false; }
    uint16_t width();
    uint16_t height();
  private:
    int8_t   _fadeStep = -1;         /* крок плавної зміни екрана */
    uint32_t _fadeTick = 0;
    displayMode_e _fadeTo = PLAYER;
    volatile bool _redrawReq = false;
    bool     _locked = false;
    uint8_t  _bootStep = 0;
    bool     _startWait = false;        /* чекаємо, поки заставка доказує вступ */
    bool     _lostPending = false;   /* зв'язок зник ще на заставці — показати, щойно плеєр готовий */
    bool     _playerBuilt = false;   /* старт завершено (зі мережею) */
    volatile uint32_t _splashDemoMs = 0;
    uint32_t _splashDemoUntil = 0;
    void _beginFade(displayMode_e to);
    bool _ensurePlayer();            /* стартували без мережі, а вона з'явилась — показати плеєр */
    void _finishStart(bool draw);
    void _noNetScreen();             /* мережі немає: одразу список мереж */
    void _swichMode(displayMode_e newmode);
    void _createDspTask();
    void _bootScreen();
};

extern Display display;

#endif
