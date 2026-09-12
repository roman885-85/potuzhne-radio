#ifndef widgets_h
#define widgets_h
#if DSP_MODEL!=DSP_DUMMY
#include "widgetsconfig.h"

#ifndef DSP_LCD
  #define CHARWIDTH   6
  #define CHARHEIGHT  8
#else
  #define CHARWIDTH   1
  #define CHARHEIGHT  1
#endif

class psFrameBuffer;

class Widget{
  public:
    Widget(){ _active   = false; }
    virtual ~Widget(){}
    virtual void loop(){}
    virtual void init(WidgetConfig conf, uint16_t fgcolor, uint16_t bgcolor){
      _config = conf;
      _fgcolor  = fgcolor;
      _bgcolor  = bgcolor;
      _width = _backMove.width = 0;
      _backMove.x = _config.left;
      _backMove.y = _config.top;
      _moved = _locked = false;
    }
    void setAlign(WidgetAlign align){
      _config.align = align;
    }
    void setActive(bool act, bool clr=false) { _active = act; if(_active && !_locked) _draw(); if(clr && !_locked) _clear(); }
    void lock(bool lck=true) { _locked = lck; if(_locked) _reset(); if(_locked && _active) _clear();  }
    void unlock() { _locked = false; }
    bool locked() { return _locked; }
    bool active() const { return _active; }
    int16_t left() const { return _config.left; }
    int16_t top()  const { return _config.top; }
    uint16_t widthOf() const { return _width; }
    void moveTo(MoveConfig mv){
      if(mv.width<0) return;
      _moved = true;
      if(_active && !_locked) _clear();
      _config.left = mv.x;
      _config.top = mv.y;
      if(mv.width>0) _width = mv.width;
      _reset();
      _draw();
    }
    void moveBack(){
      if(!_moved) return;
      if(_active && !_locked) _clear();
      _config.left = _backMove.x;
      _config.top = _backMove.y;
      _width = _backMove.width;
      _moved = false;
      _reset();
      _draw();
    }
  protected:
    bool _active, _moved, _locked;
    uint16_t _fgcolor, _bgcolor, _width;
    WidgetConfig _config;
    MoveConfig   _backMove;
    virtual void _draw() {}
    virtual void _clear() {}
    virtual void _reset() {}
};

class TextWidget: public Widget {
  public:
    TextWidget() {}
    TextWidget(WidgetConfig wconf, uint16_t buffsize, bool uppercase, uint16_t fgcolor, uint16_t bgcolor) { init(wconf, buffsize, uppercase, fgcolor, bgcolor); }
    ~TextWidget();
    using Widget::init;
    void init(WidgetConfig wconf, uint16_t buffsize, bool uppercase, uint16_t fgcolor, uint16_t bgcolor);
    void setText(const char* txt);
    void setText(int val, const char *format);
    void setText(const char* txt, const char *format);
    bool uppercase() { return _uppercase; }
  protected:
    char *_text;
    char *_oldtext;
    bool _uppercase;
    uint16_t  _buffsize, _textwidth, _oldtextwidth, _oldleft, _textheight;
    int16_t   _baseline = 0;
    uint8_t _charWidth;
  protected:
    void _draw();
    uint16_t _realLeft(bool w_fb=false);
    void _charSize(uint8_t textsize, uint8_t& width, uint16_t& height);
};

class FillWidget: public Widget {
  public:
    FillWidget() {}
    FillWidget(FillConfig conf, uint16_t bgcolor) { init(conf, bgcolor); }
    using Widget::init;
    void init(FillConfig conf, uint16_t bgcolor);
    void setHeight(uint16_t newHeight);
  protected:
    uint16_t _height;
    void _draw();
};

