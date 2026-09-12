#include "../../core/options.h"
#if DSP_MODEL!=DSP_DUMMY
#include "../dspcore.h"
#include "Arduino.h"
#include "widgets.h"
#if DSP_MODEL==DSP_ILI9341
/*  Полотно поверх готового буфера: малюємо смугу у внутрішній пам'яті,
    щоб не возитися з повільним записом у PSRAM.  */
class RowCanvas : public Adafruit_GFX {
  public:
    RowCanvas() : Adafruit_GFX(1,1) {}
    /*  Обмежити письмо тексту зліва направо: далі в рядку намальовані
        значки (замок, сигнал), і бігуча назва не повинна лізти під них.  */
    void setClipW(int16_t w){ _clipW = w > 0 ? w : 32767; }
    void setBuffer(uint16_t* buf, int16_t w, int16_t h){
      _buf = buf; _bw = w; _bh = h; _width = w; _height = h;
    }
    /*  Рядки буфера від b0 до b1 малюються кольором fgBand — так текст
        перефарбовується саме там, де рядок заходить у жовту смугу вибору.
        swap: кольори кладуться одразу в порядку байтів дисплея, і буфер
        можна віддати DMA без переставляння.  */
    void setBand(int16_t b0, int16_t b1, uint16_t fgBand){ _b0 = b0; _b1 = b1; _fgBand = fgBand; }
    void setSwap(bool s){ _swap = s; }
    void drawPixel(int16_t x, int16_t y, uint16_t c) override {
      if(!_buf || x < 0 || y < 0 || x >= _bw || y >= _bh) return;
      if(y >= _b0 && y < _b1) c = _fgBand;
      _buf[(size_t)y * _bw + x] = _swap ? (uint16_t)((c << 8) | (c >> 8)) : c;
    }
    void fill(uint16_t c){
      if(!_buf) return;
      uint16_t v = _swap ? (uint16_t)((c << 8) | (c >> 8)) : c;
      size_t n=(size_t)_bw*_bh; uint16_t* p=_buf; while(n--) *p++ = v;
    }
    /*  Вивід знаків шрифту GFX прямо в буфер рядка. Через Adafruit кожна
        точка йшла окремим віртуальним викликом із перевірками, і складання
        дев'яти рядків займало 11 мс на кадр — третину всього кадру.  */
    /*  cy0..cy1 — які рядки буфера можна фарбувати: так в один рядок
        кладуться дрібний текст поза смугою вибору й крупний у ній (лупа).  */
    void textFast(const char* s, int16_t x, int16_t baseline, uint16_t color, const GFXfont* f,
                  int16_t cy0 = -32768, int16_t cy1 = 32767){
      if(!_buf || !f) return;
      if(cy0 < 0) cy0 = 0; if(cy1 > _bh) cy1 = _bh;
      if(cy1 <= cy0) return;
      uint16_t cN = _swap ? (uint16_t)((color << 8) | (color >> 8)) : color;
      uint16_t cB = _swap ? (uint16_t)((_fgBand << 8) | (_fgBand >> 8)) : _fgBand;
      for(; *s; s++){
        uint8_t c = (uint8_t)*s;
        if(c < f->first || c > f->last) continue;
        const GFXglyph* g = &f->glyph[c - f->first];
        const uint8_t* bmp = f->bitmap + pgm_read_word(&g->bitmapOffset);
        uint8_t w = pgm_read_byte(&g->width), h = pgm_read_byte(&g->height);
        int16_t gx = x + (int8_t)pgm_read_byte(&g->xOffset);
        int16_t gy = baseline + (int8_t)pgm_read_byte(&g->yOffset);
        uint16_t bit = 0; uint8_t bits = 0;
        for(uint8_t yy = 0; yy < h; yy++){
          int16_t py = gy + yy;
          bool on = (py >= cy0 && py < cy1);
          uint16_t col = (py >= _b0 && py < _b1) ? cB : cN;
          uint16_t* line = _buf + (size_t)(on ? py : 0) * _bw;
          for(uint8_t xx = 0; xx < w; xx++){
            if(!(bit & 7)) bits = pgm_read_byte(&bmp[bit >> 3]);
            bit++;
            if((bits & 0x80) && on){
              int16_t px = gx + xx;
              if(px >= 0 && px < _bw && px < _clipW) line[px] = col;
            }
            bits <<= 1;
          }
        }
        x += pgm_read_byte(&g->xAdvance);
        if(x >= _bw || x >= _clipW) break;
      }
    }
    /*  Заливка кількох рядків буфера одним кольором — по два пікселі за раз.  */
    void fillLines(int16_t y0, int16_t y1, uint16_t c){
      if(!_buf) return;
      if(y0 < 0) y0 = 0; if(y1 > _bh) y1 = _bh;
      if(y1 <= y0) return;
      uint16_t v = _swap ? (uint16_t)((c << 8) | (c >> 8)) : c;
      uint32_t vv = ((uint32_t)v << 16) | v;
      size_t n = (size_t)(y1 - y0) * _bw;
      uint16_t* p = _buf + (size_t)y0 * _bw;
      if(((uintptr_t)p & 2) && n){ *p++ = v; n--; }
      uint32_t* q = (uint32_t*)p;
      for(size_t i = 0; i < n / 2; i++) q[i] = vv;
      if(n & 1) p[n - 1] = v;
    }
    void fillLine(int16_t y, uint16_t c){
      if(!_buf || y < 0 || y >= _bh) return;
      uint16_t v = _swap ? (uint16_t)((c << 8) | (c >> 8)) : c;
      uint16_t* p = _buf + (size_t)y * _bw;
      for(int16_t i = 0; i < _bw; i++) p[i] = v;
    }
    void text(const char* s, int16_t x, int16_t baseline, uint16_t color, const GFXfont* f){
      setFont(f); setTextSize(1); setTextColor(color); setCursor(x, baseline); print(s);
    }
  private:
    uint16_t* _buf = nullptr; int16_t _bw = 0, _bh = 0;
    int16_t _clipW = 32767;          /* праворуч від цього в рядку живуть значки */
    int16_t _b0 = 0, _b1 = 0; uint16_t _fgBand = 0; bool _swap = false;
};
static RowCanvas _rowCanvas;
static void plRowText(const char* nm, int16_t lb0, int16_t lb1, int16_t sx, bool playing, int16_t wrap);

  #include "../fonts/yoMono18.h"
  #include "../fonts/yoUI15.h"
  #include "../fonts/yoMono12.h"
  #include "../fonts/yoUI11.h"     /* список станцій: звичайна Verdana, не жирна */
  #include "../fonts/nxbuttons.h"
  #include "../tools/spidma.h"
  #include "esp_heap_caps.h"
  /*  Гладкі накреслення замість розтягнутого 5x7. Вони моноширинні і мають
      рівно той самий крок (18 і 12 px), що textsize 3 і 2 у вбудованому
      шрифті, тому вся верстка й логіка прокрутки лишаються незмінними.
      Змінюється тільки те, що y для такого шрифту — це базова лінія,
      а не верх рядка, звідси зсув.  */
  static inline const GFXfont* yoFont(uint8_t ts, int16_t &baseline){
    if(ts==3){ baseline=19; return &yoMono18; }
    if(ts==2){ baseline=13; return &yoMono12; }
    baseline=0; return nullptr;
  }
#else
  static inline const GFXfont* yoFont(uint8_t ts, int16_t &baseline){ (void)ts; baseline=0; return nullptr; }
#endif
#include "../../core/player.h"    //  for VU widget
#include "../../core/network.h"   //  for Clock widget
#include "../../core/config.h"
#include "../tools/l10n.h"
#include "../tools/psframebuffer.h"

/************************
      FILL WIDGET
 ************************/
void FillWidget::init(FillConfig conf, uint16_t bgcolor){
  Widget::init(conf.widget, bgcolor, bgcolor);
  _width = conf.width;
  _height = conf.height;
  
}

void FillWidget::_draw(){
  if(!_active) return;
  dsp.fillRect(_config.left, _config.top, _width, _height, _bgcolor);
}

void FillWidget::setHeight(uint16_t newHeight){
  _height = newHeight;
  //_draw();
}
/************************
      TEXT WIDGET
 ************************/
TextWidget::~TextWidget() {
  free(_text);
  free(_oldtext);
}

void TextWidget::_charSize(uint8_t textsize, uint8_t& width, uint16_t& height){
#ifndef DSP_LCD
  width = textsize * CHARWIDTH;
  height = textsize * CHARHEIGHT;
#else
  width = 1;
  height = 1;
#endif
}

void TextWidget::init(WidgetConfig wconf, uint16_t buffsize, bool uppercase, uint16_t fgcolor, uint16_t bgcolor) {
  Widget::init(wconf, fgcolor, bgcolor);
  _buffsize = buffsize;
  _text = (char *) malloc(sizeof(char) * _buffsize);
  memset(_text, 0, _buffsize);
  _oldtext = (char *) malloc(sizeof(char) * _buffsize);
  memset(_oldtext, 0, _buffsize);
  _charSize(_config.textsize, _charWidth, _textheight);
  _textwidth = _oldtextwidth = _oldleft = 0;
  _uppercase = uppercase;
}

void TextWidget::setText(const char* txt) {
  strlcpy(_text, utf8Rus(txt, _uppercase), _buffsize);
  _textwidth = strlen(_text) * _charWidth;
  if (strcmp(_oldtext, _text) == 0) return;
  if (_active) dsp.fillRect(_oldleft == 0 ? _realLeft() : min(_oldleft, _realLeft()),  _config.top, max(_oldtextwidth, _textwidth), _textheight, _bgcolor);
  _oldtextwidth = _textwidth;
  _oldleft = _realLeft();
  if (_active) _draw();
}

