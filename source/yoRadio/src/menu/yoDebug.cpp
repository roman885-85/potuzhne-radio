#include "yoDebug.h"
#ifdef YO_DEBUG

#include "../core/config.h"
#include "../core/display.h"
#include "../core/player.h"
#include "../core/network.h"
#include "../core/timekeeper.h"
#include "../core/sdmanager.h"
#include "../displays/dspcore.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include "freertos_stats.h"
#include "esp_heap_caps.h"
#include <SPIFFS.h>
#include "../ES8311/yoES8311.h"
#include "yoMenu.h"
#include "../core/touchscreen.h"
#include "../extras/yoExtras.h"
#include "../extras/yoRecorder.h"
#include "../extras/yoSermons.h"
#include "../extras/yoMic.h"
#include "../extras/yoSfx.h"
#include "../extras/yoDsp.h"

extern DspCore dsp;

static uint16_t vuSampleLeft = 0;
static bool vuWasOn = false;
static uint8_t vuMin = 255, vuMax = 0;
static uint8_t vuBarMin = 255, vuBarMax = 0;   /* те саме, але вже на екрані */

/*  Перевірка анімації гучності: знімаємо, де насправді стоїть смуга, кожні
    20 мс. Без затримок у циклі — інакше зупиниться звук.  */
static uint8_t  slideLeft = 0;
static uint32_t slideTick = 0;
static uint8_t  slideBack = 0;

/*  Приймання файлу в SPIFFS порядково, шістнадцятковим текстом. Потрібне,
    щоб оновити сторінки веб-інтерфейсу, не стираючи розділ цілком: поруч
    із ними в /data лежать збережена мережа й список станцій.  */
static File     putFile;
static uint32_t putBytes = 0;
static uint32_t vuTick = 0;

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void b64row(const uint8_t* d, size_t n){
  char out[4];
  for(size_t i=0;i<n;i+=3){
    uint32_t v = d[i] << 16 | (i+1<n ? d[i+1]<<8 : 0) | (i+2<n ? d[i+2] : 0);
    out[0]=B64[(v>>18)&63]; out[1]=B64[(v>>12)&63];
    out[2]=(i+1<n)?B64[(v>>6)&63]:'='; out[3]=(i+2<n)?B64[v&63]:'=';
    Serial.write((uint8_t*)out, 4);
  }
}

/*  Самоперевірка: малюємо смужку відомого кольору, читаємо назад і
    порівнюємо. Якщо не збіглося — знімкам вірити не можна.  */
bool yodbgReadTest(){
  static uint16_t row[320];
  /*  Читання відеопам'яті забирає шину SPI і змінює її частоту, а задача
      дисплея малює виджети з іншого ядра. Без замикання це гонка за шину —
      саме вона лишає сміття на екрані.  */
  display.lock();
  delay(60);
  uint16_t probe = 0xF81F;                 /* яскраво-рожевий, у інтерфейсі не трапляється */
  dsp.fillRect(0, 0, 320, 4, probe);
  dsp.dbgReadRow(1, 320, row);
  uint16_t got = row[10];
  bool ok = (got == probe);
  Serial.printf("SCRTEST: записано 0x%04X, прочитано 0x%04X -> %s\n", probe, got, ok?"ЧИТАННЯ ПРАЦЮЄ":"ЧИТАННЯ НЕ ПРАЦЮЄ");
  display.unlock();
  display.forceRedraw();      /* прибираємо смужку, якою перевіряли */
  return ok;
}

void yodbgScreenshot(){
  static uint16_t row[320];
  display.lock();             /* див. пояснення в yodbgReadTest() */
  delay(60);
  Serial.println("SCR_BEGIN 320 240");
  for(uint16_t y=0; y<240; y++){
    dsp.dbgReadRow(y, 320, row);
    b64row((uint8_t*)row, 640);
    Serial.println();
    if((y & 15)==15) delay(2);             /* даємо USB CDC віддихатись */
  }
  Serial.println("SCR_END");
  display.unlock();
}

static void addStation(const char* name, const char* url){
  File f = SPIFFS.open(PLAYLIST_PATH, FILE_APPEND);
  if(!f){ Serial.println("DBG: не відкрився плейлист"); return; }
  f.printf("%s\t%s\t0\n", name, url);
  f.close();
}

