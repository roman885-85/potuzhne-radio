/*  ---------------------------------------------------------------------------
 *  Входи в меню й робота з мережами.
 *
 *  Саме меню — нове (src/m2). Старе меню yoRadio (перенесення інтерфейсу
 *  Nextion) прибране разом зі старим виглядом плеєра. Звідси нове меню бере
 *  перевірену на живих мережах логіку Wi-Fi: пошук, збережений список,
 *  вибір і підключення (m2/m2menu.cpp, struct m2::WB), а решта радіо — входи:
 *  відкрити меню, мережі, обране, закрити й передати дотик.
 *  ------------------------------------------------------------------------- */
#ifndef yoMenu_h
#define yoMenu_h

#include "../core/options.h"

#if (TS_MODEL!=TS_MODEL_UNDEFINED) && (DSP_MODEL!=DSP_DUMMY)
  #define USE_YOMENU
#endif

#ifdef USE_YOMENU
#include <Arduino.h>

namespace m2 { struct WB; int8_t bridgeRssi(); }

#define YOM_SSIDS     5
#define YOM_SSID_LEN  30
#define YOM_PASS_LEN  40

class YoMenu {
    friend struct m2::WB;
    friend int8_t m2::bridgeRssi();
  public:
    bool active() const;                        /* відкрите меню */
    bool fading() const;                        /* триває плавна зміна */
    void wifiTick();                            /* з головного циклу: дії меню, пошук мереж */
    bool scanning() const { return _scanning; }
    uint8_t scanCount() const { return _scanN; }
    void open();                                /* головне меню (пульт) */
    void openHome() { open(); }
    void openWifi(bool lock = true);            /* одразу мережі; lock — без виходу (радіо не в мережі) */
    void openFav();                             /* обране */
    void close();
    void render();                              /* із задачі дисплея */
    void onPress(uint16_t x, uint16_t y);
    void onDrag(uint16_t x, uint16_t y);
    void onRelease(uint16_t x, uint16_t y, uint32_t held = 0);

  private:
    bool     _apLock = false;
    struct WScan { char ssid[33]; int8_t rssi; uint8_t enc; };
    static const uint8_t WS_MAX = 16;
    WScan    _scan[WS_MAX];
    uint8_t  _scanN = 0;
    volatile bool _scanning = false;
    volatile bool _scanReq = false;             /* сторінка попросила пошук; робить головний цикл */
    uint32_t _scanT0 = 0, _scanAgain = 0;       /* пошук не почався — за мить спробуємо ще */
    uint8_t  _scanFails = 0;
    char     _wSsid[YOM_SSID_LEN] = {0}, _wPass[YOM_PASS_LEN] = {0};
    char     _curSsid[33] = {0};
    bool     _staUp = false;
    int8_t   _rssi = 0;                         /* сигнал і адреса — з головного циклу */
    char     _ipStr[16] = {0};
    bool     _m2Wifi = false;                   /* меню показує сторінку мереж — пошук потрібен */
    char     _ssid[YOM_SSIDS][YOM_SSID_LEN];
    char     _pass[YOM_SSIDS][YOM_PASS_LEN];
    void _wifiScan();
    void _wifiPoll();
    void _loadWifi();
    void _savedWrite();
    void _wifiSaveCurrent();
};

extern YoMenu yomenu;
#endif
#endif
