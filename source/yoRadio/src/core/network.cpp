#include "options.h"
#include <ESPmDNS.h>
#include "time.h"
#include "rtcsupport.h"
#include "network.h"
#include <esp_wifi.h>
#include "display.h"
#include "config.h"
#include "telnet.h"
#include "netserver.h"
#include "player.h"
#include "../extras/yoVersion.h"
#include "mqtt.h"
#include "timekeeper.h"
#include "../pluginsManager/pluginsManager.h"

#ifndef WIFI_ATTEMPTS
  #define WIFI_ATTEMPTS  16
#endif

#ifndef SEARCH_WIFI_CORE_ID
  #define SEARCH_WIFI_CORE_ID  0
#endif
MyNetwork network;

void MyNetwork::WiFiReconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  network.beginReconnect = false;
  network.linkLost = false;
  network._reTry = 0;
  network._reAt = 0;
  /*  Підключились не до тієї, що була: запам'ятовуємо, щоб наступного разу
      радіо починало саме з неї.  */
  if(network._reCur >= 0){
    if(config.store.lastSSID != network._reCur + 1) config.setLastSSID(network._reCur + 1);
    network._reCur = -1;
  }
  player.lockOutput = false;
  delay(100);
  display.putRequest(NEWMODE, PLAYER);
  if(config.getMode()==PM_SDCARD) {
    network.status=CONNECTED;
    display.putRequest(NEWIP, 0);
  }else{
    display.putRequest(NEWMODE, PLAYER);
    display.putRequest(NEWIP, 0);        /* повертаємо адресу замість підказки */
    if (network.lostPlaying) player.sendCommand({PR_PLAY, config.lastStation()});
  }
  #ifdef MQTT_ROOT_TOPIC
    connectToMqtt();
  #endif
}

void MyNetwork::WiFiLostConnection(WiFiEvent_t event, WiFiEventInfo_t info){
  if(!network.beginReconnect){
    Serial.printf("Lost connection, reconnecting to %s...\n", config.ssids[config.store.lastSSID-1].ssid);
    if(config.getMode()==PM_SDCARD) {
      network.status=SDREADY;
      display.putRequest(NEWIP, 0);
    }else{
      network.lostPlaying = player.isRunning();
      if (network.lostPlaying) { player.lockOutput = true; player.sendCommand({PR_STOP, 0}); }
      display.putRequest(NEWMODE, LOST);
    }
  }
  network.beginReconnect = true;
  /*  Тут раніше стояв WiFi.reconnect(). Кожна невдала спроба знову кидала
      цю саму подію — виходило нескінченне коло без жодної паузи: ядро теж
      саме перепідключалось, радіомодуль не встигав нічого, пошук мереж не
      запускався зовсім, а меню й звук стояли. Тепер лише позначаємо втрату,
      а спроби робить loop() — по одній, з паузами.  */
  network.linkLost = true;
}

bool MyNetwork::wifiBegin(bool silent){
  uint8_t ls = (config.store.lastSSID == 0 || config.store.lastSSID > config.ssidsCount) ? 0 : config.store.lastSSID - 1;
  uint8_t startedls = ls;
  uint8_t errcnt = 0;
  WiFi.mode(WIFI_STA);
  /*
  char buf[MDNS_LENGTH];
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  if(strlen(config.store.mdnsname)>0){
    WiFi.setHostname(config.store.mdnsname);
  }else{
    snprintf(buf, MDNS_LENGTH, "yoradio-%x", config.getChipId());
    WiFi.setHostname(buf);
  }
  */
  while (true) {
    if(!silent){
      Serial.printf("##[BOOT]#\tAttempt to connect to %s\n", config.ssids[ls].ssid);
      Serial.print("##[BOOT]#\t");
      display.putRequest(BOOTSTRING, ls);
    }
    WiFi.disconnect(true, true); //disconnect & erase internal credentials https://github.com/e2002/yoradio/pull/164/commits/89d8b4450dde99cd7930b84bb14d81dab920b879
    delay(100);
    WiFi.begin(config.ssids[ls].ssid, config.ssids[ls].password);
    while (WiFi.status() != WL_CONNECTED) {
      if(!silent) Serial.print(".");
      delay(500);
      if(REAL_LEDBUILTIN!=255 && !silent) digitalWrite(REAL_LEDBUILTIN, !digitalRead(REAL_LEDBUILTIN));
      errcnt++;
      /*  Мережі просто немає в ефірі або пароль не той — чекати решту восьми
          секунд нема чого, пробуємо наступну збережену.  */
      if(errcnt > 6 && (WiFi.status() == WL_NO_SSID_AVAIL || WiFi.status() == WL_CONNECT_FAILED)) errcnt = WIFI_ATTEMPTS + 1;
      if (errcnt > WIFI_ATTEMPTS) {
        errcnt = 0;
        ls++;
        if (ls > config.ssidsCount - 1) ls = 0;
        if(!silent) Serial.println();
        break;
      }
    }
    if (WiFi.status() != WL_CONNECTED && ls == startedls) {
      return false; break;
    }
    if (WiFi.status() == WL_CONNECTED) {
      config.setLastSSID(ls + 1);
      return true; break;
    }
  }
  return false;
}