void TextWidget::setText(int val, const char *format){
  char buf[_buffsize];
  snprintf(buf, _buffsize, format, val);
  setText(buf);
}

void TextWidget::setText(const char* txt, const char *format){
  char buf[_buffsize];
  snprintf(buf, _buffsize, format, txt);
  setText(buf);
}

uint16_t TextWidget::_realLeft(bool w_fb) {
  uint16_t realwidth = (_width>0 && w_fb)?_width:dsp.width();
  switch (_config.align) {
    case WA_CENTER: return (realwidth - _textwidth) / 2; break;
    case WA_RIGHT: return (realwidth - _textwidth - (!w_fb?_config.left:0)); break;
    default: return !w_fb?_config.left:0; break;
  }
}

void TextWidget::_draw() {
  if(!_active) return;
  int16_t bl; const GFXfont* f = yoFont(_config.textsize, bl);
  dsp.setTextColor(_fgcolor, _bgcolor);
  dsp.setFont(f);
  dsp.setTextSize(f ? 1 : _config.textsize);
  dsp.setCursor(_realLeft(), _config.top + bl);
  dsp.print(_text);
  strlcpy(_oldtext, _text, _buffsize);
}

/************************
      SCROLL WIDGET
 ************************/
ScrollWidget::ScrollWidget(const char* separator, ScrollConfig conf, uint16_t fgcolor, uint16_t bgcolor) {
  init(separator, conf, fgcolor, bgcolor);
}

ScrollWidget::~ScrollWidget() {
  free(_fb);
  free(_sep);
  free(_window);
}

void ScrollWidget::init(const char* separator, ScrollConfig conf, uint16_t fgcolor, uint16_t bgcolor) {
  TextWidget::init(conf.widget, conf.buffsize, conf.uppercase, fgcolor, bgcolor);
  _sep = (char *) malloc(sizeof(char) * 4);
  memset(_sep, 0, 4);
  snprintf(_sep, 4, " %.*s ", 1, separator);
  _x = conf.widget.left;
  _startscrolldelay = conf.startscrolldelay;
  _scrolldelta = conf.scrolldelta;
  _scrolltime = conf.scrolltime;
  _charSize(_config.textsize, _charWidth, _textheight);
  _sepwidth = strlen(_sep) * _charWidth;
  _width = conf.width;
  _backMove.width = _width;
  _window = (char *) malloc(sizeof(char) * ((MAX_WIDTH) / _charWidth + 4));   /* +3 знаки на вхід праворуч */
  memset(_window, 0, ((MAX_WIDTH) / _charWidth + 4));
  _doscroll = false;
  #ifdef PSFBUFFER
  _fb = new psFrameBuffer(dsp.width(), dsp.height());
  uint16_t _rl = (_config.align==WA_CENTER)?(dsp.width()-_width)/2:_config.left;
  _fb->begin(&dsp, _rl, _config.top, _width, _textheight, _bgcolor);
  #endif
}

void ScrollWidget::_setTextParams() {
  if (_config.textsize == 0) return;
  int16_t bl; const GFXfont* f = yoFont(_config.textsize, bl);
  _baseline = bl;
  if(_fb->ready()){
  #ifdef PSFBUFFER
    _fb->setFont(f);
    _fb->setTextSize(f ? 1 : _config.textsize);
    _fb->setTextColor(_fgcolor, _bgcolor);
  #endif
  }else{
    dsp.setFont(f);
    dsp.setTextSize(f ? 1 : _config.textsize);
    dsp.setTextColor(_fgcolor, _bgcolor);
  }
}

bool ScrollWidget::_checkIsScrollNeeded() {
  return _textwidth > _width;
}

void ScrollWidget::setText(const char* txt) {
  strlcpy(_text, utf8Rus(txt, _uppercase), _buffsize - 1);
  if (strcmp(_oldtext, _text) == 0) return;
  _textwidth = strlen(_text) * _charWidth;
  _x = _fb->ready()?0:_config.left;
  _doscroll = _checkIsScrollNeeded();
  if (dsp.getScrollId() == this) dsp.setScrollId(NULL);
  _scrolldelay = millis();
  if (_active) {
    _setTextParams();
    if (_doscroll) {
      if(_fb->ready()){
      #ifdef PSFBUFFER
        _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
        _fb->setCursor(0, 0);
        snprintf(_window, _width / _charWidth + 1, "%s", _text); //TODO
        _fb->print(_window);
        _fb->display();
      #endif
      } else {
        dsp.fillRect(_config.left,  _config.top, _width, _textheight, _bgcolor);
        dsp.setCursor(_config.left, _config.top);
        snprintf(_window, _width / _charWidth + 3, "%s", _text); //TODO
        dsp.setClipping({_config.left, _config.top, _width, _textheight});
        dsp.print(_window);
        dsp.clearClipping();
      }
    } else {
      if(_fb->ready()){
      #ifdef PSFBUFFER
        _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
        _fb->setCursor(_realLeft(true), _baseline);
        _fb->print(_text);
        _fb->display();
      #endif
      } else {
        dsp.fillRect(_config.left, _config.top, _width, _textheight, _bgcolor);
        dsp.setCursor(_realLeft(), _config.top);
        //dsp.setClipping({_config.left, _config.top, _width, _textheight});
        dsp.print(_text);
        //dsp.clearClipping();
      }
    }
    strlcpy(_oldtext, _text, _buffsize);
  }
}

void ScrollWidget::setText(const char* txt, const char *format){
  char buf[_buffsize];
  snprintf(buf, _buffsize, format, txt);
  setText(buf);
}

void ScrollWidget::loop() {
  if(_locked) return;
#if DSP_MODEL==DSP_ILI9341
  /*  Тут кожен рядок біжить сам: штатно yoRadio дозволяє рухатись лише
      одному рядку за раз, і решта стояла, чекаючи черги.  */
  if (!_doscroll || _config.textsize == 0) return;
#else
  if (!_doscroll || _config.textsize == 0 || (dsp.getScrollId() != NULL && dsp.getScrollId() != this)) return;
#endif
  uint16_t fbl = _fb->ready()?0:_config.left;
  if (_checkDelay(_x == fbl ? _startscrolldelay : _scrolltime, _scrolldelay)) {
    _calcX();
    if (_active) _draw();
  }
}

void ScrollWidget::_clear(){
  if(_fb->ready()){
    #ifdef PSFBUFFER
    _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
    _fb->display();
    #endif
  } else {
    dsp.fillRect(_config.left, _config.top, _width, _textheight, _bgcolor);
  }
}

void ScrollWidget::_draw() {
  if(!_active || _locked) return;
  _setTextParams();
  if (_doscroll) {
    uint16_t fbl = _fb->ready()?0:_config.left;
    uint16_t _newx = fbl - _x;
    const char* _cursor = _text + _newx / _charWidth;
    uint16_t hiddenChars = _cursor - _text;
    if (hiddenChars < strlen(_text)) {
      /*  +3 знаки: той, що заходить праворуч, малюємо частково й обрізаємо
          по краю поля — тоді він з'являється попіксельно, а не цілим
          знаком. Раніше у вікні були лише ті, що вміщались повністю.  */
      snprintf(_window, _width / _charWidth + 3, "%s%s%s", _cursor, _sep, _text);
    } else {
      const char* _scursor = _sep + (_cursor - (_text + strlen(_text)));
      snprintf(_window, _width / _charWidth + 3, "%s%s", _scursor, _text);
    }
    if(_fb->ready()){
    #ifdef PSFBUFFER
      _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
      _fb->setCursor(_x + hiddenChars * _charWidth, _baseline);
      _fb->print(_window);
      _fb->display();
    #endif
    } else {
      dsp.setCursor(_x + hiddenChars * _charWidth, _config.top + _baseline);
      dsp.setClipping({_config.left, _config.top, _width, _textheight});
      dsp.print(_window);
      #ifndef DSP_LCD
        dsp.print(" ");
      #endif
      dsp.clearClipping();
    }
  } else {
    if(_fb->ready()){
    #ifdef PSFBUFFER
      _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
      _fb->setCursor(_realLeft(true), _baseline);
      _fb->print(_text);
      _fb->display();
    #endif
    } else {
      dsp.fillRect(_config.left, _config.top, _width, _textheight, _bgcolor);
      dsp.setCursor(_realLeft(), _config.top);
      dsp.setClipping({_realLeft(), _config.top, _width, _textheight});
      dsp.print(_text);
      dsp.clearClipping();
    }
  }
}

void ScrollWidget::_calcX() {
  if (!_doscroll || _config.textsize == 0) return;
  _x -= _scrolldelta;
  uint16_t fbl = _fb->ready()?0:_config.left;
  if (-_x > _textwidth + _sepwidth - fbl) {
    _x = fbl;
    dsp.setScrollId(NULL);
  } else {
    dsp.setScrollId(this);
  }
}

bool ScrollWidget::_checkDelay(int m, uint32_t &tstamp) {
  if (millis() - tstamp > m) {
    tstamp = millis();
    return true;
  } else {
    return false;
  }
}

void ScrollWidget::_reset(){
  dsp.setScrollId(NULL);
  _x = _fb->ready()?0:_config.left;
  _scrolldelay = millis();
  _doscroll = _checkIsScrollNeeded();
  #ifdef PSFBUFFER
  _fb->freeBuffer();
  uint16_t _rl = (_config.align==WA_CENTER)?(dsp.width()-_width)/2:_config.left;
  _fb->begin(&dsp, _rl, _config.top, _width, _textheight, _bgcolor);
  #endif
}

/************************
      SLIDER WIDGET
 ************************/
