/*  Один порт I2S на двох: звук пише в кодек, мікрофон читає з кодека.
    Перенастроювання драйвера (частота нового потоку, перевстановлення)
    посеред i2s_read мікрофона роняло радіо: читання брало вказівник буфера,
    який драйвер щойно обнулив (дамп: YoMic::_run → i2s_read → memcpy(NULL)).
    Часті «грати/стоп» — часті зміни частоти, тож і падіння.
    Тепер і перенастроювання, і читання беруть цей замок.  */
#ifndef yoI2sLock_h
#define yoI2sLock_h
#include <Arduino.h>

inline SemaphoreHandle_t yoI2sMux(){
  static SemaphoreHandle_t m = nullptr;
  if(!m){
    SemaphoreHandle_t n = xSemaphoreCreateRecursiveMutex();
    static portMUX_TYPE mx = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&mx);
    if(!m){ m = n; n = nullptr; }
    portEXIT_CRITICAL(&mx);
    if(n) vSemaphoreDelete(n);
  }
  return m;
}

struct YoI2sGuard {
  bool ok;
  explicit YoI2sGuard(TickType_t wait = portMAX_DELAY){ SemaphoreHandle_t m = yoI2sMux(); ok = m && xSemaphoreTakeRecursive(m, wait) == pdTRUE; }
  ~YoI2sGuard(){ if(ok) xSemaphoreGiveRecursive(yoI2sMux()); }
};

#endif
