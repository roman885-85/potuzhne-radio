#ifndef touchscreen_h
#define touchscreen_h

enum tsDirection_e { TSD_STAY, TSD_LEFT, TSD_RIGHT, TSD_UP, TSD_DOWN, TDS_REQUEST };

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
    void dbgState();          /* стан жесту для діагностики */
  private:
    uint16_t _oldTouchX, _oldTouchY, _width, _height;
    uint32_t _touchdelay;
    bool     _volSlide = false;
    int      _lastVol = -1, _pendVol = -1;
    uint32_t _volSent = 0, _lastTap = 0;
    /*  Інерційна прокрутка списку  */
    float    _plAccum = 0.0f;     /* накопичений зсув пальця, пікселі */
    float    _plVel   = 0.0f;     /* швидкість після відриву, рядків/с */
    float    _plCarry = 0.0f;     /* дробова частина рядка під час розгону */
    uint32_t _plLastMs = 0;
    int16_t  _plPrevY = 0;
    int      _plPending = 0;
    uint32_t _plDrawMs = 0;
    bool     _plActive = false;
    float    _plPos = 1.0f;
    float    _plTarget = 1.0f;          /* куди тягне палець; _plPos іде за ним згладжено */
    float    _plTapPos = 1.0f;          /* де був список (на екрані) у мить дотику */
    bool     _plTapFling = false;       /* дотик зупинив швидкий накат */
    int16_t  _plHistY[8] = {0};         /* останні положення пальця — для швидкості кидка */
    uint32_t _plHistT[8] = {0};
    uint8_t  _plHistN = 0;
    void     _plHist(int16_t y, uint32_t t);
    float    _plFlingVel();
    /*  Сторінка плейлиста як у Nextion: кнопки праворуч, дотик по рядку.  */
    int8_t   _plBtn = -1;               /* натиснута кнопка, -1 — жодна */
    bool     _plTap = false, _plCaught = false, _plRepeated = false;
    int16_t  _plTapX = 0, _plTapY = 0;
    uint32_t _plTapMs = 0, _plBtnMs = 0, _plRepMs = 0;
    bool     _plAnim = false;           /* плавний підвід до рядка */
    float    _plAnimFrom = 0.0f, _plAnimTo = 0.0f;
    uint32_t _plAnimT0 = 0;
    void     _plAction(int8_t b);
    void     _plAnimateTo(float target);
    void     _plSnap();                /* дотягнути до найближчої станції пружиною */
    int8_t   _sdZone = -1;              /* пульт картки: 0 попередній, 1 наступний, 2 смуга */
    float    _sdFrac = 0.0f;
    float    _plClamp(float p);
    bool     _inject = false, _injTouched = false;
    uint16_t _injX = 0, _injY = 0;
    uint32_t _menuT0 = 0;              /* початок дотику в меню */
    bool _checklpdelay(int m, uint32_t &tstamp);
    tsDirection_e _tsDirection(uint16_t x, uint16_t y);
    bool _istouched();
};

extern TouchScreen touchscreen;

#endif