void SliderWidget::init(FillConfig conf, uint16_t fgcolor, uint16_t bgcolor, uint32_t maxval, uint16_t oucolor) {
  Widget::init(conf.widget, fgcolor, bgcolor);
  _width = conf.width; _height = conf.height; _outlined = conf.outlined; _oucolor = oucolor, _max = maxval;
  _oldvalwidth = _value = 0;
}

void SliderWidget::setValue(uint32_t val) {
  _value = val;                       /* лише ціль — доїде loop() */
  if (_active && !_locked) _drawslider();
}

/*  Крок анімації: смуга не стрибає до нового значення, а доїжджає до нього,
    проходячи за крок частину відстані, що лишилась. Домальовується тільки
    різниця, тож рух нічого не блимає.  */
void SliderWidget::loop() {
  if(!_active || _locked) return;
  uint32_t now = millis();
  if(now - _animTick < 20) return;
  _animTick = now;
  _drawslider();
}

void SliderWidget::_drawslider() {
  uint16_t target = map(_value, 0, _max, 0, _width - _outlined * 2);
  if (_oldvalwidth == target) return;
  int32_t d = (int32_t)target - (int32_t)_oldvalwidth;
  int32_t step = ((d < 0 ? -d : d) * 5) >> 4;
  if(step < 1) step = 1;
  uint16_t nw = (uint16_t)((int32_t)_oldvalwidth + (d < 0 ? -step : step));
  dsp.fillRect(_config.left + _outlined + min(nw, _oldvalwidth), _config.top + _outlined,
               abs((int)_oldvalwidth - (int)nw), _height - _outlined * 2,
               _oldvalwidth > nw ? _bgcolor : _fgcolor);
  _oldvalwidth = nw;
}

void SliderWidget::_draw() {
  if(_locked) return;
  _clear();
  if(!_active) return;
  if (_outlined) dsp.drawRect(_config.left, _config.top, _width, _height, _oucolor);
  uint16_t valwidth = map(_value, 0, _max, 0, _width - _outlined * 2);
  dsp.fillRect(_config.left + _outlined, _config.top + _outlined, valwidth, _height - _outlined * 2, _fgcolor);
  _oldvalwidth = valwidth;            /* повна перемальовка — одразу на місце */
}

void SliderWidget::_clear() {
//  _oldvalwidth = 0;
  dsp.fillRect(_config.left, _config.top, _width, _height, _bgcolor);
}
void SliderWidget::_reset() {
  _oldvalwidth = 0;
}
/************************
      SD CONTROL WIDGET
 ************************/
#if !defined(DSP_LCD) && !defined(DSP_OLED)
#include "../fonts/yoUI9.h"
/*  Пульт у мові самого плеєра: жовті знаки без кружків (як значки шапки),
    смуга позиції — такою ж рамкою із заливкою, як смуга гучності внизу,
    час — тим самим приглушеним кольором, що й дата. Раніше тут були сірі
    кола з білим бігунком, і пульт виглядав прибульцем з іншого інтерфейсу.  */
#define SD_C_YEL   0xE68B

void SdCtlWidget::_clear(){
  dsp.fillRect(_config.left, SD_Y, dsp.width() - _config.left, SD_H, _bgcolor);
  _knobX = -1; _shownCur = _shownDur = 0xFFFFFFFF;
}

void SdCtlWidget::_drawButton(uint8_t i, bool on){
  int16_t cx = i ? SD_NEXT_CX : SD_PREV_CX, cy = SD_BAR_Y;
  uint16_t fg = on ? 0xFFFF : SD_C_YEL;       /* натиснута — біла */
  dsp.fillRect(cx - 14, cy - 12, 28, 24, _bgcolor);
  if(i == 0){                                   /* ⏮ */
    dsp.fillRect(cx - 9, cy - 8, 3, 17, fg);
    dsp.fillTriangle(cx + 8, cy - 9, cx + 8, cy + 9, cx - 5, cy, fg);
  }else{                                        /* ⏭ */
    dsp.fillRect(cx + 7, cy - 8, 3, 17, fg);
    dsp.fillTriangle(cx - 8, cy - 9, cx - 8, cy + 9, cx + 5, cy, fg);
  }
}

static void sdTime(char* b, size_t n, uint32_t sec){
  if(sec >= 3600) snprintf(b, n, "%u:%02u:%02u", (unsigned)(sec/3600), (unsigned)(sec/60%60), (unsigned)(sec%60));
  else            snprintf(b, n, "%u:%02u", (unsigned)(sec/60), (unsigned)(sec%60));
}

/*  Перемальовуємо лише смугу й час, і лише коли щось змінилось.  */
void SdCtlWidget::_drawBar(float f, uint32_t cur, uint32_t dur, bool force){
  if(f < 0) f = 0; if(f > 1) f = 1;
  int16_t kx = SD_BAR_X0 + 1 + (int16_t)(f * (SD_BAR_X1 - SD_BAR_X0 - 2));
  if(force || kx != _knobX){
    /*  Рамка, як у смуги гучності, заливка до поточного місця. Бігунка
        немає: гортають дотиком будь-де по смузі (M4A — теж, за таблицями
        кадрів, див. Audio::yoM4aLocate).  */
    if(force || _knobX < 0){
      dsp.fillRect(SD_BAR_X0 - 2, SD_BAR_Y - 6, SD_BAR_X1 - SD_BAR_X0 + 4, 13, _bgcolor);
      dsp.drawRect(SD_BAR_X0, SD_BAR_Y - 4, SD_BAR_X1 - SD_BAR_X0, 9, config.theme.volbarout);
      dsp.fillRect(SD_BAR_X0 + 1, SD_BAR_Y - 3, kx - SD_BAR_X0 - 1, 7, config.theme.volbarin);
    }else if(kx > _knobX){
      dsp.fillRect(_knobX, SD_BAR_Y - 3, kx - _knobX, 7, config.theme.volbarin);
    }else{
      dsp.fillRect(kx, SD_BAR_Y - 3, _knobX - kx, 7, _bgcolor);
    }
    _knobX = kx;
  }
  if(force || cur != _shownCur || dur != _shownDur){
    char a[12], b[12];
    sdTime(a, sizeof(a), cur); sdTime(b, sizeof(b), dur);
    /*  Надписи часу на два пікселі нижче верху смуги: верхівки цифр виходили
        на рядок вище області, яку чистимо, і після зміни часу там лишались
        «рисочки» від попередніх цифр.  */
    dsp.fillRect(SD_BAR_X0 - 7, SD_Y, SD_BAR_X1 - SD_BAR_X0 + 15, 16, _bgcolor);
    dsp.setFont(&yoUI9); dsp.setTextSize(1); dsp.setTextColor(config.theme.date);
    dsp.setCursor(SD_BAR_X0 - 4, SD_Y + 14); dsp.print(a);
    int16_t x1, y1; uint16_t w, h;
    dsp.getTextBounds(b, 0, 40, &x1, &y1, &w, &h);
    dsp.setCursor(SD_BAR_X1 + 4 - (int16_t)(x1 + w), SD_Y + 14); dsp.print(b);
    dsp.setFont();
    _shownCur = cur; _shownDur = dur;
  }
}

void SdCtlWidget::_draw(){
  if(!_active || _locked) return;
  _clear();
  _drawButton(0, _press[0]); _drawButton(1, _press[1]);
  _dirtyBtn = false;
  uint32_t dur = player.durSec(), cur = player.posSec();
  _drawBar(dur ? (float)cur / dur : 0.0f, cur, dur, true);
  _dirtyBar = false;
}

void SdCtlWidget::loop(){
  if(!_active || _locked) return;
  if(_dirtyBtn){ _dirtyBtn = false; _drawButton(0, _press[0]); _drawButton(1, _press[1]); }
  uint32_t now = millis();
  if(!_dirtyBar && now - _tick < 250) return;
  _tick = now; _dirtyBar = false;
  uint32_t dur = player.durSec();
  float pv = _preview;
  if(pv >= 0.0f) _drawBar(pv, (uint32_t)(pv * dur), dur, false);     /* палець веде смугою */
  else{
    uint32_t cur = player.posSec();
    _drawBar(dur ? (float)cur / dur : 0.0f, cur, dur, false);
  }
}
#else
void SdCtlWidget::loop(){}
void SdCtlWidget::_draw(){}
void SdCtlWidget::_clear(){}
void SdCtlWidget::_drawButton(uint8_t i, bool on){ (void)i; (void)on; }
void SdCtlWidget::_drawBar(float f, uint32_t cur, uint32_t dur, bool force){ (void)f; (void)cur; (void)dur; (void)force; }
#endif

/************************
    WEATHER ICON WIDGET
 ************************/
#if !defined(DSP_LCD) && !defined(DSP_OLED)
#include "../fonts/weathericons.h"

#include "../fonts/yoUI14b.h"
#include "../fonts/nxweather.h"

/*  Розмітка блоку погоди в смузі 84..122 праворуч від покажчика рівня.
    Екран Nextion ширший (400 проти 320), тож тиск і вологість стоять одне
    над одним ліворуч від температури — в один рядок усе не вміщається.  */
#define WB_Y      84
#define WB_H      38
#define WB_L      160         /* звідси чистимо: лівіше — компактний годинник (обране на головному) */
#define WB_SX     70          /* значки тиску й вологості */
#define WB_TX     89          /* їхній текст */
#define WB_TEMP_R 243         /* правий край температури */
#define WB_DEG_X  245
#define WB_ICON_X 264
#define WB_C_SMALL 0xC618     /* сірий, як дрібний текст Nextion */
#define WB_C_TEMP  0xFFFF

static void wbBlit(int16_t x, int16_t y, const uint16_t* img, int16_t w, int16_t h){
  dsp.startWrite();
  dsp.setAddrWindow(x, y, w, h);
  dsp.writePixels((uint16_t*)img, (uint32_t)w * h);
  dsp.endWrite();
}

