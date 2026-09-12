#ifndef network_h
#define network_h
#include <WiFi.h>

enum n_Status_e { CONNECTED, SOFT_AP, FAILED, SDREADY };

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
  private:
    uint32_t _reAt = 0;              /* час наступної спроби */
    uint32_t _pauseAt = 0;           /* коли спинили спроби */
    uint8_t  _reTry = 0;             /* скільки спроб поспіль не вдалося */
    uint8_t  _reNext = 0;            /* яку зі збережених мереж пробувати далі */
    int8_t   _reCur = -1;            /* яку пробуємо зараз — щоб запам'ятати вдалу */
    void raiseSoftAP();
    static void WiFiLostConnection(WiFiEvent_t event, WiFiEventInfo_t info);
    static void WiFiReconnected(WiFiEvent_t event, WiFiEventInfo_t info);
};

extern MyNetwork network;

extern __attribute__((weak)) void network_on_connect();

#endif
