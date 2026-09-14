/*  Нове меню: кольори, розміри, шрифти.  */
#ifndef m2theme_h
#define m2theme_h
#include <Arduino.h>
#include "../displays/fonts/aa/m2Title.h"
#include "../displays/fonts/aa/m2Big.h"
#include "../displays/fonts/aa/m2Mid.h"
#include "../displays/fonts/aa/m2Row.h"
#include "../displays/fonts/aa/m2RowB.h"
#include "../displays/fonts/aa/m2Sm.h"
#include "../displays/fonts/aa/m2SmB.h"
#include "../displays/fonts/aa/m2Key.h"
#include "../displays/fonts/aa/aaUI6.h"
#include "../displays/fonts/aa/aaUI26b.h"

namespace m2 {

constexpr uint16_t RGB(uint8_t r, uint8_t g, uint8_t b){ return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

/*  Темна сіро-синя гама замість чорного тла; жовтий — фірмовий, як на заставці.  */
const uint16_t C_BG     = RGB(12, 15, 21);
const uint16_t C_BGTOP  = RGB(20, 25, 35);
const uint16_t C_SURF   = RGB(27, 32, 42);     /* картки */
const uint16_t C_SURF2  = RGB(42, 48, 61);     /* кнопки на картках, доріжки */
const uint16_t C_LINE   = RGB(46, 53, 66);     /* роздільники */
const uint16_t C_TXT    = RGB(242, 242, 236);
const uint16_t C_TXT2   = RGB(140, 148, 162);  /* другорядне */
const uint16_t C_TXT3   = RGB(92, 100, 114);   /* вимкнене */
const uint16_t C_ACC    = 0xE68B;              /* жовтий радіо (#E3D25F) */
const uint16_t C_ACCTXT = RGB(40, 36, 16);     /* текст на жовтому */
const uint16_t C_KNOB   = RGB(250, 250, 248);
const uint16_t C_ORANGE = RGB(255, 138, 61);
const uint16_t C_BLUE   = RGB(77, 163, 255);
const uint16_t C_TEAL   = RGB(52, 211, 160);
const uint16_t C_VIOLET = RGB(176, 124, 255);
const uint16_t C_RED    = RGB(255, 84, 84);
const uint16_t C_GREY   = RGB(120, 128, 142);
const uint16_t C_PINK   = RGB(255, 99, 160);
const uint16_t C_GREEN  = RGB(70, 200, 110);
const uint16_t C_REC    = RGB(235, 60, 60);

const int16_t SW = 320, SH = 240;
const int16_t HDR = 40;                        /* шапка */
const int16_t CH = SH - HDR;                   /* висота вмісту */
const int16_t MX = 10;                         /* поля карток */
const int16_t CWID = SW - 2 * MX;              /* ширина картки */
const uint8_t R_CARD = 12, R_BTN = 10, R_BADGE = 7;

#define F_TITLE (&m2Title)
#define F_BIG   (&m2Big)
#define F_MID   (&m2Mid)
#define F_ROW   (&m2Row)
#define F_ROWB  (&m2RowB)
#define F_SM    (&m2Sm)
#define F_SMB   (&m2SmB)
#define F_KEY   (&m2Key)
#define F_TINY  (&aaUI6)
#define F_POP   (&aaUI26b)

}  // namespace m2
#endif
