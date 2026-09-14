#include "../core/options.h"
#include "menuwidgets.h"
#ifdef USE_YOMENU

#include "../displays/dspcore.h"
#include "../displays/tools/utf8Rus.h"
extern DspCore dsp;
#include "uicanvas.h"
#define dsp ui          /* віджети меню малюють у кадр меню (menu/uicanvas) */

/*  ---------------- UiText ---------------- */

void UiText::init(WidgetConfig conf, const GFXfont* font, uint16_t boxw, uint16_t boxh,
                  uint16_t fgcolor, uint16_t bgcolor){
  Widget::init(conf, fgcolor, bgcolor);
  _font = font; _boxw = boxw; _boxh = boxh;
  _text[0] = _old[0] = '\0';
}

void UiText::setText(const char* txt){
  strlcpy(_text, utf8Rus(txt, false), sizeof(_text));
  if(strcmp(_old, _text) == 0) return;          /* нічого не змінилось — не чіпаємо екран */
  if(_active && !_locked){ _clear(); _draw(); }
}

void UiText::setText(int val, const char* fmt){
  char buf[32]; snprintf(buf, sizeof(buf), fmt, val); setText(buf);
}

void UiText::_clear(){
  if(_boxw && _boxh) dsp.fillRect(_config.left + _inset, _config.top, _boxw - 2 * _inset, _boxh, _bgcolor);
}

void UiText::_draw(){
  if(!_active || _locked) return;
  dsp.setFont(_font);
  dsp.setTextSize(1);
  dsp.setTextColor(_fgcolor);
  int16_t x = _config.left;
  if(_config.align != WA_LEFT){
    int16_t x1,y1; uint16_t w,h;
    dsp.getTextBounds(_text, 0, 40, &x1, &y1, &w, &h);
    x = (_config.align == WA_RIGHT) ? (_config.left + _boxw - (int16_t)w)
                                    : (_config.left + (_boxw - (int16_t)w)/2);
  }
  dsp.setCursor(x, _config.top + _boxh - 4);     /* у GFX-шрифті y — базова лінія */
  dsp.print(_text);
  dsp.setFont();
  strlcpy(_old, _text, sizeof(_old));
}

/*  ---------------- UiSlider ---------------- */

void UiSlider::init(WidgetConfig conf, const GFXfont* font, uint16_t boxw,
                    int lo, int hi, uint16_t fgcolor, uint16_t bgcolor, uint16_t barcolor){
  Widget::init(conf, fgcolor, bgcolor);
  _font = font; _boxw = boxw; _lo = lo; _hi = hi; _barcolor = barcolor;
  _val = lo; _oldfill = -1;
}

void UiSlider::setLabel(const char* label){ strlcpy(_label, label, sizeof(_label)); }

int UiSlider::valueAt(uint16_t x) const {
  const int16_t kr = 7;                          /* ручка: крайні значення — по центру ручки */
  /*  Западинка біля нуля. На смугу припадає 32 поділки, тобто на одну
      поділку менше десятка пікселів, і поціляти пальцем рівно в нуль
      майже неможливо. Тому середина притягує: шість пікселів праворуч і
      ліворуч від нуля дають рівно нуль.  */
  const int span = (int)_boxw - 2 * kr;
  if(_lo < 0 && _hi > 0 && span > 0){
    int zx = (int)_config.left + kr + (int)((long)(0 - _lo) * span / (_hi - _lo));
    if((int)x >= zx - 6 && (int)x <= zx + 6) return 0;
  }
  int v = _lo + (int)lroundf((float)((int)x - (int)_config.left - kr) * (_hi - _lo) / (span > 0 ? span : 1));
  if(v < _lo) v = _lo;
  if(v > _hi) v = _hi;
  return v;
}

