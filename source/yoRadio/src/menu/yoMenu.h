/*  ---------------------------------------------------------------------------
 *  Меню налаштувань — перенесення інтерфейсу Nextion (NX4024K032.HMI) на
 *  графічний екран, на штатних сторінках і віджетах yoRadio.
 *
 *  Структура з оригіналу: ліва колонка з п'яти іконок перемикає розділи в один
 *  дотик, жовта шапка містить шестерню, назву сторінки та стрілку повернення.
 *  Вміст сторінок той самий, що надсилає nextion.cpp.
 *
 *  Перемальовується лише те, що змінилось: сторінка цілком — тільки при
 *  переході між розділами, далі кожен віджет оновлює себе сам і мовчить,
 *  якщо значення не змінилось.
 *
 *  Свідомі відступи від оригіналу:
 *   - яскравість: у Nextion це функція самої панелі (dim), обробника в ESP
 *     немає, тут вона керує BRIGHTNESS_PIN через config.store.brightness;
 *   - перемикач «радіо / картка»: у меню Nextion відсутній, доданий рядком
 *     на сторінці «Система» у вигляді тамтешніх прапорців.
 *  ------------------------------------------------------------------------- */
#ifndef yoMenu_h
#define yoMenu_h

#include "menuwidgets.h"

#ifdef USE_YOMENU
#include <Arduino.h>

class Page;

#define YOM_SSIDS     5
#define YOM_SSID_LEN  30
#define YOM_PASS_LEN  40

class YoMenu {
  public:
    bool active() const { return _cur != PG_OFF; }
    int8_t page() const { return _cur; }        /* для службової консолі */
    const char* rowName(int idx);               /* рядок списку — для plGenericDraw */
    void wifiTick();                            /* з головного циклу: пошук мереж */
    bool scanning() const { return _scanning; }
    uint8_t scanCount() const { return _scanN; }
    bool fading() const { return _fadeStep >= 0; }   /* триває плавна зміна */
    void open();                 /* шестерня у шапці плеєра */
    void openWifi(bool lock = true);   /* одразу Wi-Fi; lock — без виходу (точка доступу) */
    void openFav();              /* обране */
    void openHome();             /* кнопка «☰» у шапці плеєра: головне меню */
    void openPage(int8_t p);     /* службова консоль: будь-яка сторінка */
    void openKbdTest(){ if(_cur == PG_OFF){ _build(); _apLock = false; } _kbTest[0] = 0; _openKbd(_kbTest, sizeof(_kbTest), true, "перевірка"); }
    const char* kbdTest() const { return _kbTest; }
    void close();
    void render();               /* із задачі дисплея */
    void onRelease(uint16_t x, uint16_t y, uint32_t held = 0);
    void onPress(uint16_t x, uint16_t y);      /* палець торкнувся */
    void onDrag(uint16_t x, uint16_t y);       /* палець ведуть */
    void tick();                 /* раз на секунду, як printClock() у Nextion */

  private:
    /*  Сторінки 5 і 6 — мої доповнення, у Nextion їх немає.  */
    enum page_e { PG_OFF=-1, PG_INFO=0, PG_EQ=1, PG_WIFI=2, PG_TIME=3, PG_SYS=4,
                  PG_SLEEP=5, PG_NIGHT=6, PG_KBD=7, PG_FAV=8, PG_SERM=9, PG_HOME=10, PG_SETUP=11,
                  PG_DEV=12, PG_DAC=13, PG_DACINFO=14, PG_POWER=15, PG_WSAVED=16, PG_WPICK=17, PG_WCONN=18,
                  PG_SND=19, PG_ROOM=20, PG_MIC=21, PG_MGEST=22, PG_MPRES=23, PG_N };   /* PG_N — завжди останній: розмір масиву сторінок */
    static const uint8_t NSIDE = 7;   /* значків у лівій колонці */

    int8_t _cur = PG_OFF;
    bool   _built = false;
    bool   _apLock = false;
    bool   _live = false;
    uint32_t _held = 0;               /* тривалість останнього дотику */
    uint32_t _lastAct = 0;            /* останній дотик у меню — для автоповернення */

