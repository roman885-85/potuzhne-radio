/*  =============================================================================
 *  yoRadio  ->  ES3C28P  /  ES3N28P
 *  ESP32-S3 2.8" IPS 240x320 ILI9341V + FT6336 (CTP) + ES8311 + FM8002E
 *  -----------------------------------------------------------------------------
 *  Плата: 16 MB Flash (QIO 80 MHz), 8 MB OPI PSRAM, USB-CDC (без UART-моста).
 *
 *  Распиновка взята из «ESP32-S3芯片IO资源分配表.xlsx» и штатных примеров
 *  производителя (Example_01 spi_dev.h, Example_16_music, Example_17_echo,
 *  Example_29_touch_pen, Example_05_show_SD_jpg_picture).
 *
 *  Свободные (выведенные на разъём) выводы: 2, 3, 14, 21, 43(TX0), 44(RX0).
 *  =============================================================================
 */
#ifndef myoptions_h
#define myoptions_h

/*  ----------  ДИСПЛЕЙ  ILI9341V 240x320 (работает в ландшафте 320x240)  ----- */
#define DSP_MODEL         DSP_ILI9341
#define TFT_CS            10        // LCD_CS
#define TFT_DC            46        // LCD_DC   (1 = data, 0 = command)
#define TFT_RST           -1        // сброс дисплея заведён на CHIP_PU
#define DSP_HSPI          false     // FSPI по умолчанию: SCLK 12, MOSI 11, MISO 13
#define BRIGHTNESS_PIN    45        // подсветка, высокий уровень = включена
#define DSP_INVERT_TITLE  false
/*  Панель IPS: в фирменной инициализации стоит команда 0x21 (инверсия ВКЛ).
    Без этого картинка идёт негативом.                                       */
#define DSP_INVERT_BASE   true

/*  ----------  ШИНА I2C (общая: FT6336 + ES8311)  --------------------------- */
#define I2C_SDA           16
#define I2C_SCL           15

/*  ----------  СЕНСОРНЫЙ ЭКРАН  FT6336  ------------------------------------- */
#define TS_MODEL          TS_MODEL_FT6336
#define TS_SDA            16
#define TS_SCL            15
#define TS_INT            17
#define TS_RST            18

/*  ----------  ЗВУК: I2S -> ES8311 -> FM8002E -> динамик  ------------------- */
#define I2S_DOUT          8         // ESP32 -> кодек (DSDIN)
#define I2S_BCLK          5         // SCLK
#define I2S_LRC           7         // LRCK / WS
#define I2S_MCLK          4         // MCLK, обязателен для ES8311
#define I2S_DIN           6         // кодек -> ESP32 (ASDOUT): вбудований мікрофон
#define I2S_ES8311        true      // включить драйвер кодека
#define ES8311_ADDR       0x18      // вывод CE кодека притянут к земле
#define ES8311_VOLUME     75        // 75 = 0 dB. Выше 75 — цифровое усиление

/*  Разрешение усилителя FM8002E: GPIO1, НИЗКИЙ уровень = усилитель включён.
 *  yoRadio держит на MUTE_PIN значение MUTE_VAL, пока плеер остановлен.        */
#define MUTE_PIN          1
#define MUTE_VAL          HIGH
#define MUTE_LOCK         false

/*  ----------  SD-КАРТА (SDIO 4 бит, не SPI)  ------------------------------- */
#define SD_SDMMC          true
#define SDMMC_CLK         38
#define SDMMC_CMD         40
#define SDMMC_D0          39
#define SDMMC_D1          41
#define SDMMC_D2          48
#define SDMMC_D3          47

/*  ----------  КНОПКА  ------------------------------------------------------ */
/*  GPIO0 — это кнопка BOOT. Коротко — пуск/стоп, долго — переход в список.
 *  Во время включения питания её держать нельзя: плата уйдёт в режим прошивки. */
#define BTN_CENTER        0
#define BTN_INTERNALPULLUP true

/*  ----------  СВЕТОДИОД  --------------------------------------------------- */
/*  На плате только адресный WS2812 (GPIO42), обычным digitalWrite не мигает,
 *  а LED_BUILTIN у «ESP32S3 Dev Module» = 48 и совпал бы с SD_D2.              */
#define USE_BUILTIN_LED   false
#define LED_BUILTIN_S3    255

/*  ----------  ПРОЧЕЕ  ------------------------------------------------------ */
#define L10N_LANGUAGE     RU
#define ROTATE_90         false
#define SD_AUTOPLAY       true
#define VS1053_CS         255
#define WATCHDOG_INTERVAL 8

/*  Діагностика через послідовний порт: знімок екрана, керування плеєром,
    стан картки. Потрібна, поки пристрій перевіряється без людини поруч.   */
#define YO_DEBUG          true

/*  ----------  НАЗВА ПРИСТРОЮ  --------------------------------------------- */
/*  SSID і mDNS лишаємо латиницею: кирилиця в іменах мереж і hostname
    підтримується не всюди й ламає доступ з частини пристроїв.               */

#endif