void WeatherIconWidget::setIcon(uint8_t code){
  if(code > 9) code = 9;
  if(code == _code) return;
  _code = code;
  if(_active && !_locked) _draw();
}

void WeatherIconWidget::setWeather(float t, int16_t press, int16_t hum, uint8_t code){
  if(code > 9) code = 9;
  bool same = _have && code == _code && press == _press && hum == _hum &&
              (int)(t * 10) == (int)(_t * 10);
  _t = t; _press = press; _hum = hum; _code = code; _have = true;
  if(!same && _active && !_locked) _draw();       /* тільки коли щось змінилось */
}

void WeatherIconWidget::_draw(){
  if(!_active || _locked) return;
  dsp.fillRect(WB_L, WB_Y, WB_ICON_X + WEATHER_ICON_W - WB_L, WB_H, _bgcolor);
  if(_code <= 9) wbBlit(WB_ICON_X, WB_Y, weatherIcons[_code], WEATHER_ICON_W, WEATHER_ICON_H);
  _shown = _code;
  if(!_have) return;
  char b[16];
  /*  Температура — велика, праворуч притиснута до «°C».  */
  snprintf(b, sizeof(b), "%.1f", _t);
  dsp.setFont(&yoUI14b); dsp.setTextSize(1); dsp.setTextColor(WB_C_TEMP);
  uint16_t adv = 0;
  for(const char* q = b; *q; q++){
    uint8_t c = (uint8_t)*q;
    if(c >= yoUI14b.first && c <= yoUI14b.last) adv += pgm_read_byte(&yoUI14b.glyph[c - yoUI14b.first].xAdvance);
  }
  dsp.setCursor(WB_TEMP_R - (int16_t)adv, WB_Y + 29);
  dsp.print(b);
  wbBlit(WB_DEG_X, WB_Y + 6, nxw_degc, NXW_DEGC_W, NXW_DEGC_H);
  /*  Тиск і вологість на головному екрані більше не показуємо — він
      був перевантажений. Вони є на сторінці «інформація».  */
  dsp.setFont();
}

void WeatherIconWidget::_clear(){
  dsp.fillRect(WB_L, WB_Y, WB_ICON_X + WEATHER_ICON_W - WB_L, WB_H, _bgcolor);
  _shown = 255;
}
#else
void WeatherIconWidget::setIcon(uint8_t code){ (void)code; }
void WeatherIconWidget::setWeather(float t, int16_t press, int16_t hum, uint8_t code){ (void)t; (void)press; (void)hum; (void)code; }
void WeatherIconWidget::_draw(){}
void WeatherIconWidget::_clear(){}
#endif

/************************
      VU WIDGET
 ************************/
#if !defined(DSP_LCD) && !defined(DSP_OLED)
VuWidget::~VuWidget() {
  if(_canvas) free(_canvas);
}

void VuWidget::init(WidgetConfig wconf, VUBandsConfig bands, uint16_t vumaxcolor, uint16_t vumincolor, uint16_t bgcolor) {
  Widget::init(wconf, bgcolor, bgcolor);
  _vumaxcolor = vumaxcolor;
  _vumincolor = vumincolor;
  _bands = bands;
  _canvas = new Canvas(_bands.width * 2 + _bands.space, _bands.height);
}


/*  Інший розмір на ходу: коли на головному рядок обраного, покажчик стає
    вузьким і коротким — поруч із компактним годинником.  */
void VuWidget::reshape(WidgetConfig wconf, VUBandsConfig bands){
  if(_canvas && _active && !_locked) dsp.fillRect(_config.left, _config.top, _bands.width * 2 + _bands.space, _bands.height, _bgcolor);
  if(_canvas) delete _canvas;
  _config = wconf;
  _bands = bands;
  _canvas = new Canvas(_bands.width * 2 + _bands.space, _bands.height);
}

void VuWidget::_draw(){
  if(!_active || _locked) return;
#if !defined(USE_NEXTION) && I2S_DOUT==255
/*  static uint8_t cc = 0;
  cc++;
  if(cc>0){
    player.getVUlevel();
    cc=0;
  }*/
#endif
  static uint16_t measL, measR;
  uint16_t bandColor;
  uint16_t dimension = _config.align?_bands.width:_bands.height;
  uint16_t vulevel = player.get_VUlevel(dimension);
  
  uint8_t L = (vulevel >> 8) & 0xFF;
  uint8_t R = vulevel & 0xFF;
  
  /*  Хід стовпчика прив'язаний до часу, а не до кадрів: задача малювання
      крутиться значно швидше за сорок разів на секунду, і без прив'язки
      будь-яке згладжування вироджується назад у стрибок.  */
  extern uint8_t yoVuFade, yoVuRise;    /* підбираються командою vuset */
  /*  Лічильники для перевірки: як часто задача малювання нас кличе і де
      насправді стоїть стовпчик. Без них плавність можна лише припускати. */
  extern uint32_t yoVuCalls; extern uint16_t yoVuMeasL, yoVuMeasR;
  yoVuCalls++;
  static uint32_t vuTick = 0, vuDrawn = 0;
  static uint16_t prevL = 0xFFFF, prevR = 0xFFFF;
  uint32_t now = millis();
  if(now - vuTick < 25) return;
  vuTick = now;

  bool played = player.isRunning();
  uint16_t tL = played ? L : dimension;  /* мовчить — стовпчик іде донизу */
  uint16_t tR = played ? R : dimension;

  /*  За крок проходимо частку відстані до цілі, а не весь шлях і не сталу
      сходинку: вгору швидко, вниз повільніше — так стрілка й поводиться.
      Частки задані в шістнадцятих, щоб рахувати цілими числами.  */
  int16_t dL = (int16_t)tL - (int16_t)measL;
  int16_t dR = (int16_t)tR - (int16_t)measR;
  int16_t sL = (int16_t)(((dL < 0 ? -dL : dL) * (dL < 0 ? yoVuRise : yoVuFade)) >> 4);
  int16_t sR = (int16_t)(((dR < 0 ? -dR : dR) * (dR < 0 ? yoVuRise : yoVuFade)) >> 4);
  if(!sL && dL) sL = 1;                 /* інакше застрягне за крок до цілі */
  if(!sR && dR) sR = 1;
  measL = (uint16_t)((int16_t)measL + (dL < 0 ? -sL : sL));
  measR = (uint16_t)((int16_t)measR + (dR < 0 ? -sR : sR));
  if(measL>dimension) measL=dimension;
  if(measR>dimension) measR=dimension;

  /*  Нічого не зрушило — екран не чіпаємо. Раз на пів секунди малюємо все
      одно, щоб віджет відновився після перемикання сторінок.  */
  yoVuMeasL = measL; yoVuMeasR = measR;
  if(measL==prevL && measR==prevR && now - vuDrawn < 500) return;
  prevL = measL; prevR = measR; vuDrawn = now;
  uint8_t h=(dimension/_bands.perheight)-_bands.vspace;
  _canvas->fillRect(0,0,_bands.width * 2 + _bands.space,_bands.height, _bgcolor);
  for(int i=0; i<dimension; i++){
    if(i%(dimension/_bands.perheight)==0){
      if(_config.align){
        #ifndef BOOMBOX_STYLE
          bandColor = (i>_bands.width-(_bands.width/_bands.perheight)*4)?_vumaxcolor:_vumincolor;
          _canvas->fillRect(i, 0, h, _bands.height, bandColor);
          _canvas->fillRect(i + _bands.width + _bands.space, 0, h, _bands.height, bandColor);
        #else
          bandColor = (i>(_bands.width/_bands.perheight))?_vumincolor:_vumaxcolor;
          _canvas->fillRect(i, 0, h, _bands.height, bandColor);
          bandColor = (i>_bands.width-(_bands.width/_bands.perheight)*3)?_vumaxcolor:_vumincolor;
          _canvas->fillRect(i + _bands.width + _bands.space, 0, h, _bands.height, bandColor);
        #endif
      }else{
        bandColor = (i<(_bands.height/_bands.perheight)*3)?_vumaxcolor:_vumincolor;
        _canvas->fillRect(0, i, _bands.width, h, bandColor);
        _canvas->fillRect(_bands.width + _bands.space, i, _bands.width, h, bandColor);
      }
    }
  }
  if(_config.align){
    #ifndef BOOMBOX_STYLE
      _canvas->fillRect(_bands.width-measL, 0, measL, _bands.width, _bgcolor);
      _canvas->fillRect(_bands.width * 2 + _bands.space - measR, 0, measR, _bands.width, _bgcolor);
      dsp.drawRGBBitmap(_config.left, _config.top, _canvas->getBuffer(), _bands.width * 2 + _bands.space, _bands.height);
    #else
      _canvas->fillRect(0, 0, _bands.width-(_bands.width-measL), _bands.width, _bgcolor);
      _canvas->fillRect(_bands.width * 2 + _bands.space - measR, 0, measR, _bands.width, _bgcolor);
      #if DSP_MODEL!=DSP_ILI9225
      dsp.startWrite();
      dsp.setAddrWindow(_config.left, _config.top, _bands.width * 2 + _bands.space, _bands.height);
      dsp.writePixels((uint16_t*)_canvas->getBuffer(), (_bands.width * 2 + _bands.space)*_bands.height);
      dsp.endWrite();
      #else
      dsp.drawRGBBitmap(_config.left, _config.top, _canvas->getBuffer(), _bands.width * 2 + _bands.space, _bands.height);
      #endif
    #endif
  }else{
    _canvas->fillRect(0, 0, _bands.width, measL, _bgcolor);
    _canvas->fillRect(_bands.width + _bands.space, 0, _bands.width, measR, _bgcolor);
    #if DSP_MODEL!=DSP_ILI9225
    dsp.startWrite();
    dsp.setAddrWindow(_config.left, _config.top, _bands.width * 2 + _bands.space, _bands.height);
    dsp.writePixels((uint16_t*)_canvas->getBuffer(), (_bands.width * 2 + _bands.space)*_bands.height);
    dsp.endWrite();
    #else
    dsp.drawRGBBitmap(_config.left, _config.top, _canvas->getBuffer(), _bands.width * 2 + _bands.space, _bands.height);
    #endif
  }
}