    Page*     _pg[PG_N] = {nullptr};
    /*  «розробник»: що показувати й куди йде звук  */
    UiCheck   _chkBat, _chkIp, _chkSdEn;
    int8_t    _dacSel = 0;
    bool      _dacAlt = false;          /* VS1053: вид «де підпаятись» */
    void _drawDevBtn();
    void _drawDacList();
    void _drawDacInfo();
    int8_t    _parent(int8_t p) const;   /* куди веде стрілка «назад» */
    void _drawSetup();
    void _drawPower();                   /* живлення: перезавантажити / вимкнути */
    volatile int8_t _pwrArm = -1, _pwrGo = -1;   /* перше торкання «зводить», друге — виконує */
    uint32_t _pwrArmT = 0;
    /*  список проповідей — у вигляді списку станцій  */
    int16_t  _smSel = 1, _smShift = 0;
    uint16_t _smMqW = 0;
    uint32_t _smMqT = 0;
    int8_t   _smBtn = -1;
    uint32_t _smBtnT = 0;
    void _smDraw(bool bandOnly);
    float    _smCur = 1.0f, _smFrom = 1.0f;   /* де список зараз — для пружини */
    uint32_t _smT0 = 0;
    bool     _smAnim = false;
    void _smGo(int16_t sel);
    /*  прокрутка пальцем, накат і повтор кнопок — як у списку станцій  */
    volatile bool _dActive = false, _dMoved = false, _smFling = false, _smDragDirty = false;
    int16_t  _dY0 = 0, _dLastY = 0;
    float    _dPos0 = 1.0f, _dVel = 0.0f;
    uint32_t _dLastT = 0, _smFlingT = 0;
    volatile int8_t _smHold = -1, _smBtnDraw = -1;
    uint32_t _smHoldT = 0, _smRepT = 0;
    bool     _smRep = false;
    void _smStep(int8_t d);
    void _smMarquee();
    uint8_t   _afterClose = 0;         /* 1 — після закриття меню відкрити список станцій */
    void _drawHome();
    void _infoLive();                  /* живі рядки сторінки «інформація» */
    void _drawHomeTile(uint8_t i);
    /*  обране і проповіді малюються без віджетів: плитки міняються цілком  */
    volatile bool _favDirty = false, _smDirty = false, _msgDirty = false;
    void _drawHdrMsg();
    int16_t  _titleEnd = 104;         /* де закінчується назва в шапці */
    uint16_t _smTop = 0;
    uint32_t _smVer = 0xFFFFFFFF;
    bool     _smLoad = false;
    char     _msg[48] = {0};          /* коротке пояснення в шапці обраного */
    uint32_t _msgUntil = 0;
    void _drawFav();
    void _drawTile(uint8_t i);
    void _drawBottom();
    void _drawSerm();
    void _setMsg(const char* m);
    /*  сон і будильник  */
    UiSeg     _slSeg, _alDays;
    UiText    _slStat, _alStat, _alH, _alM, _alSta;
    UiCheck   _chkAlarm;
    /*  ніч і світло  */
    UiCheck   _chkNight;
    UiText    _nFrom, _nTo, _batTxt;
    UiSlider  _nLevel;
    UiSeg     _ledSeg, _saveSeg;
    void _syncExtras();               /* значення сторінок 5 і 6 у віджети */
    /*  Wi-Fi: список знайдених мереж замість ручного вводу назви  */
    struct WScan { char ssid[33]; int8_t rssi; uint8_t enc; };
    static const uint8_t WS_MAX = 16;
    WScan    _scan[WS_MAX];
    uint8_t  _scanN = 0;
    volatile bool _scanning = false, _wlDirty = false;
    char     _wSsid[YOM_SSID_LEN] = {0}, _wPass[YOM_PASS_LEN] = {0};
    char     _kbdTitleBuf[48] = {0};
    uint8_t  _kbdNext = 0;            /* 1 — після назви пароль, 2 — після пароля підключення */
    uint32_t _scanT0 = 0, _scanAgain = 0;   /* пошук не почався — за мить спробуємо ще */
    bool     _dCaught = false;        /* палець зупинив список, що ще їхав */
    volatile bool _scanReq = false;         /* сторінка попросила пошук; робить головний цикл */
    /*  Стан мережі для малювання: питати радіомодуль із задачі дисплея не
        можна — поки він перебирає канали, кожне питання коштує мілісекунд,
        а таких питань на кожен кадр десяток.  */
    char     _curSsid[33] = {0};
    bool     _staUp = false;
    uint8_t  _scanFails = 0;
    /*  Списки проповідей і мереж — однакові: повноекранний список станцій
        з «лупою», кнопками ▲▼▶↶, прокруткою пальцем і накатом.  */
    bool _isList(int8_t p) const { return p == PG_SERM || p == PG_WIFI; }
    int  _listCount() const;
    int  _listPlay() const;                     /* рядок «зараз»: проповідь чи мережа */
    void _listPick(int idx);
    const char* _sermRowName(int idx);
    const char* _wifiRowName(int idx);
    static const uint8_t WIFI_ACTS = 3;         /* «шукати ще раз», «вручну», «відомі» */
    void _wifiScan();
    void _wifiPoll();
    void _wifiBars(float pos, bool bandOnly);   /* смуги сигналу поверх рядків */
    void _wifiPick(uint8_t i);
    void _wifiConnect();
    /*  відомі мережі: забути або підняти першою  */
    int8_t   _wsArm = -1;             /* рядок, де спитали «забути?» */
    uint32_t _wsArmT = 0;
    void _drawSaved();
    void _drawWpick();                /* що зробити з уже знайомою мережею */
    void _drawWconn();                /* хід підключення та його результат */
    void _wifiSaveCurrent();          /* мережа, до якої підключились, — перша в списку */
    int8_t _wcShown = -1;             /* що вже намальовано на сторінці підключення */
    uint32_t _wcOkAt = 0;             /* коли вийшло — щоб піти на плеєр */
    int8_t   _wpArm = -1;             /* «забути?» на сторінці мережі */
    uint32_t _wpArmT = 0;
    void _savedWrite();               /* _ssid/_pass → файл, без перезавантаження */
    void _stepper(int16_t x, int16_t y, int16_t w, int16_t h, bool plus);
    UiText    _info[8][2];
    UiText    _wifiS[YOM_SSIDS], _wifiP[YOM_SSIDS];
    UiText    _tmH, _tmM, _tmNow;
    UiCheck   _chkStart, _chkInfo, _chkSrc;
    UiSlider  _bright;
    UiText    _kbdField;

