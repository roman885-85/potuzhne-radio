#ifndef touchscreen_h
#define touchscreen_h

/*  Сенсор екрана. Сам він жестів не розбирає: дотики йдуть або в меню, або на
    головний екран (src/m2) — ті знають, що під пальцем. Тут лишилось читання
    панелі, «перший дотик лише будить погаслий екран» і штучні дотики для
    службової консолі.  */
class TouchScreen {
  public:
    TouchScreen() {}
    void init(uint16_t w, uint16_t h);
    void loop();
    void flip();
    /*  Штучне касання: дає змогу перевіряти жести без пальця — команда
        проходить рівно тим самим шляхом, що й справжній дотик.  */
    void injectBegin(uint16_t x, uint16_t y);
    void injectMove(uint16_t x, uint16_t y);
    void injectEnd();
    void dbgState();          /* стан дотику для діагностики */
  private:
    uint16_t _x = 0, _y = 0, _width = 0, _height = 0;
    uint32_t _touchdelay = 0, _t0 = 0;
    bool     _inject = false, _injTouched = false;
    uint16_t _injX = 0, _injY = 0;
    bool _istouched();
    void _point(uint16_t& x, uint16_t& y);
};

extern TouchScreen touchscreen;

#endif
