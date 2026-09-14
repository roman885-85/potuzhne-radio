#include "Arduino.h"
#include "core/options.h"
#include "core/config.h"
#include "pluginsManager/pluginsManager.h"
#include "core/telnet.h"
#include "core/player.h"
#include "core/display.h"
#include "core/network.h"
#include "core/netserver.h"
#include "core/controls.h"
#include "core/optionschecker.h"
#include "menu/yoDebug.h"
#include "core/timekeeper.h"
#include "extras/yoExtras.h"
#include "menu/yoMenu.h"
#include "extras/yoMic.h"
#include "extras/yoDsp.h"
#include "extras/yoSfx.h"
#include "extras/yoOta.h"
#ifdef USE_NEXTION
#include "displays/nextion.h"
#endif

#if USE_OTA
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
#include <NetworkUdp.h>
#else
#include <WiFiUdp.h>
#endif
#include <ArduinoOTA.h>
#endif

#if DSP_HSPI || TS_HSPI || VS_HSPI
SPIClass  SPI2(HSPI);
#endif

extern __attribute__((weak)) void yoradio_on_setup();

#if USE_OTA
void setupOTA(){
  if(strlen(config.store.mdnsname)>0)
    ArduinoOTA.setHostname(config.store.mdnsname);
#ifdef OTA_PASS
  ArduinoOTA.setPassword(OTA_PASS);
#endif
  ArduinoOTA
    .onStart([]() {
      player.sendCommand({PR_STOP, 0});
      display.putRequest(NEWMODE, UPDATING);
      telnet.printf("Start OTA updating %s\n", ArduinoOTA.getCommand() == U_FLASH?"firmware":"filesystem");
    })
    .onEnd([]() {
      telnet.printf("\nEnd OTA update, Rebooting...\n");
      ESP.restart();
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      telnet.printf("Progress OTA: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      telnet.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        telnet.printf("Auth Failed\n");
      } else if (error == OTA_BEGIN_ERROR) {
        telnet.printf("Begin Failed\n");
      } else if (error == OTA_CONNECT_ERROR) {
        telnet.printf("Connect Failed\n");
      } else if (error == OTA_RECEIVE_ERROR) {
        telnet.printf("Receive Failed\n");
      } else if (error == OTA_END_ERROR) {
        telnet.printf("End Failed\n");
      }
    });
  ArduinoOTA.begin();
}
#endif

void setup() {
  YoExtras::earlyBoot();      /* після сну «вимкнено»: зняти фіксацію виводів, доки їх ніхто не чіпав */
  Serial.begin(115200);
  if(REAL_LEDBUILTIN!=255) pinMode(REAL_LEDBUILTIN, OUTPUT);
  if (yoradio_on_setup) yoradio_on_setup();
  pm.on_setup();
  config.init();
  YoSfx::mount();           /* розділ ресурсів: заставка потрібна вже екрану завантаження */
  display.init();
  player.init();
  extras.begin();           /* таймер сну, будильник, ніч, батарея, світлодіод */
  Serial.printf("##[BOOT]#\tостаннє перезавантаження: %s\n", YoExtras::resetReason());
  mic.begin();              /* вбудований мікрофон: задача слухає, лише коли його ввімкнули */
  sfx.begin();              /* звуки подій: розділ ресурсів і своя задача виводу */
  sfx.play(SFX_START);
  network.begin();
  if (network.status != CONNECTED && network.status!=SDREADY) {
    netserver.begin();
    initControls();
    display.putRequest(DSP_START);
    while(!display.ready()) delay(10);
    return;
  }
  if(SDC_CS!=255) {
    display.putRequest(WAITFORSD, 0);
    Serial.print("##[BOOT]#\tSD search\t");
  }
  config.initPlaylistMode();
  netserver.begin();
  telnet.begin();
  initControls();
  display.putRequest(DSP_START);
  while(!display.ready()) delay(10);
  #ifdef MQTT_ROOT_TOPIC
    mqttInit();
  #endif
  #if USE_OTA
    setupOTA();
  #endif
  if (config.getMode()==PM_SDCARD) player.initHeaders(config.station.url);
  player.lockOutput=false;
  if (config.store.smartstart == 1) {
    player.sendCommand({PR_PLAY, config.lastStation()});
  }
  pm.on_end_setup();
}

#ifdef YO_DEBUG
/*  Хто саме тримає головний цикл: дотик, звук і меню живуть тут же, і будь-яка
    затримка довша за глибину буфера звуку чутна й видна. Нічого не друкуємо:
    запис у USB-порт сам блокує цикл, коли з того боку ніхто не читає, — лише
    запам'ятовуємо найдовший крок, а показує його команда «page».  */
char     yoSlowWhat[40] = {0};
uint32_t yoSlowMs = 0;
/*  Як часто взагалі крутиться цикл: дотик опитується раз на оберт, тож
    рідкий цикл — це і є «тормоза», навіть коли жоден крок не довгий.  */
uint32_t yoLoopN = 0, yoLoopMax = 0, yoLoopFrom = 0;
static inline void yoSlow(const char* what, uint32_t t0){
  uint32_t d = millis() - t0;
  if(d > 100 && d > yoSlowMs){ yoSlowMs = d; strlcpy(yoSlowWhat, what, sizeof(yoSlowWhat)); }
}
  #define STEP(call) { uint32_t _t0 = millis(); call; yoSlow(#call, _t0); }
#else
  #define STEP(call) call
#endif

void loop() {
#ifdef YO_DEBUG
  {
    static uint32_t prev = 0;
    uint32_t now = millis();
    if(!yoLoopFrom) yoLoopFrom = now;
    if(prev){ uint32_t d = now - prev; if(d > yoLoopMax) yoLoopMax = d; }
    prev = now; yoLoopN++;
  }
  yodbgLoop();
#endif
  STEP(timekeeper.loop1());
  STEP(config.eepromLoop());      /* відкладений запис налаштувань */
  STEP(network.loop());           /* мережа зникла — повертаємось у неї без зупинок */
  STEP(telnet.loop());
  /*  Плеєр крутимо завжди: навіть без мережі його чергу треба розгрібати,
      інакше вона забивається, а той, хто в неї пише (задача екрана), стоїть
      на кожній посилці. Саме через це радіо «тормозило» без мережі.  */
  STEP(player.loop());
  if (network.status == CONNECTED || network.status==SDREADY) {
#if USE_OTA
    STEP(ArduinoOTA.handle());
#endif
  }
  STEP(loopControls());
#ifdef USE_YOMENU
  STEP(ota.loop());               /* оновлення з GitHub: автоперевірка двічі на добу */
  STEP(yomenu.wifiTick());        /* пошук мереж — тут, а не в задачі дисплея */
  STEP(yoDsp.roomTick());         /* налаштування під кімнату: тони, замір, поправка */
  STEP(mic.loop());               /* мікрофон: дії на хлопки й стук, присутність, сон */
#endif
  STEP(extras.loop());
  #ifdef NETSERVER_LOOP1
  STEP(netserver.loop());
  #endif
}

#include "core/audiohandlers.h"