class ScrollWidget: public TextWidget {
  public:
    ScrollWidget(){}
    ScrollWidget(const char* separator, ScrollConfig conf, uint16_t fgcolor, uint16_t bgcolor);
    ~ScrollWidget();
    using Widget::init;
    void init(const char* separator, ScrollConfig conf, uint16_t fgcolor, uint16_t bgcolor);
    void loop();
    void setText(const char* txt);
    void setText(const char* txt, const char *format);
  private:
    char *_sep;
    char *_window;
    int16_t _x;
    bool _doscroll;
    uint8_t _scrolldelta;
    uint16_t _scrolltime;
    uint32_t _scrolldelay;
    uint16_t _sepwidth, _startscrolldelay;
    uint8_t _charWidth;
    psFrameBuffer* _fb=nullptr;
  private:
    void _setTextParams();
    void _calcX();
    void _drawFrame();
    void _draw();
    bool _checkIsScrollNeeded();
    bool _checkDelay(int m, uint32_t &tstamp);
    void _clear();
    void _reset();
};

class SliderWidget: public Widget {
  public:
    SliderWidget(){}
    SliderWidget(FillConfig conf, uint16_t fgcolor, uint16_t bgcolor, uint32_t maxval, uint16_t oucolor=0){
      init(conf, fgcolor, bgcolor, maxval, oucolor);
    }
    using Widget::init;
    void init(FillConfig conf, uint16_t fgcolor, uint16_t bgcolor, uint32_t maxval, uint16_t oucolor=0);
    void setValue(uint32_t val);
    void loop() override;               /* крок анімації ходу смуги */
    uint16_t shown() const { return _oldvalwidth; }  /* де смуга насправді */
  protected:
    uint32_t _animTick = 0;
    uint16_t _height, _oucolor, _oldvalwidth;
    uint32_t _max, _value;
    bool _outlined;
    void _draw();
    void _drawslider();
    void _clear();
    void _reset();
};

/*  Пульт відтворення з картки: попередній трек, смуга позиції з перемоткою,
    наступний трек. Стоїть на місці погоди, поки грає картка. Малюється в
    задачі дисплея (loop), а дотик лише ставить прапорці — шину не ділять.  */
#define SD_Y        84
#define SD_H        38
#define SD_CY       (SD_Y + SD_H/2)
#define SD_PREV_CX  84
#define SD_NEXT_CX  296
#define SD_BTN_R    17
#define SD_BAR_X0   110
#define SD_BAR_X1   270
#define SD_BAR_Y    108
class SdCtlWidget: public Widget {
  public:
    SdCtlWidget(){}
    using Widget::init;
    void press(int8_t which, bool on){ if(which < 0 || which > 1) return; _press[which] = on; _dirtyBtn = true; }
    void preview(float f){ _preview = f; _dirtyBar = true; }
    void loop() override;
  protected:
    volatile bool  _press[2] = { false, false };
    volatile bool  _dirtyBtn = true, _dirtyBar = true;
    volatile float _preview = -1.0f;
    uint32_t _tick = 0;
    int16_t  _knobX = -1;
    uint32_t _shownCur = 0xFFFFFFFF, _shownDur = 0xFFFFFFFF;
    void _draw() override;
    void _clear() override;
    void _drawButton(uint8_t i, bool on);
    void _drawBar(float f, uint32_t cur, uint32_t dur, bool force);
};

/*  Значок погоди — картинка з рідного проєкту Nextion, без перемальовування.
    Малюється однією передачею, як і смуга плейлиста.  */
class WeatherIconWidget: public Widget {
  public:
    WeatherIconWidget(){}
    using Widget::init;
    void setIcon(uint8_t code);
    /*  Увесь блок погоди, як на плеєрі Nextion: тиск і вологість зі своїми
        значками, велика температура з «°C» і значок стану.  */
    void setWeather(float t, int16_t press, int16_t hum, uint8_t code);
  protected:
    uint8_t _code = 255, _shown = 255;
    float   _t = 0.0f;
    int16_t _press = 0, _hum = 0;
    bool    _have = false;
    void _draw() override;
    void _clear() override;
};

