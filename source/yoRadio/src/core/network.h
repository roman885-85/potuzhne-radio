#ifndef network_h
#define network_h
#include <WiFi.h>

enum n_Status_e { CONNECTED, SOFT_AP, FAILED, SDREADY };
/*  Спроба підключитись на прохання людини: без перезавантаження, з
    негайною відповіддю — не вийшло через пароль чи мережі просто немає.  */
enum n_Try_e { TRY_NONE, TRY_RUN, TRY_OK, TRY_BADPASS, TRY_NOTFOUND, TRY_FAIL };

class MyNetwork {
  public:
    n_Status_e status;
    struct tm timeinfo;
    bool lostPlaying = false, beginReconnect = false;
    /*  Мережа зникла: повертаємось у неї самі — по одній спробі за раз,
        не блокуючи ні звук, ні екран, ні пошук мереж у меню.  */
    bool linkLost = false;
    bool staPaused = false;          /* спроби спинено: шукаємо мережі */
  public:
    MyNetwork() {};
    void begin();
    void loop();                     /* з головного циклу: повернення в мережу */
    void pauseSta(bool on);          /* на час пошуку мереж звільняємо радіомодуль */
    uint8_t lostTries() const { return _reTry; }
  public:
    void requestTimeSync(bool withTelnetOutput=false, uint8_t clientId=0);
    void requestWeatherSync();
    void setWifiParams();
    bool wifiBegin(bool silent=false);
    bool wifiRemembered();           /* мережа, яку пам'ятає сам драйвер Wi-Fi */
    void connectTo(const char* ssid, const char* pass);   /* спроба просто зараз */
    n_Try_e tryState() const { return _try; }
    const char* trySsid() const { return _tryS; }
    void tryClear(){ _try = TRY_NONE; }
  private:
    uint32_t _reAt = 0;              /* час наступної спроби */
    uint32_t _pauseAt = 0;           /* коли спинили спроби */
    uint8_t  _reTry = 0;             /* скільки спроб поспіль не вдалося */
    uint8_t  _reNext = 0;            /* яку зі збережених мереж пробувати далі */
    int8_t   _reCur = -1;            /* яку пробуємо зараз — щоб запам'ятати вдалу */
    volatile n_Try_e _try = TRY_NONE;
    char     _tryS[33] = {0}, _tryP[65] = {0};
    uint32_t _tryAt = 0;
    bool     _staReady = false;      /* setWifiParams уже зроблено */
    bool     _apWas = false;         /* на час спроби точку доступу прибрали */
    void     _staUp();               /* мережа з'явилась на ходу: підняти служби */
    void raiseSoftAP();
    static void WiFiLostConnection(WiFiEvent_t event, WiFiEventInfo_t info);
    static void WiFiReconnected(WiFiEvent_t event, WiFiEventInfo_t info);
};

extern MyNetwork network;

extern __attribute__((weak)) void network_on_connect();

#endif
