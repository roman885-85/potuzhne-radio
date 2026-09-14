/*  Нове меню: зв'язок із рештою радіо.  */
#include "../core/options.h"
#include "../menu/yoMenu.h"
#ifdef USE_YOMENU
#include "m2pages.h"
#include "m2bridge.h"
#include <WiFi.h>
#include "../core/config.h"
#include "../core/network.h"
#include "../core/display.h"
#include "../extras/yoExtras.h"

namespace m2 {

/*  ---------- для шапки й затемнення ---------- */
int8_t bridgeRssi(){ return yomenu._staUp ? yomenu._rssi : 0; }

bool bridgeClock(char* out, uint8_t cap, uint8_t& minute){
  if(network.timeinfo.tm_year < 120) return false;           /* годинник ще не звірено */
  snprintf(out, cap, "%02d:%02d", network.timeinfo.tm_hour, network.timeinfo.tm_min);
  minute = (uint8_t)network.timeinfo.tm_min;
  return true;
}

void bridgeClosed(){
  display.forceRedraw();
  uint8_t a = afterClose;
  afterClose = 0;
  if(a == 1) display.openStationsNow();
  if(a == 2) display.splashDemo(7000);
}

void bridgeFade(uint16_t level){
#if BRIGHTNESS_PIN!=255
  if(level == 0xFFFF){ extras.pwmSet(extras.pwmTarget()); return; }
  if(config.store.dspon) analogWrite(BRIGHTNESS_PIN, level);
#else
  (void)level;
#endif
}

uint16_t bridgeBright(){
#if BRIGHTNESS_PIN!=255
  return extras.pwmTarget();
#else
  return 255;
#endif
}

/*  ---------- Wi-Fi: функції старого меню ---------- */
bool        WB::staUp(){ return yomenu._staUp; }
const char* WB::curSsid(){ return yomenu._curSsid; }
int8_t      WB::rssi(){ return yomenu._rssi; }
const char* WB::ip(){ return yomenu._ipStr; }
void        WB::scanStart(){ yomenu._scanFails = 0; yomenu._wifiScan(); }
bool        WB::scanning(){ return yomenu._scanning || yomenu._scanReq; }
uint8_t     WB::scanCount(){ return yomenu._scanN; }
const char* WB::scanSsid(uint8_t i){ return i < yomenu._scanN ? yomenu._scan[i].ssid : ""; }
int8_t      WB::scanRssi(uint8_t i){ return i < yomenu._scanN ? yomenu._scan[i].rssi : -100; }
bool        WB::scanOpen(uint8_t i){ return i < yomenu._scanN && yomenu._scan[i].enc == WIFI_AUTH_OPEN; }
bool        WB::isSaved(const char* ssid){ for(uint8_t k = 0; k < YOM_SSIDS; k++) if(yomenu._ssid[k][0] && !strcmp(yomenu._ssid[k], ssid)) return true; return false; }
uint8_t     WB::savedCount(){ uint8_t n = 0; for(uint8_t k = 0; k < YOM_SSIDS; k++) if(yomenu._ssid[k][0]) n++; return n; }
const char* WB::savedSsid(uint8_t i){ return i < YOM_SSIDS ? yomenu._ssid[i] : ""; }
char*       WB::ssidBuf(){ return yomenu._wSsid; }
char*       WB::passBuf(){ return yomenu._wPass; }
size_t      WB::ssidCap(){ return sizeof(yomenu._wSsid); }
size_t      WB::passCap(){ return sizeof(yomenu._wPass); }
void        WB::saveCurrent(){ yomenu._wifiSaveCurrent(); }
void        WB::load(){ yomenu._loadWifi(); }
bool        WB::apLock(){ return yomenu._apLock; }
void        WB::apUnlock(){ yomenu._apLock = false; }

void WB::savedForget(uint8_t i){
  if(i >= YOM_SSIDS) return;
  for(uint8_t k = i; k + 1 < YOM_SSIDS; k++){
    strlcpy(yomenu._ssid[k], yomenu._ssid[k + 1], YOM_SSID_LEN);
    strlcpy(yomenu._pass[k], yomenu._pass[k + 1], YOM_PASS_LEN);
  }
  yomenu._ssid[YOM_SSIDS - 1][0] = 0; yomenu._pass[YOM_SSIDS - 1][0] = 0;
  yomenu._savedWrite();
  yomenu._wPass[0] = 0; yomenu._wSsid[0] = 0;
}

void WB::savedFirst(uint8_t i){
  if(i == 0 || i >= YOM_SSIDS) return;
  char a[YOM_SSID_LEN], b[YOM_PASS_LEN];
  strlcpy(a, yomenu._ssid[i], sizeof(a)); strlcpy(b, yomenu._pass[i], sizeof(b));
  for(int8_t k = i; k > 0; k--){
    strlcpy(yomenu._ssid[k], yomenu._ssid[k - 1], YOM_SSID_LEN);
    strlcpy(yomenu._pass[k], yomenu._pass[k - 1], YOM_PASS_LEN);
  }
  strlcpy(yomenu._ssid[0], a, YOM_SSID_LEN); strlcpy(yomenu._pass[0], b, YOM_PASS_LEN);
  config.setLastSSID(1);
  yomenu._savedWrite();
}

void WB::forgetCurrent(){
  uint8_t k = 0;
  while(k < YOM_SSIDS && strcmp(yomenu._ssid[k], yomenu._wSsid)) k++;
  if(k < YOM_SSIDS){
    for(uint8_t j = k; j + 1 < YOM_SSIDS; j++){
      strlcpy(yomenu._ssid[j], yomenu._ssid[j + 1], YOM_SSID_LEN);
      strlcpy(yomenu._pass[j], yomenu._pass[j + 1], YOM_PASS_LEN);
    }
    yomenu._ssid[YOM_SSIDS - 1][0] = 0; yomenu._pass[YOM_SSIDS - 1][0] = 0;
    yomenu._savedWrite();
  }
  yomenu._wPass[0] = 0; yomenu._wSsid[0] = 0;
}

uint8_t WB::pick(uint8_t i){
  if(i >= yomenu._scanN) return 0;
  /*  та сама мережа, що й щойно, — лишаємо набраний пароль  */
  bool again = !strcmp(yomenu._wSsid, yomenu._scan[i].ssid) && yomenu._wPass[0];
  strlcpy(yomenu._wSsid, yomenu._scan[i].ssid, sizeof(yomenu._wSsid));
  bool known = false;
  for(uint8_t k = 0; k < config.ssidsCount && k < YOM_SSIDS; k++)
    if(!strcmp(config.ssids[k].ssid, yomenu._wSsid)) known = true;
  if(!again){
    yomenu._wPass[0] = 0;
    for(uint8_t k = 0; k < config.ssidsCount && k < YOM_SSIDS; k++)
      if(!strcmp(config.ssids[k].ssid, yomenu._wSsid)) strlcpy(yomenu._wPass, config.ssids[k].password, sizeof(yomenu._wPass));
  }
  if(known){ yomenu._loadWifi(); return 1; }
  if(yomenu._scan[i].enc == WIFI_AUTH_OPEN){ yomenu._wPass[0] = 0; return 2; }
  return 0;
}

void WB::connect(){
  if(!yomenu._wSsid[0]) return;
  yomenu._scanReq = false; yomenu._scanning = false;      /* пошук більше не потрібен — він лише заважає */
  network.tryClear();
  network.connectTo(yomenu._wSsid, yomenu._wPass);
}

void WB::pages(bool wifi, bool connecting){
  yomenu._m2Wifi = wifi;
  network.tryHeld = connecting;
  /*  пошук мереж і спроби повернутися в мережу — один радіомодуль: поки
      людина вибирає мережу, спроби спинено  */
  network.pauseSta(wifi || connecting);
}

}  // namespace m2
#endif