void VuWidget::loop(){
  if(_active || !_locked) _draw();
}

void VuWidget::_clear(){
  dsp.fillRect(_config.left, _config.top, _bands.width * 2 + _bands.space, _bands.height, _bgcolor);
}
#else // DSP_LCD
VuWidget::~VuWidget() { }
void VuWidget::init(WidgetConfig wconf, VUBandsConfig bands, uint16_t vumaxcolor, uint16_t vumincolor, uint16_t bgcolor) {
  Widget::init(wconf, bgcolor, bgcolor);
}
void VuWidget::_draw(){ }
void VuWidget::loop(){ }
void VuWidget::_clear(){ }
#endif

/************************
      NUM & CLOCK
 ************************/
#if !defined(DSP_LCD)
  #if TIME_SIZE<19 //19->NOKIA
  const GFXfont* Clock_GFXfontPtr = nullptr;
  #define CLOCKFONT5x7
  #else
  const GFXfont* Clock_GFXfontPtr = &Clock_GFXfont;
  #endif
#endif //!defined(DSP_LCD)

#if !defined(CLOCKFONT5x7) && !defined(DSP_LCD)
  inline GFXglyph *pgm_read_glyph_ptr(const GFXfont *gfxFont, uint8_t c) {
    return gfxFont->glyph + c;
  }
  uint8_t _charWidth(unsigned char c){
    GFXglyph *glyph = pgm_read_glyph_ptr(&Clock_GFXfont, c - 0x20);
    return pgm_read_byte(&glyph->xAdvance);
  }
  uint16_t _textHeight(){
    GFXglyph *glyph = pgm_read_glyph_ptr(&Clock_GFXfont, '8' - 0x20);
    return pgm_read_byte(&glyph->height);
  }
#else //!defined(CLOCKFONT5x7) && !defined(DSP_LCD)
  uint8_t _charWidth(unsigned char c){
  #ifndef DSP_LCD
    return CHARWIDTH * TIME_SIZE;
  #else
    return 1;
  #endif
  }
  uint16_t _textHeight(){
    return CHARHEIGHT * TIME_SIZE;
  }
#endif
uint16_t _textWidth(const char *txt){
  uint16_t w = 0, l=strlen(txt);
  for(uint16_t c=0;c<l;c++) w+=_charWidth(txt[c]);
  #if DSP_MODEL==DSP_ILI9225
  return w+l;
  #else
  return w;
  #endif
}

/************************
      NUM WIDGET
 ************************/
void NumWidget::init(WidgetConfig wconf, uint16_t buffsize, bool uppercase, uint16_t fgcolor, uint16_t bgcolor) {
  Widget::init(wconf, fgcolor, bgcolor);
  _buffsize = buffsize;
  _text = (char *) malloc(sizeof(char) * _buffsize);
  memset(_text, 0, _buffsize);
  _oldtext = (char *) malloc(sizeof(char) * _buffsize);
  memset(_oldtext, 0, _buffsize);
  _textwidth = _oldtextwidth = _oldleft = 0;
  _uppercase = uppercase;
  _textheight = TIME_SIZE/*wconf.textsize*/;
}

void NumWidget::setText(const char* txt) {
  strlcpy(_text, txt, _buffsize);
  _getBounds();
  if (strcmp(_oldtext, _text) == 0) return;
  uint16_t realth = _textheight;
#if defined(DSP_OLED) && DSP_MODEL!=DSP_SSD1322
  if(Clock_GFXfontPtr==nullptr) realth = _textheight * 8; //CHARHEIGHT
#endif
  if (_active)
  #ifndef CLOCKFONT5x7
    dsp.fillRect(_oldleft == 0 ? _realLeft() : min(_oldleft, _realLeft()),  _config.top-_textheight+1, max(_oldtextwidth, _textwidth), realth, _bgcolor);
  #else
    dsp.fillRect(_oldleft == 0 ? _realLeft() : min(_oldleft, _realLeft()),  _config.top, max(_oldtextwidth, _textwidth), realth, _bgcolor);
  #endif

  _oldtextwidth = _textwidth;
  _oldleft = _realLeft();
  if (_active) _draw();
}

void NumWidget::setText(int val, const char *format){
  char buf[_buffsize];
  snprintf(buf, _buffsize, format, val);
  setText(buf);
}

void NumWidget::_getBounds() {
  _textwidth= _textWidth(_text);
}

void NumWidget::_draw() {
#ifndef DSP_LCD
  if(!_active || TIME_SIZE<2) return;
  dsp.setTextSize(Clock_GFXfontPtr==nullptr?TIME_SIZE:1);
  dsp.setFont(Clock_GFXfontPtr);
  dsp.setTextColor(_fgcolor, _bgcolor);
#endif
  if(!_active) return;
  dsp.setCursor(_realLeft(), _config.top);
  dsp.print(_text);
  strlcpy(_oldtext, _text, _buffsize);
  dsp.setFont();
}

/**************************
      PROGRESS WIDGET
 **************************/
void ProgressWidget::_progress() {
  char buf[_width + 1];
  snprintf(buf, _width, "%*s%.*s%*s", _pg <= _barwidth ? 0 : _pg - _barwidth, "", _pg <= _barwidth ? _pg : 5, ".....", _width - _pg, "");
  _pg++; if (_pg >= _width + _barwidth) _pg = 0;
  setText(buf);
}

bool ProgressWidget::_checkDelay(int m, uint32_t &tstamp) {
  if (millis() - tstamp > m) {
    tstamp = millis();
    return true;
  } else {
    return false;
  }
}

void ProgressWidget::loop() {
  if (_checkDelay(_speed, _scrolldelay)) {
    _progress();
  }
}

/**************************
      CLOCK WIDGET
 **************************/
void ClockWidget::init(WidgetConfig wconf, uint16_t fgcolor, uint16_t bgcolor){
  Widget::init(wconf, fgcolor, bgcolor);
  _timeheight = _textHeight();
  _fullclock = TIME_SIZE>35 || DSP_MODEL==DSP_ILI9225;
  if(_fullclock) _superfont = TIME_SIZE / 17; //magick
  else if(TIME_SIZE==19 || TIME_SIZE==2) _superfont=1;
  else _superfont=0;
  _space = (5*_superfont)/2; //magick
  if(_fullclock){
    _dateheight = _superfont<4?1:2;
    /*  Висоту смуги під датою рахуємо по тому шрифту, яким її справді
        малюють. Гладке накреслення вище за розтягнутий 5x7, і якщо лишити
        стару висоту, полотно годинника обріже дату знизу.  */
    int16_t _dbl; const GFXfont* _df = yoFont(2, _dbl);
    uint16_t _dh = _df ? (uint16_t)pgm_read_byte(&_df->yAdvance)
                       : (uint16_t)(CHARHEIGHT * _dateheight);
    _clockheight = _timeheight + _space + _dh;
  } else {
    _clockheight = _timeheight;
  }
  _getTimeBounds();
#ifdef PSFBUFFER
  _fb = new psFrameBuffer(dsp.width(), dsp.height());
  _begin();
#endif
}

void ClockWidget::_begin(){
#ifdef PSFBUFFER
  _fb->begin(&dsp, _clockleft, _config.top-_timeheight, _clockwidth, _clockheight+1, config.theme.background);
#endif
}

bool ClockWidget::_getTime(){
  strftime(_timebuffer, sizeof(_timebuffer), "%H:%M", &network.timeinfo);
  bool ret = network.timeinfo.tm_sec==0 || _forceflag!=network.timeinfo.tm_year;
  _forceflag = network.timeinfo.tm_year;
  return ret;
}

uint16_t ClockWidget::_left(){
  if(_fb->ready()) return 0; else return _clockleft;
}
uint16_t ClockWidget::_top(){
  if(_fb->ready()) return _timeheight; else return _config.top;
}

void ClockWidget::_getTimeBounds() {
  _timewidth = _textWidth(_timebuffer);
  uint8_t fs = _superfont>0?_superfont:TIME_SIZE;
  uint16_t rightside = CHARWIDTH * fs * 2; // seconds
  if(_fullclock){
    rightside += _space*2+1; //2space+vline
    _clockwidth = _timewidth+rightside;
  } else {
    if(_superfont==0)
      _clockwidth = _timewidth;
    else
      _clockwidth = _timewidth + rightside;
  }
  switch(_config.align){
    case WA_LEFT: _clockleft = _config.left; break;
    case WA_RIGHT: _clockleft = dsp.width()-_clockwidth-_config.left; break;
    default:
      _clockleft = (dsp.width()/2 - _clockwidth/2)+_config.left;
      break;
  }
  char buf[4];
  strftime(buf, 4, "%H", &network.timeinfo);
  _dotsleft=_textWidth(buf);
}

#ifndef DSP_LCD
#if DSP_MODEL==DSP_ILI9225
  auto& ClockWidget::getRealDsp(){
    return dsp;
  }
#else
  Adafruit_GFX& ClockWidget::getRealDsp(){
  #ifdef PSFBUFFER
    if (_fb && _fb->ready()) return *_fb;
  #endif
    return dsp;
  }
