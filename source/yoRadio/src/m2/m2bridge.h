/*  Нове меню: доступ до того, що вже вміє старе меню (Wi-Fi) і решта радіо.
    Логіку мереж навмисно не переписано: пошук, спроби, збережений список —
    ті самі функції YoMenu, що вже перевірені на живих мережах.  */
#ifndef m2bridge_h
#define m2bridge_h
#include <Arduino.h>

namespace m2 {

struct WB {
  static bool        staUp();
  static const char* curSsid();
  static int8_t      rssi();
  static const char* ip();
  static void        scanStart();
  static bool        scanning();
  static uint8_t     scanCount();
  static const char* scanSsid(uint8_t i);
  static int8_t      scanRssi(uint8_t i);
  static bool        scanOpen(uint8_t i);
  static bool        isSaved(const char* ssid);
  static uint8_t     savedCount();
  static const char* savedSsid(uint8_t i);
  static void        savedForget(uint8_t i);
  static void        savedFirst(uint8_t i);
  /*  вибрали знайдену мережу: 1 — знайома, 2 — відкрита, 0 — потрібен пароль  */
  static uint8_t     pick(uint8_t i);
  static char*       ssidBuf();
  static char*       passBuf();
  static size_t      ssidCap();
  static size_t      passCap();
  static void        connect();           /* _wSsid/_wPass — спробувати зараз */
  static void        saveCurrent();       /* вийшло — першою в список */
  static void        forgetCurrent();     /* забути _wSsid */
  static void        pages(bool wifi, bool connecting);   /* спроби радіо спинено / підсумок тримається */
  static void        load();
  static bool        apLock();            /* точка доступу: виходу з меню нема, доки мережу не задано */
  static void        apUnlock();
};

}  // namespace m2
#endif
