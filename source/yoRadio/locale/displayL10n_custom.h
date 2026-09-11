#ifndef dsp_full_loc
#define dsp_full_loc
#include <pgmspace.h>
/*************************************************************************************
    Українська локалізація для плати ES3C28P.
    Літери є, і, ї, ґ додані у шрифт yoRadio/fonts/glcdfont.c на їхні законні
    місця в CP1251 та навчений виводити їх utf8Rus() — штатний конвертер знав
    лише російську абетку й такі літери просто губив.
*************************************************************************************/
const char mon[] PROGMEM = "пн";
const char tue[] PROGMEM = "вт";
const char wed[] PROGMEM = "ср";
const char thu[] PROGMEM = "чт";
const char fri[] PROGMEM = "пт";
const char sat[] PROGMEM = "сб";
const char sun[] PROGMEM = "нд";

const char monf[] PROGMEM = "понеділок";
const char tuef[] PROGMEM = "вівторок";
const char wedf[] PROGMEM = "середа";
const char thuf[] PROGMEM = "четвер";
const char frif[] PROGMEM = "п'ятниця";
const char satf[] PROGMEM = "субота";
const char sunf[] PROGMEM = "неділя";

const char jan[] PROGMEM = "січня";
const char feb[] PROGMEM = "лютого";
const char mar[] PROGMEM = "березня";
const char apr[] PROGMEM = "квітня";
const char may[] PROGMEM = "травня";
const char jun[] PROGMEM = "червня";
const char jul[] PROGMEM = "липня";
const char aug[] PROGMEM = "серпня";
const char sep[] PROGMEM = "вересня";
const char octt[] PROGMEM = "жовтня";
const char nov[] PROGMEM = "листопада";
const char decc[] PROGMEM = "грудня";

const char wn_N[]      PROGMEM = "ПН";
const char wn_NNE[]    PROGMEM = "ПНПС";
const char wn_NE[]     PROGMEM = "ПНС";
const char wn_ENE[]    PROGMEM = "СПНС";
const char wn_E[]      PROGMEM = "СХ";
const char wn_ESE[]    PROGMEM = "СПДС";
const char wn_SE[]     PROGMEM = "ПДС";
const char wn_SSE[]    PROGMEM = "ПДПС";
const char wn_S[]      PROGMEM = "ПД";
const char wn_SSW[]    PROGMEM = "ПДПЗ";
const char wn_SW[]     PROGMEM = "ПДЗ";
const char wn_WSW[]    PROGMEM = "ЗПДЗ";
const char wn_W[]      PROGMEM = "ЗХ";
const char wn_WNW[]    PROGMEM = "ЗПНЗ";
const char wn_NW[]     PROGMEM = "ПНЗ";
const char wn_NNW[]    PROGMEM = "ПНПЗ";

const char* const dow[]     PROGMEM = { sun, mon, tue, wed, thu, fri, sat };
const char* const dowf[]    PROGMEM = { sunf, monf, tuef, wedf, thuf, frif, satf };
const char* const mnths[]   PROGMEM = { jan, feb, mar, apr, may, jun, jul, aug, sep, octt, nov, decc };
const char* const wind[]    PROGMEM = { wn_N, wn_NNE, wn_NE, wn_ENE, wn_E, wn_ESE, wn_SE, wn_SSE, wn_S, wn_SSW, wn_SW, wn_WSW, wn_W, wn_WNW, wn_NW, wn_NNW, wn_N };

const char    const_PlReady[]    PROGMEM = "[готовий]";
const char  const_PlStopped[]    PROGMEM = "[зупинено]";
const char  const_PlConnect[]    PROGMEM = "[з'єднання]";
const char  const_DlgVolume[]    PROGMEM = "ГУЧНІСТЬ";
const char    const_DlgLost[]    PROGMEM = "НЕМАЄ ЗВ'ЯЗКУ";
const char  const_DlgUpdate[]    PROGMEM = "ОНОВЛЕННЯ";
const char const_DlgNextion[]    PROGMEM = "NEXTION";
const char const_getWeather[]    PROGMEM = "";
const char  const_waitForSD[]    PROGMEM = "ІНДЕКС SD";

const char        apNameTxt[]    PROGMEM = "ТОЧКА ДОСТУПУ";
const char        apPassTxt[]    PROGMEM = "ПАРОЛЬ";
const char       bootstrFmt[]    PROGMEM = "З'єднуюся з %s";
const char        apSettFmt[]    PROGMEM = "НАЛАШТУВАННЯ: HTTP://%s/";
#if EXT_WEATHER
const char       weatherFmt[]    PROGMEM = "%s, %.1f\011C \007 відчувається: %.1f\011C \007 тиск: %d мм \007 вологість: %d%% \007 вітер: %.1f м/с [%s]";
#else
const char       weatherFmt[]    PROGMEM = "%s, %.1f\011C \007 тиск: %d мм \007 вологість: %d%%";
#endif
const char     weatherUnits[]    PROGMEM = "metric";
const char      weatherLang[]    PROGMEM = "ua";

#endif
