/*  ---------------------------------------------------------------------------
 *  Вбудований мікрофон кодека ES8311.
 *
 *  Окрема задача на ядрі 0 читає I2S (приймання йде на тих самих тактах, що й
 *  звук), зводить сигнал до 16 кГц моно й рахує: рівень, фоновий шум, голос
 *  (WebRTC VAD від Espressif), удари — хлопки й стук по корпусу. Поки грає
 *  звук, мікрофон чує насамперед сам динамік, тож власний звук радіо
 *  віднімається через esp_aec (Espressif). Опорний сигнал дає сам кодек:
 *  правий слот АЦП — це вихід ЦАП зсередини (REG44), вирівняний до відліку.
 *
 *  Удари шукаємо методом «complex domain onset detection» (Duxbury, Davies,
 *  Sandler, DAFx-03) у тому вигляді, в якому він працює на ESP32 у проєкті
 *  pector (Joren Six, GPL-3.0) на основі aubio; правила ритму — звідти ж.
 *  ------------------------------------------------------------------------- */
#pragma once
#include <Arduino.h>

/*  що зробити на жест  */
enum MicAction : uint8_t { MA_DEFAULT = 0, MA_NONE, MA_TOGGLE, MA_NEXT, MA_PREV,
                           MA_VOLUP, MA_VOLDN, MA_SCREEN, MA_FAV1, MA_N };

/*  жест: вид × кількість  */
enum MicGesture : uint8_t { MG_NONE = 0, MG_CLAP2, MG_CLAP3, MG_KNOCK2, MG_KNOCK3 };

class YoMic {
  public:
    void     begin();
    bool     running() const { return _task != nullptr; }
    void     apply();                         /* налаштування змінились */
    void     loop();                          /* головний цикл: дії на жести, присутність, сон */

    /*  що чутно  */
    int8_t   levelDb() const { return _lvl; }       /* поточний рівень, дБ повної шкали */
    int8_t   noiseDb() const { return _noise; }     /* фон кімнати */
    bool     speech()  const { return _speech; }
    uint32_t lastVoiceMs() const { return _lastVoice; }
    uint32_t lastSoundMs() const { return _lastSound; }
    MicGesture takeGesture();                      /* жест, що стався (одноразово) */
    uint8_t  heard() const { return _heard; }       /* останній почутий жест — для сторінки */
    uint32_t heardMs() const { return _heardMs; }
    uint32_t lastRoomMs() const { return _lastRoom; }
    float    excessDb() const { return _excess; }  /* на скільки кімната гучніша за власний звук */
    float    coupleDb() const { return _couple; }
    static const char* actionName(uint8_t a);
    static const char* gestureName(uint8_t g);
    static uint8_t actionFor(MicGesture g);
    bool     listening() const;                    /* мікрофон зараз слухає — для значка */

    /*  для відладки  */
    uint32_t blocks() const { return _blocks; }
    float    onsetVal() const { return _onLast; }
    int8_t   slotDb(uint8_t ch) const { return ch ? _slotR : _slotL; }   /* L — мікрофон, R — ЦАП зсередини кодека */
    float    echoCut() const { return _erle; }       /* на скільки дБ віднято власний звук */
    bool     aecActive() const { return _aecOn; }
    int      aecChunk() const { return _aecChunk; }
    void     aecSet(uint8_t mode, uint8_t len) { _aecMode = mode; _aecLen = len; }   /* для перевірки режимів esp_aec */
    uint8_t  aecMode() const { return _aecModeOn; }

