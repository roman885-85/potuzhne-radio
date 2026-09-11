/*************************************************************************************
    ILI9341 320x240 displays configuration file.
    Copy this file to yoRadio/src/displays/conf/displayILI9341conf_custom.h
    and modify it
    More info on https://github.com/e2002/yoradio/wiki/Widgets#widgets-description
*************************************************************************************/

#ifndef displayILI9341conf_h
#define displayILI9341conf_h

#define DSP_WIDTH       320
#define TFT_FRAMEWDT    8
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2

/*  Лаконічний плеєр: значок бітрейту й число гучності з головного екрана
    прибрано — бітрейт є на сторінці «інформація», а смуга гучності говорить
    сама за себе. IP лишається: без нього не знати, куди заходити з браузера.
    Рядки назви пісні тепер на всю ширину.  */
#define TITLE_FIX 0
#define HIDE_VOL
#define bootLogoTop     68

/* SROLLS  */                            /* {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
/*  Назва станції не доходить до правого краю: там шестерня входу в
    налаштування, як на сторінці плеєра Nextion.  */
/*  Шапка: ліворуч маленький значок джерела, далі назва, праворуч одна
    кнопка меню «☰» — усе інше живе в меню.  */
/*  Бігучі рядки — плавно, по колу й без зупинок (побажання власника):
    пауза на початку кола 0, крок 2 пікселі кожні 30 мс.  */
const ScrollConfig metaConf       PROGMEM = {{ 32, TFT_FRAMEWDT, 3, WA_LEFT }, 140, true, 238, 0, 2, 30 };
const ScrollConfig title1Conf     PROGMEM = {{ TFT_FRAMEWDT, 50, 2, WA_LEFT }, 140, true, MAX_WIDTH-TITLE_FIX, 0, 2, 30 };
const ScrollConfig title2Conf     PROGMEM = {{ TFT_FRAMEWDT, 70, 2, WA_LEFT }, 140, true, MAX_WIDTH-TITLE_FIX, 0, 2, 30 };
const ScrollConfig playlistConf   PROGMEM = {{ TFT_FRAMEWDT, 112, 2, WA_LEFT }, 140, true, MAX_WIDTH, 1000, 4, 30 };
const ScrollConfig apTitleConf    PROGMEM = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 3, WA_CENTER }, 140, false, MAX_WIDTH, 0, 4, 20 };
const ScrollConfig apSettConf     PROGMEM = {{ TFT_FRAMEWDT, 240-TFT_FRAMEWDT-16, 2, WA_LEFT }, 140, false, MAX_WIDTH, 0, 4, 30 };
/*  Ліворуч від погоди тепер стоїть значок 48x38 із проєкту Nextion, тому
    текст починається правіше, а сам значок трохи вище рядка.  */
/*  Погода читається, а не миготить: крок 1 піксель раз на 20 мс — це 50
    пікселів за секунду замість 133, і дві секунди паузи перед рухом, щоб
    початок рядка можна було прочитати нерухомим.  */
const ScrollConfig weatherConf    PROGMEM = {{ 122, 97, 2, WA_LEFT }, 140, true, MAX_WIDTH-122+TFT_FRAMEWDT, 2000, 1, 20 };
const WidgetConfig weatherIconConf PROGMEM = { 66, 84, 0, WA_LEFT };

/* BACKGROUNDS  */                       /* {{ left, top, fontsize, align }, width, height, outlined } */
const FillConfig   metaBGConf     PROGMEM = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 38, false };
const FillConfig   metaBGConfInv  PROGMEM = {{ 0, 38, 0, WA_LEFT }, DSP_WIDTH, 1, false };
/*  Смуга гучності за пропорціями Nextion: там вона 15 пікселів заввишки на
    панелі 400x240 і тягнеться майже на всю ширину. У перерахунку на 320x240
    це 14 пікселів; шкала 0..254, як в оригіналі (95 з 254 давало 37% заливки). */
const FillConfig   volbarConf     PROGMEM = {{ TFT_FRAMEWDT, 224, 0, WA_LEFT }, MAX_WIDTH, 14, true };
const FillConfig  playlBGConf     PROGMEM = {{ 0, 107, 0, WA_LEFT }, DSP_WIDTH, 24, false };
const FillConfig  heapbarConf     PROGMEM = {{ 0, 239, 0, WA_LEFT }, DSP_WIDTH, 1, false };

/* WIDGETS  */                           /* { left, top, fontsize, align } */
const WidgetConfig bootstrConf    PROGMEM = { 0, 182, 1, WA_CENTER };
const WidgetConfig bitrateConf    PROGMEM = { 70, 191, 1, WA_LEFT };
const WidgetConfig voltxtConf     PROGMEM = { 0, 214, 1, WA_CENTER };
const WidgetConfig  iptxtConf     PROGMEM = { TFT_FRAMEWDT, 214, 1, WA_LEFT };
const WidgetConfig   rssiConf     PROGMEM = { TFT_FRAMEWDT, 214-6, 2, WA_RIGHT };
const WidgetConfig numConf        PROGMEM = { 0, 120+30, 0, WA_CENTER };
const WidgetConfig apNameConf     PROGMEM = { TFT_FRAMEWDT, 66, 2, WA_CENTER };
const WidgetConfig apName2Conf    PROGMEM = { TFT_FRAMEWDT, 90, 2, WA_CENTER };
const WidgetConfig apPassConf     PROGMEM = { TFT_FRAMEWDT, 130, 2, WA_CENTER };
const WidgetConfig apPass2Conf    PROGMEM = { TFT_FRAMEWDT, 154, 2, WA_CENTER };
const WidgetConfig  clockConf     PROGMEM = { TFT_FRAMEWDT*2, 176, 0, WA_RIGHT };
const WidgetConfig vuConf         PROGMEM = { TFT_FRAMEWDT, 100, 1, WA_LEFT };

const WidgetConfig bootWdtConf    PROGMEM = { 0, 162, 1, WA_CENTER };
const ProgressConfig bootPrgConf  PROGMEM = { 90, 14, 4 };
const BitrateConfig fullbitrateConf PROGMEM = {{DSP_WIDTH-TFT_FRAMEWDT-34, 43, 2, WA_LEFT}, 42 };

/* BANDS  */                             /* { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
/*  Останнє число — швидкість спаду стовпчика за кадр. Було 2: при 19 кадрах
    на секунду падіння з максимуму займало дві з половиною секунди, і показ
    здавався млявим. 7 дає близько семисот мілісекунд.  */
const VUBandsConfig bandsConf     PROGMEM = { 24, 100, 4, 2, 10, 7 };

/* STRINGS  */
const char         numtxtFmt[]    PROGMEM = "%d";
const char           rssiFmt[]    PROGMEM = "WiFi %d";
const char          iptxtFmt[]    PROGMEM = "\010 %s";
const char         voltxtFmt[]    PROGMEM = "\023\025%d";
const char        bitrateFmt[]    PROGMEM = "%d kBs";

/* MOVES  */                             /* { left, top, width } */
const MoveConfig    clockMove     PROGMEM = { 0, 176, -1 };
const MoveConfig   weatherMove    PROGMEM = { 60, 97, MAX_WIDTH-52 };
const MoveConfig   weatherMoveVU  PROGMEM = { 122, 97, MAX_WIDTH-122+TFT_FRAMEWDT };
const MoveConfig   wIconMove      PROGMEM = { 8, 84, 0 };
const MoveConfig   wIconMoveVU    PROGMEM = { 66, 84, 0 };

#endif
