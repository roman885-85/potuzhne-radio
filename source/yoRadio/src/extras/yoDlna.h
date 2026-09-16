/*  ---------------------------------------------------------------------------
 *  Бездротова колонка: DLNA / UPnP MediaRenderer.
 *
 *  Телефон, планшет чи комп'ютер бачить радіо в мережі як колонку й надсилає
 *  йому посилання на доріжку (BubbleUPnP, Hi-Fi Cast, VLC, «Передати на
 *  пристрій» у Windows, foobar2000 з UPnP). Сам файл радіо забирає й грає тим
 *  самим програвачем, що й станції та проповіді — нічого нового в звук не
 *  додається.
 *
 *  Чому саме DLNA:
 *   - Google Cast приймачем стати не можна: пристрій має бути сертифікований
 *     Google і мати його ключі, відкритої реалізації приймача не існує;
 *   - Bluetooth-колонкою ця плата теж не може: в ESP32-S3 немає класичного
 *     Bluetooth (лише BLE), а A2DP живе саме в класичному;
 *   - DLNA — відкритий стандарт, і телефон надсилає лише посилання, тож звук
 *     іде повз радіо з роутера напряму, без перекодування.
 *
 *  Тут: SSDP (пошук у мережі), опис пристрою, керування AVTransport
 *  (що грати, грати/пауза/стоп) і RenderingControl (гучність) — усе на своєму
 *  порту 8080, окремою задачею, щоб не заважати сторінці радіо.
 *  ------------------------------------------------------------------------- */
#pragma once
#include <Arduino.h>
#include <WiFi.h>

class YoDlna {
  public:
    void begin();                       /* мережа піднялась */
    void stop();                        /* вимкнули в налаштуваннях */
    bool on() const;                    /* увімкнено в налаштуваннях */
    void setOn(bool v);
    bool playing() const { return _playing; }
    const char* title() const { return _title; }      /* що зараз надіслали */
    const char* device() const { return _device; }    /* хто надіслав (ім'я з опису) */
    void stopped();                     /* програвач зупинився — зняти позначку */

  private:
    friend void yoDlnaTask(void*);
    void _run();
    void _ssdp();
    void _http();
    void _notify(bool alive);
    void _search(const char* st, const char* addrHost, uint16_t port);
    void _client(WiFiClient& c);
    void _avt(WiFiClient& c, const char* action, const char* body);
    void _rc(WiFiClient& c, const char* action, const char* body);
    void _cm(WiFiClient& c, const char* action, const char* body);
    void _play();
    volatile bool _playing = false;
    uint32_t _startMs = 0;           /* коли почали: поки з'єднується, «грає» не знімаємо */
    char _uri[512] = {0};
    char _title[96] = {0};
    char _device[32] = {0};
    char _uuid[40] = {0};
    uint8_t _mute = 0;
    uint8_t _muteVol = 0;
    bool _started = false;
    void* _task = nullptr;
};

extern YoDlna dlna;
