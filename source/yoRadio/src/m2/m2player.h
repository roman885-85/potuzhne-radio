/*  ---------------------------------------------------------------------------
 *  Новий головний екран (плеєр) у стилі нового меню.
 *
 *  Коли вибрано новий вигляд (extras.s.menuClassic == 0), сторінка плеєра
 *  yoRadio зі старими віджетами не показується зовсім (замість неї порожня),
 *  а весь екран малює ця сцена — тими самими смугами через DMA, що й нове
 *  меню: шапка зі станцією й кнопкою меню, картка «зараз грає», великий
 *  годинник з датою й погодою, рядок обраного / пульт картки / рівень звуку
 *  і гучність. Малюється лише змінене.
 *
 *  Дотики сюди приходять із головного циклу (touchscreen.cpp): дії —
 *  одразу там, а що підсвітити й перемалювати — прапорцями для задачі дисплея.
 *  ------------------------------------------------------------------------- */
#ifndef m2player_h
#define m2player_h
#include "m2ui.h"

namespace m2 {

class Player {
  public:
    bool on() const;                     /* новий вигляд вибрано */
    void show();                         /* задача дисплея: плеєр щойно став на екран */
    void hide(){ _shown = false; }
    bool shown() const { return _shown; }
    void render();                       /* задача дисплея, щооберта, поки плеєр на екрані */
    void invalAll(){ _mark(0, 0, SW, SH); }
    /*  головний цикл  */
    void onPress(int16_t x, int16_t y);
    void onDrag(int16_t x, int16_t y);
    void onRelease(int16_t x, int16_t y);
  private:
    volatile bool _shown = false;
    uint32_t _dirty[15] = { 0 };
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
    uint16_t* _bg = nullptr;             /* тло з підсвіткою кольором логотипа (PSRAM) */
    uint16_t  _bgColor = 0xFFFF;
    uint16_t* _logo = nullptr;           /* логотип 58×58 */
    bool      _logoOk = false;
    uint32_t  _logoKey = 0xFFFFFFFF;
    uint16_t  _glow = 0;
    uint16_t* _favPix[6] = { nullptr };   /* логотипи обраного 26×26 */
    bool      _favOk[6] = { false };
    uint32_t  _favKey = 0xFFFFFFFF;
    /*  підписи, щоб знати, що змінилось  */
    uint32_t _sTop = 0, _sCard = 0, _sClock = 0, _sSec = 0, _sRow = 0, _sVol = 0, _sigT = 0;
    uint32_t _t0Name = 0, _t0Title = 0;
    /*  біжучі рядки: крок рівно в піксель кожні 25 мс (не за часом — інакше пізній кадр перескакує)  */
    int16_t  _offName = 0, _offTitle = 0;
    uint32_t _mqT = 0, _barT = 0;
    uint32_t _frameT = 0;                 /* час поточного кадру — однаковий для всіх смуг */
    float    _specH[32] = { 0 };          /* намальована висота рисок (дробова — край згладжений) */
    uint32_t _animT = 0, _specT = 0;
    float    _spec[32] = { 0 };
    float    _bars[5] = { 0 };
    /*  дотик  */
    volatile int8_t _zone = -1;          /* 0 шапка-джерело, 1 шапка-назва, 2 меню, 3 картка, 4 рядок, 5 гучність, 6 годинник */
    volatile int16_t _px = 0, _py = 0, _lx = 0, _ly = 0;
    volatile uint32_t _pt = 0, _upT = 0;
    volatile bool _down = false;
    volatile int16_t _volDrag = -1;       /* гучність під пальцем (0..254) */
    uint32_t _volSent = 0; int16_t _volLast = -1;
    volatile float _seek = -1;           /* пульт: куди перемотає */
    volatile int8_t _btn = -1;           /* пульт: натиснута кнопка */
    int16_t _rowMode = -1;               /* 0 рівень, 1 обране, 2 пульт */
    /*  пропозиція оновитись: показуємо, доки людина не вибере; «пізніше» — до наступного запуску  */
    char     _dismiss[32] = { 0 };
    volatile int8_t _popup = 0;          /* 0 немає, 1 нова версія, 2 оновлення не вдалося */
    volatile int8_t _popBtn = -1;        /* натиснута кнопка в картці */
    bool     _otaWasInstalling = false;
    static Rect _popRect(){ return Rect(16, 44, SW - 32, 152); }
    void _drawPopup(Gfx& g);

    void _mark(int16_t x, int16_t y, int16_t w, int16_t h);
    void _flush();
    void _draw(Gfx& g);
    void _drawTop(Gfx& g, uint32_t now);
    void _drawCard(Gfx& g, uint32_t now);
    void _drawClock(Gfx& g);
    void _drawRow(Gfx& g);
    void _drawVol(Gfx& g);
    void _ripple(Gfx& g);
    void _loadLogo();
    void _loadFav();
    void _makeBg(uint16_t glow);
    uint8_t _mode() const;               /* що в третьому рядку */
    int16_t _statusLeft() const;
};

extern Player P;

}  // namespace m2
#endif
