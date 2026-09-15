#include "Arduino.h"
#include "options.h"
#include "../extras/yoHang.h"
#include "WiFi.h"
#include "time.h"
#include "config.h"
#include "display.h"
#include "../extras/yoMic.h"
#include "sdmanager.h"
#include "player.h"
#include "network.h"
#include "netserver.h"
#include "timekeeper.h"
#include "../pluginsManager/pluginsManager.h"
#include "../displays/dspcore.h"
#include "../displays/widgets/widgets.h"
#include "../displays/widgets/pages.h"
#include "../displays/tools/l10n.h"
#include "../menu/yoMenu.h"
#include "../menu/uicanvas.h"
#include "../m2/m2player.h"
#include "../m2/m2update.h"
#include "../m2/m2pages.h"
#include "../extras/yoExtras.h"
#include "../extras/yoRecorder.h"
#include "../extras/yoSplash.h"
#include "../extras/yoSfx.h"
#include "../extras/yoSermons.h"
#include "../extras/yoLogos.h"
#include <SPIFFS.h>
#include "../displays/fonts/yoUI9.h"
#include "../displays/fonts/yoUI9b.h"
#include "../displays/fonts/yoUI12b.h"
#include "../displays/fonts/yoUI11.h"
#include "../displays/fonts/yoDigi28.h"
#include "../displays/tools/utf8Rus.h"

Display display;
#ifdef USE_NEXTION
#include "../displays/nextion.h"
Nextion nextion;
#endif

#ifndef CORE_STACK_SIZE
  #define CORE_STACK_SIZE  1024*4
#endif
#ifndef DSP_TASK_PRIORITY
  #define DSP_TASK_PRIORITY  4
#endif
#ifndef DSP_TASK_CORE_ID
  #define DSP_TASK_CORE_ID  0
#endif
#ifndef DSP_TASK_DELAY
  #define DSP_TASK_DELAY pdMS_TO_TICKS(10) // cap for 50 fps
#endif

#define DSP_QUEUE_TICKS 0

#ifndef DSQ_SEND_DELAY
  //#define DSQ_SEND_DELAY portMAX_DELAY
  #define DSQ_SEND_DELAY  pdMS_TO_TICKS(200)
#endif

QueueHandle_t displayQueue;