/*  Остання надія, коли список мереж зник: сам драйвер Wi-Fi тримає назву й
    пароль останньої мережі в NVS. Підключаємось за ними й одразу кладемо
    мережу назад у список — інакше радіо пішло б у точку доступу, звідки без
    пароля не вибратись.  */
bool MyNetwork::wifiRemembered(){
  if(Config::wifiWiped()) return false;      /* мережі прибрали свідомо */
  WiFi.mode(WIFI_STA);
  wifi_config_t c;
  if(esp_wifi_get_config(WIFI_IF_STA, &c) != ESP_OK || !c.sta.ssid[0]) return false;
  BOOTLOG("wifi list is empty, trying remembered %s", (const char*)c.sta.ssid);
  WiFi.begin();
  for(uint8_t i = 0; i < 24 && WiFi.status() != WL_CONNECTED; i++) delay(500);
  if(WiFi.status() != WL_CONNECTED) return false;
  char line[110];
  snprintf(line, sizeof(line), "%s\t%s\n", (const char*)c.sta.ssid, (const char*)c.sta.password);
  config.saveWifiList(line);
  config.setLastSSID(1);
  BOOTLOG("wifi list restored from radio module: %s", (const char*)c.sta.ssid);
  return true;
}

void searchWiFi(void * pvParameters){
  if(!network.wifiBegin(true)){
    delay(10000);
    xTaskCreatePinnedToCore(searchWiFi, "searchWiFi", 1024 * 4, NULL, 0, NULL, SEARCH_WIFI_CORE_ID);
  }else{
    network.status = CONNECTED;
    netserver.begin(true);
    telnet.begin(true);
    network.setWifiParams();
    display.putRequest(NEWIP, 0);
  }
  vTaskDelete( NULL );
}

#define DBGAP false

void MyNetwork::begin() {
  BOOTLOG("network.begin");
  config.initNetwork();
  if (config.ssidsCount == 0 && !DBGAP && wifiRemembered()) {
    /*  Список пропав, але драйвер Wi-Fi пам'ятає останню мережу сам —
        радіо лишається в мережі, а список ми тут-таки відновлюємо.  */
    status = CONNECTED;
    setWifiParams();
    Serial.println("##[BOOT]#\tdone");
    if (network_on_connect) network_on_connect();
    pm.on_connect();
    return;
  }
  if (config.ssidsCount == 0 || DBGAP) {
    raiseSoftAP();
    return;
  }
  if(config.getMode()!=PM_SDCARD){
    if(!wifiBegin()){
      raiseSoftAP();
      Serial.println("##[BOOT]#\tdone");
      return;
    }
    Serial.println(".");
    status = CONNECTED;
    setWifiParams();
  }else{
    status = SDREADY;
    xTaskCreatePinnedToCore(searchWiFi, "searchWiFi", 1024 * 4, NULL, 0, NULL, SEARCH_WIFI_CORE_ID);
  }
  
  Serial.println("##[BOOT]#\tdone");
  if(REAL_LEDBUILTIN!=255) digitalWrite(REAL_LEDBUILTIN, LOW);
  
#if RTCSUPPORTED
  if(config.isRTCFound()){
    rtc.getTime(&network.timeinfo);
    mktime(&network.timeinfo);
    display.putRequest(CLOCK);
  }
#endif
  if (network_on_connect) network_on_connect();
  pm.on_connect();
}