class VuWidget: public Widget {
  public:
    VuWidget() {}
    VuWidget(WidgetConfig wconf, VUBandsConfig bands, uint16_t vumaxcolor, uint16_t vumincolor, uint16_t bgcolor)
            { init(wconf, bands, vumaxcolor, vumincolor, bgcolor); }
    ~VuWidget();
    using Widget::init;
    void init(WidgetConfig wconf, VUBandsConfig bands, uint16_t vumaxcolor, uint16_t vumincolor, uint16_t bgcolor);
    void reshape(WidgetConfig wconf, VUBandsConfig bands);
    void loop();
  protected:
    #if !defined(DSP_LCD) && !defined(DSP_OLED)
      Canvas *_canvas;
    #endif
    VUBandsConfig _bands;
    uint16_t _vumaxcolor, _vumincolor;
    void _draw();
    void _clear();
};

class NumWidget: public TextWidget {
  public:
    using Widget::init;
    void init(WidgetConfig wconf, uint16_t buffsize, bool uppercase, uint16_t fgcolor, uint16_t bgcolor);
    void setText(const char* txt);
    void setText(int val, const char *format);
  protected:
    void _getBounds();
    void _draw();
};

class ProgressWidget: public TextWidget {
  public:
    ProgressWidget() {}
    ProgressWidget(WidgetConfig conf, ProgressConfig pconf, uint16_t fgcolor, uint16_t bgcolor) { 
      init(conf, pconf, fgcolor, bgcolor);
    }
    using Widget::init;
    void init(WidgetConfig conf, ProgressConfig pconf, uint16_t fgcolor, uint16_t bgcolor){
      TextWidget::init(conf, pconf.width, false, fgcolor, bgcolor);
      _speed = pconf.speed; _width = pconf.width; _barwidth = pconf.barwidth;
      _pg = 0; 
    }
    void loop();
  private:
    uint8_t _pg;
    uint16_t _speed, _barwidth;
    uint32_t _scrolldelay;
    void _progress();
    bool _checkDelay(int m, uint32_t &tstamp);
};

class ClockWidget: public Widget {
  public:
    using Widget::init;
    void init(WidgetConfig wconf, uint16_t fgcolor, uint16_t bgcolor);
    void draw();
    uint8_t textsize(){ return _config.textsize; }
    void clear(){ _clearClock(); }
    inline uint16_t dateSize(){ return _space+ _dateheight; }
    inline uint16_t clockWidth(){ return _clockwidth; }
  private:
  #ifndef DSP_LCD
    #if DSP_MODEL==DSP_ILI9225
    auto &getRealDsp();
    #else
    Adafruit_GFX &getRealDsp();
    #endif
  #endif
  protected:
    char  _timebuffer[20]="00:00";
    char _tmp[30], _datebuf[30];
    uint8_t _superfont;
    uint16_t _clockleft, _clockwidth, _timewidth, _dotsleft, _linesleft;
    uint8_t  _clockheight, _timeheight, _dateheight, _space;
    uint16_t _forceflag = 0;
    bool dots = true;
    bool _fullclock;
    psFrameBuffer* _fb=nullptr;
    void _draw();
    void _clear();
    void _reset();
    void _getTimeBounds();
    void _printClock(bool force=false);
    void _clearClock();
    bool _getTime();
    uint16_t _left();
    uint16_t _top();
    void _begin();
};

class BitrateWidget: public Widget {
  public:
    BitrateWidget() {}
    BitrateWidget(BitrateConfig bconf, uint16_t fgcolor, uint16_t bgcolor) { init(bconf, fgcolor, bgcolor); }
    ~BitrateWidget(){}
    using Widget::init;
    void init(BitrateConfig bconf, uint16_t fgcolor, uint16_t bgcolor);
    void setBitrate(uint16_t bitrate);
    void setFormat(BitrateFormat format);
  protected:
    BitrateFormat _format;
    char _buf[6];
    uint8_t _charWidth;
    uint16_t _dimension, _bitrate, _textheight;
    void _draw();
    void _clear();
    void _charSize(uint8_t textsize, uint8_t& width, uint16_t& height);
};