    char   _ssid[YOM_SSIDS][YOM_SSID_LEN];
    char   _pass[YOM_SSIDS][YOM_PASS_LEN];
    char*  _kbdTarget = nullptr;
    char   _kbdUndo[YOM_PASS_LEN] = {0};  /* що було до правки — для відміни */
    size_t _kbdMax = 0;
    uint8_t _kbdPage = 0;
    bool   _kbdIsPass = false;
    bool   _kbdShow = true;           /* пароль видно, поки не сховали оком */
    /*  Клавіатура: дотик лише запам'ятовує, яка клавіша під пальцем, а малює
        все задача дисплея. Раніше спалах клавіші й рядок вводу малювались
        просто з потоку дотику, поки паралельно малювала задача дисплея, —
        шрифт у дисплея спільний, і текст то зникав, то міняв розмір.  */
    char     _kbTest[YOM_PASS_LEN] = {0};   /* поле для перевірки клавіатури з консолі */
    volatile int8_t  _kbKey = -1;     /* клавіша під пальцем */
    volatile bool    _kbDown = false;
    volatile uint8_t _kbDirty = 0;    /* 1 рядок, 2 клавіші, 4 око, 8 підсвітка */
    int8_t   _kbShown = -1;           /* що зараз підсвічено на екрані */
    int16_t  _kbPopX = -1, _kbPopY = 0;   /* збільшена клавіша над пальцем */
    uint32_t _kbDownT = 0, _kbRepT = 0;
    bool     _kbRep = false;
    int8_t   _kbKeyAt(int16_t x, int16_t y) const;
    bool     _kbKeyRect(int8_t k, int16_t& x, int16_t& y, int16_t& w, int16_t& h) const;
    void     _kbDrawKey(int8_t k, bool hl);
    void     _kbRestore(int16_t x, int16_t y, int16_t w, int16_t h);
    void     _kbAction(int8_t k);
    void     _kbRender();
    void   _kbdRefresh();             /* рядок вводу: зірочки чи самі знаки (лише з задачі дисплея) */
    void   _drawKbdEye();
    int8_t _kbdBack = PG_WIFI;
    const char* _kbdTitle = "";

    void _build();
    void _show(int8_t p);
    void _paint();               /* сама сторінка — із задачі дисплея */
    void _fade();                /* наплив тла перед нею */
    int8_t   _fadeStep = -1;
    bool     _closeReq = false;
    uint32_t _fadeTick = 0;
    void _chrome(const char* title, uint8_t icon = 0);   /* 0 шестерня, 1 зірка, 2 «☰» */
    void _sidebar();
    void _drawKbdKeys();
    void _openKbd(char* target, size_t max, bool isPass, const char* title);
    void _loadWifi();
    void _showWifi();            /* оновити написи полів без перечитування */
    void _saveWifi();
    void _syncSys();
    void _hit(uint16_t x, uint16_t y);

    /*  ---------- звук і мікрофон (yoDsp, yoMic) ---------- */
    bool _isSound(int8_t p) const { return p == PG_EQ || p == PG_SND || p == PG_ROOM || p == PG_MIC || p == PG_MGEST || p == PG_MPRES; }
    volatile uint16_t _sndMask = 0;   /* що перемалювати: біти 0..9 — смуги, 10 — верх, 11 — низ, 12 — рядки жестів */
    volatile int8_t _eqBand = -1;     /* смуга, яку тягне палець */
    bool     _eqPend = false;
    uint32_t _eqApplyT = 0;
    void _paintSound(int8_t p);
    void _renderSound();
    bool _hitSound(uint16_t x, uint16_t y);
    void _eqTouch(uint16_t y);
    void _eqApply(bool force);
    void _drawEqTop();
    void _drawEqBand(uint8_t b);
    void _drawEqBottom();
    UiSeg    _sGuard, _sVb, _sLoud;
    UiSlider _sBal;
    void _drawRoom(bool full);
    uint8_t  _rmShown = 255, _rmProg = 255;
    char     _rmErr[48] = {0};
    UiCheck  _mOn, _mPlay;
    UiSeg    _mGain;
    uint32_t _micLiveT = 0;
    void _drawMicLive();
    UiSeg    _gTab, _gSens;
    UiCheck  _gOn;
    uint8_t  _gKind = 0;              /* 0 хлопки, 1 стук */
    void _drawGestRows();
    void _syncGest();
    UiCheck  _pEar, _pWake;
    UiSeg    _pEarMin, _pOff;
    void _syncPres();
    void _buildSound();
};

extern YoMenu yomenu;
#endif
#endif