#endif
void ClockWidget::_printClock(bool force){
  auto& gfx = getRealDsp();
  gfx.setTextSize(Clock_GFXfontPtr==nullptr?TIME_SIZE:1);
  gfx.setFont(Clock_GFXfontPtr);
  bool clockInTitle=!config.isScreensaver && _config.top<_timeheight; //DSP_SSD1306x32
  if(force){
    _clearClock();
    _getTimeBounds();
    #ifndef DSP_OLED
    if(CLOCKFONT_MONO) {
      gfx.setTextColor(config.theme.clockbg, config.theme.background);
      gfx.setCursor(_left(), _top());
      gfx.print("88:88");
    }
    #endif
    if(clockInTitle)
      gfx.setTextColor(config.theme.meta, config.theme.metabg);
    else
      gfx.setTextColor(config.theme.clock, config.theme.background);
    gfx.setCursor(_left(), _top());
    gfx.print(_timebuffer);
    if(_fullclock){
      // lines, date & dow
      bool fullClockOnScreensaver = (!config.isScreensaver || (_fb->ready() && FULL_SCR_CLOCK));
      _linesleft = _left()+_timewidth+_space;
      if(fullClockOnScreensaver){
        gfx.drawFastVLine(_linesleft, _top()-_timeheight, _timeheight, config.theme.div);
        gfx.drawFastHLine(_linesleft, _top()-(_timeheight)/2, CHARWIDTH * _superfont * 2 + _space, config.theme.div);
        /*  День тижня, секунди й дата раніше малювались вбудованим 5x7,
            розтягнутим у три й у два рази — звідси сходинки з квадратів.
            yoMono18 і yoMono12 мають рівно той самий крок (18 і 12 px),
            тому розмітка та розділові лінії лишаються на місці.  */
        int16_t dbl; const GFXfont* dfont = yoFont(_superfont, dbl);
        gfx.setFont(dfont);
        gfx.setTextSize(dfont ? 1 : _superfont);
        /*  Те саме й для дня тижня: чистимо місце перед написом.  */
        if(dfont){
          uint8_t dw = pgm_read_byte(&dfont->glyph[(uint8_t)'0' - dfont->first].xAdvance);
          gfx.fillRect(_linesleft+_space+1, _top()-CHARHEIGHT * _superfont,
                       dw*3, pgm_read_byte(&dfont->yAdvance), config.theme.background);
        }
        gfx.setCursor(_linesleft+_space+1, _top()-CHARHEIGHT * _superfont + dbl);
        gfx.setTextColor(config.theme.dow, config.theme.background);
        gfx.print(utf8Rus(LANG::dow[network.timeinfo.tm_wday], true));
        sprintf(_tmp, "%2d %s %d", network.timeinfo.tm_mday,LANG::mnths[network.timeinfo.tm_mon], network.timeinfo.tm_year+1900);
        strlcpy(_datebuf, utf8Rus(_tmp, true), sizeof(_datebuf));
        /*  Дату піднімаємо з розміру 1 до гладкого накреслення кроком 12:
            шість пікселів на літеру — це вже не читання, а вгадування.  */
        int16_t datebl; const GFXfont* datef = yoFont(2, datebl);
        uint16_t _datewidth = strlen(_datebuf) * (datef ? 12 : CHARWIDTH*_dateheight);
        gfx.setFont(datef);
        gfx.setTextSize(datef ? 1 : _dateheight);
        #if DSP_MODEL==DSP_GC9A01A
        gfx.setCursor((dsp.width()-_datewidth)/2, _top() + _space);
        #else
        gfx.setCursor(_left()+_clockwidth-_datewidth, _top() + _space + datebl);
        #endif
        gfx.setTextColor(config.theme.date, config.theme.background);
        gfx.print(_datebuf);
        gfx.setFont();
      }
    }
  }
  if(_fullclock || _superfont>0){
    int16_t sbl; const GFXfont* sfont = yoFont(_superfont, sbl);
    gfx.setFont(sfont);
    gfx.setTextSize(sfont ? 1 : _superfont);
    int16_t scx, scy;                    /* верхній лівий кут місця під секунди */
    if(!_fullclock){
      #ifndef CLOCKFONT5x7
      scx = _left()+_timewidth+_space; scy = _top()-_timeheight+_space;
      #else
      scx = _left()+_timewidth+_space; scy = _top();
      #endif
    }else{
      scx = _linesleft+_space+1;       scy = _top()-_timeheight;
    }
    /*  Гладке накреслення малює лише сам знак і тла під ним не заливає —
        на відміну від вбудованого 5x7. Тому місце під секунди чистимо самі,
        інакше кожна нова секунда лягає поверх попередньої.  */
    if(sfont){
      uint8_t sw = pgm_read_byte(&sfont->glyph[(uint8_t)'0' - sfont->first].xAdvance);
      gfx.fillRect(scx, scy, sw*2, pgm_read_byte(&sfont->yAdvance), config.theme.background);
    }
    gfx.setCursor(scx, scy + (sfont ? sbl : 0));
    gfx.setTextColor(config.theme.seconds, config.theme.background);
    sprintf(_tmp, "%02d", network.timeinfo.tm_sec);
    gfx.print(_tmp);
    gfx.setFont();
  }
  gfx.setTextSize(Clock_GFXfontPtr==nullptr?TIME_SIZE:1);
  gfx.setFont(Clock_GFXfontPtr);
  #ifndef DSP_OLED
  gfx.setTextColor(dots ? config.theme.clock : (CLOCKFONT_MONO?config.theme.clockbg:config.theme.background), config.theme.background);
  #else
  if(clockInTitle)
    gfx.setTextColor(dots ? config.theme.meta:config.theme.metabg, config.theme.metabg);
  else
    gfx.setTextColor(dots ? config.theme.clock:config.theme.background, config.theme.background);
  #endif
  dots=!dots;
  gfx.setCursor(_left()+_dotsleft, _top());
  gfx.print(":");
  gfx.setFont();
  if(_fb->ready()) _fb->display();
}

void ClockWidget::_clearClock(){
#ifdef PSFBUFFER
  if(_fb->ready()) _fb->clear();
  else
#endif
#ifndef CLOCKFONT5x7
  dsp.fillRect(_left(), _top()-_timeheight, _clockwidth, _clockheight+1, config.theme.background);
#else
  dsp.fillRect(_left(), _top(), _clockwidth+1, _clockheight+1, config.theme.background);
#endif
}

void ClockWidget::draw(){
  if(!_active || _locked) return;          /* заблокований — на його місці рядок обраного */
  _printClock(_getTime());
}

void ClockWidget::_draw(){
  if(!_active || _locked) return;
  _printClock(true);
}

void ClockWidget::_reset(){
#ifdef PSFBUFFER
  if(_fb->ready()) {
    _fb->freeBuffer();
    _getTimeBounds();
    _begin();
  }
#endif
}

void ClockWidget::_clear(){
  _clearClock();
}
#else //#ifndef DSP_LCD

void ClockWidget::_printClock(bool force){
  strftime(_timebuffer, sizeof(_timebuffer), "%H:%M", &network.timeinfo);
  if(force){
    dsp.setCursor(dsp.width()-5, 0);
    dsp.print(_timebuffer);
  }
  dsp.setCursor(dsp.width()-5+2, 0);
  dsp.print((network.timeinfo.tm_sec % 2 == 0)?":":" ");
}

void ClockWidget::_clearClock(){}

void ClockWidget::draw(){
  if(!_active) return;
  _printClock(true);
}
void ClockWidget::_draw(){
  if(!_active) return;
  _printClock(true);
}
void ClockWidget::_reset(){}
void ClockWidget::_clear(){}
#endif //#ifndef DSP_LCD

/**************************
      BITRATE WIDGET
 **************************/
void BitrateWidget::init(BitrateConfig bconf, uint16_t fgcolor, uint16_t bgcolor){
  Widget::init(bconf.widget, fgcolor, bgcolor);
  _dimension = bconf.dimension;
  _bitrate = 0;
  _format = BF_UNKNOWN;
  _charSize(bconf.widget.textsize, _charWidth, _textheight);
  memset(_buf, 0, 6);
}

void BitrateWidget::setBitrate(uint16_t bitrate){
  _bitrate = bitrate;
  if(_bitrate>999) _bitrate=999;
  _draw();
}

void BitrateWidget::setFormat(BitrateFormat format){
  _format = format;
  _draw();
}

//TODO move to parent
void BitrateWidget::_charSize(uint8_t textsize, uint8_t& width, uint16_t& height){
#ifndef DSP_LCD
  width = textsize * CHARWIDTH;
  height = textsize * CHARHEIGHT;
#else
  width = 1;
  height = 1;
#endif
}

void BitrateWidget::_draw(){
  _clear();
  if(!_active || _format == BF_UNKNOWN || _bitrate==0) return;
  dsp.drawRect(_config.left, _config.top, _dimension, _dimension, _fgcolor);
  dsp.fillRect(_config.left, _config.top + _dimension/2, _dimension, _dimension/2, _fgcolor);
  /*  Те саме, що й у годиннику: вбудований 5x7 у подвійному масштабі давав
      сходинки. yoMono12 має рівно той самий крок 12 px, тож віконце з
      бітрейтом лишається того ж розміру, змінюється лише накреслення.  */
  int16_t bbl; const GFXfont* bfont = yoFont(_config.textsize, bbl);
  dsp.setFont(bfont);
  dsp.setTextSize(bfont ? 1 : _config.textsize);
  dsp.setTextColor(_fgcolor, _bgcolor);
  snprintf(_buf, 6, "%d", _bitrate);
  dsp.setCursor(_config.left + _dimension/2 - _charWidth*strlen(_buf)/2 + 1, _config.top + _dimension/4 - _textheight/2+1 + bbl);
  dsp.print(_buf);
  dsp.setTextColor(_bgcolor, _fgcolor);
  dsp.setCursor(_config.left + _dimension/2 - _charWidth*3/2 + 1, _config.top + _dimension - _dimension/4 - _textheight/2 + bbl);
  switch(_format){
    case BF_MP3:  dsp.print("MP3"); break;
    case BF_AAC:  dsp.print("AAC"); break;
    case BF_FLAC: dsp.print("FLC"); break;
    case BF_OGG:  dsp.print("OGG"); break;
    case BF_WAV:  dsp.print("WAV"); break;
    default:                        break;
  }
  dsp.setFont();
}