/*  Сторінка плейлиста як у Nextion: сім рядків по 32 пікселі з жовтою
    смугою вибору посередині, праворуч стовпчик круглих кнопок ▲ ▼ ▶ ↶.  */
#define PL_ROWS      7
#define PL_CUR       3            /* рядок-смуга вибору, рахуючи від нуля */
#define PL_ROW_H     32
#define PL_TOP       8
#define PL_X0        2
#define PL_LIST_W    254
#define PL_BASE      21           /* базова лінія тексту в рядку (Verdana 11pt) */
#define PL_BASE_BIG  25           /* у жовтій смузі — Verdana 15pt, «лупа» (+36%, 13pt було непомітно) */
#define PL_BTN_X     265
#define PL_BTN_Y(i)  (8 + (i) * 58)
#define PL_C_BAND    0xE68B       /* жовтий Nextion #E3D25F */
#define PL_C_BANDTXT 0x0000
#define PL_C_STRIPE  0x3186       /* сірий #333333 */
#define PL_C_BLACK   0x0000
#define PL_C_TXT     0xDEFB
/*  Той самий список для інших джерел (проповіді з сайту): рамка, кнопки й
    рядки малюються тим самим кодом, що й станції, лише назви беруться з
    функції. shift — зсув тексту жовтого рядка для бігучого рядка;
    bandOnly — перемалювати лише його.  */
void plGenericChrome();
void plGenericButton(uint8_t i, bool on);
void plGenericDraw(float pos, int count, const char* (*nameAt)(int), int16_t shift, bool bandOnly, int playIdx = -1, int16_t wrapW = 0, int16_t rightPad = 0, int padUntil = 0);
uint16_t plTextWidth(const char* utf8);
enum { PLB_UP = 0, PLB_DOWN, PLB_PLAY, PLB_BACK };

class PlayListWidget: public Widget {
  public:
    void marqueeTick();              /* бігучий рядок у смузі, коли список стоїть */
    using Widget::init;
    void init(ScrollWidget* current);
    void drawPlaylist(uint16_t currentItem);
    inline uint16_t itemHeight(){ return _plItemHeight; }
    inline uint16_t currentTop(){ return _plYStart+_plCurrentPos*_plItemHeight; }
    /*  Плавна прокрутка: назви тримаємо в пам'яті й малюємо зі зсувом у
        пікселях. Інакше кожен кадр відкривав би playlist.csv та index.dat,
        і про плавність не було б мови.  */
    void loadCache(int from);
    void drawSmooth(float pos);
    inline uint16_t itemsCount(){ return _plTtemsCount; }
    inline uint16_t centerPos(){ return _plCurrentPos; }
    void enterPage();                 /* сторінку щойно відкрили: рамку й кнопки малювати наново */
    void drawButton(uint8_t i, bool on);
  private:
    ScrollWidget* _current;
    uint16_t _plItemHeight, _plTtemsCount, _plCurrentPos;
    int _plYStart;
    int16_t  _bandShift = 0;
    bool     _bandOnly = false;
    int      _mqPos = 0, _mqShownPos = -1;
    bool     _mqFrac = false;
    uint16_t _mqW = 0;
    uint32_t _mqT = 0;
    static const uint8_t PL_CACHE = 16;
    char _cache[PL_CACHE][40] = {{0}};
    int  _cacheFrom = 0;              /* номер першої станції в кеші, 0 = порожньо */
    uint16_t* _row = nullptr;         /* смуга одного рядка у внутрішній пам'яті, доступній DMA */
    bool _chrome = false;             /* рамку й кнопки вже намальовано */
    void _drawChrome();
    uint8_t _fillPlMenu(int from, uint8_t count);
    void _printPLitem(uint8_t pos, const char* item);
    
};


#endif
#endif




