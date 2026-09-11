#include "../core/options.h"
#include "menuwidgets.h"
#ifdef USE_YOMENU

#include "../displays/dspcore.h"
#include "../displays/tools/utf8Rus.h"
extern DspCore dsp;

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
  if(_boxw && _boxh) dsp.fillRect(_config.left, _config.top, _boxw, _boxh, _bgcolor);
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
  /*  Западинка біля нуля. На смугу припадає 32 поділки, тобто на одну
      поділку менше десятка пікселів, і поціляти пальцем рівно в нуль
      майже неможливо. Тому середина притягує: шість пікселів праворуч і
      ліворуч від нуля дають рівно нуль.  */
  if(_lo < 0 && _hi > 0 && _boxw){
    int zx = (int)_config.left + (int)((long)(0 - _lo) * _boxw / (_hi - _lo));
    if((int)x >= zx - 6 && (int)x <= zx + 6) return 0;
  }
  int v = _lo + (int)((long)((int)x - (int)_config.left) * (_hi - _lo) / (_boxw ? _boxw : 1));
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

void UiSlider::_clear(){ dsp.fillRect(_config.left, _config.top, _boxw, 30, _bgcolor); }

/*  Малюємо лише те, що змінилось: різницю заповнення смуги й саме число.  */
void UiSlider::_drawbar(){
  int target = (int)((long)(_val - _lo) * _boxw / (_hi - _lo));
  if(target < 0) target = 0;
  if(target > (int)_boxw) target = _boxw;
  int16_t by = _config.top + 16;
  if(_oldfill < 0){                    /* перше малювання — одразу на місце */
    dsp.fillRect(_config.left, by, _boxw, 10, _bgcolor);
    dsp.fillRect(_config.left, by, target, 10, _barcolor);
    _oldfill = target;
  }else if(target != _oldfill){
    /*  Смуга не стрибає до нового значення, а доїжджає: за крок проходить
        частину відстані, що лишилась. Домальовуємо тільки різницю, тому
        рух нічого не блимає.  */
    int d = target - _oldfill;
    int step = ((d < 0 ? -d : d) * 5) >> 4;
    if(step < 1) step = 1;
    int nf = _oldfill + (d < 0 ? -step : step);
    if(d > 0) dsp.fillRect(_config.left + _oldfill, by, nf - _oldfill, 10, _barcolor);
    else      dsp.fillRect(_config.left + nf, by, _oldfill - nf, 10, _bgcolor);
    _oldfill = nf;
  }
  int fill = _oldfill;

  /*  Мітки середини тут була, але на чотирьох рядках вони збиралися в
      пунктирну вертикаль через увесь екран і читалися як зайва смуга.
      Нуль і без неї береться легко — його притягує западинка у valueAt().  */
  (void)fill;

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
  int target = (int)((long)(_val - _lo) * _boxw / (_hi - _lo));
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

/*  З тієї ж причини, що й у повзунка: малює лише задача дисплея.  */
void UiCheck::loop(){
  if(!_dirty || !_active || _locked) return;
  _dirty = false;
  dsp.fillRect(_config.left, _config.top, 15, 15, _on ? _oncolor : _bgcolor);
  dsp.drawRect(_config.left, _config.top, 15, 15, _fgcolor);
  _drawn = true;
}

void UiCheck::_clear(){ dsp.fillRect(_config.left, _config.top, _boxw, 16, _bgcolor); }

void UiCheck::_draw(){
  if(!_active || _locked) return;
  dsp.fillRect(_config.left, _config.top, 15, 15, _on ? _oncolor : _bgcolor);
  dsp.drawRect(_config.left, _config.top, 15, 15, _fgcolor);
  dsp.setFont(_font); dsp.setTextSize(1); dsp.setTextColor(_fgcolor);
  dsp.setCursor(_config.left + 24, _config.top + 12);
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

void UiSeg::loop(){
  if(!_dirty || !_active || _locked) return;
  _dirty = false;
  _draw();
}

void UiSeg::_clear(){ dsp.fillRect(_config.left, _config.top, _boxw, _boxh, _bgcolor); }

void UiSeg::_draw(){
  if(!_active || _locked) return;
  int16_t w = _segW();
  dsp.setFont(_font); dsp.setTextSize(1);
  for(uint8_t i = 0; i < _n; i++){
    int16_t x = _config.left + i * (w + 4);
    bool on = (i == _sel);
    dsp.fillRect(x, _config.top, w, _boxh, on ? _oncolor : _panel);
    char t[24]; snprintf(t, sizeof(t), "%s", utf8Rus(_lbl[i] ? _lbl[i] : "", false));
    int16_t x1, y1; uint16_t tw, th;
    dsp.getTextBounds(t, 0, 40, &x1, &y1, &tw, &th);
    dsp.setTextColor(on ? _bgcolor : _fgcolor);
    dsp.setCursor(x + (w - (int16_t)tw) / 2 - x1, _config.top + _boxh / 2 + 5);
    dsp.print(t);
  }
  dsp.setFont();
}

#endif