void yodbgLoop(){
  /*  Вибірка рівня: один відлік за оберт головного циклу, без затримок.  */
  if(slideLeft && millis() - slideTick >= 20){
    slideTick = millis();
    Serial.printf("  смуга=%u\n", (unsigned)display.volbarShown());
    if(--slideLeft == 0){
      player.setVol(slideBack);            /* повертаємо як було */
      Serial.println("гучність повернуто");
    }
  }
  if(vuSampleLeft && millis() - vuTick >= 40){
    vuTick = millis();
    uint16_t v = player.get_VUlevel(100);
    uint8_t l = 100 - ((v>>8)&0xFF), r = 100 - (v&0xFF);
    if(l < vuMin) vuMin = l;
    if(l > vuMax) vuMax = l;
    extern uint16_t yoVuMeasL;
    uint8_t bar = (uint8_t)(yoVuMeasL > 100 ? 0 : 100 - yoVuMeasL);
    if(bar < vuBarMin) vuBarMin = bar;
    if(bar > vuBarMax) vuBarMax = bar;
    if((vuSampleLeft % 6) == 0) Serial.printf("  рівень L=%3d R=%3d\n", l, r);
    if(--vuSampleLeft == 0){
      Serial.printf("розмах лівого каналу за 2.4 с: %d..%d (розкид %d)\n", vuMin, vuMax, vuMax-vuMin);
      Serial.printf("  а на екрані, після згладжування: %d..%d (розкид %d)\n",
                    vuBarMin, vuBarMax, vuBarMax-vuBarMin);
      config.store.vumeter = vuWasOn;
    }
  }

  static char buf[160];
  static uint8_t pos = 0;
  while(Serial.available()){
    char c = Serial.read();
    if(c=='\r') continue;
    if(c!='\n'){ if(pos < sizeof(buf)-1) buf[pos++]=c; continue; }
    buf[pos]=0; pos=0;
    if(!buf[0]) continue;

    if(putFile){                       /* йде приймання файлу */
      if(!strcmp(buf,".")){
        putFile.close();
        Serial.printf("файл записано, %lu байт\n", (unsigned long)putBytes);
      }else{
        uint8_t chunk[96]; size_t n = strlen(buf)/2;
        if(n > sizeof(chunk)) n = sizeof(chunk);
        for(size_t i=0;i<n;i++){
          char hi = buf[i*2], lo = buf[i*2+1];
          uint8_t h = (uint8_t)(hi<='9' ? hi-'0' : (hi|32)-'a'+10);
          uint8_t l = (uint8_t)(lo<='9' ? lo-'0' : (lo|32)-'a'+10);
          chunk[i] = (uint8_t)((h<<4)|l);
        }
        putFile.write(chunk, n);
        putBytes += n;
      }
      continue;
    }

    if(!strcmp(buf,"scr"))            yodbgScreenshot();
    else if(!strcmp(buf,"scrtest"))   yodbgReadTest();
    else if(!strcmp(buf,"stop"))      player.sendCommand({PR_STOP,0});
    else if(!strncmp(buf,"play ",5))  player.sendCommand({PR_PLAY,(uint16_t)atoi(buf+5)});
    else if(!strncmp(buf,"vol ",4))   player.sendCommand({PR_VOL,(uint16_t)atoi(buf+4)});
    else if(!strcmp(buf,"mode"))      config.changeMode();
    else if(!strncmp(buf,"add ",4)){
      char* tab = strchr(buf+4,'|');
      if(tab){ *tab=0; addStation(buf+4, tab+1); Serial.println("DBG: станцію додано"); }
    }
    else if(!strcmp(buf,"reindex")){ config.indexPlaylist(); Serial.println("DBG: індекс перебудовано"); }
    else if(!strcmp(buf,"sd")){
      Serial.printf("SD: ready=%d ", sdman.ready ? 1 : 0);
      if(!sdman.ready) Serial.printf("start=%d ", sdman.start() ? 1 : 0);
      if(sdman.ready) Serial.printf("тип=%d розмір=%lluМБ", (int)sdman.cardType(), sdman.cardSize()/(1024ULL*1024ULL));
      Serial.println();
    }
    else if(!strncmp(buf,"micgest ",8)){ mic.injectGesture((uint8_t)atoi(buf+8)); Serial.printf("MICGEST %d\n", atoi(buf+8)); }
    else if(!strcmp(buf,"info")){
      Serial.printf("остання перезавантаження: %s\n", YoExtras::resetReason());
      Serial.printf("режим=%s станція=%d гучність=%d грає=%d\n",
        config.getMode()==PM_SDCARD?"SD":"WEB", config.lastStation(),
        config.store.volume, player.isRunning()?1:0);
      Serial.printf("плейлист=%d станцій, ip=%s, heap=%u\n",
        config.playlistLength(), WiFi.localIP().toString().c_str(), (unsigned)ESP.getFreeHeap());
    }
    else if(!strcmp(buf,"net")){
      /*  Де саме рветься зв'язок: розбір імені чи саме з'єднання.  */
      Serial.printf("ip=%s шлюз=%s маска=%s dns=%s\n",
        WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str(),
        WiFi.subnetMask().toString().c_str(), WiFi.dnsIP().toString().c_str());
      Serial.printf("RSSI=%d dBm канал=%d\n", (int)WiFi.RSSI(), (int)WiFi.channel());
      Serial.printf("PSRAM: всього=%u вільно=%u | heap=%u\n",
        (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFreePsram(), (unsigned)ESP.getFreeHeap());
      const char* hosts[] = { "stream.radioparadise.com", "ice1.somafm.com", "pool.ntp.org" };
      for(uint8_t i=0;i<3;i++){
        IPAddress a;
        uint32_t t0=millis();
        bool ok = WiFi.hostByName(hosts[i], a);
        Serial.printf("DNS %-26s %s (%s) %lums\n", hosts[i], ok?"OK":"ПОМИЛКА",
                      ok?a.toString().c_str():"-", (unsigned long)(millis()-t0));
        if(ok){
          WiFiClient c; c.setTimeout(5);
          t0=millis();
          bool con = c.connect(a, 80);
          Serial.printf("TCP %-26s %s %lums\n", hosts[i], con?"OK":"ПОМИЛКА", (unsigned long)(millis()-t0));
          c.stop();
        }
      }
    }
    else if(!strncmp(buf,"tcp ",4)){
      /*  Довільна адреса й порт: відрізнити «немає маршруту назовні»
          від «не працює сам клієнт».  */
      char* sp = strchr(buf+4,' ');
      if(sp){
        *sp = 0;
        IPAddress a; a.fromString(buf+4);
        WiFiClient c;
        uint32_t t0 = millis();
        bool ok = c.connect(a, (uint16_t)atoi(sp+1));
        Serial.printf("TCP %s:%d -> %s %lums\n", buf+4, atoi(sp+1), ok?"OK":"ПОМИЛКА", (unsigned long)(millis()-t0));
        c.stop();
      }
    }
    else if(!strncmp(buf,"esw ",4)){ unsigned r=0,v=0; sscanf(buf+4, "%x %x", &r, &v); Serial.printf("ES8311 reg%02X <- %02X: %s\n", r, v, es8311_write((uint8_t)r, (uint8_t)v) ? "OK" : "ПОМИЛКА"); }
    else if(!strcmp(buf,"es")){
      es8311_dump();
      /*  Читаємо не змінюючи режим виводу: перемикання в INPUT знімає
          рівень, який нога видає, і показує сміття.  */
      Serial.printf("вивід підсилювача GPIO%d = %s (низький = увімкнено), грає=%d\n",
                    MUTE_PIN, digitalRead(MUTE_PIN) ? "високий" : "низький",
                    player.isRunning()?1:0);
    }
    else if(!strcmp(buf,"vu")){
      /*  Не можна міряти рівень із затримками в цьому ж циклі: декодер звуку
          крутиться тут само, і delay() його зупиняє — значення завмирають.  */
      Serial.printf("частота=%u Гц біт=%d канали=%d бітрейт=%d\n",
        (unsigned)player.getSampleRate(), (int)player.getBitsPerSample(),
        (int)player.getChannels(), (int)player.getBitRate());
      vuSampleLeft = 60; vuWasOn = config.store.vumeter;
      config.store.vumeter = true; vuMin = 255; vuMax = 0;
      vuBarMin = 255; vuBarMax = 0;
    }
    else if(!strncmp(buf,"vuset",5)){
      /*  vuset <дБ> <спад> <підйом> <опускання> — підбір поведінки покажчика
          без перезбирання. Без аргументів просто показує поточні значення.  */
      extern uint8_t yoVuRange, yoVuRelease, yoVuRise, yoVuFade;
      int a=0,b=0,c=0,d=0;
      if(sscanf(buf+5, "%d %d %d %d", &a,&b,&c,&d)==4){
        if(a>=12 && a<=72)  yoVuRange   = (uint8_t)a;
        if(b>=2  && b<=8)   yoVuRelease = (uint8_t)b;
        if(c>=1  && c<=16)  yoVuRise    = (uint8_t)c;
        if(d>=1  && d<=16)  yoVuFade    = (uint8_t)d;
      }
      Serial.printf("покажчик: шкала=%d дБ спад=%d підйом=%d/16 опускання=%d/16\n",
                    yoVuRange, yoVuRelease, yoVuRise, yoVuFade);
    }
    else if(!strcmp(buf,"vufps")){
      /*  Скільки разів на секунду малюється покажчик і де стоїть стовпчик.
          Без цього плавність можна лише припускати.  */
      extern uint32_t yoVuCalls; extern uint16_t yoVuMeasL, yoVuMeasR;
      static uint32_t was = 0, wasms = 0;
      uint32_t now = millis(), dc = yoVuCalls - was, dt = now - wasms;
      Serial.printf("покажчик: %lu звернень за %lu мс (%lu на секунду), стовпчик L=%d R=%d з 100\n",
                    (unsigned long)dc, (unsigned long)dt,
                    dt ? (unsigned long)(dc*1000UL/dt) : 0UL,
                    (int)(100-yoVuMeasL), (int)(100-yoVuMeasR));
      was = yoVuCalls; wasms = now;
    }
    else if(!strncmp(buf,"put ",4)){
      /*  put <шлях> — далі рядки шістнадцяткою, крапка в окремому рядку
          завершує. Стирається тільки цей файл, решта розділу ціла.  */
      putFile = SPIFFS.open(buf+4, FILE_WRITE);
      putBytes = 0;
      Serial.printf("приймаю %s: %s\n", buf+4, putFile ? "готовий" : "НЕ ВІДКРИВСЯ");
    }
    else if(!strncmp(buf,"ls",2)){
      File d = SPIFFS.open(strlen(buf)>3 ? buf+3 : "/www");
      if(d && d.isDirectory()){
        File f = d.openNextFile();
        while(f){ Serial.printf("  %s %u\n", f.name(), (unsigned)f.size()); f = d.openNextFile(); }
      }else Serial.println("теки немає");
      Serial.printf("SPIFFS: зайнято %u з %u байт\n",
                    (unsigned)SPIFFS.usedBytes(), (unsigned)SPIFFS.totalBytes());
    }
    else if(!strcmp(buf,"weather")){
      Serial.printf("погода: показ=%d ключ=%s широта=%s довгота=%s\n",
                    config.store.showweather ? 1 : 0,
                    strlen(config.store.weatherkey) ? "є" : "НЕМАЄ",
                    config.store.weatherlat, config.store.weatherlon);
      Serial.printf("  рядок: \"%s\"\n", timekeeper.weatherBuf ? timekeeper.weatherBuf : "(не виділено)");
      Serial.printf("  значок за умовою: %d\n", (int)timekeeper.weatherIcon);
      display.dbgWeather();
    }
    else if(!strcmp(buf,"slide")){
      /*  Зсуваємо гучність і дивимось, як смуга доїжджає. Через 0.5 с
          повертаємо попереднє значення.  */
      slideBack = config.store.volume;
      uint8_t to = slideBack > 120 ? slideBack - 60 : slideBack + 60;
      Serial.printf("гучність %u -> %u, стежу за смугою\n", slideBack, to);
      player.setVol(to);
      slideLeft = 25; slideTick = 0;
    }
    else if(!strcmp(buf,"sdindex")){
      /*  Пересобрати плейлист картки: після зміни правил відбору файлів
          старий індекс на картці лишається, доки його не перебудувати.  */
      if(config.getMode() != PM_SDCARD) Serial.println("sdindex: спершу перемкніть на картку");
      else{ sdman.indexSDPlaylist(); config.initPlaylistMode(); Serial.printf("sdindex: треків %u\n", (unsigned)config.playlistLength()); }
    }
    else if(!strncmp(buf,"wifidrop",8)){
      /*  Обрив зв'язку на замовлення: так перевіряють, як радіо повертається
          в мережу й чи лишається живим екран і звук. З числом — скільки
          секунд не намагатися назад (щоб устигнути подивитись на екран).  */
      int hold = atoi(buf + 8);
      Serial.printf("розриваю зв'язок з мережею, тримаю %d с\n", hold);
      WiFi.disconnect();
      if(hold > 0){
        network.pauseSta(true);
        timekeeper.waitAndDo((uint8_t)(hold > 250 ? 250 : hold), [](){ network.pauseSta(false); });
      }
    }
    else if(!strcmp(buf,"mic")){
      Serial.printf("мікрофон: слухає=%d рівень=%d дБ фон=%d дБ голос=%d блоків=%u луна=%d режим=%u кадр=%d віднято=%.1f дБ удар=%.2f слоти L=%d R=%d почуто=%s кімната=+%.1f зв'язок=%.1f\n",
        mic.listening()?1:0, (int)mic.levelDb(), (int)mic.noiseDb(), mic.speech()?1:0,
        (unsigned)mic.blocks(), mic.aecActive()?1:0, (unsigned)mic.aecMode(), mic.aecChunk(), mic.echoCut(), mic.onsetVal(),
        (int)mic.slotDb(0), (int)mic.slotDb(1), YoMic::gestureName(mic.heard()), mic.excessDb(), mic.coupleDb());
    }
    else if(!strncmp(buf,"micgain ",8)){
      extras.s.micGain = (uint8_t)atoi(buf+8); extras.changed(); mic.apply();
      Serial.printf("підсилення мікрофона: крок %u\n", (unsigned)extras.s.micGain);
    }
    else if(!strncmp(buf,"micset ",7)){
      /*  micset <хлопки 0/1> <стук 0/1> <під час звуку 0/1>  */
      int c=0,k=0,p=0; sscanf(buf+7, "%d %d %d", &c, &k, &p);
      extras.s.clapOn = c; extras.s.knockOn = k; extras.s.micPlay = p; extras.changed();
      Serial.printf("хлопки=%d стук=%d під час звуку=%d\n", c, k, p);
    }
    else if(!strncmp(buf,"mictest",7)){
      /*  mictest [рівень тону, дБ; типово -20] [noamp — підсилювач вимкнений] [quick — 5 частот]  */
      int lv = buf[7] == ' ' ? atoi(buf+8) : -20;
      if(!lv) lv = -20;
      bool amp = !strstr(buf, "noamp");
      uint32_t mask = strstr(buf, "quick") ? (1UL<<3 | 1UL<<9 | 1UL<<12 | 1UL<<17 | 1UL<<21) : 0;
      const char* why = mic.sweepStart((int8_t)lv, amp, mask, strstr(buf, " dsp") != nullptr);
      if(why) Serial.printf("самоперевірка звуку: не почато — %s\n", why);
      else    Serial.printf("самоперевірка звуку: почато, тон %d дБ, частота тактів %u Гц\n", lv, (unsigned)player.getSampleRate());
    }
    else if(!strcmp(buf,"eq")){
      const ExtStore& e = extras.s;
      Serial.printf("EQ on=%u preset=%u(%s) loud=%u guard=%u vbass=%u room=%u pre=%.1f us=%.2f limit=%u vol=%u rate=%u\n",
        e.eqOn, e.eqPreset, YoDsp::PRESET_NAME[e.eqPreset < EQ_PRESETS ? e.eqPreset : 0], e.eqLoud, e.eqGuard, e.vbass, e.eqRoomOn,
        yoDsp.preampDb(), yoDsp.usPerFrame(), (unsigned)yoDsp.limitHits(), (unsigned)player.getVolume(), (unsigned)player.getSampleRate());
      for(uint8_t b = 0; b < EQ_BANDS; b++)
        Serial.printf("EQB %u %d %d %.1f\n", YoDsp::BAND_HZ[b], e.eq[b], e.eqRoom[b], yoDsp.responseDb(YoDsp::BAND_HZ[b]));
    }
    else if(!strncmp(buf,"eqp ",4)){ yoDsp.applyPreset((uint8_t)atoi(buf+4)); Serial.printf("пресет %u\n", extras.s.eqPreset); }
    else if(!strncmp(buf,"eqb ",4)){ int b=0,d=0; sscanf(buf+4, "%d %d", &b, &d); yoDsp.setBand((uint8_t)b, (int8_t)d); Serial.printf("смуга %d = %d\n", b, d); }
    else if(!strncmp(buf,"eqo ",4)){
      /*  eqo <еквалайзер 0/1> <тонкомпенсація 0..2> <захист 0..2> <віртуальний бас 0..3> <кімната 0/1>  */
      int on=1, ld=0, g=1, vb=0, rm=0; sscanf(buf+4, "%d %d %d %d %d", &on, &ld, &g, &vb, &rm);
      extras.s.eqOn = on; extras.s.eqLoud = ld; extras.s.eqGuard = g; extras.s.vbass = vb; extras.s.eqRoomOn = rm;
      extras.changed(); yoDsp.changed();
      Serial.printf("звук: eq=%d тонкомп=%d захист=%d бас=%d кімната=%d\n", on, ld, g, vb, rm);
    }
    else if(!strcmp(buf,"dspbench")){
      if(player.isRunning()) Serial.println("DSPBENCH спершу зупиніть звук");
      else {
        float us = yoDsp.bench(44100);
        Serial.printf("DSPBENCH us=%.3f ланок=%u частка ядра=%.1f%%\n", us, (unsigned)yoDsp.stages(), us * 44100.0f / 1e4f);
      }
    }
    else if(!strcmp(buf,"eqmigrate")){
      /*  повторити перенесення трьох старих повзунків (після виправлення меж)  */
      yoDsp.fromTone(config.store.bass, config.store.middle, config.store.trebble);
      Serial.printf("перенесено: низькі %d середні %d високі %d\n", config.store.bass, config.store.middle, config.store.trebble);
    }
    else if(!strcmp(buf,"eqroom")){ char w[48]; bool ok = yoDsp.roomFromSweep(w, sizeof(w)); Serial.printf("кімната: %s %s\n", ok ? "OK" : "ні", w); }
    else if(!strcmp(buf,"roomtune")){ const char* w = yoDsp.roomTuneStart(); Serial.printf("ROOM %s\n", w ? w : "почато"); }
    else if(!strcmp(buf,"roomst")){ Serial.printf("ROOM state=%u progress=%u msg=%s\n", yoDsp.roomState(), yoDsp.roomProgress(), yoDsp.roomMsg()); }
    else if(!strcmp(buf,"eqroomclr")){ yoDsp.roomClear(); Serial.println("кімната: поправку скинуто"); }
    else if(!strcmp(buf,"eqresp")){
      /*  розрахункова АЧХ на частотах заміру — для tools/selftest.py  */
      for(uint8_t i = 0; i < YoMic::SWEEP_N; i++) Serial.printf("RESP %u %.2f\n", (unsigned)YoMic::sweepHz(i), yoDsp.responseDb(YoMic::sweepHz(i)));
      Serial.println("RESP end");
    }
    else if(!strncmp(buf,"aecmode ",8)){ int m=0,l=4; sscanf(buf+8, "%d %d", &m, &l); mic.aecSet((uint8_t)m, (uint8_t)l); Serial.printf("AEC режим %d, фільтр %d\n", m, l); }
    else if(!strncmp(buf,"micdbg ",7)){ mic._dbg = atoi(buf+7) != 0; Serial.printf("удари: %s\n", mic._dbg ? "друкую" : "мовчу"); }
    else if(!strncmp(buf,"micfast ",8)){ mic.minMs = atoi(buf+8) ? 1000 : 60000; Serial.printf("хвилина мікрофона = %u мс\n", (unsigned)mic.minMs); }
    else if(!strcmp(buf,"pres")){
      uint32_t now = millis();
      Serial.printf("PRES темно=%d пригашено=%d голос=%d тиша=%u с дотик=%u с сон=%u с ear=%u wake=%u off=%u\n",
        extras.presenceDark()?1:0, extras.screenDim()?1:0, mic.speech()?1:0,
        (unsigned)(mic.lastRoomMs() ? (now - mic.lastRoomMs()) / 1000 : 9999), (unsigned)((now - extras.lastTouchMs()) / 1000),
        (unsigned)extras.sleepLeftSec(), extras.s.sleepEar, extras.s.presWake, extras.s.presOff);
    }
    else if(!strncmp(buf,"micsim ",7)){
      /*  micsim <clap|knock> <скільки> <крок, мс>  */
      char kind[8] = {0}; int n = 2, gap = 300;
      sscanf(buf+7, "%7s %d %d", kind, &n, &gap);
      const char* w = mic.simStart(!strcmp(kind, "voice") ? 2 : !strcmp(kind, "knock") ? 1 : 0, (uint8_t)n, (uint16_t)gap);
      Serial.printf("MICSIM %s\n", w ? w : "почато");
    }
    else if(!strncmp(buf,"micext ",7)){
      /*  micext <clap|knock> <скільки> <крок, мс> <пік, дБ> — удар «із кімнати» в сигнал мікрофона  */
      char kind[8] = {0}; int n = 2, gap = 300, pk = -20;
      sscanf(buf+7, "%7s %d %d %d", kind, &n, &gap, &pk);
      const char* w = mic.extStart(!strcmp(kind, "knock") ? 1 : 0, (uint8_t)n, (uint16_t)gap, (int8_t)pk);
      Serial.printf("MICEXT %s\n", w ? w : "почато");
    }
    else if(!strcmp(buf,"micstop")){ mic.sweepAbort(); Serial.println("самоперевірка звуку: зупиняю"); }
    else if(!strcmp(buf,"micres")){
      /*  Машинний формат — його читає tools/selftest.py:
          SWEEP state=<0..3> pos=<n> of=<N> rate=<Гц> level=<дБ>
          SW <Гц> <тон дБ> <фон дБ> <2-га гарм. дБ> <3-тя гарм. дБ>  */
      Serial.printf("SWEEP state=%u pos=%u of=%u rate=%u level=%d amp=%d dsp=%d\n", (unsigned)mic.sweepState(), (unsigned)mic.sweepPos(),
        (unsigned)YoMic::SWEEP_N, (unsigned)mic.sweepRate(), (int)mic.sweepLevel(), mic.sweepAmp()?1:0, mic.sweepDsp()?1:0);
      if(mic.sweepState() >= 2){
        for(uint8_t i = 0; i < mic.sweepPos() && i < YoMic::SWEEP_N; i++)
          Serial.printf("SW %u %.1f %.1f %.1f %.1f\n", (unsigned)YoMic::sweepHz(i), mic.sweepDb(i), mic.sweepNoise(i), mic.sweepH2(i), mic.sweepH3(i));
      }
      Serial.println("SWEEP end");
    }
    else if(!strcmp(buf,"heap")){
      Serial.printf("HEAP internal=%u blk=%u min=%u psram=%u psblk=%u\n",
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    }
    else if(!strncmp(buf,"micon ",6)){
      extras.s.micOn = atoi(buf+6) ? 1 : 0; extras.changed(); mic.apply();
      Serial.printf("мікрофон %s\n", extras.s.micOn ? "увімкнено" : "вимкнено");
    }
    else if(!strcmp(buf,"tasks")){
      /*  Хто скільки процесора з'їв: список задач FreeRTOS від самого ядра.  */
      printRunningTasks(Serial);
    }
    else if(!strcmp(buf,"wscan")){
      /*  Що бачить радіо: назва, сигнал, канал, захист, BSSID. 5 ГГц ESP32-S3 не бачить зовсім.  */
      network.pauseSta(true);
      int16_t n = WiFi.scanNetworks(false, true);
      static const char* AUTH[] = { "відкрита", "WEP", "WPA", "WPA2", "WPA/WPA2", "WPA2-Ent", "WPA3", "WPA2/WPA3", "WAPI", "OWE", "WPA3-Ent", "WPA3-Ent192" };
      for(int16_t i = 0; i < n; i++){
        uint8_t a = WiFi.encryptionType(i);
        Serial.printf("WSCAN %-32s %4d dBm  кан %2d  %-10s %s\n", WiFi.SSID(i).c_str(), (int)WiFi.RSSI(i), (int)WiFi.channel(i),
                      a < 12 ? AUTH[a] : "?", WiFi.BSSIDstr(i).c_str());
      }
      Serial.printf("WSCAN end %d\n", (int)n);
      WiFi.scanDelete();
      network.pauseSta(false);
    }
    else if(!strncmp(buf,"wjoin ",6)){
      /*  wjoin <назва>|<пароль> — та сама спроба, що й з меню  */
      char* bar = strchr(buf+6, '|');
      if(bar){ *bar = 0; network.connectTo(buf+6, bar+1); Serial.printf("WJOIN %s\n", buf+6); }
    }
    else if(!strcmp(buf,"wlist")){
      /*  Збережені мережі: назва й довжина пароля (самого пароля не друкуємо).  */
      for(uint8_t i = 0; i < config.ssidsCount; i++)
        Serial.printf("WLIST %u%s %s  пароль %u симв.\n", (unsigned)(i + 1), config.store.lastSSID == i + 1 ? "*" : " ",
                      config.ssids[i].ssid, (unsigned)strlen(config.ssids[i].password));
      File f = SPIFFS.open(SSIDS_PATH, "r");
      Serial.printf("WLIST end %u, файл %u байт\n", (unsigned)config.ssidsCount, f ? (unsigned)f.size() : 0);
      if(f) f.close();
    }
    else if(!strcmp(buf,"wstate")) network.dump();
    else if(!strncmp(buf,"sfx ",4)){
      /*  sfx <подія або номер> — програти (навіть вимкнену)  */
      int e = YoSfx::find(buf + 4);
      if(e >= 0){ sfx.test((SfxEvent)e); Serial.printf("SFX %s (%u мс)\n", YoSfx::id((SfxEvent)e), (unsigned)sfx.clipMs((SfxEvent)e)); }
      else Serial.println("SFX невідома подія");
    }
    else if(!strcmp(buf,"sfxls")){
      Serial.printf("SFX розділ: %s, %u з %u байт\n", sfx.fsOk() ? "є" : "немає", (unsigned)sfx.fsUsed(), (unsigned)sfx.fsTotal());
      for(uint8_t i = 0; i < SFX_N; i++)
        Serial.printf("SFX %u %-8s %s %u мс%s\n", i, YoSfx::id((SfxEvent)i), (extras.s.sfxMask >> i) & 1 ? "увімк" : "вимк ",
                      (unsigned)sfx.clipMs((SfxEvent)i), (sfx.userMask() >> i) & 1 ? " (свій)" : "");
    }
    else if(!strcmp(buf,"wnonet")){
      Serial.println("WNONET перезавантажуюсь, наче мережі поруч немає");
      MyNetwork::skipBootWifi();
      delay(300);
      ESP.restart();
    }
    else if(!strcmp(buf,"wtry")){
      Serial.printf("WTRY стан=%d status()=%d ssid='%s' ip=%s\n", (int)network.tryState(), (int)WiFi.status(), WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    }
    else if(!strcmp(buf,"wifioff")){
      /*  Погасити радіомодуль зовсім — перевірити, чи це він душить екран.  */
      WiFi.scanDelete();
      WiFi.mode(WIFI_OFF);
      Serial.println("радіомодуль вимкнено");
    }
    else if(!strcmp(buf,"wifikeep")){
      /*  Записати мережу, у якій радіо зараз, у список — назву й пароль
          бере в самого радіомодуля, тож набирати нічого не треба.  */
      wifi_config_t c;
      if(esp_wifi_get_config(WIFI_IF_STA, &c) != ESP_OK || !c.sta.ssid[0]){
        Serial.println("радіомодуль не пам'ятає жодної мережі");
      }else{
        /*  Додаємо першою, а решту збережених лишаємо. Раніше команда
            записувала лише цю одну — і стерла власникові домашню мережу.  */
        String out = String((const char*)c.sta.ssid) + "\t" + String((const char*)c.sta.password) + "\n";
        uint8_t n = 1;
        for(uint8_t i = 0; i < config.ssidsCount && n < 5; i++){
          if(!strcmp(config.ssids[i].ssid, (const char*)c.sta.ssid)) continue;
          out += String(config.ssids[i].ssid) + "\t" + String(config.ssids[i].password) + "\n";
          n++;
        }
        config.saveWifiList(out.c_str());
        config.setLastSSID(1);
        Serial.printf("мережу %s збережено першою, усього в списку %u\n", (const char*)c.sta.ssid, (unsigned)config.ssidsCount);
      }
    }
    else if(!strcmp(buf,"page")){
      extern char yoSlowWhat[40]; extern uint32_t yoSlowMs;
      extern uint32_t yoLoopN, yoLoopMax, yoLoopFrom;
      uint32_t secs = (millis() - yoLoopFrom) / 1000;
      Serial.printf("цикл: %u обертів за %u с (%u/с), найдовший проміжок %u мс\n",
                    (unsigned)yoLoopN, (unsigned)secs, (unsigned)(secs ? yoLoopN/secs : 0), (unsigned)yoLoopMax);
      yoLoopN = 0; yoLoopMax = 0; yoLoopFrom = millis();
      extern uint32_t yoDspN, yoDspMax, yoDspFrom, yoDspDraw, yoDspNet;
      uint32_t ds = (millis() - yoDspFrom) / 1000;
      Serial.printf("екран: %u кадрів за %u с (%u/с), найдовший проміжок %u мс; малювання %u мс, веб %u мс\n",
                    (unsigned)yoDspN, (unsigned)ds, (unsigned)(ds ? yoDspN/ds : 0),
                    (unsigned)yoDspMax, (unsigned)yoDspDraw, (unsigned)yoDspNet);
      extern uint32_t yoMenuMs, yoFadeMs, yoDspWhatMs; extern char yoDspWhat[24];
      Serial.printf("  з них: меню %u мс, наплив %u мс, плеєр: %s %u мс\n",
                    (unsigned)yoMenuMs, (unsigned)yoFadeMs,
                    yoDspWhat[0] ? yoDspWhat : "-", (unsigned)yoDspWhatMs);
      yoMenuMs = 0; yoFadeMs = 0; yoDspWhatMs = 0; yoDspWhat[0] = 0;
      yoDspN = 0; yoDspMax = 0; yoDspDraw = 0; yoDspNet = 0; yoDspFrom = millis();
      if(yoSlowMs) Serial.printf("найдовший крок циклу: %s %u мс\n", yoSlowWhat, (unsigned)yoSlowMs);
      else         Serial.println("довгих кроків циклу не було");
      yoSlowMs = 0; yoSlowWhat[0] = 0;
      Serial.printf("сторінка меню=%d активне=%d | режим екрана=%d\n",
        (int)yomenu.page(), yomenu.active()?1:0, (int)display.mode());
      Serial.printf("пошук: іде=%d знайдено=%u scanComplete=%d режим=%d\n",
        yomenu.scanning()?1:0, (unsigned)yomenu.scanCount(), (int)WiFi.scanComplete(), (int)WiFi.getMode());
      Serial.printf("мережа: статус=%d втрачено=%d спроби спинено=%d спроб=%u status()=%d ssid='%s'\n",
        (int)network.status, network.linkLost?1:0, network.staPaused?1:0,
        (unsigned)network.lostTries(), (int)WiFi.status(), WiFi.SSID().c_str());
    }
    else if(!strcmp(buf,"menu"))   yomenu.open();
    else if(!strcmp(buf,"mwifi"))  yomenu.openWifi(false);   /* без замка: це перевірка, а не режим точки доступу */
    else if(!strcmp(buf,"mclose")) yomenu.close();
    else if(!strcmp(buf,"mkbd"))   yomenu.openKbdTest();
    else if(!strcmp(buf,"kbtext")) Serial.printf("KBTEXT '%s'\n", yomenu.kbdTest());
    else if(!strncmp(buf,"tdown ",6)){ int x=0,y=0; if(sscanf(buf+6,"%d %d",&x,&y)==2){ touchscreen.injectBegin(x,y); touchscreen.loop(); Serial.println("TDOWN"); } }
    else if(!strncmp(buf,"tmove ",6)){ int x=0,y=0; if(sscanf(buf+6,"%d %d",&x,&y)==2){ touchscreen.injectMove(x,y); touchscreen.loop(); Serial.println("TMOVE"); } }
    else if(!strcmp(buf,"tup"))    { touchscreen.injectEnd(); touchscreen.loop(); Serial.println("TUP"); }
    else if(!strncmp(buf,"mpage ",6)) yomenu.openPage((int8_t)atoi(buf+6));
    else if(!strncmp(buf,"tap ",4)){
      /*  Імітація дотику: дозволяє перевірити меню без людини біля екрана. */
      char* sp = strchr(buf+4,' ');
      if(sp){ *sp=0; uint16_t x=atoi(buf+4), y=atoi(sp+1);
              Serial.printf("tap %d,%d\n", x, y); yomenu.onRelease(x,y); }
    }
    else if(!strncmp(buf,"list",4)) display.putRequest(NEWMODE, STATIONS);
    else if(!strncmp(buf,"drag ",5)){
      /*  drag <x> <y0> <y1> <мс> — веде «палець» по прямій і відпускає.
          Йде тим самим шляхом, що й справжній дотик.  */
      int x=0,y0=0,y1=0,ms=400;
      if(sscanf(buf+5, "%d %d %d %d", &x,&y0,&y1,&ms) >= 3){
        const int steps = ms/15 < 4 ? 4 : ms/15;
        Serial.printf("drag %d: %d -> %d за %d мс, %d кроків\n", x, y0, y1, ms, steps);
        touchscreen.injectBegin(x, y0);
        touchscreen.loop();
        for(int i=1;i<=steps;i++){
          delay(ms/steps);
          touchscreen.injectMove(x, y0 + (y1-y0)*i/steps);
          touchscreen.loop();
        }
        touchscreen.injectEnd();
        for(int i=0;i<120;i++){ delay(15); touchscreen.loop(); }   /* даємо накату доїхати */
        Serial.printf("після жесту: станція %d\n", display.currentPlItem);
      }
    }
    else if(!strncmp(buf,"stap ",5)){
      int x=0,y=0;
      if(sscanf(buf+5, "%d %d", &x,&y)==2){
        Serial.printf("дотик %d,%d\n", x, y);
        touchscreen.injectBegin(x,y); touchscreen.loop();
        delay(80); touchscreen.loop();
        touchscreen.injectEnd(); touchscreen.loop();
        delay(50); touchscreen.loop();
      }
    }
    /*  ---- доповнення: сон, будильник, ніч, батарея, світлодіод ---- */
    else if(!strcmp(buf,"ext")){
      ExtStore& s = extras.s;
      Serial.printf("сон: %u хв, лишилось %u с; будильник %s %02u:%02u %s, через %d хв, дзвонить=%d\n",
        extras.sleepMinutes(), (unsigned)extras.sleepLeftSec(), s.alarmOn?"увімк":"вимк",
        s.alarmH, s.alarmM, s.alarmDays?"будні":"щодня", (int)extras.alarmInMin(), extras.alarmRinging()?1:0);
      Serial.printf("ніч: %s %02u:%02u..%02u:%02u рівень %u, зараз ніч=%d темно=%d; підсвітка ціль %u\n",
        s.nightOn?"увімк":"вимк", s.nightFrom/2, (s.nightFrom%2)*30, s.nightTo/2, (s.nightTo%2)*30,
        s.nightLevel, extras.nightActive()?1:0, extras.dark()?1:0, extras.pwmTarget());
      Serial.printf("батарея %u мВ %d%% usb=%d; світлодіод %u; гучність %u override %d\n",
        extras.batMv(), extras.batPct(), extras.onUsb()?1:0, s.ledMode, config.store.volume, (int)player.volOverride);
    }
    else if(!strncmp(buf,"batsave ",8)){ extras.s.batSave = (uint8_t)atoi(buf+8) % 5; extras.changed(); Serial.printf("економія: %u\n", extras.s.batSave); }
    else if(!strncmp(buf,"batsim ",7)){
      int pc=-1,ch=-1; sscanf(buf+7, "%d %d", &pc, &ch);
      extras.batSim(pc, ch); Serial.printf("батарея: підставлено %d%%, заряджання %d\n", pc, ch);
    }
    else if(!strcmp(buf,"bat")){
      for(int i=0;i<8;i++){ Serial.printf("GPIO9: %u мВ (x2)\n", extras.batRaw()); delay(100); }
    }
    else if(!strncmp(buf,"sleeptest ",10)) { extras.sleepTestSec(atoi(buf+10)); Serial.println("таймер сну запущено"); }
    else if(!strncmp(buf,"sleep ",6))      { extras.setSleep(atoi(buf+6)); Serial.println("таймер сну"); }
    else if(!strcmp(buf,"alarmnow"))       { extras.alarmNow(); Serial.println("будильник запущено"); }
    else if(!strncmp(buf,"shold ",6)){
      /*  shold <x> <y> <мс> — утримати палець  */
      int x=0,y=0,ms=0;
      if(sscanf(buf+6, "%d %d %d", &x,&y,&ms)==3){
        touchscreen.injectBegin(x,y); touchscreen.loop();
        uint32_t t0 = millis();
        while(millis() - t0 < (uint32_t)ms){ delay(15); touchscreen.loop(); }
        touchscreen.injectEnd(); touchscreen.loop();
        delay(50); touchscreen.loop();
        Serial.printf("утримано %d,%d %d мс\n", x, y, ms);
      }
    }
    else if(!strcmp(buf,"rec"))            { if(recorder.active()) recorder.stop(); else if(!recorder.start()) Serial.printf("запис: %s\n", recorder.lastError()); }
    else if(!strcmp(buf,"recst"))          { Serial.printf("запис=%d %s %u байт %u с\n", recorder.active(), recorder.fileName(), (unsigned)recorder.bytes(), (unsigned)recorder.seconds()); }
    else if(!strcmp(buf,"serm"))           { sermons.fetch(); Serial.println("завантаження проповідей"); }
    else if(!strcmp(buf,"sermls")){
      for(uint16_t i=0;i<sermons.count();i++){ const Sermon* s=sermons.at(i); Serial.printf("%2u %s | %s | %s | %u\n", i, s->date, s->title, s->preacher, s->dur); }
      Serial.printf("усього %u, помилка '%s', вантажиться=%d\n", sermons.count(), sermons.error(), sermons.loading());
    }
    else if(!strncmp(buf,"night ",6))      { extras.forceNight(atoi(buf+6)); Serial.println("ніч примусово"); }
    else if(!strncmp(buf,"led ",4)){
      int r=0,g=0,b=0;
      if(sscanf(buf+4, "%d %d %d", &r,&g,&b)==3){ extras.ledTest(r,g,b,3000); Serial.println("світлодіод 3 с"); }
    }
    else if(!strcmp(buf,"sfps")){
      extern uint32_t yoPlRender, yoPlBlit;
      yoPlRender = yoPlBlit = 0;
      uint32_t el = display.plBenchmark(40);
      Serial.printf("з них складання рядків %.1f мс/кадр, вивід у дисплей %.1f мс/кадр\n",
                    yoPlRender/40000.0f, yoPlBlit/40000.0f);
      if(el) Serial.printf("плавна відмальовка: %u мс на 40 кадрів = %.1f мс/кадр = %.0f кадрів/с\n",
                           (unsigned)el, el/40.0f, 40000.0f/el);
      else Serial.println("sfps: недоступно");
    }
    else if(!strcmp(buf,"plstate")) touchscreen.dbgState();
    else if(!strcmp(buf,"fps")){
      /*  Скільки перемальовок списку встигає екран за секунду.  */
      if(display.mode()!=STATIONS){ Serial.println("fps: спершу відкрийте список"); }
      else{
        uint32_t t0=millis(); int n=0;
        while(millis()-t0 < 1000){ display.putRequest(DRAWPLAYLIST, display.currentPlItem); n++; delay(5); }
        Serial.printf("fps: запитів за секунду %d\n", n);
      }
    }
    else if(!strcmp(buf,"reboot")) ESP.restart();
    else Serial.printf("DBG: невідома команда '%s'\n", buf);
  }
}
#endif