void BitrateWidget::_clear() {
  dsp.fillRect(_config.left, _config.top, _dimension, _dimension, _bgcolor);
}


/**************************
      PLAYLIST WIDGET
 **************************/
void PlayListWidget::init(ScrollWidget* current){
  Widget::init({0, 0, 0, WA_LEFT}, 0, 0);
  _current = current;
  #ifndef DSP_LCD
  _plItemHeight = playlistConf.widget.textsize*(CHARHEIGHT-1)+playlistConf.widget.textsize*4;
  _plTtemsCount = round((float)dsp.height()/_plItemHeight);
  if(_plTtemsCount%2==0) _plTtemsCount++;
  _plCurrentPos = _plTtemsCount/2;
  _plYStart = (dsp.height() / 2 - _plItemHeight / 2) - _plItemHeight * (_plTtemsCount - 1) / 2 + playlistConf.widget.textsize*2;
  #else
  _plTtemsCount = PLMITEMS;
  _plCurrentPos = 1;
  #endif
}

uint8_t PlayListWidget::_fillPlMenu(int from, uint8_t count) {
  int     ls      = from;
  uint8_t c       = 0;
  bool    finded  = false;
  if (config.playlistLength() == 0) {
    return 0;
  }
  File playlist = config.SDPLFS()->open(REAL_PLAYL, "r");
  File index = config.SDPLFS()->open(REAL_INDEX, "r");
  while (true) {
    if (ls < 1) {
      ls++;
      _printPLitem(c, "");
      c++;
      continue;
    }
    if (!finded) {
      index.seek((ls - 1) * 4, SeekSet);
      uint32_t pos;
      index.readBytes((char *) &pos, 4);
      finded = true;
      index.close();
      playlist.seek(pos, SeekSet);
    }
    bool pla = true;
    while (pla) {
      pla = playlist.available();
      String stationName = playlist.readStringUntil('\n');
      stationName = stationName.substring(0, stationName.indexOf('\t'));
      if(config.store.numplaylist && stationName.length()>0) stationName = String(from+c)+" "+stationName;
      _printPLitem(c, stationName.c_str());
      c++;
      if (c >= count) break;
    }
    break;
  }
  playlist.close();
  return c;
}
#ifndef DSP_LCD
void PlayListWidget::drawPlaylist(uint16_t currentItem) {
  /*  Нерухомий список малюється тим самим кодом, що й прокрутка: інакше
      після зупинки картинка хоч трохи, а стрибала б.  */
  if(!_chrome) _drawChrome();
  drawSmooth((float)currentItem);
}

void PlayListWidget::enterPage(){ _chrome = false; }

/*  Рамка списку й кнопки — один раз при відкритті сторінки. Під час
    прокрутки вони не змінюються, тож і не передаються.  */
void PlayListWidget::_drawChrome(){
  dsp.fillScreen(PL_C_BLACK);
  dsp.drawRect(PL_X0 - 1, PL_TOP - 1, PL_LIST_W + 2, PL_ROWS * PL_ROW_H + 2, PL_C_BAND);
  for(uint8_t i = 0; i < 4; i++) drawButton(i, false);
  _chrome = true;
}

void PlayListWidget::drawButton(uint8_t i, bool on){
  static const uint8_t img[4][2] = {
    { NXB_UP, NXB_UP_ON }, { NXB_DOWN, NXB_DOWN_ON }, { NXB_PLAY, NXB_PLAY_ON }, { NXB_BACK, NXB_BACK_ON } };
  if(i > 3) return;
  dsp.startWrite();
  dsp.setAddrWindow(PL_BTN_X, PL_BTN_Y(i), NXBTN_W, NXBTN_H);
  dsp.writePixels((uint16_t*)nxButtons[img[i][on ? 1 : 0]], NXBTN_W * NXBTN_H);
  dsp.endWrite();
}

/*  Читаємо потрібний шматок списку один раз і кладемо в пам'ять.  */
void PlayListWidget::loadCache(int from){
  int cs = config.playlistLength();
  if(cs < 1) return;
  for(uint8_t i=0;i<PL_CACHE;i++) _cache[i][0] = 0;
  _cacheFrom = from;
  File playlist = config.SDPLFS()->open(REAL_PLAYL, "r");
  File index    = config.SDPLFS()->open(REAL_INDEX, "r");
  if(!playlist || !index){ if(playlist) playlist.close(); if(index) index.close(); return; }
  int first = from;
  while(first < 1) first++;
  if(first <= cs){
    index.seek((first - 1) * 4, SeekSet);
    uint32_t pos = 0;
    index.readBytes((char*)&pos, 4);
    playlist.seek(pos, SeekSet);
    for(uint8_t i = first - from; i < PL_CACHE; i++){
      if(!playlist.available()) break;
      String nm = playlist.readStringUntil('\n');
      int tab = nm.indexOf('\t');
      if(tab > 0) nm = nm.substring(0, tab);
      if(config.store.numplaylist && nm.length()>0) nm = String(from + i) + " " + nm;
      strlcpy(_cache[i], nm.c_str(), sizeof(_cache[i]));
    }
  }
  index.close();
  playlist.close();
}

/*  pos — дробове положення списку в рядках. Ціла частина каже, яка станція
    посередині, дробова дає зсув у пікселях.  */