void MyNetwork::setWifiParams(){
  WiFi.setSleep(false);
  /*  Перепідключенням керуємо самі (loop()): у ядра воно без пауз і без
      переходу на інші збережені мережі.  */
  WiFi.setAutoReconnect(false);
  WiFi.onEvent(WiFiReconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiLostConnection, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  //config.setTimeConf(); //??
  if(strlen(config.store.mdnsname)>0 && MDNS.begin(config.store.mdnsname)){
    /*  Оголошуємо радіо в мережі, щоб програми-клієнти (Mac, Windows,
        Android) знаходили його самі, без введення адреси. Свій тип
        «_potuzhne._tcp» — лише наші радіо; «_http._tcp» — для браузерів.  */
    MDNS.setInstanceName("ПОТУЖНЕ РАДІО");
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("potuzhne", "tcp", 80);
    MDNS.addServiceTxt("potuzhne", "tcp", "board", "ES3C28P");
    MDNS.addServiceTxt("potuzhne", "tcp", "ver", prVersion());
    MDNS.addServiceTxt("potuzhne", "tcp", "host", (const char*)config.store.mdnsname);
  }
}

void MyNetwork::requestTimeSync(bool withTelnetOutput, uint8_t clientId) {
  if (withTelnetOutput) {
    char timeStringBuff[50];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    if (config.store.tzHour < 0) {
      telnet.printf(clientId, "##SYS.DATE#: %s%03d:%02d\n> ", timeStringBuff, config.store.tzHour, config.store.tzMin);
    } else {
      telnet.printf(clientId, "##SYS.DATE#: %s+%02d:%02d\n> ", timeStringBuff, config.store.tzHour, config.store.tzMin);
    }
  }
}

void rebootTime() {
  ESP.restart();
}

void MyNetwork::raiseSoftAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid, apPassword);
  Serial.println("##[BOOT]#");
  BOOTLOG("************************************************");
  BOOTLOG("Running in AP mode");
  BOOTLOG("Connect to AP %s with password %s", apSsid, apPassword);
  BOOTLOG("and go to http:/192.168.4.1/ to configure");
  BOOTLOG("************************************************");
  status = SOFT_AP;
  if(config.store.softapdelay>0)
    timekeeper.waitAndDo(config.store.softapdelay*60, rebootTime);
}

/*  Повернення в мережу: одна спроба за раз, пауза між ними росте, а
    збережені мережі перебираємо по черзі. Якщо тієї, що була, більше немає —
    радіо саме перейде на іншу знайому, і все це не чіпає ні звук, ні дотики.  */
void MyNetwork::loop(){
  if(status == SOFT_AP || !linkLost) return;
  if(WiFi.status() == WL_CONNECTED) return;            /* подія про адресу ось-ось прийде */
  if(staPaused){
    /*  пауза на час пошуку мереж не може тривати вічно  */
    if(millis() - _pauseAt > 180000UL) pauseSta(false);
    return;
  }
  if((int32_t)(millis() - _reAt) < 0) return;
  /*  поки триває пошук мереж, підключатися нема чим: модуль один  */
  if(WiFi.scanComplete() == WIFI_SCAN_RUNNING){ _reAt = millis() + 2000; return; }
  if(config.ssidsCount == 0){ linkLost = false; return; }
  if(_reNext >= config.ssidsCount) _reNext = 0;
  _reCur = _reNext++;
  WiFi.disconnect(false, false);
  WiFi.begin(config.ssids[_reCur].ssid, config.ssids[_reCur].password);
  if(_reTry < 200) _reTry++;
  /*  перше коло по всіх збережених — швидко, далі рідше: і батарея, і ефір  */
  uint32_t pause = _reTry <= config.ssidsCount ? 9000 : (_reTry < 12 ? 20000 : 45000);
  _reAt = millis() + pause;
}

/*  Пошук мереж і спроби підключитися живуть в одному радіомодулі: поки
    йдуть спроби, скан не стартує взагалі. Тож на час пошуку спроби спиняємо.  */
void MyNetwork::pauseSta(bool on){
  if(staPaused == on) return;
  staPaused = on;
  _pauseAt = millis();
  if(on){
    /*  Обірвати треба саме незавершену спробу: доки вона триває, пошук мереж
        не запускається. WiFi.disconnect() тут не годиться — поки з'єднання ще
        не встановлене, він мовчки не робить нічого.  */
    if(WiFi.status() != WL_CONNECTED) esp_wifi_disconnect();
  }else{
    _reTry = 0;
    _reAt = millis() + 800;
  }
}

void MyNetwork::requestWeatherSync(){
  display.putRequest(NEWWEATHER);
}