void UiSlider::setValue(int val){
  if(val < _lo) val = _lo;
  if(val > _hi) val = _hi;
  if(val == _val) return;                        /* значення те саме — виходимо */
  _val = val;
  /*  Малює лише задача дисплея, у своєму такті. Дотик приходить із головного
      циклу, і якби він малював сам, два ядра писали б у шину дисплея
      одночасно — саме звідси беруться артефакти на екрані.  */
}

void UiSlider::_clear(){ dsp.fillRect(_config.left, _config.top - 2, _boxw, 35, _bgcolor); }

/*  Смуга — тонка доріжка з круглою ручкою. Ручка не стрибає до нового
    значення, а доїжджає; малюємо лише доріжку під нею (у кадрі це дешево,
    а на екран іде тільки змінене).  */
static const int16_t SL_KR = 7;                  /* радіус ручки */
void UiSlider::_drawbar(){
  const int16_t span = (int16_t)_boxw - 2 * SL_KR;
  int target = (int)((long)(_val - _lo) * span / (_hi - _lo));
  if(target < 0) target = 0;
  if(target > span) target = span;
  if(_oldfill < 0) _oldfill = target;            /* перше малювання — одразу на місце */
  else if(target != _oldfill){
    int d = target - _oldfill;
    int step = ((d < 0 ? -d : d) * 5) >> 4;
    if(step < 1) step = 1;
    _oldfill += (d < 0 ? -step : step);
  }
  /*  доріжка 4 пікселі нижче підпису: хвостики «у», «р» підпису не зачіпаємо  */
  const int16_t ty = _config.top + 22, cy = ty + 2;
  const float kx = _config.left + SL_KR + _oldfill;
  dsp.fillRect(_config.left, _config.top + 16, _boxw, 17, _bgcolor);
  dsp.box(_config.left, ty, _boxw, 4, 2, 0x39E7, _bgcolor);
  if(_lo < 0 && _hi > 0){
    /*  двобічна (баланс): заповнення від середини  */
    const float zx = _config.left + SL_KR + (float)(0 - _lo) * span / (_hi - _lo);
    float a = zx < kx ? zx : kx, b = zx < kx ? kx : zx;
    if(b - a >= 1) dsp.fillRect((int16_t)a, ty, (int16_t)(b - a), 4, _barcolor);
    dsp.fillRect((int16_t)zx - 1, ty - 3, 2, 10, 0x8410);
  }else if(kx - _config.left >= 2){
    dsp.box(_config.left, ty, (int16_t)(kx - _config.left) + 2, 4, 2, _barcolor, 0x39E7);
  }
  dsp.fillCircleAA(kx, cy, 7.5f, _bgcolor);                  /* тонкий вирізок довкола ручки */
  dsp.fillCircleAA(kx, cy, 6.3f, _fgcolor);

  char v[12]; snprintf(v, sizeof(v), "%d", _val);
  if(strcmp(v, _shown) != 0){          /* значення не змінилось — не блимаємо */
    /*  Стовпчик чисел вирівнюємо по ширині пера, а не по ширині чорнил:
        getTextBounds() повертає рамку самих чорнил без бічних виносів, і
        якщо рахувати по ній, у кожної цифри свій правий край — колонка
        виходить рваною. Ширина пера у цифр однакова, тож край стає рівним. */
    uint16_t adv = 0;
    if(_font){
      for(const char* q = v; *q; q++){
        uint8_t c = (uint8_t)*q;
        if(c < _font->first || c > _font->last) continue;
        adv += pgm_read_byte(&_font->glyph[c - _font->first].xAdvance);
      }
    }else{
      adv = (uint16_t)(strlen(v) * 6);        /* вбудований шрифт 6 пікселів */
    }
    dsp.setFont(_font); dsp.setTextSize(1);
    /*  Чистимо на рядок вище базової лінії: верхівки цифр вилазять над
        _config.top, і при коротшому числі там лишалися хвости.  */
    dsp.fillRect(_config.left + _boxw - 44, _config.top - 2, 44, 17, _bgcolor);
    dsp.setTextColor(_fgcolor);
    dsp.setCursor(_config.left + _boxw - (int16_t)adv, _config.top + 12);
    dsp.print(v);
    dsp.setFont();
    strlcpy(_shown, v, sizeof(_shown));
  }
}

