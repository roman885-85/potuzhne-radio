/*  Екран ходу оновлення з GitHub — поверх усього (див. m2update.cpp).  */
#ifndef m2update_h
#define m2update_h

namespace m2 {
bool otaViewActive();      /* іде встановлення — екран належить йому */
void otaViewRender();      /* задача дисплея */
bool otaViewEnded();       /* щойно закінчилось (не вийшло) — плеєр перемалювати */
}

#endif
