/*  ---------------------------------------------------------------------------
 *  Віджети меню — на тій самій основі, що й решта інтерфейсу yoRadio.
 *
 *  Кожен успадковує Widget, тож Page сам вмикає й вимикає їх, а перемальовує
 *  себе лише той, чиє значення справді змінилось. Саме тому екран не мерехтить:
 *  дотик міняє одне число — оновлюється один прямокутник, а не сторінка.
 *
 *  Від штатного TextWidget довелося відійти лише в одному: він жорстко кличе
 *  dsp.setFont() і тому завжди малює вбудованим шрифтом 5x7. Тут шрифт задається
 *  ззовні, інакше гладкі накреслення не застосувати.
 *  ------------------------------------------------------------------------- */
#ifndef menuwidgets_h
#define menuwidgets_h

#include "../core/options.h"

#if (TS_MODEL!=TS_MODEL_UNDEFINED) && (DSP_MODEL!=DSP_DUMMY)
  #define USE_YOMENU
#endif

#ifdef USE_YOMENU
/*  dspcore.h має йти першим: він приносить Adafruit_GFX, GFXfont і Canvas,
    без яких widgets.h не компілюється сам по собі.  */
#include "../displays/dspcore.h"
#include "../displays/widgets/widgets.h"

class UiText : public Widget {
  public:
    UiText(){}
    void init(WidgetConfig conf, const GFXfont* font, uint16_t boxw, uint16_t boxh,
              uint16_t fgcolor, uint16_t bgcolor);
    void setText(const char* txt);
    void setText(int val, const char* fmt);
    void setColor(uint16_t fg){ _fgcolor = fg; }
  protected:
    const GFXfont* _font = nullptr;
    char     _text[52] = {0};
    char     _old[52]  = {0};
    uint16_t _boxw = 0, _boxh = 0;
    void _draw() override;
    void _clear() override;
};

/*  Підпис ліворуч, число праворуч, під ними смуга заповнення — рядок
    еквалайзера з Nextion.  */
class UiSlider : public Widget {
  public:
    UiSlider(){}
    void init(WidgetConfig conf, const GFXfont* font, uint16_t boxw,
              int lo, int hi, uint16_t fgcolor, uint16_t bgcolor, uint16_t barcolor);
    void setLabel(const char* label);
    void setValue(int val);
    int  value() const { return _val; }
    int  valueAt(uint16_t x) const;       /* значення під пальцем */
    void loop() override;                 /* крок анімації ходу смуги */
  protected:
    const GFXfont* _font = nullptr;
    char     _label[48] = {0};
    uint16_t _boxw = 0, _barcolor = 0;
    int      _lo = 0, _hi = 100, _val = 0, _oldfill = -1;
    char     _shown[12] = {0};            /* що вже написано на екрані */
    uint32_t _animTick = 0;               /* крок анімації ходу смуги */
    void _draw() override;
    void _clear() override;
    void _drawbar();
};

/*  Прапорець — smartstart / audioinfo, і в тому ж вигляді перемикач джерела. */
class UiCheck : public Widget {
  public:
    UiCheck(){}
    void init(WidgetConfig conf, const GFXfont* font, uint16_t boxw,
              uint16_t fgcolor, uint16_t bgcolor, uint16_t oncolor);
    void setLabel(const char* label);
    void setValue(bool on);
    void loop() override;                 /* малює із такту сторінки */
    bool value() const { return _on; }
  protected:
    const GFXfont* _font = nullptr;
    char     _label[48] = {0};
    uint16_t _boxw = 0, _oncolor = 0;
    bool     _on = false, _drawn = false, _dirty = false;
    void _draw() override;
    void _clear() override;
};

/*  Ряд кнопок, з яких вибрана одна: «вимк / 15 / 30 / 60 / 90»,
    «щодня / будні», режим світлодіода. Вибрана — жовта, як шапка.  */
#define UISEG_MAX 5
class UiSeg : public Widget {
  public:
    UiSeg(){}
    void init(WidgetConfig conf, const GFXfont* font, uint16_t boxw, uint16_t boxh,
              uint16_t fgcolor, uint16_t bgcolor, uint16_t oncolor, uint16_t panel);
    void setItems(uint8_t n, const char* const* labels);
    void setSel(int8_t i);                /* лише запам'ятати, малює такт сторінки */
    int8_t sel() const { return _sel; }
    int8_t indexAt(uint16_t x) const;     /* -1 — повз кнопки */
    void loop() override;
  protected:
    const GFXfont* _font = nullptr;
    const char* _lbl[UISEG_MAX] = {nullptr};
    uint8_t  _n = 0;
    uint16_t _boxw = 0, _boxh = 0, _oncolor = 0, _panel = 0;
    int8_t   _sel = -1;
    bool     _dirty = false;
    int16_t  _segW() const { return _n ? (int16_t)((_boxw - 4 * (_n - 1)) / _n) : 0; }
    void _draw() override;
    void _clear() override;
};

#endif  // USE_YOMENU
#endif  // menuwidgets_h