/*  Крок анімації. Кличеться з такту сторінки налаштувань — там само, де
    малюється решта меню, тож малює завжди та сама задача.  */
void UiSlider::loop(){
  if(!_active || _locked) return;
  uint32_t now = millis();
  if(now - _animTick < 20) return;
  _animTick = now;
  int target = (int)((long)(_val - _lo) * ((int)_boxw - 14) / (_hi - _lo));
  if(target != _oldfill) _drawbar();
}

void UiSlider::_draw(){
  if(!_active || _locked) return;
  dsp.setFont(_font); dsp.setTextSize(1); dsp.setTextColor(_fgcolor);
  dsp.setCursor(_config.left, _config.top + 12);
  dsp.print(utf8Rus(_label, false));
  dsp.setFont();
  _oldfill = -1;
  _shown[0] = 0;                       /* після повної перемальовки писати заново */
  _drawbar();
}

/*  ---------------- UiCheck ---------------- */

void UiCheck::init(WidgetConfig conf, const GFXfont* font, uint16_t boxw,
                   uint16_t fgcolor, uint16_t bgcolor, uint16_t oncolor){
  Widget::init(conf, fgcolor, bgcolor);
  _font = font; _boxw = boxw; _oncolor = oncolor; _drawn = false;
}

void UiCheck::setLabel(const char* label){ strlcpy(_label, label, sizeof(_label)); }

void UiCheck::setValue(bool on){
  if(on == _on && _drawn) return;                /* стан не змінився */
  _on = on;
  _dirty = true;                                 /* намалює такт сторінки */
}

/*  Квадратик із заокругленими кутами: вимкнений — тонка сіра рамка,
    увімкнений — жовтий із темною галочкою.  */
static void checkBox(int16_t x, int16_t y, bool on, uint16_t oncolor, uint16_t bg){
  if(on){
    dsp.box(x, y, 16, 16, 4, oncolor, bg);
    dsp.lineAA(x + 4.0f, y + 8.3f, x + 6.9f, y + 11.2f, 2.1f, bg);
    dsp.lineAA(x + 6.9f, y + 11.2f, x + 12.2f, y + 5.0f, 2.1f, bg);
  }else{
    dsp.frame(x, y, 16, 16, 4, 1.5f, 0x8410, bg, bg);
  }
}

/*  З тієї ж причини, що й у повзунка: малює лише задача дисплея.  */
void UiCheck::loop(){
  if(!_dirty || !_active || _locked) return;
  _dirty = false;
  checkBox(_config.left, _config.top - 1, _on, _oncolor, _bgcolor);
  _drawn = true;
}

void UiCheck::_clear(){ dsp.fillRect(_config.left, _config.top - 1, _boxw, 18, _bgcolor); }

void UiCheck::_draw(){
  if(!_active || _locked) return;
  checkBox(_config.left, _config.top - 1, _on, _oncolor, _bgcolor);
  dsp.setFont(_font); dsp.setTextSize(1); dsp.setTextColor(_fgcolor);
  dsp.setCursor(_config.left + 25, _config.top + 12);
  dsp.print(utf8Rus(_label, false));
  dsp.setFont();
  _drawn = true;
}

/*  ---------------- UiSeg ---------------- */

void UiSeg::init(WidgetConfig conf, const GFXfont* font, uint16_t boxw, uint16_t boxh,
                 uint16_t fgcolor, uint16_t bgcolor, uint16_t oncolor, uint16_t panel){
  Widget::init(conf, fgcolor, bgcolor);
  _font = font; _boxw = boxw; _boxh = boxh; _oncolor = oncolor; _panel = panel;
}

