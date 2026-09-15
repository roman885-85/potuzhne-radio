#include "yoMenu.h"
#ifdef USE_YOMENU

#include <WiFi.h>
#include "../core/config.h"
#include "../core/network.h"
#include "../m2/m2pages.h"

YoMenu yomenu;

/*  ---------- входи ---------- */

void YoMenu::open(){
  if(m2::M.active()) return;
  _apLock = false; _loadWifi();
  m2::M.open(&m2::pgPult);
}

void YoMenu::openFav(){
  if(m2::M.active()) return;
  _apLock = false;
  m2::M.open(&m2::pgFav);
}

/*  Без мережі одразу Wi-Fi і без виходу далі. Коли зв'язок просто зник
    (мережа ще збережена), виходу не замикаємо — мережа може повернутись сама.  */
void YoMenu::openWifi(bool lock){
  if(m2::M.active()) return;
  _apLock = lock; _loadWifi();
  m2::M.open(&m2::pgWifi);
}

void YoMenu::close(){ if(m2::M.active() && !_apLock) m2::M.close(); }
bool YoMenu::active() const { return m2::M.active(); }
bool YoMenu::fading() const { return m2::M.fading(); }
void YoMenu::render(){ if(m2::M.active() || m2::M.fading()) m2::M.render(); }
void YoMenu::onPress(uint16_t x, uint16_t y){ if(m2::M.active()) m2::M.onPress(x, y); }
void YoMenu::onDrag(uint16_t x, uint16_t y){ if(m2::M.active()) m2::M.onDrag(x, y); }
void YoMenu::onRelease(uint16_t x, uint16_t y, uint32_t held){ (void)held; if(m2::M.active()) m2::M.onRelease(x, y); }

/*  ---------- мережі ---------- */

void YoMenu::_wifiScan(){ _scanReq = true; }

void YoMenu::wifiTick(){
  m2::M.loop();                                  /* дії меню — тут, у головному циклі */
  if(!m2::M.active()) return;
  /*  Раз на 200 мс запам'ятовуємо, у якій ми мережі: далі малювання бере
      готове, а не смикає радіомодуль по десять разів на кадр.  */
  static uint32_t ask = 0;
  if(millis() - ask >= 200){
    ask = millis();
    bool up = (WiFi.status() == WL_CONNECTED);
    char now[33] = {0};
    if(up){
      strlcpy(now, WiFi.SSID().c_str(), sizeof(now));
      _rssi = (int8_t)WiFi.RSSI();
      strlcpy(_ipStr, WiFi.localIP().toString().c_str(), sizeof(_ipStr));
    }
    if(up != _staUp || strcmp(now, _curSsid)){ _staUp = up; strlcpy(_curSsid, now, sizeof(_curSsid)); }
  }
  if(!_m2Wifi && !_scanning && !_scanReq) return;
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
    WiFi.scanDelete(); _scanning = false; _scanN = 0; return;
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
}

void YoMenu::_loadWifi(){
  for(uint8_t i = 0; i < YOM_SSIDS; i++){
    _ssid[i][0] = '\0'; _pass[i][0] = '\0';
    if(i < config.ssidsCount){
      snprintf(_ssid[i], YOM_SSID_LEN, "%s", config.ssids[i].ssid);
      snprintf(_pass[i], YOM_PASS_LEN, "%s", config.ssids[i].password);
    }
  }
}

void YoMenu::_savedWrite(){
  String out;
  for(uint8_t i = 0; i < YOM_SSIDS; i++)
    if(_ssid[i][0]) out += String(_ssid[i]) + "\t" + String(_pass[i]) + "\n";
  config.saveWifiList(out.c_str());
  _loadWifi();
}

/*  Щойно підключились до нової мережі — вона перша у збереженому списку.  */
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

#endif  // USE_YOMENU