void PlayListWidget::drawSmooth(float pos){
  int   base = (int)floorf(pos);
  float frac = pos - base;
  int   from = base - PL_CUR - 1;
  if(_cacheFrom == 0 || from < _cacheFrom || from + PL_ROWS + 2 > _cacheFrom + PL_CACHE)
    loadCache(from - 2);
  if(!_row){
    _row = (uint16_t*)heap_caps_malloc((size_t)PL_LIST_W * PL_ROW_H * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if(!_row) return;
  }
  static int8_t dmaOk = -1;
  if(dmaOk < 0) dmaOk = spidmaBegin() ? 1 : 0;

  const int16_t top = PL_TOP, bot = PL_TOP + PL_ROWS * PL_ROW_H;
  const int16_t band0 = PL_TOP + PL_CUR * PL_ROW_H, band1 = band0 + PL_ROW_H;
  int16_t off = (int16_t)(frac * PL_ROW_H);
  int cs = config.playlistLength();
  extern uint32_t yoPlRender, yoPlBlit;
  uint32_t tR = 0, tB = 0;

  _rowCanvas.setBuffer(_row, PL_LIST_W, PL_ROW_H);
  _rowCanvas.setSwap(true);
  dsp.startWrite();
  if(!_bandOnly){ _mqPos = base; _mqFrac = (off != 0); }
  for(int r = -1; r <= PL_ROWS; r++){
    if(_bandOnly && r != PL_CUR) continue;
    int16_t y  = top + r * PL_ROW_H - off;
    int16_t y0 = y < top ? top : y;
    int16_t y1 = (y + PL_ROW_H) > bot ? bot : (int16_t)(y + PL_ROW_H);
    if(y1 <= y0) continue;
    int idx = base - PL_CUR + r;                  /* номер станції в цьому рядку */
    const char* nm = "";
    int ci = idx - _cacheFrom;
    if(idx >= 1 && idx <= cs && ci >= 0 && ci < PL_CACHE) nm = _cache[ci];

    uint32_t ta = micros();
    /*  Смуги чергуються за номером станції й їдуть разом із нею; жовта смуга
        вибору стоїть на місці, і рядок перефарбовується рівно там, де в неї
        заходить.  */
    uint16_t stripe = (idx & 1) ? PL_C_STRIPE : PL_C_BLACK;
    int16_t lb0 = band0 - y, lb1 = band1 - y;     /* смуга вибору в координатах рядка */
    _rowCanvas.fillLines(0, PL_ROW_H, stripe);
    if(lb1 > 0 && lb0 < PL_ROW_H) _rowCanvas.fillLines(lb0, lb1, PL_C_BAND);
    _rowCanvas.setBand(lb0, lb1, PL_C_BANDTXT);
    /*  Звичайна Verdana 11pt і рядкові літери: жирні великі 12pt виглядали
        важко й різали назви на першому ж слові.  */
    if(nm[0]){
      /*  Лупа: поза жовтою смугою — звичайна Verdana 11, у смузі — трохи
          більша 13. Рядок, що заходить у смугу, «виростає» саме в її межах.  */
      bool band = (r == PL_CUR && off == 0);
      int16_t sx = band ? _bandShift : 0;
      int16_t wrap = (band && _mqW > PL_LIST_W - 16) ? _mqW + 40 : 0;
      bool playing = (player.status() == PLAYING && !player.remoteStationName && idx == (int)config.lastStation());
      plRowText(nm, lb0, lb1, sx, playing, wrap);
    }
    _rowCanvas.setBand(0, 0, 0);
    tR += micros() - ta;

    uint32_t tb = micros();
    const uint16_t* src = _row + (size_t)(y0 - y) * PL_LIST_W;
    size_t n = (size_t)(y1 - y0) * PL_LIST_W;
    dsp.setAddrWindow(PL_X0, y0, PL_LIST_W, y1 - y0);
    if(dmaOk) spidmaWrite(src, n * 2);
    else      dsp.writePixels((uint16_t*)src, n, true, true);   /* запасний шлях: байти вже переставлені */
    tB += micros() - tb;
  }
  dsp.endWrite();
  _rowCanvas.setSwap(false);
  yoPlRender += tR;
  yoPlBlit   += tB;
}

/*  Бігучий рядок у жовтій смузі списку станцій: довга назва, що не
    влазить, повільно їде вліво, потім пауза й знову з початку. Лише коли
    список стоїть — під час прокрутки не заважає.  */
void PlayListWidget::marqueeTick(){
  uint32_t now = millis();
  if(_mqFrac){ _bandShift = 0; return; }
  if(_mqPos != _mqShownPos){
    _mqShownPos = _mqPos; _bandShift = 0; _mqT = now + 600;   /* короткий подих, поки список осяде */
    int ci = _mqPos - _cacheFrom;
    _mqW = (_mqPos >= 1 && ci >= 0 && ci < PL_CACHE && _cache[ci][0]) ? plTextWidth(_cache[ci]) : 0;
    return;
  }
  if(_mqW <= PL_LIST_W - 16) return;               /* влазить — стоїть */
  if((int32_t)(now - _mqT) < 0) return;
  /*  По колу й без зупинок: за назвою — проміжок 40 пікселів і вона ж
      знову, тож рядок тече безперервно, по пікселю кожні 25 мс.  */
  _mqT = now + 25;
  _bandShift++;
  if(_bandShift >= (int16_t)(_mqW + 40)) _bandShift = 0;
  _bandOnly = true;
  drawSmooth((float)_mqPos);
  _bandOnly = false;
}

/*  Текст рядка списку. Поза жовтою смугою — дрібний шрифт, у смузі —
    крупний («лупа»). Те, що зараз грає, видно завжди: крупний жовтий текст
    і знак ▶ ліворуч, де б рядок не стояв.  */
static void plRowText(const char* nm, int16_t lb0, int16_t lb1, int16_t sx, bool playing, int16_t wrap){
  char big[96]; strlcpy(big, utf8Rus(nm, false), sizeof(big));
  /*  Лупа — лише в жовтій смузі. Те, що зараз грає, поза смугою звичайного
      розміру, але жовте й зі знаком ▶: раніше воно лишалось збільшеним і
      після прокрутки виглядало як лупа, що «застрягла».  */
  int16_t x0 = playing ? 12 : 8;
  uint16_t cOut = playing ? PL_C_BAND : PL_C_TXT;
  _rowCanvas.textFast(big, x0, PL_BASE, cOut, &yoUI11, 0, lb0);
  _rowCanvas.textFast(big, x0, PL_BASE, cOut, &yoUI11, lb1, PL_ROW_H);
  _rowCanvas.textFast(big, x0 - sx, PL_BASE_BIG, PL_C_TXT, &yoUI15, lb0, lb1);
  if(wrap) _rowCanvas.textFast(big, x0 - sx + wrap, PL_BASE_BIG, PL_C_TXT, &yoUI15, lb0, lb1);   /* друга копія — коло без розриву */
  if(playing){
    /*  під знаком у смузі — жовта підкладка: бігучий рядок іде під неї  */
    for(int16_t yy = (lb0 > 0 ? lb0 : 0); yy < (lb1 < PL_ROW_H ? lb1 : PL_ROW_H); yy++)
      for(int16_t xx = 0; xx < 11; xx++) _rowCanvas.drawPixel(xx, yy, PL_C_BAND);
    /*  ▶: у смузі — чорний, поза нею — жовтий  */
    for(int16_t yy = 0; yy < 9; yy++){
      int16_t py = PL_ROW_H / 2 - 4 + yy, w = yy < 5 ? yy : 8 - yy;
      uint16_t c = (py >= lb0 && py < lb1) ? PL_C_BANDTXT : PL_C_BAND;
      for(int16_t xx = 0; xx <= w; xx++) _rowCanvas.drawPixel(2 + xx, py, c);
    }
  }
}

void plGenericChrome(){
  dsp.fillScreen(PL_C_BLACK);
  dsp.drawRect(PL_X0 - 1, PL_TOP - 1, PL_LIST_W + 2, PL_ROWS * PL_ROW_H + 2, PL_C_BAND);
  for(uint8_t i = 0; i < 4; i++) plGenericButton(i, false);
}

void plGenericButton(uint8_t i, bool on){
  static const uint8_t img[4][2] = {
    { NXB_UP, NXB_UP_ON }, { NXB_DOWN, NXB_DOWN_ON }, { NXB_PLAY, NXB_PLAY_ON }, { NXB_BACK, NXB_BACK_ON } };
  if(i > 3) return;
  dsp.startWrite();
  dsp.setAddrWindow(PL_BTN_X, PL_BTN_Y(i), NXBTN_W, NXBTN_H);
  dsp.writePixels((uint16_t*)nxButtons[img[i][on ? 1 : 0]], NXBTN_W * NXBTN_H);
  dsp.endWrite();
}

uint16_t plTextWidth(const char* utf8){
  const char* s = utf8Rus(utf8, false);
  uint16_t w = 0;
  for(; *s; s++){
    uint8_t c = (uint8_t)*s;
    if(c < yoUI15.first || c > yoUI15.last) continue;
    w += pgm_read_byte(&yoUI15.glyph[c - yoUI15.first].xAdvance);   /* у смузі — крупний шрифт */
  }
  return w;
}

void plGenericDraw(float pos, int count, const char* (*nameAt)(int), int16_t shift, bool bandOnly, int playIdx, int16_t wrapW, int16_t rightPad){
  static uint16_t* row = nullptr;
  if(!row){
    row = (uint16_t*)heap_caps_malloc((size_t)PL_LIST_W * PL_ROW_H * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if(!row) return;
  }
  static int8_t dmaOk = -1;
  if(dmaOk < 0) dmaOk = spidmaBegin() ? 1 : 0;
  int   base = (int)floorf(pos);
  float frac = pos - base;
  const int16_t top = PL_TOP, bot = PL_TOP + PL_ROWS * PL_ROW_H;
  const int16_t band0 = PL_TOP + PL_CUR * PL_ROW_H, band1 = band0 + PL_ROW_H;
  int16_t off = (int16_t)(frac * PL_ROW_H);
  _rowCanvas.setBuffer(row, PL_LIST_W, PL_ROW_H);
  _rowCanvas.setSwap(true);
  _rowCanvas.setClipW(rightPad > 0 ? PL_LIST_W - rightPad : 0);
  dsp.startWrite();
  for(int r = -1; r <= PL_ROWS; r++){
    if(bandOnly && r != PL_CUR) continue;
    int16_t y  = top + r * PL_ROW_H - off;
    int16_t y0 = y < top ? top : y;
    int16_t y1 = (y + PL_ROW_H) > bot ? bot : (int16_t)(y + PL_ROW_H);
    if(y1 <= y0) continue;
    int idx = base - PL_CUR + r;
    const char* nm = (idx >= 1 && idx <= count) ? nameAt(idx) : "";
    uint16_t stripe = (idx & 1) ? PL_C_STRIPE : PL_C_BLACK;
    int16_t lb0 = band0 - y, lb1 = band1 - y;
    _rowCanvas.fillLines(0, PL_ROW_H, stripe);
    if(lb1 > 0 && lb0 < PL_ROW_H) _rowCanvas.fillLines(lb0, lb1, PL_C_BAND);
    _rowCanvas.setBand(lb0, lb1, PL_C_BANDTXT);
    if(nm && nm[0]){
      bool band = (r == PL_CUR && off == 0);
      plRowText(nm, lb0, lb1, band ? shift : 0, idx == playIdx, band ? wrapW : 0);
    }
    _rowCanvas.setBand(0, 0, 0);
    const uint16_t* src = row + (size_t)(y0 - y) * PL_LIST_W;
    size_t n = (size_t)(y1 - y0) * PL_LIST_W;
    dsp.setAddrWindow(PL_X0, y0, PL_LIST_W, y1 - y0);
    if(dmaOk) spidmaWrite(src, n * 2);
    else      dsp.writePixels((uint16_t*)src, n, true, true);
  }
  dsp.endWrite();
  _rowCanvas.setSwap(false);
  _rowCanvas.setClipW(0);
}

void PlayListWidget::_printPLitem(uint8_t pos, const char* item){
  /*  Шрифт треба задавати явно: інакше список успадковує накреслення,
      залишене іншим віджетом, ще й множить його на textsize — звідси
      велетенські літери й поїхані позиції.  */
  int16_t bl; const GFXfont* f = yoFont(playlistConf.widget.textsize, bl);
  dsp.setFont(f);
  dsp.setTextSize(f ? 1 : playlistConf.widget.textsize);
  if (pos == _plCurrentPos) {
    _current->setText(item);
  } else {
    uint8_t plColor = (abs(pos - _plCurrentPos)-1)>4?4:abs(pos - _plCurrentPos)-1;
    dsp.setTextColor(config.theme.playlist[plColor], config.theme.background);
    dsp.fillRect(0, _plYStart + pos * _plItemHeight - 1, dsp.width(), _plItemHeight - 2, config.theme.background);
    dsp.setCursor(TFT_FRAMEWDT, _plYStart + pos * _plItemHeight + bl);
    dsp.print(utf8Rus(item, true));
  }
  dsp.setFont();
}
#else
void PlayListWidget::_printPLitem(uint8_t pos, const char* item){
  if (pos == _plCurrentPos) {
    _current->setText(item);
  } else {
    dsp.setCursor(1, pos);
    char tmp[dsp.width()] = {0};
    strlcpy(tmp, utf8Rus(item, true), dsp.width());
    dsp.print(tmp);
  }
}

void PlayListWidget::drawPlaylist(uint16_t currentItem) {
  dsp.clear();
  _fillPlMenu(currentItem - _plCurrentPos, _plTtemsCount);
  dsp.setCursor(0,1);
  dsp.write(uint8_t(126));
}
#endif


#endif // #if DSP_MODEL!=DSP_DUMMY