#ifdef YO_DEBUG
uint32_t yoDspN = 0, yoDspMax = 0, yoDspFrom = 0, yoDspDraw = 0, yoDspNet = 0;
uint32_t yoMenuMs = 0, yoFadeMs = 0;   /* скільки триває саме меню й наплив */
char     yoDspWhat[24] = {0};          /* найдовший крок відмальовки плеєра */
uint32_t yoDspWhatMs = 0;
#define DSTEP(call) { uint32_t _s0 = millis(); call; uint32_t _sd = millis() - _s0; \
                      if(_sd > yoDspWhatMs){ yoDspWhatMs = _sd; strlcpy(yoDspWhat, #call, sizeof(yoDspWhat)); } }
#endif

static void loopDspTask(void * pvParameters){
  while(true){
    yoHbDsp++;                                   /* сторож зависань (extras/yoHang) */
    if(yoHangTestMs){ uint32_t ms = yoHangTestMs; yoHangTestMs = 0; Serial.printf("##HANG#\tперевірка: задача екрана спить %u с\n", (unsigned)(ms / 1000)); vTaskDelay(pdMS_TO_TICKS(ms)); }
  #ifndef DUMMYDISPLAY
    if(displayQueue==NULL) break;
#ifdef YO_DEBUG
    /*  Скільки встигає задача екрана й хто в ній довгий: саме тут і дотик
        «відстає від пальця», навіть коли головний цикл летить.  */
    {
      static uint32_t prev = 0;
      uint32_t t0 = millis();
      if(!yoDspFrom) yoDspFrom = t0;
      if(prev){ uint32_t d = t0 - prev; if(d > yoDspMax) yoDspMax = d; }
      prev = t0; yoDspN++;
      if(timekeeper.loop0()){
        display.loop();
        uint32_t t1 = millis(); if(t1 - t0 > yoDspDraw) yoDspDraw = t1 - t0;
      #ifndef NETSERVER_LOOP1
        netserver.loop();
        uint32_t t2 = millis(); if(t2 - t1 > yoDspNet) yoDspNet = t2 - t1;
      #endif
      }
    }
#else
    if(timekeeper.loop0()){
      display.loop();
    #ifndef NETSERVER_LOOP1
      netserver.loop();
    #endif
    }
#endif
  #else
    timekeeper.loop0();
    #ifndef NETSERVER_LOOP1
      netserver.loop();
    #endif
  #endif
  #if DSP_MODEL==DSP_ILI9341
    /*  Щойно вивели кадр (прокрутка, перехід, хвиля) — наступний без сну:
        10 мс між обертами самі по собі обмежували прокрутку до ~30 кадрів.
        Нічого не рухається — спимо, як і раніше.  */
    /*  Але зовсім не віддавати ядро не можна: прокрутка без пауз не пускала
        задачу простою, і сторож задач перезавантажував радіо (дамп: DspTask у
        Menu::_flush). Тож між кадрами — 2 мс, а раз на чверть секунди руху — 8.  */
    {
      static uint32_t seen = 0, busyT = 0;
      const uint32_t f = g_m2Frames, now = millis();
      if(f != seen){
        if(!busyT) busyT = now;
        if(now - busyT > 250){ vTaskDelay(pdMS_TO_TICKS(8)); busyT = now; }
        else vTaskDelay(pdMS_TO_TICKS(2));
      }else{ busyT = 0; vTaskDelay(DSP_TASK_DELAY); }
      seen = f;
    }
  #else
    vTaskDelay(DSP_TASK_DELAY);
  #endif
  }
  vTaskDelete( NULL );
}

void Display::_createDspTask(){
  xTaskCreatePinnedToCore(loopDspTask, "DspTask", CORE_STACK_SIZE,  NULL,  DSP_TASK_PRIORITY, NULL, DSP_TASK_CORE_ID);
}

#ifndef DUMMYDISPLAY
//============================================================================================================================
DspCore dsp;

Page *pages[] = { new Page(), new Page(), new Page(), new Page() };

#if !((DSP_MODEL==DSP_ST7735 && DTYPE==INITR_BLACKTAB) || DSP_MODEL==DSP_ST7789 || DSP_MODEL==DSP_ST7796 || DSP_MODEL==DSP_ILI9488 || DSP_MODEL==DSP_ILI9486 || DSP_MODEL==DSP_ILI9341 || DSP_MODEL==DSP_ILI9225)
  #undef  BITRATE_FULL
  #define BITRATE_FULL     false
#endif


void returnPlayer(){
  display.putRequest(NEWMODE, PLAYER);
}

Display::~Display() {
  delete _pager;
  delete _footer;
  delete _plwidget;
  delete _nums;
  delete _clock;
  delete _meta;
  delete _title1;
  delete _title2;
  delete _plcurrent;
}

void Display::init() {
  Serial.print("##[BOOT]#\tdisplay.init\t");
#ifdef USE_NEXTION
  nextion.begin();
#endif
#if LIGHT_SENSOR!=255
  analogSetAttenuation(ADC_0db);
#endif
  _bootStep = 0;
  dsp.initDisplay();
  displayQueue=NULL;
  displayQueue = xQueueCreate( 5, sizeof( requestParams_t ) );
  while(displayQueue==NULL){;}
  _createDspTask();
  while(!_bootStep==0) { delay(10); }
  //_pager.begin();
  //_bootScreen();
  _pager = new Pager();
  _footer = new Page();
  _plwidget = new PlayListWidget();
  _nums = new NumWidget();
  _clock = new ClockWidget();
  _meta = new ScrollWidget();
  _title1 = new ScrollWidget();
  _plcurrent = new ScrollWidget();
  Serial.println("done");
}

uint16_t Display::width(){ return dsp.width(); }
uint16_t Display::height(){ return dsp.height(); }
#if TIME_SIZE>19
  #if DSP_MODEL==DSP_SSD1322
    #define BOOT_PRG_COLOR    WHITE
    #define BOOT_TXT_COLOR    WHITE
    #define PINK              WHITE
  #elif DSP_MODEL==DSP_SSD1327
    #define BOOT_PRG_COLOR    0x07
    #define BOOT_TXT_COLOR    0x3f
    #define PINK              0x02
  #else
    #define BOOT_PRG_COLOR    0xE68B
    #define BOOT_TXT_COLOR    0xFFFF
    #define PINK              0xF97F
  #endif
#endif

void Display::_bootScreen(){
  _boot = new Page();
  _boot->addWidget(new ProgressWidget(bootWdtConf, bootPrgConf, BOOT_PRG_COLOR, 0));
  _bootstring = (TextWidget*) &_boot->addWidget(new TextWidget(bootstrConf, 50, true, BOOT_TXT_COLOR, 0));
  _pager->addPage(_boot);
  _pager->setPage(_boot, true);
  /*  анімована заставка з розділу ресурсів; немає — старий логотип  */
  if(!(extras.s.splashOff == 0 && !YoExtras::wokeForAlarm() && splash.begin())) dsp.drawLogo(bootLogoTop);
  _bootStep = 1;
}

void Display::_buildPager(){
  _meta->init("*", metaConf, config.theme.meta, config.theme.metabg);
  _title1->init("*", title1Conf, config.theme.title1, config.theme.background);
  _clock->init(clockConf, 0, 0);
  #if DSP_MODEL==DSP_NOKIA5110
    _plcurrent->init("*", playlistConf, 0, 1);
  #else
    _plcurrent->init("*", playlistConf, config.theme.plcurrent, config.theme.plcurrentbg);
  #endif
  _plwidget->init(_plcurrent);
  #if !defined(DSP_LCD)
    _plcurrent->moveTo({TFT_FRAMEWDT, (uint16_t)(_plwidget->currentTop()), (int16_t)playlistConf.width});
  #endif
  #ifndef HIDE_TITLE2
    _title2 = new ScrollWidget("*", title2Conf, config.theme.title2, config.theme.background);
  #endif
  #if !defined(DSP_LCD) && DSP_MODEL!=DSP_NOKIA5110
    _plbackground = new FillWidget(playlBGConf, config.theme.plcurrentfill);
    #if DSP_INVERT_TITLE || defined(DSP_OLED)
      _metabackground = new FillWidget(metaBGConf, config.theme.metafill);
    #else
      _metabackground = new FillWidget(metaBGConfInv, config.theme.metafill);
    #endif
  #endif
  #if DSP_MODEL==DSP_NOKIA5110
    _plbackground = new FillWidget(playlBGConf, 1);
    //_metabackground = new FillWidget(metaBGConf, 1);
  #endif
  #ifndef HIDE_VU
    _vuwidget = new VuWidget(vuConf, bandsConf, config.theme.vumax, config.theme.vumin, config.theme.background);
  #endif
  #ifndef HIDE_VOLBAR
    _volbar = new SliderWidget(volbarConf, config.theme.volbarin, config.theme.background, 254, config.theme.volbarout);
  #endif
  #ifndef HIDE_HEAPBAR
    _heapbar = new SliderWidget(heapbarConf, config.theme.buffer, config.theme.background, psramInit()?300000:1600 * config.store.abuff);
  #endif
  #ifndef HIDE_VOL
    _voltxt = new TextWidget(voltxtConf, 10, false, config.theme.vol, config.theme.background);
  #endif
  #ifndef HIDE_IP
    _volip = new TextWidget(iptxtConf, 30, false, config.theme.ip, config.theme.background);
  #endif
  #if !defined(HIDE_RSSI) && DSP_MODEL!=DSP_ILI9341
    _rssi = new TextWidget(rssiConf, 20, false, config.theme.rssi, config.theme.background);
  #else
    /*  На цьому екрані штатний індикатор не малювався (у шрифті нема
        його значків). Рівень Wi-Fi тепер у рядку стану — _statusBar().  */
    _rssi = nullptr;
  #endif
  _nums->init(numConf, 10, false, config.theme.digit, config.theme.background);
  #ifndef HIDE_WEATHER
    _weather = new ScrollWidget("\007", weatherConf, config.theme.weather, config.theme.background);
    _weathericon = new WeatherIconWidget();
    _weathericon->init(weatherIconConf, config.theme.weather, config.theme.background);
  #endif
  #if DSP_MODEL==DSP_ILI9341
    _sdctl = new SdCtlWidget();
    _sdctl->init({66, SD_Y, 0, WA_LEFT}, config.theme.weather, config.theme.background);
  #endif
  
  if(_volbar)   _footer->addWidget( _volbar);
  if(_voltxt)   _footer->addWidget( _voltxt);
  if(_volip)    _footer->addWidget( _volip);
  if(_rssi)     _footer->addWidget( _rssi);
  if(_heapbar)  _footer->addWidget( _heapbar);
  
  if(_metabackground) pages[PG_PLAYER]->addWidget( _metabackground);
  pages[PG_PLAYER]->addWidget(_meta);
  pages[PG_PLAYER]->addWidget(_title1);
  if(_title2) pages[PG_PLAYER]->addWidget(_title2);
#if DSP_MODEL!=DSP_ILI9341
  if(_weather) pages[PG_PLAYER]->addWidget(_weather);
#else
  /*  Бігучого рядка погоди на ILI9341 немає: як на плеєрі Nextion, погода
      стоїть нерухомим блоком (його малює _weathericon). Сам рядок лишається
      поза сторінкою — він ще потрібен веб-інтерфейсу й журналу.  */
#endif
  if(_weathericon) pages[PG_PLAYER]->addWidget(_weathericon);
  if(_sdctl)       pages[PG_PLAYER]->addWidget(_sdctl);
  #if BITRATE_FULL
    _fullbitrate = new BitrateWidget(fullbitrateConf, config.theme.bitrate, config.theme.background);
  #if DSP_MODEL!=DSP_ILI9341
    pages[PG_PLAYER]->addWidget( _fullbitrate);   /* на ILI9341 бітрейт — на сторінці «інформація» */
  #endif
  #else
    _bitrate = new TextWidget(bitrateConf, 30, false, config.theme.bitrate, config.theme.background);
    pages[PG_PLAYER]->addWidget( _bitrate);
  #endif
  if(_vuwidget) pages[PG_PLAYER]->addWidget( _vuwidget);
  pages[PG_PLAYER]->addWidget(_clock);
  pages[PG_SCREENSAVER]->addWidget(_clock);
  pages[PG_PLAYER]->addPage(_footer);

  if(_metabackground) pages[PG_DIALOG]->addWidget( _metabackground);   /* у новому вигляді не малюється (FillWidget::_draw) */
  pages[PG_DIALOG]->addWidget(_meta);
  pages[PG_DIALOG]->addWidget(_nums);
  
  #if !defined(DSP_LCD) && DSP_MODEL!=DSP_NOKIA5110
    pages[PG_DIALOG]->addPage(_footer);
  #endif
  #if !defined(DSP_LCD)
#if DSP_MODEL!=DSP_ILI9341
  /*  На ILI9341 смугу вибору й поточну назву малює сам список, як у Nextion.  */
  if(_plbackground) {
    pages[PG_PLAYLIST]->addWidget( _plbackground);
    _plbackground->setHeight(_plwidget->itemHeight());
    _plbackground->moveTo({0,(uint16_t)(_plwidget->currentTop()-playlistConf.widget.textsize*2), (int16_t)playlBGConf.width});
  }
  #endif
  pages[PG_PLAYLIST]->addWidget(_plcurrent);
#endif
  pages[PG_PLAYLIST]->addWidget(_plwidget);
  for(const auto& p: pages) _pager->addPage(p);
  _m2empty = new Page();
  _pager->addPage(_m2empty);
}

/*  Мережі немає. Власної точки доступу радіо більше не піднімає, тож і
    окремого «вікна підключення» з її назвою й паролем не треба: одразу
    список мереж, у ньому людина й вибирає свою.  */
#if DSP_MODEL==DSP_ILI9341
bool yoM2On(){ return m2::P.on(); }
#endif

void Display::_noNetScreen() {
  if(_boot){ _pager->removePage(_boot); _boot = nullptr; }
  dsp.fillScreen(config.theme.background);
#ifdef USE_YOMENU
  yomenu.openWifi(true);
#endif
}

/*  Дві іконки в правому куті шапки плеєра. У Nextion плейлист відкривався
 *  дотиком по назві станції, а шестерня була окремою кнопкою — тут обидва
 *  входи зроблено видимими, бо невидимий жест ніхто не знайде.  */
void Display::drawHeaderIcons(){
#if DSP_MODEL==DSP_ILI9341
  /*  Лаконічна шапка: ліворуч — звідки звук (значок, не кнопка), праворуч
      — одна кнопка меню. Список відкривається дотиком по назві, як і було;
      обране, проповіді, запис, джерело й налаштування — у меню.  */
  uint16_t fg = config.theme.meta, bg = config.theme.metabg;
  dsp.fillRect(0, 0, 30, 38, bg);
  dsp.fillRect(276, 0, 44, 38, bg);
  /*  Значок джерела — це й кнопка «радіо ↔ картка» (дотик у лівому куті
      шапки), тому він того ж кольору, що й «☰», а не приглушений.  */
  const uint16_t dim = fg;
  int16_t x = 6, y = 10;
  if(player.remoteStationName){
    /*  проповідь із сайту: хрест  */
    dsp.fillRect(x+7, y-1, 4, 20, dim);
    dsp.fillRect(x+1, y+4, 16, 4, dim);
  }else if(config.getMode() == PM_SDCARD){
    /*  картка пам'яті  */
    dsp.fillRect(x+2, y, 10, 18, dim);
    dsp.fillRect(x+12, y+4, 4, 14, dim);
    dsp.fillTriangle(x+12, y, x+12, y+4, x+16, y+4, dim);
    for(uint8_t i = 0; i < 3; i++) dsp.fillRect(x+4 + i*3, y+2, 2, 4, bg);
  }else{
    /*  радіо: щогла з хвилями  */
    dsp.fillRect(x+8, y+7, 3, 12, dim);
    dsp.fillCircle(x+9, y+6, 2, dim);
    for(int8_t r = 6; r <= 9; r += 3)
      for(int16_t a = -55; a <= 55; a += 8){
        float rad = a * 3.14159f / 180.0f;
        dsp.drawPixel(x+9 - (int16_t)(r * cosf(rad)), y+6 + (int16_t)(r * sinf(rad)), dim);
        dsp.drawPixel(x+9 + (int16_t)(r * cosf(rad)), y+6 + (int16_t)(r * sinf(rad)), dim);
      }
  }
  /*  меню «☰»  */
  for(uint8_t i = 0; i < 3; i++) dsp.fillRect(288, 10 + i*7, 22, 3, fg);
#endif
}

/*  Проповідь із сайту: під шапкою ліворуч обкладинка 80x45, праворуч
    проповідник і дата, нижче — той самий пульт, що й для картки: смуга
    перемотки й сусідні проповіді. Рядки назви пісні й погода на цей час
    ховаються — інакше знову був би перевантажений екран.  */
#define SMB_X   8
#define SMB_Y   39
#define SMB_TX  (SMB_X + COVER_W + 8)

void Display::_sermonLayout(){
#if DSP_MODEL==DSP_ILI9341
  /*  Вирішуємо лише на екрані плеєра: у списку станцій чи заставці
      перебудова плеєра намалювала б його поверх них.  */
  if(_bootStep != 2 || _mode != PLAYER || _locked) return;
  bool want = player.remoteStationName && sermons.playing() >= 0;
  if(want != _smLayout){
    _smLayout = want;
    if(want){
      if(_title1) _title1->lock(true);
      if(_title2) _title2->lock(true);
      _applySdLayout();                    /* ховає погоду й пульт */
      dsp.fillRect(SMB_X, SMB_Y, 320 - SMB_X, COVER_H, config.theme.background);
      _smShown = 0xFFFFFFFF;
    }else{
      if(_title1) _title1->unlock();
      if(_title2) _title2->unlock();
      dsp.fillRect(SMB_X, SMB_Y, 320 - SMB_X, COVER_H, config.theme.background);
      forceRedraw();
      return;
    }
  }
  if(_smLayout){ _sermonBlock(); _sermonMarquee(false); }
#endif
}

/*  Бігучий рядок імені проповідника: кадр складаємо в маленькому полотні
    й виводимо цілком — без мерехтіння, як і список.  */
void Display::_sermonMarquee(bool force){
#if DSP_MODEL==DSP_ILI9341
  if(!_smPrW) return;
  uint32_t now = millis();
  if(!force){
    if((int32_t)(now - _smPrT) < 0) return;
    _smPrT = now + 30;
    _smPrShift++;
    if(_smPrShift >= _smPrW + 30) _smPrShift = 0;                  /* по колу, без зупинки */
  }
  const int16_t W = 320 - SMB_TX - 6, H = 20;
  static GFXcanvas16* cv = nullptr;
  if(!cv){ cv = new GFXcanvas16(W, H); if(!cv || !cv->getBuffer()){ _smPrW = 0; return; } }
  cv->fillScreen(config.theme.background);
  cv->setFont(&yoUI9b); cv->setTextSize(1); cv->setTextColor(0xFFFF); cv->setTextWrap(false);
  cv->setCursor(-_smPrShift, 15); cv->print(_smPr);
  cv->setCursor(-_smPrShift + _smPrW + 30, 15); cv->print(_smPr);
  dsp.drawRGBBitmap(SMB_TX, SMB_Y, cv->getBuffer(), W, H);
#endif
}

void Display::_sermonBlock(){
#if DSP_MODEL==DSP_ILI9341
  uint32_t v = sermons.coverVersion() * 1024 + (uint32_t)(sermons.playing() + 1);
  if(v == _smShown) return;
  _smShown = v;
  const uint16_t bg = config.theme.background;
  const uint16_t* pix = sermons.coverPix();
  dsp.fillRect(SMB_X, SMB_Y, 320 - SMB_X, COVER_H, bg);   /* разом із щілиною між обкладинкою й текстом */
  if(pix){
    dsp.startWrite();
    dsp.setAddrWindow(SMB_X, SMB_Y, COVER_W, COVER_H);
    dsp.writePixels((uint16_t*)pix, (uint32_t)COVER_W * COVER_H);
    dsp.endWrite();
  }else{
    /*  поки обкладинка вантажиться — темна плашка з хрестом  */
    dsp.fillRect(SMB_X, SMB_Y, COVER_W, COVER_H, 0x2124);
    int16_t cx = SMB_X + COVER_W / 2, cy = SMB_Y + COVER_H / 2;
    dsp.fillRect(cx - 2, cy - 14, 5, 28, 0x4A69); dsp.fillRect(cx - 9, cy - 7, 19, 5, 0x4A69);
  }
  const Sermon* it = sermons.at(sermons.playing());
  if(!it) return;
  const int16_t W = 320 - SMB_TX - 6;
  char t[80], a[40];
  int16_t x1, y1; uint16_t tw, th;
  /*  проповідник — рядок, якщо не влазить — із «..»  */
  dsp.setFont(&yoUI9b); dsp.setTextSize(1); dsp.setTextColor(0xFFFF);
  snprintf(t, sizeof(t), "%s", utf8Rus(it->preacher, false));
  /*  Не влазить — бігучий рядок (див. _sermonMarquee), влазить — стоїть.  */
  {
    uint16_t w = 0;
    for(const char* q = t; *q; q++){ uint8_t c = (uint8_t)*q;
      if(c >= yoUI9b.first && c <= yoUI9b.last) w += pgm_read_byte(&yoUI9b.glyph[c - yoUI9b.first].xAdvance); }
    if(w > (uint16_t)W){ strlcpy(_smPr, t, sizeof(_smPr)); _smPrW = w; _smPrShift = 0; _smPrT = millis() + 600; }
    else _smPrW = 0;
  }
  (void)x1; (void)y1; (void)tw; (void)th;
  if(_smPrW) _sermonMarquee(true);
  else{ dsp.setCursor(SMB_TX, SMB_Y + 15); dsp.print(t); }
  dsp.setFont(&yoUI9b);
  /*  дата й тривалість  */
  dsp.setFont(&yoUI9); dsp.setTextColor(config.theme.date);
  snprintf(a, sizeof(a), "%u хв", (unsigned)((it->dur + 30) / 60));
  if(strlen(it->date) >= 10) snprintf(t, sizeof(t), "%.2s.%.2s.%.4s   %s", it->date + 8, it->date + 5, it->date, utf8Rus(a, false));
  else snprintf(t, sizeof(t), "%s", utf8Rus(a, false));
  dsp.setCursor(SMB_TX, SMB_Y + 37); dsp.print(t);
  dsp.setFont();
#endif
}

/*  Логотип станції: 45x45 ліворуч під шапкою, рядки пісні — правіше.
    Картинка — /logo/<crc32 адреси потоку>.jpg у SPIFFS (ім'я від адреси,
    а не від номера, щоб правка плейлиста їх не плутала). Якщо картинки
    нема — плитка з ініціалами на кольорі, щоб «обкладинка» була в кожної.  */
#define LG_X  8
#define LG_Y  41
#define LG_S  45
#define LG_TX 60

static uint32_t crc32str(const char* s){
  uint32_t c = 0xFFFFFFFF;
  for(; *s; s++){ c ^= (uint8_t)*s; for(uint8_t k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320 & (0 - (c & 1))); }
  return ~c;
}

void Display::_stationLogo(){
#if DSP_MODEL==DSP_ILI9341
  /*  До кінця _start() сторінку плеєра ще не збудовано: рядки назви пісні
      створені, але не ініціалізовані, і moveTo() на них валив плату.  */
  if(_bootStep != 2 || _mode != PLAYER || _locked) return;
  bool want = config.getMode() == PM_WEB && !player.remoteStationName && config.station.url[0];
  uint32_t crc = want ? crc32str(config.station.url) : 0;
  /*  у фоні знайшовся новий логотип — перечитати  */
  static uint32_t lv = 0;
  if(logos.version() != lv){ lv = logos.version(); _logoCrc = 1; }
  if(crc == _logoCrc) return;
  _logoCrc = crc;
  const uint16_t bg = config.theme.background;
  if(!want){
    if(_logoOn){
      _logoOn = false;
      dsp.fillRect(LG_X, LG_Y, LG_S, LG_S, bg);
      if(_title1) _title1->moveBack();
      if(_title2) _title2->moveBack();
    }
    return;
  }
  if(!_logoOn){
    _logoOn = true;
    int16_t w = DSP_WIDTH - TFT_FRAMEWDT - LG_TX;
    if(_title1) _title1->moveTo({LG_TX, title1Conf.widget.top, w});
    if(_title2) _title2->moveTo({LG_TX, title2Conf.widget.top, w});
  }
  bool ok = false;
  if(!_logoPix) _logoPix = (uint16_t*)ps_malloc(LG_S * LG_S * 2);
  char path[24];
  /*  готові пікселі, які знайшла й зберегла сама плата  */
  snprintf(path, sizeof(path), "/logo/%08x.565", (unsigned)crc);
  if(_logoPix && SPIFFS.exists(path)){
    File f = SPIFFS.open(path, "r");
    if(f && f.size() == LG_S * LG_S * 2) ok = f.read((uint8_t*)_logoPix, LG_S * LG_S * 2) == LG_S * LG_S * 2;
    if(f) f.close();
  }
  snprintf(path, sizeof(path), "/logo/%08x.jpg", (unsigned)crc);
  if(!ok && _logoPix && SPIFFS.exists(path)){
    File f = SPIFFS.open(path, "r");
    size_t n = f ? f.size() : 0;
    if(n > 100 && n < 32768){
      uint8_t* b = (uint8_t*)ps_malloc(n);
      if(b){ f.read(b, n); ok = yoJpegFit(b, n, _logoPix, LG_S, LG_S); free(b); }
    }
    if(f) f.close();
  }
  if(ok){
    dsp.startWrite(); dsp.setAddrWindow(LG_X, LG_Y, LG_S, LG_S);
    dsp.writePixels(_logoPix, (uint32_t)LG_S * LG_S); dsp.endWrite();
    return;
  }
  /*  Логотипа ще нема — просимо знайти в каталозі (у фоні), а поки плитка.
      Кілька станцій каталог віддає з чужими логотипами — їх не шукаємо.  */
  {
    static const uint32_t SKIP[] = { 0xb7d51e17, 0x6936f052, 0xec492392 };   /* НВ, Radio Gold, Наше */
    bool skip = false; for(uint32_t v : SKIP) if(v == crc) skip = true;
    snprintf(path, sizeof(path), "/logo/%08x.no", (unsigned)crc);
    if(!skip && !SPIFFS.exists(path)) logos.want(config.station.url, config.station.name);
  }
  /*  плитка з ініціалами  */
  static const uint16_t PAL[8] = { 0x3A8D, 0x5A4B, 0x2C6A, 0x6A28, 0x2B0F, 0x7A6C, 0x4B09, 0x31CC };
  dsp.fillRoundRect(LG_X, LG_Y, LG_S, LG_S, 6, PAL[crc & 7]);
  char ini[3] = {0}; uint8_t k = 0; bool word = true;
  const char* t = utf8Rus(config.station.name, true);
  for(; *t && k < 2; t++){
    uint8_t ch = (uint8_t)*t;
    bool letter = (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch >= 0xC0 || ch == 0xAA || ch == 0xAF || ch == 0xB2 || ch == 0xA5;
    if(letter && word){ ini[k++] = (char)ch; word = false; }
    else if(!letter) word = true;
  }
  dsp.setFont(&yoUI12b); dsp.setTextSize(1); dsp.setTextColor(0xFFFF);
  int16_t x1, y1; uint16_t tw, th;
  dsp.getTextBounds(ini, 0, 40, &x1, &y1, &tw, &th);
  dsp.setCursor(LG_X + (LG_S - (int16_t)tw) / 2 - x1, LG_Y + LG_S / 2 + 8); dsp.print(ini);
  dsp.setFont();
#endif
}

/*  ---------- обране на головному (варіант А) ----------
    Коли в «обраному» є хоч одна станція, великий годинник поступається
    місцем рядку з шести логотипів — дотик по логотипу вмикає станцію.
    Зверху лишаються компактний годинник, погода й дата. Лише для радіо:
    у картки й проповідей на цьому місці свій пульт.  */
static const VUBandsConfig fmBands = { 10, 38, 3, 2, 6, 7 };
static const WidgetConfig  fmVuConf = { 6, 84, 1, WA_LEFT };
#define FM_CLK_X   34
#define FM_CLK_Y   121                 /* базова лінія цифр (висота 35) */
#define FM_DATE_Y  141

void Display::_favMain(){
#if DSP_MODEL==DSP_ILI9341
  if(_bootStep != 2 || _mode != PLAYER || _locked) return;
  bool any = false;
  for(uint8_t i = 0; i < FAV_N; i++) if(extras.fav[i].url[0]) any = true;
  bool want = any && !extras.s.favHide && config.getMode() == PM_WEB && !_smLayout;
  if(want != _fmOn) _fmApply(want);
  if(!_fmOn) return;
  _fmClock(false);
  uint32_t now = millis();
  if(now - _fmSigT < 250 && _fmSig) return;
  _fmSigT = now;
  uint32_t sig = logos.version() * 131u + (uint32_t)(extras.favPlaying() + 2);
  for(uint8_t i = 0; i < FAV_N; i++) sig = sig * 31u + crc32str(extras.fav[i].url);
  if(!sig) sig = 1;
  if(sig != _fmSig){ _fmSig = sig; _fmRow(); }
#endif
}

void Display::_fmApply(bool on){
#if DSP_MODEL==DSP_ILI9341
  const uint16_t bg = config.theme.background;
  _fmOn = on;
  if(on){
    _clock->lock(true);                        /* чистить своє місце */
    dsp.fillRect(0, 123, DSP_WIDTH, 80, bg);   /* решта великого годинника й дати */
    if(_vuwidget) _vuwidget->reshape(fmVuConf, fmBands);
    _fmMin = -1; _fmSig = 0;
  }else{
    dsp.fillRect(0, 84, 158, 40, bg);          /* компактний годинник і покажчик */
    dsp.fillRect(0, 123, DSP_WIDTH, 80, bg);   /* дата й рядок обраного */
    if(_vuwidget) _vuwidget->reshape(vuConf, bandsConf);
    _clock->unlock();
  }
  _layoutChange(player.status() == PLAYING);
  /*  повністю: draw() малює лише те, що змінилось, і цифри з'явились би
      аж на наступній хвилині — видно було тільки секунди  */
  if(!on) _clock->setActive(true);
#endif
}

void Display::_fmClock(bool full){
#if DSP_MODEL==DSP_ILI9341
  const struct tm& t = network.timeinfo;
  if(t.tm_year < 100) return;                  /* часу ще немає */
  int16_t m = t.tm_hour * 60 + t.tm_min;
  const uint16_t bg = config.theme.background;
  dsp.setFont(&yoDigi28); dsp.setTextSize(1);
  if(full || m != _fmMin){
    char b[6]; snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min);
    /*  «88:88» блідим — сегменти, як у великого годинника; поверх — час  */
    dsp.setTextColor(config.theme.clockbg); dsp.setCursor(FM_CLK_X, FM_CLK_Y); dsp.print("88:88");
    dsp.setTextColor(config.theme.clock);   dsp.setCursor(FM_CLK_X, FM_CLK_Y); dsp.print(b);
    /*  дата: день тижня й число з місяцем  */
    dsp.fillRect(0, FM_DATE_Y - 13, DSP_WIDTH, 17, bg);
    dsp.setFont(&yoUI9); dsp.setTextColor(config.theme.date);
    char d[48];
    snprintf(d, sizeof(d), "%s, %d %s", LANG::dowf[t.tm_wday], t.tm_mday, LANG::mnths[t.tm_mon]);
    dsp.setCursor(8, FM_DATE_Y); dsp.print(utf8Rus(d, false));
    _fmMin = m; _fmSec = -1;
  }
  /*  двокрапка блимає раз на секунду  */
  if(t.tm_sec != _fmSec){
    _fmSec = t.tm_sec;
    dsp.setFont(&yoDigi28);
    dsp.setTextColor((t.tm_sec & 1) ? config.theme.clockbg : config.theme.clock);
    dsp.setCursor(FM_CLK_X + 2 * 27, FM_CLK_Y); dsp.print(":");
  }
  dsp.setFont();
#endif
}

void Display::_fmRow(){
#if DSP_MODEL==DSP_ILI9341
  static const uint16_t PAL[8] = { 0x3A8D, 0x5A4B, 0x2C6A, 0x6A28, 0x2B0F, 0x7A6C, 0x4B09, 0x31CC };
  const uint16_t bg = config.theme.background;
  if(!_fmPix) _fmPix = (uint16_t*)ps_malloc(6 * FM_TILE * FM_TILE * 2);
  if(logos.version() != _fmLogoVer){ _fmLogoVer = logos.version(); memset(_fmPixCrc, 0, sizeof(_fmPixCrc)); }
  int8_t on = extras.favPlaying();
  for(uint8_t i = 0; i < FAV_N; i++){
    int16_t x = FM_X(i), y = FM_TY;
    dsp.fillRect(x - 3, y - 3, FM_TILE + 6, FM_TILE + 6, bg);
    const FavItem& f = extras.fav[i];
    if(!f.url[0]){
      /*  порожня клітинка: пунктир і «+» — дотик відкриває «обране»  */
      for(int16_t k = 0; k < FM_TILE; k += 6){
        dsp.drawFastHLine(x + k, y, 3, 0x4208); dsp.drawFastHLine(x + k, y + FM_TILE - 1, 3, 0x4208);
        dsp.drawFastVLine(x, y + k, 3, 0x4208); dsp.drawFastVLine(x + FM_TILE - 1, y + k, 3, 0x4208);
      }
      dsp.fillRect(x + FM_TILE / 2 - 1, y + 14, 3, 17, 0x6B4D);
      dsp.fillRect(x + 14, y + FM_TILE / 2 - 1, 17, 3, 0x6B4D);
      continue;
    }
    uint32_t crc = crc32str(f.url);
    uint16_t* px = _fmPix ? _fmPix + i * FM_TILE * FM_TILE : nullptr;
    bool ok = px && _fmPixCrc[i] == crc;
    if(px && !ok){
      char path[24];
      snprintf(path, sizeof(path), "/logo/%08x.565", (unsigned)crc);
      if(SPIFFS.exists(path)){
        File fl = SPIFFS.open(path, "r");
        if(fl && fl.size() == FM_TILE * FM_TILE * 2) ok = fl.read((uint8_t*)px, FM_TILE * FM_TILE * 2) == FM_TILE * FM_TILE * 2;
        if(fl) fl.close();
      }
      snprintf(path, sizeof(path), "/logo/%08x.jpg", (unsigned)crc);
      if(!ok && SPIFFS.exists(path)){
        File fl = SPIFFS.open(path, "r");
        size_t n = fl ? fl.size() : 0;
        if(n > 100 && n < 32768){
          uint8_t* b = (uint8_t*)ps_malloc(n);
          if(b){ fl.read(b, n); ok = yoJpegFit(b, n, px, FM_TILE, FM_TILE); free(b); }
        }
        if(fl) fl.close();
      }
      if(ok) _fmPixCrc[i] = crc;
    }
    if(ok){
      dsp.startWrite(); dsp.setAddrWindow(x, y, FM_TILE, FM_TILE);
      dsp.writePixels(px, (uint32_t)FM_TILE * FM_TILE); dsp.endWrite();
    }else{
      /*  логотипа немає — плитка з ініціалами, як у самої станції  */
      dsp.fillRoundRect(x, y, FM_TILE, FM_TILE, 6, PAL[crc & 7]);
      char ini[3] = {0}; uint8_t k = 0; bool word = true;
      for(const char* t = utf8Rus(f.name, true); *t && k < 2; t++){
        uint8_t ch = (uint8_t)*t;
        bool letter = (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch >= 0xC0 || ch == 0xAA || ch == 0xAF || ch == 0xB2 || ch == 0xA5;
        if(letter && word){ ini[k++] = (char)ch; word = false; }
        else if(!letter) word = true;
      }
      dsp.setFont(&yoUI12b); dsp.setTextSize(1); dsp.setTextColor(0xFFFF);
      int16_t x1, y1; uint16_t tw, th;
      dsp.getTextBounds(ini, 0, 40, &x1, &y1, &tw, &th);
      dsp.setCursor(x + (FM_TILE - (int16_t)tw) / 2 - x1, y + FM_TILE / 2 + 8); dsp.print(ini);
      dsp.setFont();
    }
    if(on == (int8_t)i){                       /* що грає — у жовтій рамці */
      dsp.drawRect(x - 3, y - 3, FM_TILE + 6, FM_TILE + 6, config.theme.meta);
      dsp.drawRect(x - 2, y - 2, FM_TILE + 4, FM_TILE + 4, config.theme.meta);
    }
  }
#endif
}

/*  Батарея 20x10 із носиком. Колір заливки — за рівнем: зелений, жовтий,
    помаранчевий, червоний. Під час заряджання заливка біжить від рівня до
    повної; нижче 10% без зарядника — блимає червоним.  */
/*  Колір заряду тече від зеленого через жовтий до червоного: різкі
    «сходинки» кольору читались як помилка, а не як рівень.  */
static uint16_t lerp565(uint16_t a, uint16_t b, uint8_t t){
  int ar = (a>>11)&31, ag = (a>>5)&63, ab = a&31;
  int br = (b>>11)&31, bg = (b>>5)&63, bb = b&31;
  int r = ar + (br-ar)*t/255, g = ag + (bg-ag)*t/255, l = ab + (bb-ab)*t/255;
  return (uint16_t)((r<<11)|(g<<5)|l);
}

/*  Шкала кольорів, а не два кольори: червоний → жовтогарячий → жовтий →
    салатовий → зелений, і між сусідніми відтінок перетікає плавно.  */
static uint16_t batColor(uint8_t pct){
  static const uint16_t stop[6] = { 0xF800, 0xFA00, 0xFC80, 0xFFE0, 0x8FE0, 0x07E0 };
  if(pct > 100) pct = 100;
  uint8_t i = pct / 20;                     /* 0..5 */
  if(i >= 5) return stop[5];
  uint8_t t = (uint8_t)((pct - i * 20) * 255 / 20);
  return lerp565(stop[i], stop[i+1], t);
}

/*  Смуга заряду на 50 поділок: одна поділка — два відсотки, тож рівень видно
    точно, а не «десь між чвертями». Під час заряджання поверх рівня біжить
    світла хвиля до кінця смуги.  */
/*  Блискавка ліворуч від батарейки на час заряджання: так видно одразу,
    що батарея набирає, а не просто щось блимає.  */
static void chargeBolt(int16_t x, uint16_t c, uint16_t bg){
  const int16_t y = 210;
  dsp.fillRect(x, y, 8, 13, bg);
  dsp.fillTriangle(x+5, y,    x,   y+7,  x+4, y+7,  c);
  dsp.fillTriangle(x+3, y+6,  x+8, y+6,  x+3, y+12, c);
}
#define BAT_N   25                 /* поділок: по 4% — і смуга лишається схожою на батарейку */
#define BAT_W   (BAT_N + 4)        /* рамка */
static void batIcon(int16_t x, uint8_t pct, bool chg, bool low, uint8_t ph, uint16_t bg, bool full = false){
  const int16_t by = 212, H = 10;
  const uint16_t outl = 0x8410;
  uint16_t lc = batColor(pct);
  dsp.fillRect(x, by, BAT_W + 3, H, bg);
  dsp.drawRect(x, by, BAT_W, H, (low && ph) ? 0xF800 : outl);
  dsp.fillRect(x + BAT_W, by + 3, 2, 4, (low && ph) ? 0xF800 : outl);
  if(low && !ph) return;                                  /* блимання: порожня фаза */
  int16_t lvl = (int16_t)BAT_N * pct / 100;               /* 0..50 поділок */
  if(full){ lvl = BAT_N; lc = 0x07E0; }
  if(lvl < 1) lvl = 1;
  dsp.fillRect(x + 2, by + 2, lvl, H - 4, lc);
  if(chg && !full){
    /*  Заряджання — як у телефонах: від рівня поділки одна за одною
        доливаються до кінця, мить стоїть повна, і знову від рівня. Рух лише
        в один бік, до повної, — з блиманням розрядженої не сплутати.  */
    int16_t span = BAT_N - lvl;
    if(span > 0){
      int16_t g = (int16_t)((millis() / 90) % (span + 5));   /* +5 кадрів — пауза повною */
      if(g > span) g = span;
      if(g > 0) dsp.fillRect(x + 2 + lvl, by + 2, g, H - 4, lerp565(0x0000, lc, 170));
    }
  }
}

void Display::_lowBat(){
#if DSP_MODEL==DSP_ILI9341
  static uint32_t shownAt = 0, nextAt = 0;
  uint32_t now = millis();
  if(_bootStep != 2 || _mode != PLAYER || _locked){ shownAt = 0; return; }
  bool low = extras.lowBattery();
  if(shownAt){
    if(now - shownAt > 6000 || !low){
      shownAt = 0;
      dsp.fillRect(66, 84, 254, 38, config.theme.background);
      _applySdLayout();                      /* погода або пульт — назад */
    }
    return;
  }
  if(!low){ nextAt = 0; return; }
  if(nextAt && (int32_t)(now - nextAt) < 0) return;
  shownAt = now; nextAt = now + 60000UL;     /* повторюємо щохвилини (разом зі звуком) */
  if(_sdctl) _sdctl->lock(true);
  if(_weathericon) _weathericon->lock(true);
  dsp.fillRect(66, 84, 254, 38, config.theme.background);
  dsp.fillRoundRect(66, 85, 246, 36, 6, 0xA000);
  /*  порожня батарея з одним червоним «діленням»  */
  dsp.drawRect(76, 96, 22, 13, 0xFFFF); dsp.fillRect(98, 100, 3, 5, 0xFFFF);
  dsp.fillRect(78, 98, 3, 9, 0xF800);
  dsp.setFont(&yoUI9b); dsp.setTextSize(1); dsp.setTextColor(0xFFFF);
  char t[48];
  snprintf(t, sizeof(t), "%s", utf8Rus("Низький заряд", false));
  dsp.setCursor(110, 100); dsp.print(t);
  dsp.setFont(&yoUI9);
  snprintf(t, sizeof(t), "%s", utf8Rus("зарядіть пристрій", false));
  dsp.setCursor(110, 115); dsp.print(t);
  dsp.setFont();
#endif
}

/*  Рядок стану праворуч унизу плеєра: таймер сну, будильник, батарея,
    рівень Wi-Fi. Перемальовується лише тоді, коли щось із цього змінилось,
    — для цього «підпис» усього рядка складено в одне число.  */
void Display::_statusBar(){
#if DSP_MODEL==DSP_ILI9341
  if(_bootStep != 2 || _mode != PLAYER || _locked) return;
  uint32_t now = millis();
  bool chg = extras.charging(), low = extras.lowBattery();
  if(_sbSig != 0xFFFFFFFF && now - _sbTick < ((chg || low) ? 100 : 500)) return;
  _sbTick = now;
  uint8_t ph = chg ? (uint8_t)(now / 90) : (low ? (uint8_t)((now / 500) % 2) : 0);   /* заряджання: лише щоб знати, коли перемалювати */

  int rssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -127;
  uint8_t wl = rssi > -55 ? 4 : rssi > -65 ? 3 : rssi > -75 ? 2 : rssi > -85 ? 1 : 0;
  uint16_t mv = extras.batMv();
  bool bat = mv >= 2800 && !extras.s.noBat;   /* банки нема (чи вимкнено в «розробнику») — нема й значка */
  uint8_t pct = bat ? (uint8_t)extras.batPct() : 0;
  bool usb = extras.onUsb();
  bool alarm = extras.s.alarmOn;
  uint16_t sl = extras.sleepLeft();
  if(sl > 255) sl = 255;
  bool rec = recorder.active();
  uint16_t recMin = rec ? (uint16_t)(recorder.seconds() / 60) : 0;
  pct = (pct + 2) / 4 * 4;                 /* 25 поділок: крок 4% */
  usb = extras.charged();
  bool ear = mic.listening();              /* мікрофон слухає — значок завжди, це приватність */
  uint32_t sig = wl | (bat << 3) | (usb << 4) | (alarm << 5) | ((uint32_t)pct << 6) | ((uint32_t)sl << 13) | ((uint32_t)rec << 21) | ((uint32_t)low << 22) | ((uint32_t)(recMin & 0x7F) << 23) | ((uint32_t)ear << 30) | ((uint32_t)chg << 31);
  if(sig == _sbSig){
    /*  змінилась лише фаза анімації — перемальовуємо саму батарею  */
    if(bat && (chg || low) && ph != _sbPh && _sbBatX >= 0){ _sbPh = ph; batIcon(_sbBatX, pct, chg, low, ph, config.theme.background); }
    return;
  }
  _sbSig = sig; _sbPh = ph;

  /*  Лише значки, лише коли є що показати, і приглушеним кольором:
      це довідка, а не головне на екрані. Кольором — тільки те, що
      вимагає уваги: запис і розряджена батарея.  */
  const uint16_t bg = config.theme.background;
  const uint16_t fg = 0x8410, dim = 0x2945;
  dsp.fillRect(104, 205, 216, 19, bg);        /* лівіше — IP; з 205 — верхівки літер «REC» сягають вище */
  int16_t x = 312;
  /*  Wi-Fi: чотири стовпчики  */
  x -= 15;
  for(uint8_t i = 0; i < 4; i++){
    int16_t h = 3 + i * 2;
    dsp.fillRect(x + i * 4, 222 - h, 3, h, i < wl ? fg : dim);
  }
  /*  батарея  */
  _sbBatX = -1;
  if(bat){
    x -= BAT_W + 9;
    _sbBatX = x;
    batIcon(x, pct, chg, low, ph, bg, extras.charged());
    if(chg && !extras.charged()){ x -= 11; chargeBolt(x, 0xFFE0, bg); }
  }
  /*  мікрофон слухає: капсула на ніжці  */
  if(ear){
    x -= 13;
    dsp.fillRoundRect(x + 2, 209, 5, 9, 2, fg);
    dsp.drawFastVLine(x, 213, 4, fg); dsp.drawFastVLine(x + 8, 213, 4, fg);
    dsp.drawFastHLine(x + 1, 218, 7, fg);
    dsp.drawFastVLine(x + 4, 219, 2, fg);
    dsp.drawFastHLine(x + 2, 221, 5, fg);
  }
  /*  будильник: дзвіночок  */
  if(alarm){
    x -= 18;
    int16_t ay = 211;
    dsp.fillCircle(x + 5, ay + 5, 4, fg);
    dsp.fillRect(x + 1, ay + 5, 9, 4, fg);
    dsp.fillRect(x, ay + 8, 11, 2, fg);
    dsp.fillRect(x + 4, ay + 10, 3, 2, fg);
  }
  dsp.setFont(&yoUI9); dsp.setTextSize(1);
  /*  таймер сну: місяць і хвилини  */
  if(sl){
    char b[6]; snprintf(b, sizeof(b), "%u", sl);
    int16_t x1, y1; uint16_t tw, th;
    dsp.getTextBounds(b, 0, 40, &x1, &y1, &tw, &th);
    x -= 16 + tw;
    dsp.fillCircle(x + 5, 216, 5, fg);
    dsp.fillCircle(x + 8, 214, 4, bg);
    dsp.setTextColor(fg);
    dsp.setCursor(x + 13, 221); dsp.print(b);
  }
  /*  іде запис ефіру  */
  /*  іде запис: червона крапка й хвилини запису (без англійського REC)  */
  if(rec){
    char b[8]; snprintf(b, sizeof(b), "%u'", recMin);
    int16_t x1, y1; uint16_t tw, th;
    dsp.getTextBounds(b, 0, 40, &x1, &y1, &tw, &th);
    x -= 16 + tw;
    dsp.fillCircle(x + 4, 216, 4, 0xF800);
    dsp.setTextColor(0xF800);
    dsp.setCursor(x + 11, 221); dsp.print(b);
  }
  dsp.setFont();
#endif
}

/*  Малювання йде із задачі дотиків, тому на час жесту звичайна відмальовка
    замикається — інакше два ядра билися б за шину SPI.  */
void Display::plSmooth(float pos){
#if DSP_MODEL==DSP_ILI9341
  if(_plwidget) _plwidget->drawSmooth(pos);
#endif
}

uint32_t Display::plBenchmark(uint16_t frames){
#if DSP_MODEL==DSP_ILI9341
  if(!_plwidget) return 0;
  lock();
  float p = currentPlItem;
  uint32_t t0 = millis();
  for(uint16_t i=0;i<frames;i++){ p += 0.13f; _plwidget->drawSmooth(p); }
  uint32_t el = millis() - t0;
  unlock();
  forceRedraw();
  return el;
#else
  (void)frames; return 0;
#endif
}

/*  Дотик лише повідомляє нове положення. Малює задача дисплея — там само,
    де й решта інтерфейсу. Так немає ні замикання екрана, ні гонки за шину
    між ядрами, ні можливості лишити відмальовку мертвою.  */
void Display::plScrollTo(float pos){ _plScrollPos = pos; _plScroll = true; }

/*  Натиснута кнопка плейлиста лише запам'ятовується тут, у потоці дотику,
    а малюється в задачі дисплея — щоб шину не ділили два ядра.  */
void Display::plButton(int8_t idx, bool on){ _plBtnIdx = idx; _plBtnOn = on; _plBtnDirty = true; }

/*  Кінець прокрутки передаємо прапорцем, а не через чергу. Під час прокрутки
    задача дисплея після кожного кадру спорожняє чергу, і запит, що прийшов
    посеред кадру, майже завжди губився: останній кадр лишався зсунутим на
    частку рядка, а головне — не продовжувався тридцятисекундний таймер, і
    список сам закривався, хоч ним і користувались.  */
void Display::plScrollStop(float finalPos){
  currentPlItem = (uint16_t)(finalPos < 1 ? 1 : finalPos);
  _plScrollPos = finalPos;
  _plFinal = true;
  _plScroll = false;
}

void Display::plSettle(uint16_t item){
  currentPlItem = item;
  putRequest(DRAWPLAYLIST, item);
}

uint16_t Display::volbarShown(){
#if DSP_MODEL==DSP_ILI9341
  return _volbar ? _volbar->shown() : 0;
#else
  return 0;
#endif
}

/*  Плавна зміна екранів — тим самим способом, що й у меню: гасимо підсвітку,
    міняємо картинку в темряві, засвічуємо назад. Перемальовки не видно, тому
    перехід виглядає суцільним, а не ривком.  */
void Display::_beginFade(displayMode_e to){
  if(_fadeStep >= 0){ _swichMode(to); return; }   /* зміна вже триває */
  _fadeTo = to; _fadeStep = 0; _fadeTick = 0;
}

void Display::fadeLoop(){
  if(_fadeStep < 0) return;
  uint32_t now = millis();
  if(now - _fadeTick < 12) return;
  _fadeTick = now;
  const int8_t STEPS = 8;
#if BRIGHTNESS_PIN!=255
  /*  «Повна» — та, що має бути зараз: уночі нічна, а не денна.  */
  uint16_t full = extras.pwmTarget();
#endif
  if(_fadeStep < STEPS){
#if BRIGHTNESS_PIN!=255
    if(config.store.dspon) analogWrite(BRIGHTNESS_PIN, full - full*(_fadeStep+1)/STEPS);
#endif
    if(++_fadeStep == STEPS) _swichMode(_fadeTo);   /* міняємо у темряві */
  }else{
    int8_t k = _fadeStep - STEPS + 1;
#if BRIGHTNESS_PIN!=255
    if(config.store.dspon) analogWrite(BRIGHTNESS_PIN, full*k/STEPS);
#endif
    if(++_fadeStep >= 2*STEPS){
      _fadeStep = -1;
#if BRIGHTNESS_PIN!=255
      if(config.store.dspon) extras.pwmSet(full);
#endif
    }
  }
}

void Display::dbgWeather(){
  if(_weather) Serial.printf("текст погоди: активний=%d замкнений=%d x=%d y=%d ширина=%u\n",
    _weather->active()?1:0, _weather->locked()?1:0,
    (int)_weather->left(), (int)_weather->top(), (unsigned)_weather->widthOf());
  else Serial.println("текст погоди: віджета немає");
  if(_weathericon) Serial.printf("значок погоди: активний=%d замкнений=%d x=%d y=%d\n",
    _weathericon->active()?1:0, _weathericon->locked()?1:0,
    (int)_weathericon->left(), (int)_weathericon->top());
  else Serial.println("значок погоди: віджета немає");
  Serial.printf("режим=%d показ погоди=%d\n", (int)_mode, config.store.showweather?1:0);
}

/*  Коли грає картка, на місці погоди стоїть пульт; на радіо — погода.
    Спершу замикаємо те, що зникає (воно стирає своє місце), потім
    відмикаємо й малюємо те, що з'являється.  */
void Display::_applySdLayout(){
#if DSP_MODEL==DSP_ILI9341
  if(m2::P.shown()) return;              /* новий головний екран: старих віджетів не вмикаємо */
#endif
  /*  Поки грає проповідь, смугу займає її обкладинка.  */
  if(_smLayout){
    if(_weather)     _weather->lock(true);
    if(_weathericon) _weathericon->lock(true);
    if(_sdctl){ _sdctl->unlock(); _sdctl->setActive(true); }   /* пульт перемотки проповіді */
    _smShown = 0xFFFFFFFF;
    return;
  }
  bool sd = (config.getMode() == PM_SDCARD);
  bool w  = config.store.showweather && !sd;
  if(!w){
    if(_weather)     _weather->lock(true);
    if(_weathericon) _weathericon->lock(true);
  }
  if(_sdctl){
    if(sd){ _sdctl->unlock(); _sdctl->setActive(true); }
    else  _sdctl->lock(true);
  }
  if(w){
    if(_weather)     _weather->lock(false);
    if(_weathericon){ _weathericon->unlock(); _weathericon->setActive(true); }
  }
}

void Display::sdPress(int8_t which, bool on){ if(_sdctl) _sdctl->press(which, on); }
void Display::sdPreview(float f){ if(_sdctl) _sdctl->preview(f); }

/*  IP на екрані можна сховати на сторінці «розробник»  */
void Display::_applyDev(){
  if(!_volip) return;
  if(extras.s.noIp) _volip->lock(true); else _volip->unlock();
}

void Display::forceRedraw(){
  if (network.status != CONNECTED && network.status != SDREADY){ _noNetScreen(); return; }
  if (!_ensurePlayer()) return;
#if DSP_MODEL==DSP_ILI9341
  if(m2::P.on()){
    /*  новий головний екран: старі віджети плеєра не показуються зовсім  */
    _mode = PLAYER;
    _pager->setPageKeep(_m2empty);
    m2::P.show();
    m2::P.render();                    /* увесь кадр одразу — поки не засвітилась підсвітка */
    return;
  }
  m2::P.hide();
#endif
  _applyDev();
  _mode = PLAYER;
  _pager->setPage( pages[PG_PLAYER]);
  _volume();
  _station();
  _time(false);
  drawHeaderIcons();
  _applySdLayout();
  _smShown = 0xFFFFFFFF;               /* обкладинку проповіді — наново */
  _sbSig = 0xFFFFFFFF;                 /* і рядок стану: після меню він зникав */
  _logoCrc = 1;                        /* і логотип станції */
  _fmMin = -1; _fmSig = 0;             /* і рядок обраного з компактним годинником */
  /*  Поки було відкрите меню, черга екрана спорожнювалась — разом із
      «почалось відтворення». Звіряємо покажчик рівня з тим, що є насправді:
      інакше після запуску проповіді чи станції з меню він не з'являвся.  */
  _layoutChange(player.status() == PLAYING);
}

void Display::_start() {
  splash.stop();
  if(_boot) _pager->removePage(_boot);
  #ifdef USE_NEXTION
    nextion.wake();
  #endif
  if (network.status != CONNECTED && network.status != SDREADY) {
    _noNetScreen();
    _bootStep = 2;
    return;
  }
  #ifdef USE_NEXTION
    //nextion.putcmd("page player");
    nextion.start();
  #endif
  _finishStart(true);
}

/*  Стартували без мережі — тоді замість плеєра лише список мереж, а сторінки й
    віджети плеєра не створювались зовсім. Мережа з'явилась (вибрали в меню чи
    радіо саме повернулось у збережену) — і перша ж перемальовка плеєра писала
    в невиділений буфер назви станції: радіо падало одразу після підключення.
    Тепер плеєр добудовується тут, у задачі екрана, перед будь-яким малюванням.  */
bool Display::_ensurePlayer(){
  if(_playerBuilt) return true;
  if(_bootStep != 2 || (network.status != CONNECTED && network.status != SDREADY)) return false;
  bool menu = false;
#ifdef USE_YOMENU
  menu = yomenu.active();
#endif
  Serial.println("##DSP#\tмережа з'явилась після старту без неї — добудовую плеєр");
  _finishStart(!menu);                 /* меню відкрите — намалюється, коли воно закриється */
  return true;
}

void Display::_finishStart(bool draw){
  _buildPager();
  _playerBuilt = true;
  _mode = PLAYER;
  config.setTitle(LANG::const_PlReady);
  
  if(_heapbar)  _heapbar->lock(!config.store.audioinfo);
  
  if(draw) _applySdLayout();
  if(_weather && config.store.showweather)  _weather->setText(LANG::const_getWeather);

  if(_vuwidget) _vuwidget->lock();
  if(_rssi && draw) _setRSSI(WiFi.RSSI());
  #ifndef HIDE_IP
    if(_volip) _volip->setText(config.ipToStr(WiFi.localIP()), iptxtFmt);
  #endif
#if DSP_MODEL==DSP_ILI9341
  if(draw && m2::P.on()){
    /*  новий головний екран — одразу, без старих віджетів  */
    _pager->setPageKeep(_m2empty);
    _station();
    m2::P.show();
    m2::P.render();
  }else
#endif
  if(draw){
    _pager->setPage( pages[PG_PLAYER]);
    _volume();
    _station();
    _time(false);
    drawHeaderIcons();      /* при завантаженні сторінка ставиться тут, а не через _swichMode */
  }
  _bootStep = 2;
  if(_lostPending){ _lostPending = false; if(network.linkLost && WiFi.status() != WL_CONNECTED) putRequest(NEWMODE, LOST); }
  pm.on_display_player();
}

void Display::_showDialog(const char *title){
  dsp.setScrollId(NULL);
  _pager->setPage( pages[PG_DIALOG]);
  #ifdef META_MOVE
    _meta->moveTo(metaMove);
  #endif
  _meta->setAlign(WA_CENTER);
  _meta->setText(title);
}

void Display::_swichMode(displayMode_e newmode) {
  #ifdef USE_NEXTION
    //nextion.swichMode(newmode);
    nextion.putRequest({NEWMODE, newmode});
  #endif
#if DSP_MODEL==DSP_ILI9341
  /*  Новий вигляд не показує старих діалогів yoRadio (гучність, сон, номер станції,
      «налаштування»): лишається головний екран — усе це він показує сам.  */
  if (m2::P.on() && (newmode == VOL || newmode == SLEEPING || newmode == NUMBERS || newmode == INFO ||
                     newmode == SETTINGS || newmode == TIMEZONE || newmode == WIFI)) newmode = PLAYER;
#endif
  if (newmode == _mode || (network.status != CONNECTED && network.status != SDREADY)) return;
  if (!_ensurePlayer()) return;
  /*  Зв'язок може зникнути ще на заставці завантаження, а віджети плеєра
      (рядок діалогу, адреса) створюються лише разом із його сторінкою:
      діалог «немає зв'язку» писав у ще не виділений буфер — і радіо
      перезавантажувалось по колу. До готового плеєра режими не міняємо —
      стан мережі наздожене її власний цикл.  */
  if (_bootStep != 2){ if(newmode == LOST) _lostPending = true; return; }
  if(_fmOn){
    /*  Годинник спільний із заставкою — розблокувати; покажчику рівня
        повернути звичний розмір. Повернемось на плеєр — _favMain() знову
        складе компактний вигляд.  */
    _fmOn = false;
    _clock->unlock();
    if(_vuwidget) _vuwidget->reshape(vuConf, bandsConf);
  }
  _mode = newmode;
  _sbSig = 0xFFFFFFFF;               /* рядок стану — наново на новому екрані */
  _logoCrc = 1;                      /* логотип — теж */
  dsp.setScrollId(NULL);
#if DSP_MODEL==DSP_ILI9341
  /*  Новий вигляд: «немає зв'язку», «картка», «оновлення» — картка на головному
      екрані, а не старий діалог yoRadio з жовтою смугою посередині.  */
  const bool m2status = m2::P.on() && (newmode == LOST || newmode == SDCHANGE || newmode == UPDATING);
  if (m2status) {
    _pager->setPageKeep(_m2empty);
    m2::P.setStatus(newmode == LOST ? 1 : (newmode == SDCHANGE ? 2 : 3));
    m2::P.show();
    m2::P.invalAll();
    m2::P.render();
    return;
  }
  if (newmode != PLAYER) m2::P.hide();
  if (newmode == PLAYER && m2::P.on()) {
    m2::P.setStatus(0);
    numOfNextStation = 0;
    config.isScreensaver = false;
    _pager->setPageKeep(_m2empty);
    config.setDspOn(config.store.dspon, false);
    m2::P.show();
    m2::P.render();                    /* перехід іде в темряві — кадр готовий до того, як засвітиться */
    pm.on_display_player();
  } else
#endif
  if (newmode == PLAYER) {
    m2::P.hide();
    if(player.isRunning())
      if(clockMove.width<0) _clock->moveBack(); else _clock->moveTo(clockMove);
    else
      _clock->moveBack();
    #ifdef DSP_LCD
      dsp.clearDsp();
    #endif
    numOfNextStation = 0;
    #ifdef META_MOVE
      _meta->moveBack();
    #endif
    _meta->setAlign(metaConf.widget.align);
    _meta->setText(config.station.name);
    _nums->setText("");
    config.isScreensaver = false;
    _applyDev();
    _pager->setPage( pages[PG_PLAYER]);
    config.setDspOn(config.store.dspon, false);
    drawHeaderIcons();
    _applySdLayout();
    if(timekeeper.weatherBuf && timekeeper.weatherBuf[0]) putRequest(NEWWEATHER);   /* погода, що прийшла, поки був новий вигляд */
    _layoutChange(player.status() == PLAYING);   /* покажчик рівня — за фактом, а не за загубленим PSTART */
    pm.on_display_player();
  }
  if (newmode == SCREENSAVER || newmode == SCREENBLANK) {
    config.isScreensaver = true;
    _pager->setPage( pages[PG_SCREENSAVER]);
    if (newmode == SCREENBLANK) {
      //dsp.clearClock();
      _clock->clear();
      config.setDspOn(false, false);
    }
  }else{
    config.screensaverTicks=SCREENSAVERSTARTUPDELAY;
    config.screensaverPlayingTicks=SCREENSAVERSTARTUPDELAY;
    config.isScreensaver = false;
  }
  if (newmode == VOL) {
    #ifndef HIDE_IP
      _showDialog(LANG::const_DlgVolume);
    #else
      _showDialog(config.ipToStr(WiFi.localIP()));
    #endif
    _nums->setText(config.store.volume, numtxtFmt);
  }
  if (newmode == LOST){
    _showDialog(LANG::const_DlgLost);
    /*  Замість адреси, якої вже немає, — що робити далі: дотик по цьому
        екрану веде просто до списку мереж.  */
    if(_volip) _volip->setText("дотик - вибрати мережу");
  }
  if (newmode == UPDATING)  _showDialog(LANG::const_DlgUpdate);
  if (newmode == SLEEPING)  _showDialog("SLEEPING");
  if (newmode == SDCHANGE)  _showDialog(LANG::const_waitForSD);
  if (newmode == INFO || newmode == SETTINGS || newmode == TIMEZONE || newmode == WIFI) _showDialog(LANG::const_DlgNextion);
  if (newmode == NUMBERS) _showDialog("");
  if (newmode == STATIONS) {
    _pager->setPage( pages[PG_PLAYLIST]);
    _plcurrent->setText("");
    currentPlItem = config.lastStation();
    #if DSP_MODEL==DSP_ILI9341
    _plwidget->enterPage();
    #endif
    _drawPlaylist();
  }
  
}

void Display::resetQueue(){
  if(displayQueue!=NULL) xQueueReset(displayQueue);
}

void Display::_drawPlaylist() {
  //dsp.drawPlaylist(currentPlItem);
  _plwidget->drawPlaylist(currentPlItem);
  timekeeper.waitAndReturnPlayer(30);
}

void Display::_drawNextStationNum(uint16_t num) {
  timekeeper.waitAndReturnPlayer(30);
  _meta->setText(config.stationByNum(num));
  _nums->setText(num, "%d");
}

void Display::putRequest(displayRequestType_e type, int payload){
  if(displayQueue==NULL) return;
#if DSP_MODEL==DSP_ILI9341
  /*  новий вигляд: список станцій — сторінка нового меню (m2/m2stations.cpp)  */
  if(type == NEWMODE && payload == STATIONS && m2::P.on()){ m2::stationsRequest(); return; }
#endif
  requestParams_t request;
  request.type = type;
  request.payload = payload;
  xQueueSend(displayQueue, &request, DSQ_SEND_DELAY);
  #ifdef USE_NEXTION
    nextion.putRequest(request);
  #endif
}

void Display::_layoutChange(bool played){
  if(config.store.vumeter && _vuwidget){
    if(played){
      if(_vuwidget) _vuwidget->unlock();
      //_clock->moveTo(clockMove);
      if(clockMove.width<0) _clock->moveBack(); else _clock->moveTo(clockMove);
      /*  Погоду й значок навмисно не рухаємо: раніше на зупинці вони
          з'їжджали вліво, на місце покажчика рівня, і блок стрибав туди-сюди
          при кожному пуску та зупинці. Тепер стоять на одному місці.  */
    }else{
      if(_vuwidget) if(!_vuwidget->locked()) _vuwidget->lock();
      _clock->moveBack();
    }
  }else{
    if(played){
      if(clockMove.width<0) _clock->moveBack(); else _clock->moveTo(clockMove);
    }else{
      _clock->moveBack();
    }
  }
}

void Display::loop() {
  if(_bootStep==0) {
    _pager->begin();
    _bootScreen();
    return;
  }
  if(displayQueue==NULL || _locked) return;
#if DSP_MODEL==DSP_ILI9341
  /*  Іде оновлення з GitHub — екран належить його ходу, хоч би що було відкрите.  */
  if(m2::otaViewActive()){
    m2::otaViewRender();
    requestParams_t drop;
    while(xQueueReceive(displayQueue, &drop, 0)) { }
    return;
  }
  if(m2::otaViewEnded() && _bootStep == 2) forceRedraw();       /* не вийшло — назад на плеєр */
#endif
  if(_splashDemoMs){
    _splashDemoUntil = millis() + _splashDemoMs; _splashDemoMs = 0;
    dsp.fillScreen(0);
    bool ok = splash.begin(true);
    Serial.printf("##DSP#\tзаставка: %s\n", ok ? "почато" : "файл не відкрився");
    if(!ok) _splashDemoUntil = 0;
    else sfx.test(SFX_START);
  }
  if(_splashDemoUntil){
#ifdef USE_YOMENU
    /*  Кнопка «показати заставку» в меню: меню закривається в темряві (гасить
        підсвітку) і засвічує її вже після. Заставка тут повертала керування
        раніше, ніж меню встигало засвітити, — і вся йшла на чорному екрані.  */
    if(yomenu.fading()) yomenu.render();
#endif
    if(millis() < _splashDemoUntil && splash.active()){ splash.tick(); return; }
    _splashDemoUntil = 0; splash.stop(); forceRedraw();
    return;
  }
  if(!_playerBuilt) _ensurePlayer();
  /*  Такт плавної зміни — до всіх дострокових виходів: якщо піти раніше,
      зміна застрягне на погашеній підсвітці.  */
#ifdef YO_DEBUG
  { uint32_t f0 = millis(); fadeLoop(); uint32_t d = millis() - f0; if(d > yoFadeMs) yoFadeMs = d; }
#else
  fadeLoop();
#endif
#if DSP_MODEL==DSP_ILI9341
  if(_plFinal){
    _plFinal = false;
    if(_mode==STATIONS) _drawPlaylist();     /* рівно на станції + таймер наново */
  }
  if(_plBtnDirty){
    _plBtnDirty = false;
    if(_mode==STATIONS && _plwidget && _plBtnIdx >= 0) _plwidget->drawButton(_plBtnIdx, _plBtnOn);
  }
  if(_plScroll && _mode==STATIONS){
    plSmooth(_plScrollPos);       /* кадр прокрутки, у своїй задачі */
    requestParams_t drop;
    while(xQueueReceive(displayQueue, &drop, 0)) { }
    return;
  }
  /*  Бігучий рядок списку не малюємо, коли зверху меню (скажімо, меню мереж
      відкрилось саме, поки список був на екрані): він писав би просто в
      дисплей поверх сторінки.  */
#ifdef USE_YOMENU
  if(_mode==STATIONS && _plwidget && !_plScroll && !yomenu.active() && !yomenu.fading()) _plwidget->marqueeTick();
#else
  if(_mode==STATIONS && _plwidget && !_plScroll) _plwidget->marqueeTick();
#endif
#endif
#ifdef USE_YOMENU
  /*  Пока открыто меню, обычная отрисовка молчит, а очередь просто
      опустошается — иначе накопившиеся запросы вывалятся на экран разом,
      как только меню закроют.  */
  if(yomenu.active() || yomenu.fading()){
#ifdef YO_DEBUG
    { uint32_t m0 = millis(); yomenu.render(); ui.flush(); uint32_t d = millis() - m0; if(d > yoMenuMs) yoMenuMs = d; }
#else
    yomenu.render();
    ui.flush();                  /* кадр меню: на екран лише змінене */
#endif
    requestParams_t drop;
    while(xQueueReceive(displayQueue, &drop, 0)) { }
    return;
  }
#endif
#if DSP_MODEL==DSP_ILI9341
  /*  Перемкнули радіо <-> картка — перебудовуємо плеєр: пульт або погода і
      значок джерела в шапці. Перехід на радіо в changeMode() не має ні
      діалогу, ні повернення на плеєр, тож без цього пульт картки лишався
      на екрані радіо. Тут, після виходу для меню, щоб не малювати поверх нього.  */
  bool m2p = ((_mode == PLAYER || _mode == LOST || _mode == SDCHANGE || _mode == UPDATING) && m2::P.shown());
  if(m2p){
    /*  новий головний екран малює себе сам; старі частини плеєра мовчать  */
    if(_redrawReq && !fading()){ _redrawReq = false; m2::P.invalAll(); }
    m2::P.render();
  }
  if(!m2p){
    static int8_t shownPlayMode = -1, shownSrc = -1;
    if(_mode == PLAYER && shownPlayMode != (int8_t)config.getMode()){
      shownPlayMode = (int8_t)config.getMode();
      _applySdLayout();
      drawHeaderIcons();
    }
    /*  значок джерела в шапці: радіо, картка чи проповідь із сайту  */
    int8_t src = player.remoteStationName ? 2 : (int8_t)config.getMode();
    if(_mode == PLAYER && shownSrc != src){ shownSrc = src; drawHeaderIcons(); }
  }
#endif
  /*  веб змінив те, що видно на плеєрі (IP, батарея, картка), — перемалювати  */
  if(_redrawReq && _mode == PLAYER && !fading()){ _redrawReq = false; forceRedraw(); }
  if(_bootStep == 1 && splash.active()) splash.tick();     /* заставка, поки радіо шукає мережу */
#ifdef YO_DEBUG
  DSTEP(_pager->loop());
  if(!m2p){
  DSTEP(_statusBar());
  DSTEP(_sermonLayout());
  DSTEP(_stationLogo());
  DSTEP(_favMain());
  DSTEP(_lowBat());
  }
#else
  _pager->loop();
  if(!m2p){
  _statusBar();
  _sermonLayout();
  _stationLogo();
  _favMain();
  _lowBat();
  }
#endif
#ifdef USE_NEXTION
  nextion.loop();
#endif
  requestParams_t request;
  if(xQueueReceive(displayQueue, &request, DSP_QUEUE_TICKS)){
    /*  Плеєра ще немає (старт без мережі) — малювати його частини нічим:
        лише старт і годинник для меню, решту відкидаємо.  */
    if(!_ensurePlayer() && request.type != DSP_START && request.type != CLOCK && request.type != BOOTSTRING && request.type != WAITFORSD) return;
    /*  Рядки назви пісні навіть сховані чистять свою рамку, коли приходить
        нова назва, — і зачіпали верх блоку проповіді. Після таких подій
        блок просто малюємо заново.  */
    if(_smLayout && (request.type == NEWTITLE || request.type == NEWSTATION ||
                     request.type == PSTART || request.type == PSTOP)) _smShown = 0xFFFFFFFF;
    bool pm_result = true;
    pm.on_display_queue(request, pm_result);
    if(pm_result)
      switch (request.type){
        case NEWMODE: {
          /*  Перехід плеєр <-> список станцій робимо плавним; решта режимів
              міняється миттєво, щоб не гальмувати службові підказки.  */
          displayMode_e nm = (displayMode_e)request.payload;
          /*  Повернення на плеєр — завжди плавне, звідки б не йшло (список за
              таймером, гучність, номер станції, заставка, картка): раніше
              плавним був лише перехід зі списку, решта — ривком. Службові
              екрани (оновлення, втрата зв'язку) міняються одразу.  */
          bool svc = (_mode==LOST || _mode==UPDATING || _mode==CLEAR || _mode==SLEEPING ||
                      nm==LOST || nm==UPDATING || nm==CLEAR || nm==SLEEPING);
          bool smooth = !svc && ((nm==STATIONS && _mode==PLAYER) || (nm==PLAYER && _mode!=PLAYER));
          if(smooth) _beginFade(nm); else _swichMode(nm);
          break;
        }
        case CLOSEPLAYLIST: player.sendCommand({PR_PLAY, request.payload});
        case CLOCK: 
        #ifdef USE_YOMENU
          yomenu.tick();       /* живі частини сторінок налаштувань */
        #endif
          if(_mode==PLAYER || _mode==SCREENSAVER) _time(); 
          /*#ifdef USE_NEXTION
            if(_mode==TIMEZONE) nextion.localTime(network.timeinfo);
            if(_mode==INFO)     nextion.rssi();
          #endif*/
          break;
        case NEWTITLE: _title(); break;
        case NEWSTATION: _station(); break;
        case NEXTSTATION: _drawNextStationNum(request.payload); break;
        case DRAWPLAYLIST: _drawPlaylist(); break;
        case DRAWVOL: _volume(); break;
        case DBITRATE: {
            char buf[20]; 
            snprintf(buf, 20, bitrateFmt, config.station.bitrate); 
            if(_bitrate) { _bitrate->setText(config.station.bitrate==0?"":buf); } 
            if(_fullbitrate) { 
              _fullbitrate->setBitrate(config.station.bitrate); 
              _fullbitrate->setFormat(config.configFmt); 
            } 
          }
          break;
        case AUDIOINFO: if(_heapbar)  { _heapbar->lock(!config.store.audioinfo); _heapbar->setValue(player.inBufferFilled()); } break;
        case SHOWVUMETER: {
          if(_vuwidget){
            _vuwidget->lock(!config.store.vumeter); 
            _layoutChange(player.isRunning());
          }
          break;
        }
        case SHOWWEATHER: {
          _applySdLayout();
          if(!config.store.showweather){
            #ifndef HIDE_IP
            if(_volip) _volip->setText(config.ipToStr(WiFi.localIP()), iptxtFmt);
            #endif
          }else{
            if(_weather) _weather->setText(LANG::const_getWeather);
          }
          break;
        }
        case NEWWEATHER: {
#if DSP_MODEL==DSP_ILI9341
          /*  Новий вигляд малює погоду сам (m2player). Старий рядок погоди в ньому
              не замкнений і на setText одразу малює в екран поверх нового кадру —
              прибрано разом із підозрою на «намертво» раз на пів години (оновлення
              погоди). Старий вигляд отримає текст, коли на нього перемкнуть.  */
          if(m2::P.on()) break;
#endif
          if(_weather && timekeeper.weatherBuf) _weather->setText(timekeeper.weatherBuf);
          if(_weathericon){
            if(timekeeper.weatherHave)
              _weathericon->setWeather(timekeeper.weatherTemp, timekeeper.weatherPress,
                                       timekeeper.weatherHum, timekeeper.weatherIcon);
            else _weathericon->setIcon(timekeeper.weatherIcon);
          }
          break;
        }
        case BOOTSTRING: {
          if(_bootstring) _bootstring->setText(config.ssids[request.payload].ssid, LANG::bootstrFmt);
          /*#ifdef USE_NEXTION
            char buf[50];
            snprintf(buf, 50, bootstrFmt, config.ssids[request.payload].ssid);
            nextion.bootString(buf);
          #endif*/
          break;
        }
        case WAITFORSD: {
          if(_bootstring) _bootstring->setText(LANG::const_waitForSD);
          break;
        }
        case SDFILEINDEX: {
          if(_mode == SDCHANGE){ _nums->setText(request.payload, "%d"); m2::P.setStatusCount(request.payload); }
          break;
        }
        case DSPRSSI: if(_rssi){ _setRSSI(request.payload); } if (_heapbar && config.store.audioinfo) _heapbar->setValue(player.isRunning()?player.inBufferFilled():0); break;
        case PSTART: _layoutChange(true);   break;
        case PSTOP:  _layoutChange(false);  break;
        case DSP_START: _start();  break;
        case NEWIP: {
          #ifndef HIDE_IP
            if(_volip) _volip->setText(config.ipToStr(WiFi.localIP()), iptxtFmt);
          #endif
          break;
        }
        default: break;

        // check if there are more messages waiting in the Q, in this case break the loop() and go
        // for another round to evict next message, do not waste time to redraw the screen, etc...
        if (uxQueueMessagesWaiting(displayQueue))
          return;
      }
  }

  dsp.loop();
/*
  #if I2S_DOUT==255
  player.computeVUlevel();
  #endif
*/
}

void Display::_setRSSI(int rssi) {
  if(!_rssi) return;
#if RSSI_DIGIT
  _rssi->setText(rssi, rssiFmt);
  return;
#endif
  char rssiG[3];
  int rssi_steps[] = {RSSI_STEPS};
  if(rssi >= rssi_steps[0]) strlcpy(rssiG, "\004\006", 3);
  if(rssi >= rssi_steps[1] && rssi < rssi_steps[0]) strlcpy(rssiG, "\004\005", 3);
  if(rssi >= rssi_steps[2] && rssi < rssi_steps[1]) strlcpy(rssiG, "\004\002", 3);
  if(rssi >= rssi_steps[3] && rssi < rssi_steps[2]) strlcpy(rssiG, "\003\002", 3);
  if(rssi <  rssi_steps[3] || rssi >=  0) strlcpy(rssiG, "\001\002", 3);
  _rssi->setText(rssiG);
}

void Display::_station() {
  _meta->setAlign(metaConf.widget.align);
  _meta->setText(config.station.name);
/*#ifdef USE_NEXTION
  nextion.newNameset(config.station.name);
  nextion.bitrate(config.station.bitrate);
  nextion.bitratePic(ICON_NA);
#endif*/
}

char *split(char *str, const char *delim) {
  char *dmp = strstr(str, delim);
  if (dmp == NULL) return NULL;
  *dmp = '\0'; 
  return dmp + strlen(delim);
}

void Display::_title() {
  if (strlen(config.station.title) > 0) {
    char tmpbuf[strlen(config.station.title)+1];
    strlcpy(tmpbuf, config.station.title, strlen(config.station.title)+1);
    char *stitle = split(tmpbuf, " - ");
    if(stitle && _title2){
      _title1->setText(tmpbuf);
      _title2->setText(stitle);
    }else{
      _title1->setText(config.station.title);
      if(_title2) _title2->setText("");
    }
    /*#ifdef USE_NEXTION
      nextion.newTitle(config.station.title);
    #endif*/
    
  }else{
    _title1->setText("");
    if(_title2) _title2->setText("");
  }
  if (player_on_track_change) player_on_track_change();
  pm.on_track_change();
}

void Display::_time(bool redraw) {
  
#if LIGHT_SENSOR!=255
  if(config.store.dspon) {
    config.store.brightness = AUTOBACKLIGHT(analogRead(LIGHT_SENSOR));
    config.setBrightness();
  }
#endif
  if(config.isScreensaver && network.timeinfo.tm_sec % 60 == 0){
    #if TIME_SIZE<19
      uint16_t ft=static_cast<uint16_t>(random(TFT_FRAMEWDT, (dsp.height()-TIME_SIZE*CHARHEIGHT-TFT_FRAMEWDT)));
    #else
      uint16_t ft=static_cast<uint16_t>(random(TFT_FRAMEWDT+TIME_SIZE, (dsp.height()-_clock->dateSize()-TFT_FRAMEWDT*2)));
    #endif
    uint16_t lt=static_cast<uint16_t>(random(TFT_FRAMEWDT, (dsp.width()-_clock->clockWidth()-TFT_FRAMEWDT)));
    if(clockConf.align==WA_CENTER) lt-=(dsp.width()-_clock->clockWidth())/2;
    //_clock->moveTo({clockConf.left, ft, 0});
    _clock->moveTo({lt, ft, 0});
  }
  _clock->draw();
  /*#ifdef USE_NEXTION
    nextion.printClock(network.timeinfo);
  #endif*/
}

void Display::_volume() {
  if(_volbar) _volbar->setValue(config.store.volume);
  #ifndef HIDE_VOL
    if(_voltxt) _voltxt->setText(config.store.volume, voltxtFmt);
  #endif
  if(_mode==VOL) {
    timekeeper.waitAndReturnPlayer(3);
    _nums->setText(config.store.volume, numtxtFmt);
  }
  /*#ifdef USE_NEXTION
    nextion.setVol(config.store.volume, _mode == VOL);
  #endif*/
}

void Display::flip(){ dsp.flip(); }

void Display::invert(){ dsp.invert(); }

void  Display::setContrast(){
  #if DSP_MODEL==DSP_NOKIA5110
    dsp.setContrast(config.store.contrast);
  #endif
}

bool Display::deepsleep(){
#if defined(LCD_I2C) || defined(DSP_OLED) || BRIGHTNESS_PIN!=255
  dsp.sleep();
  return true;
#endif
  return false;
}

void Display::wakeup(){
#if defined(LCD_I2C) || defined(DSP_OLED) || BRIGHTNESS_PIN!=255
  dsp.wake();
#endif
}
//============================================================================================================================
#else // !DUMMYDISPLAY
//============================================================================================================================
void Display::init(){
  _createDspTask();
  #ifdef USE_NEXTION
  nextion.begin(true);
  #endif
}
void Display::_start(){
  #ifdef USE_NEXTION
  //nextion.putcmd("page player");
  nextion.start();
  #endif
  config.setTitle(LANG::const_PlReady);
}

void Display::putRequest(displayRequestType_e type, int payload){
  if(type==DSP_START) _start();
  #ifdef USE_NEXTION
    requestParams_t request;
    request.type = type;
    request.payload = payload;
    nextion.putRequest(request);
  #else
    if(type==NEWMODE) mode((displayMode_e)payload);
  #endif
}
//============================================================================================================================
#endif // DUMMYDISPLAY
