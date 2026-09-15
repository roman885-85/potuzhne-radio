#include "yoHang.h"
#include <Preferences.h>
#include "esp_attr.h"
#include "esp_system.h"
#include "esp_core_dump.h"
#include "../core/options.h"
#include "../core/network.h"
#include "../core/player.h"

volatile uint32_t yoHbLoop = 0;
volatile uint32_t yoHbDsp = 0;
volatile uint32_t yoHangTestMs = 0;

#ifdef YO_DEBUG
extern char yoSlowWhat[40];
extern char yoDspWhat[24];
#endif

namespace {
  const uint32_t MAGIC = 0x48414E47;       /* «HANG» */
  const uint32_t LIMIT_MS = 60000;         /* довше за будь-яке законне очікування (Wi-Fi, картка, TLS) */

  /*  Переживає перезапуск після падіння (не вимкнення живлення).  */
  struct Rec { uint32_t magic; uint32_t up; uint32_t epoch; char what[72]; };
  RTC_NOINIT_ATTR Rec s_rec;

  char     s_last[72] = {0};
  uint32_t s_lastAt = 0;
  bool     s_thisBoot = false;
  volatile uint32_t s_pauseUntil = 0;

  void task(void*){
    uint32_t lastL = yoHbLoop, lastD = yoHbDsp;
    uint32_t tL = millis(), tD = tL;
    for(;;){
      vTaskDelay(pdMS_TO_TICKS(1000));
      const uint32_t now = millis();
      if(yoHbLoop != lastL){ lastL = yoHbLoop; tL = now; }
      if(yoHbDsp != lastD){ lastD = yoHbDsp; tD = now; }
      if((int32_t)(s_pauseUntil - now) > 0){ tL = tD = now; continue; }
      const bool dsp = now - tD > LIMIT_MS, loop = now - tL > LIMIT_MS;
      if(!dsp && !loop) continue;
      s_rec.magic = MAGIC;
      s_rec.up = now / 1000;
      { time_t t = time(nullptr); s_rec.epoch = t > 1600000000 ? (uint32_t)t : 0; }
#ifdef YO_DEBUG
      snprintf(s_rec.what, sizeof(s_rec.what), "%s%s%s (%s)", dsp ? "екран" : "", dsp && loop ? " і " : "", loop ? "головний цикл" : "",
               dsp ? (yoDspWhat[0] ? yoDspWhat : "-") : (yoSlowWhat[0] ? yoSlowWhat : "-"));
#else
      snprintf(s_rec.what, sizeof(s_rec.what), "%s%s%s", dsp ? "екран" : "", dsp && loop ? " і " : "", loop ? "головний цикл" : "");
#endif
      Serial.printf("##HANG#\tстоїть %s %u с, грає=%d — зберігаю дамп усіх задач і перезапускаю радіо\n",
                    s_rec.what, (unsigned)((now - (dsp ? tD : tL)) / 1000), player.isRunning() ? 1 : 0);
      Serial.flush();
      vTaskDelay(pdMS_TO_TICKS(50));
      abort();                                   /* дамп у флеш + перезапуск */
    }
  }
}

namespace YoHang {
  void boot(){
    Preferences p;
    if(esp_reset_reason() == ESP_RST_PANIC && s_rec.magic == MAGIC){
      s_rec.what[sizeof(s_rec.what) - 1] = 0;
      strlcpy(s_last, s_rec.what, sizeof(s_last));
      s_lastAt = s_rec.epoch;
      s_thisBoot = true;
      if(p.begin("yoHang", false)){ p.putString("what", s_last); p.putULong("at", s_lastAt); p.end(); }
      Serial.printf("##HANG#\tпопередній запуск завис: %s (на %u-й секунді роботи)\n", s_last, (unsigned)s_rec.up);
    }else if(p.begin("yoHang", true)){
      p.getString("what", s_last, sizeof(s_last));
      s_lastAt = p.getULong("at", 0);
      p.end();
    }
    s_rec.magic = 0;
  }

  void begin(){
    xTaskCreatePinnedToCore(task, "hangWd", 3072, nullptr, 10, nullptr, 0);
  }

  void pause(uint32_t ms){ s_pauseUntil = millis() + ms; }
  const char* last(){ return s_last; }
  uint32_t lastAt(){ return s_lastAt; }
  bool thisBoot(){ return s_thisBoot; }

  void clear(){
    s_last[0] = 0; s_lastAt = 0; s_thisBoot = false;
    Preferences p;
    if(p.begin("yoHang", false)){ p.clear(); p.end(); }
    esp_core_dump_image_erase();
  }
}