    /*  Самоперевірка звукового тракту: радіо грає тони через свій динамік і
        міряє їх своїм мікрофоном — рівень, фон і спотворення на кожній
        частоті. Основа для автоналаштування еквалайзера.  */
    static const uint8_t SWEEP_N = 25;           /* терції від 63 Гц до 16 кГц */
    static uint16_t sweepHz(uint8_t i);
    /*  nullptr — почали, інакше причина відмови. amp=false — підсилювач
        лишається вимкненим: що тоді чутно, то наводка всередині кодека, а не звук.
        mask — котрі з SWEEP_N частот міряти.  */
    const char* sweepStart(int8_t dbfs, bool amp = true, uint32_t mask = 0x1FFFFFF, bool dsp = false);
    bool     sweepAmp() const { return _swAmp; }
    bool     sweepDsp() const { return _swDsp; }   /* тони йшли через еквалайзер — перевірка самої обробки */
    void     sweepAbort() { if(_swState == 1) _swStop = true; }
    uint8_t  sweepState() const { return _swState; }   /* 0 не міряли, 1 міряє, 2 готово, 3 перервано */
    const char* simStart(uint8_t kind, uint8_t count, uint16_t gapMs);   /* перевірка: 0 хлопки, 1 стук, 2 голос — через свій динамік */
    volatile uint32_t minMs = 60000;              /* «хвилина» таймера сну й присутності (перевірка: 1000) */
    bool     simBusy() const { return _tsKind != 0; }
    volatile bool _dbg = false;                    /* друкувати кожен удар і рішення */
    uint8_t  sweepPos() const { return _swPos; }
    float    sweepDb(uint8_t i) const { return _swDb[i]; }      /* тон у мікрофоні, дБ повної шкали */
    float    sweepNoise(uint8_t i) const { return _swNoise[i]; } /* та сама частота в тиші */
    float    sweepH2(uint8_t i) const { return _swH2[i]; }      /* 2-га гармоніка відносно тону, дБ */
    float    sweepH3(uint8_t i) const { return _swH3[i]; }
    uint32_t sweepRate() const { return _swRate; }
    int8_t   sweepLevel() const { return _swLevel; }
    int16_t  sweepPeak() const { return _swPeak; }      /* найбільший відлік мікрофона під час тонів */
    void     gainTemp(uint8_t step);                   /* підсилення мікрофона на час заміру (0..7) */

  private:
    static void _taskFn(void*);
    void     _run();
    void     _feed(int16_t* x, int16_t* ref);     /* блок від мікрофона й опорний: відняти луну, якщо грає */
    void     _block(int16_t* x, int16_t* ref, bool playing);   /* 512 відліків 16 кГц і опорний */
    void     _onset(int16_t* x, int16_t* ref, uint32_t now);
    void     _pattern(uint32_t now);
    void     _sweep(int16_t* raw);
    void     _simChunk(int16_t* buf);
    void     _firDesign(uint32_t rate);
    int16_t  _fir(uint8_t ch, int16_t x);
    TaskHandle_t _task = nullptr;
    volatile int8_t  _lvl = -90, _noise = -60;
    volatile bool    _speech = false;
    volatile uint32_t _lastVoice = 0, _lastSound = 0;
    volatile uint8_t _gesture = MG_NONE;
    volatile uint8_t _heard = MG_NONE;
    volatile uint32_t _heardMs = 0, _lastRoom = 0;
    float    _refDb = -90, _excess = 0, _couple = 0, _bgDb = -80;
    uint32_t _foreignMs = 0;
    float    _cLo = 0, _cHi = 0;                /* «динамік → мікрофон» по смугах, дБ */
    uint16_t _cN = 0;
    uint8_t  _roomRun = 0;
    uint16_t _coupleN = 0;
    uint32_t _earFrom = 0, _presFrom = 0, _wokeFor = 0;
    uint32_t _blocks = 0;
    float    _onLast = 0;
    float    _erle = 0;
    bool     _aecOn = false;
    int      _aecChunk = 0;
    volatile bool _aecWant = false;
    volatile uint8_t _aecMode = 0, _aecLen = 4;
    uint8_t  _aecModeOn = 255, _aecLenOn = 0;
    volatile int8_t _slotL = -90, _slotR = -90;
    volatile bool    _swReq = false, _swStop = false;
    volatile uint8_t _tsKind = 0, _tsCount = 0;
    uint16_t _tsGap = 300;
    uint32_t _tsPos = 0;
    volatile uint8_t _swState = 0, _swPos = 0;
    int8_t   _swLevel = -20;
    bool     _swAmp = true, _swDsp = false;
    uint32_t _swMask = 0x1FFFFFF;
    uint32_t _swRate = 0;
    volatile int16_t _swPeak = 0;
    float    _swDb[SWEEP_N], _swNoise[SWEEP_N], _swH2[SWEEP_N], _swH3[SWEEP_N];
};

extern YoMic mic;