void UiSeg::setItems(uint8_t n, const char* const* labels){
  _n = n > UISEG_MAX ? UISEG_MAX : n;
  for(uint8_t i = 0; i < _n; i++) _lbl[i] = labels[i];
}

void UiSeg::setSel(int8_t i){
  if(i == _sel) return;
  _sel = i;
  _dirty = true;
}

int8_t UiSeg::indexAt(uint16_t x) const {
  if(!_n || (int)x < (int)_config.left || (int)x >= (int)_config.left + (int)_boxw) return -1;
  /*  Проміжки між кнопками віддаємо найближчій: палець не мусить
      влучати в чотирипіксельну щілину.  */
  int i = ((int)x - (int)_config.left + 2) / (_segW() + 4);
  return (int8_t)(i >= _n ? _n - 1 : i);
}

/*  Вибір не перескакує, а переїжджає: жовта плашка за кілька кадрів
    пливе від старої кнопки до нової (у кадрі меню це дешево — на екран
    ідуть лише змінені квадрати).  */
void UiSeg::loop(){
  if(!_active || _locked) return;
  const bool moving = _sel >= 0 && _pos >= 0 && fabsf(_pos - _sel) > 0.001f;
  if(!_dirty && !moving) return;
  uint32_t now = millis();
  if(moving && !_dirty && now - _animT < 16) return;
  _dirty = false;
  _animT = now;
  if(_sel < 0) _pos = -1;
  else if(_pos < 0) _pos = _sel;                 /* нічого не було вибрано — просто засвітити */
  else{
    float d = _sel - _pos;
    _pos += d * 0.38f;
    if(fabsf(_sel - _pos) < 0.04f) _pos = _sel;
  }
  _paint();
}

void UiSeg::_clear(){ dsp.fillRect(_config.left, _config.top, _boxw, _boxh, _bgcolor); }

void UiSeg::_paint(){
  int16_t w = _segW();
  /*  тло кнопок; і щілини між ними — плашка, що їхала, лишала там слід  */
  dsp.fillRect(_config.left, _config.top, _boxw, _boxh, _bgcolor);
  for(uint8_t i = 0; i < _n; i++) dsp.box(_config.left + i * (w + 4), _config.top, w, _boxh, 7, _panel, _bgcolor);
  /*  плашка вибору — поверх, у своєму місці (між кнопками теж)  */
  if(_pos >= 0){
    float fx = _config.left + _pos * (w + 4);
    int16_t xi = (int16_t)lroundf(fx);
    if(fabsf(fx - xi) < 0.01f) dsp.box(xi, _config.top, w, _boxh, 7, _oncolor, _bgcolor);
    else{
      /*  дробове положення: ліворуч і праворуч краї змішуємо з тлом  */
      dsp.fillRoundRectAA(xi, _config.top, w, _boxh, 7, _oncolor);
    }
  }
  dsp.setFont(_font); dsp.setTextSize(1);
  for(uint8_t i = 0; i < _n; i++){
    int16_t x = _config.left + i * (w + 4);
    bool on = _pos >= 0 && fabsf(_pos - i) < 0.5f;
    char t[24]; snprintf(t, sizeof(t), "%s", utf8Rus(_lbl[i] ? _lbl[i] : "", false));
    int16_t x1, y1; uint16_t tw, th;
    dsp.getTextBounds(t, 0, 40, &x1, &y1, &tw, &th);
    dsp.setTextColor(on ? _bgcolor : _fgcolor);
    dsp.setCursor(x + (w - (int16_t)tw) / 2 - x1, _config.top + _boxh / 2 + 5);
    dsp.print(t);
  }
  dsp.setFont();
}

void UiSeg::_draw(){
  if(!_active || _locked) return;
  if(_sel < 0) _pos = -1;
  else if(_pos < 0 || fabsf(_pos - _sel) > 0.001f) _pos = _sel;   /* сторінка відкрилась — одразу на місці */
  _paint();
}

#endif
