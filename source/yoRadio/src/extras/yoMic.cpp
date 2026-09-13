#include "yoMic.h"
#include <driver/i2s.h>
#include <math.h>
#include "esp_heap_caps.h"
#include "esp_vad.h"
#include "esp_aec.h"
#include "dsps_fft2r.h"
#include "dsps_wind.h"
#include "../core/options.h"
#include "../core/player.h"
#include "../ES8311/yoES8311.h"
#include "yoExtras.h"
#include "yoDsp.h"

YoMic mic;

static const int  MIC_FS = 16000;   /* усе, що аналізуємо, — 16 кГц моно */
static const int  BLK    = 512;     /* 32 мс: блок для ударів і кадр esp_aec */
static const int  VADN   = 480;     /* 30 мс: кадр WebRTC VAD */

bool YoMic::listening() const {
  return running() && extras.s.micOn && !extras.s.dac;
}

MicGesture YoMic::takeGesture(){
  uint8_t g = _gesture; _gesture = MG_NONE;
  return (MicGesture)g;
}

void YoMic::apply(){
  if(extras.s.dac) return;                        /* зовнішній ЦАП — кодек без тактів */
  uint8_t g = extras.s.micGain ? extras.s.micGain - 1 : 4;   /* типове — 24 дБ */
  es8311_mic(g);
}

void YoMic::gainTemp(uint8_t step){ if(!extras.s.dac) es8311_mic(step > 7 ? 7 : step); }

void YoMic::begin(){
  if(_task) return;
  apply();
  xTaskCreatePinnedToCore(_taskFn, "mic", 8192, this, 1, &_task, 0);
}

void YoMic::_taskFn(void* p){ ((YoMic*)p)->_run(); }

/*  ---------- стан аналізу (одна задача — статичні змінні досить) ---------- */
static float*   fftBuf = nullptr;      /* 2*BLK: re, im */
static float*   fftRef = nullptr;      /* те саме для опорного (що грає радіо) */
static float*   win = nullptr;
static float*   mag1 = nullptr, *ph1 = nullptr, *ph2 = nullptr;   /* у PSRAM */
static float    onMu = 0, onDev = 0;
static uint32_t onT[4] = {0};          /* часи останніх ударів одного виду */
static uint8_t  onKind[4] = {0};       /* 1 хлопок, 2 стук */
static uint8_t  onN = 0;
static vad_handle_t vad = nullptr;
static int16_t  vadBuf[VADN];
static uint16_t vadFill = 0;
static uint8_t  vadRun = 0;            /* кадрів голосу поспіль */
static aec_handle_t* aec = nullptr;
static int16_t* aecOut = nullptr;

/*  Читаємо I2S, зводимо до 16 кГц, складаємо блоки по 512.  */
void YoMic::_run(){
  const size_t RAW = 1024;                                  /* стерео 16 біт: 256 кадрів */
  /*  Внутрішньої пам'яті в радіо обмаль (її їдять Wi-Fi, TLS, екран) — усе
      робоче мікрофона кладемо в PSRAM: 31 блок на секунду там устигає.  */
  const uint32_t MEMPS = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
  int16_t* raw = (int16_t*)heap_caps_malloc(RAW, MEMPS);
  int16_t* blk = (int16_t*)heap_caps_aligned_alloc(16, BLK * sizeof(int16_t), MEMPS);
  int16_t* rblk = (int16_t*)heap_caps_aligned_alloc(16, BLK * sizeof(int16_t), MEMPS);
  fftBuf = (float*)heap_caps_aligned_alloc(16, BLK * 2 * sizeof(float), MEMPS);
  fftRef = (float*)heap_caps_aligned_alloc(16, BLK * 2 * sizeof(float), MEMPS);
  win    = (float*)heap_caps_malloc(BLK * sizeof(float), MEMPS);
  aecOut = (int16_t*)heap_caps_aligned_alloc(16, BLK * sizeof(int16_t), MEMPS);
  mag1 = (float*)heap_caps_calloc(BLK/2, sizeof(float), MEMPS);
  ph1  = (float*)heap_caps_calloc(BLK/2, sizeof(float), MEMPS);
  ph2  = (float*)heap_caps_calloc(BLK/2, sizeof(float), MEMPS);
  if(!raw || !blk || !rblk || !fftBuf || !fftRef || !win || !aecOut || !mag1 || !ph1 || !ph2){ _task = nullptr; vTaskDelete(NULL); return; }
  dsps_fft2r_init_fc32(NULL, BLK);
  dsps_wind_hann_f32(win, BLK);
  vad = vad_create_with_param(VAD_MODE_3, MIC_FS, 30, 200, 600);

  uint16_t n = 0;
  uint32_t ph = 0;
  int32_t dc = 0, rdc = 0, acc = 0, racc = 0;
  uint16_t cnt = 0;
  uint32_t firRate = 0, reads = 0;
  int16_t yPrev = 0, rPrev = 0;
  for(;;){
    if(_swReq){ _swReq = false; _sweep(raw); n = 0; continue; }
    if(!listening()){ vTaskDelay(pdMS_TO_TICKS(200)); n = 0; continue; }
    if(_tsKind) _simChunk(raw);                               /* перевірка: хлопки чи стук через свій динамік */
    size_t got = 0;
    if(i2s_read(I2S_NUM_0, raw, RAW, &got, pdMS_TO_TICKS(100)) != ESP_OK || got < 4) continue;
    uint32_t rate = player.getSampleRate();
    if(rate < 8000) rate = 44100;
    if(rate != firRate){ _firDesign(rate); firRate = rate; ph = 0; }
    size_t fr = got / 4;
    if((++reads & 7) == 0 && fr){                             /* рівні слотів — для відладки, зрідка */
      int64_t eL = 0, eR = 0;
      for(size_t i = 0; i < fr; i++){ eL += (int32_t)raw[2*i]*raw[2*i]; eR += (int32_t)raw[2*i+1]*raw[2*i+1]; }
      float rl = sqrt((float)eL/fr), rr = sqrt((float)eR/fr);
      _slotL = rl > 1 ? (int8_t)lroundf(20.0f*log10f(rl/32768.0f)) : -90;
      _slotR = rr > 1 ? (int8_t)lroundf(20.0f*log10f(rr/32768.0f)) : -90;
    }
    /*  Луну віднімаємо — тоді до 16 кГц зводимо точно й однаково обидва слоти:
        фільтр нижніх частот (КІХ, зріз 7 кГц) і відлік у потрібний момент
        лінійною інтерполяцією. Проста «середня з 2–3 відліків» міняє крок від
        відліку до відліку, і лінійний фільтр луни не повторить такий шлях.
        Без віднімання луни вистачає й середньої — вона вп'ятеро дешевша.  */
    bool exact = _aecWant;
    for(size_t i = 0; i < fr; i++){
      int32_t v = raw[2*i];
      dc += (v * 256 - dc) >> 10;   v -= dc >> 8;            /* постійна складова */
      if(v > 32767) v = 32767; else if(v < -32768) v = -32768;
      if(exact){
        int32_t r = raw[2*i+1];
        rdc += (r * 256 - rdc) >> 10;  r -= rdc >> 8;
        if(r > 32767) r = 32767; else if(r < -32768) r = -32768;
        int16_t y = _fir(0, (int16_t)v), q = _fir(1, (int16_t)r);
        ph += MIC_FS;
        if(ph >= rate){
          ph -= rate;
          int32_t f = (int32_t)((ph << 12) / MIC_FS);        /* частка назад від цього входу, 0..4095 */
          blk[n]  = (int16_t)(y + (((int32_t)yPrev - y) * f >> 12));
          rblk[n] = (int16_t)(q + (((int32_t)rPrev - q) * f >> 12));
          if(++n == BLK){ _feed(blk, rblk); n = 0; }
        }
        yPrev = y; rPrev = q;
      }else{
        acc += v; racc += raw[2*i+1]; cnt++;
        ph += MIC_FS;
        if(ph >= rate){
          ph -= rate;
          blk[n] = (int16_t)(acc / (cnt ? cnt : 1));
          rblk[n] = (int16_t)(racc / (cnt ? cnt : 1));
          acc = 0; racc = 0; cnt = 0;
          if(++n == BLK){ _feed(blk, rblk); n = 0; }
        }
      }
    }
  }
}

/*  КІХ нижніх частот для зведення до 16 кГц: віконний sinc (Блекмен), зріз
    7 кГц на частоті потоку, коефіцієнти в Q15.  */
static const int FIR_N = 23;                /* непарне: симетричний, половина множень */
static int16_t firC[FIR_N];
static int16_t firH[2][2 * FIR_N];         /* подвоєна історія — без переносу через край */
static uint8_t firI[2] = {0, 0};

void YoMic::_firDesign(uint32_t rate){
  double fc = 7000.0 / rate, sum = 0, c[FIR_N];
  for(int k = 0; k < FIR_N; k++){
    int m = k - (FIR_N - 1) / 2;
    double h = m == 0 ? 2.0 * fc : sin(2.0 * M_PI * fc * m) / (M_PI * m);
    double w = 0.42 - 0.5 * cos(2.0 * M_PI * k / (FIR_N - 1)) + 0.08 * cos(4.0 * M_PI * k / (FIR_N - 1));
    c[k] = h * w; sum += c[k];
  }
  for(int k = 0; k < FIR_N; k++) firC[k] = (int16_t)lround(c[k] / sum * 32767.0);
  memset(firH, 0, sizeof(firH));
}

int16_t YoMic::_fir(uint8_t ch, int16_t x){
  uint8_t i = firI[ch];
  int16_t* h = firH[ch];
  h[i] = x; h[i + FIR_N] = x;               /* h[i..i+FIR_N-1] — останні FIR_N відліків підряд */
  firI[ch] = (i + 1) % FIR_N;
  const int16_t* w = h + i + 1;             /* найстаріший … найновіший */
  const int M = (FIR_N - 1) / 2;
  int32_t acc = (int32_t)firC[M] * w[M];
  for(int k = 0; k < M; k++) acc += (int32_t)firC[k] * ((int32_t)w[k] + w[FIR_N - 1 - k]);
  acc >>= 15;
  return (int16_t)(acc > 32767 ? 32767 : acc < -32768 ? -32768 : acc);
}

/*  Блок від мікрофона: якщо грає звук і треба чути кімнату — віднімаємо луну
    власного динаміка (esp_aec за опорним сигналом із кодека).  */
void YoMic::_feed(int16_t* x, int16_t* ref){
  bool playing = player.isRunning();
  bool want = extras.s.micPlay;
  _aecOn = playing && want;
  _aecWant = _aecOn;                             /* наступні блоки — точним зведенням */
  if(_aecOn){
    if(aec && (_aecMode != _aecModeOn || _aecLen != _aecLenOn)){ aec_destroy(aec); aec = nullptr; }
    if(!aec){
      aec = aec_create(MIC_FS, _aecLen, 1, (aec_mode_t)_aecMode);
      _aecModeOn = _aecMode; _aecLenOn = _aecLen;
      if(aec) _aecChunk = aec_get_chunksize(aec);
      _erle = 0;
    }
    if(aec){
      aec_process(aec, x, ref, aecOut);
      double ei = 0, eo = 0;
      for(int i = 0; i < BLK; i++){ ei += (double)x[i] * x[i]; eo += (double)aecOut[i] * aecOut[i]; }
      if(ei > 1e5){
        float cut = (float)(10.0 * log10((ei + 1) / (eo + 1)));
        _erle += (cut - _erle) * 0.1f;
      }
      _block(aecOut, ref, true);
      return;
    }
  }
  _block(x, ref, playing);
}

static float blockDb(const int16_t* x){
  int64_t e = 0;
  for(int i = 0; i < BLK; i++) e += (int32_t)x[i] * x[i];
  float rms = sqrt((float)e / BLK);
  float db = rms > 1 ? 20.0f * log10f(rms / 32768.0f) : -90.0f;
  return db < -90 ? -90 : db;
}

void YoMic::_block(int16_t* x, int16_t* ref, bool playing){
  uint32_t now = millis();
  _blocks++;
  float mdb = blockDb(x);
  int db = (int)lroundf(mdb);
  _lvl = (int8_t)db;
  if(_dbg && _tsKind) Serial.printf("##LVL#\t%u %d поз=%u\n", (unsigned)now, db, (unsigned)_tsPos);
  /*  фон: униз одразу, вгору ледь-ледь — кроки й голос його не піднімають  */
  if(db < _noise) _noise = (int8_t)db;
  else if((_blocks & 15) == 0 && _noise < -20) _noise++;
  /*  тло — середній рівень за ~2 с (потужність), без самих ударів  */
  {
    float pw = pow(10.0f, mdb / 10.0f), bg = pow(10.0f, _bgDb / 10.0f);
    if(mdb < _bgDb + 10.0f) bg += (pw - bg) * 0.016f;
    _bgDb = bg > 1e-10f ? 10.0f * log10(bg) : -100.0f;
  }

  /*  Що з почутого — не наш власний звук. Радіо знає, що грає (правий слот —
      вихід ЦАП), а пара «динамік — мікрофон» у корпусі тримає співвідношення
      рівнів сталим. Вчимо його, поки в кімнаті тихо; перевищення над ним —
      звук кімнати: голос, кроки, хлопок. Так уміє будь-який детектор
      «двобічної розмови» в гучному зв'язку.  */
  float rdb = playing ? blockDb(ref) : -90.0f;
  _refDb = rdb;
  if(rdb > -60.0f){
    float r = mdb - rdb;
    if(_coupleN < 20){ _couple += (r - _couple) / (++_coupleN); }
    else if(r < _couple + 4.0f){ _couple += (r - _couple) * 0.02f; }   /* подвійну розмову не вчимо */
    _excess = mdb - (rdb + _couple);
  }else{
    _excess = mdb - _noise;                       /* своє мовчить — рахуємо від фону */
  }
  bool own = rdb > -60.0f;
  bool roomSound = own ? _excess > 8.0f : db > _noise + 12;
  if(roomSound){ _lastSound = now; _lastRoom = now; }

  /*  голос: WebRTC VAD на кадрах по 30 мс; поки грає — лише те, що
      вибивається над власним звуком (інакше співак у пісні — теж «голос»)  */
  for(int i = 0; i < BLK; i++){
    vadBuf[vadFill++] = x[i];
    if(vadFill == VADN){
      vadFill = 0;
      bool sp = vad && vad_process(vad, vadBuf, MIC_FS, 30) == VAD_SPEECH && db > _noise + 6 && (!own || _excess > 6.0f);
      vadRun = sp ? (vadRun < 255 ? vadRun + 1 : 255) : 0;
      _speech = vadRun >= 4;                       /* ~120 мс поспіль — не клацання, а голос */
      if(_speech){ _lastVoice = now; _lastRoom = now; }
    }
  }
  _onset(x, own ? ref : nullptr, now);
  _pattern(now);
}

/*  Удари: «complex domain» — наскільки спектр цього блоку не схожий на
    передбачений із двох попередніх (амплітуда й фаза). Метод Duxbury і
    ін., DAFx-03; так рахує aubio і pector на ESP32.  */
void YoMic::_onset(int16_t* x, int16_t* ref, uint32_t now){
  bool clapOn = extras.s.clapOn, knockOn = extras.s.knockOn;
  if(!clapOn && !knockOn){ _onLast = 0; return; }
  for(int i = 0; i < BLK; i++){ fftBuf[2*i] = (x[i] / 32768.0f) * win[i]; fftBuf[2*i+1] = 0; }
  dsps_fft2r_fc32(fftBuf, BLK);
  dsps_bit_rev_fc32(fftBuf, BLK);
  float val = 0, lo = 0, hi = 0;
  for(int b = 1; b < BLK/2; b++){
    float re = fftBuf[2*b], im = fftBuf[2*b+1];
    float m = sqrt(re*re + im*im);
    float p = atan2(im, re);
    float d = mag1[b]*mag1[b] + m*m - 2.0f * mag1[b] * m * cos(2.0f*ph1[b] - ph2[b] - p);
    float c = sqrt(fabs(d));
    val += c;
    /*  вид удару — за енергією самого блоку, а не за зміною: початок будь-
        якого удару широкосмуговий, а от тіло в стуку — низьке, у хлопку — високе  */
    if(b >= 2 && b <= 48)   lo += m * m;         /* ~60..1500 Гц: стук по корпусу */
    if(b >= 64 && b <= 224) hi += m * m;         /* ~2..7 кГц: хлопок */
    mag1[b] = m; ph2[b] = ph1[b]; ph1[b] = p;
  }
  _onLast = val;
  /*  Свій звук — по смугах. Радіо знає, що грає (правий слот — вихід ЦАП):
      той самий спектр рахуємо й для нього, а співвідношення «динамік →
      мікрофон» вчимо окремо для низу й верху. Удар мусить вибиватися над
      власним звуком саме у своїй смузі: бочка чи тарілки в пісні там уже є.  */
  float exLo = 99, exHi = 99;
  if(ref){
    for(int i = 0; i < BLK; i++){ fftRef[2*i] = (ref[i] / 32768.0f) * win[i]; fftRef[2*i+1] = 0; }
    dsps_fft2r_fc32(fftRef, BLK);
    dsps_bit_rev_fc32(fftRef, BLK);
    float rlo = 0, rhi = 0;
    for(int b = 2; b <= 224; b++){
      float re = fftRef[2*b], im = fftRef[2*b+1], e2 = re*re + im*im;
      if(b <= 48) rlo += e2;
      if(b >= 64) rhi += e2;
    }
    float dLo = 10.0f * log10f((lo + 1e-12f) / (rlo + 1e-12f));
    float dHi = 10.0f * log10f((hi + 1e-12f) / (rhi + 1e-12f));
    if(rlo > 1e-9f) exLo = dLo - _cLo;
    if(rhi > 1e-9f) exHi = dHi - _cHi;
    bool quietRoom = !(onN && now - onT[onN-1] < 300) && exLo < 6 && exHi < 6;
    if(_cN < 40){
      if(rlo > 1e-9f && rhi > 1e-9f){ _cLo += (dLo - _cLo) / (_cN + 1); _cHi += (dHi - _cHi) / (_cN + 1); _cN++; }
      exLo = exHi = -99;                                     /* ще не вивчили — нічого не приймаємо */
    }else if(quietRoom){
      if(rlo > 1e-9f) _cLo += (dLo - _cLo) * 0.03f;
      if(rhi > 1e-9f) _cHi += (dHi - _cHi) * 0.03f;
    }
  }else _cN = 0;
  /*  Поріг підлаштовується під кімнату: середнє й розкид звичайного фону.
      Удар займає два-три блоки (32 мс кожен) — у статистику фону не йде
      ні сам удар, ні 150 мс після нього: інакше хвіст першого хлопка
      піднімав поріг, і третій губився. Після гучного (музика стихла)
      поріг опускається вчетверо швидше, ніж піднімається.  */
  uint8_t sens = clapOn ? extras.s.clapSens : extras.s.knockSens;
  float k = sens == 1 ? 7.0f : sens == 2 ? 3.0f : 4.5f;   /* низька / висока / середня */
  float thr = onMu + k * onDev;
  bool outlier = onDev > 0 && val > thr;
  bool recent = onN && now - onT[onN-1] < 150;
  if(!outlier && !recent){
    float r = val < onMu ? 0.12f : 0.03f;
    onMu  += (val - onMu) * r;
    onDev += (fabs(val - onMu) - onDev) * 0.03f;
  }
  if(!outlier) return;
  /*  Удар — лише справжній: на 15 дБ гучніший за фон кімнати, на 10 дБ — за
      середній рівень останніх двох секунд (поверх музыки з чужих колонок
      плескати треба помітно гучніше за неї) і не тихіший за −62 дБ.  */
  bool loud = _lvl > _noise + 15 && _lvl > _bgDb + 10.0f && _lvl > -62 && val > 2.0f;
  bool apart = onN == 0 || now - onT[onN-1] > 100;          /* не частіше ніж раз на 100 мс */
  uint8_t kind = hi > lo * 0.6f ? 1 : 2;
  bool notOwn = !ref || (kind == 1 ? exHi > 8.0f : exLo > 8.0f);   /* удар у пісні радіо — не команда */
  if(kind == 1 && !clapOn) kind = 0;
  if(kind == 2 && !knockOn) kind = 0;
  bool accept = loud && notOwn && apart && kind;
  if(_dbg) Serial.printf("##ONSET#\t%u val=%.2f поріг=%.2f низ=%.2f верх=%.2f рівень=%d фон=%d тло=%.0f понад=%.0f/%.0f вид=%u %s%s%s%s\n",
                         (unsigned)now, val, thr, lo, hi, (int)_lvl, (int)_noise, _bgDb, exLo, exHi, kind,
                         loud ? "" : "тихо ", notOwn ? "" : "своє ", apart ? "" : "зарано ", accept ? "УДАР" : "");
  if(accept){
    if(onN == 4){ for(int i = 0; i < 3; i++){ onT[i] = onT[i+1]; onKind[i] = onKind[i+1]; } onN = 3; }
    onT[onN] = now; onKind[onN] = kind; onN++;
  }else if(loud && !(onN && now - onT[onN-1] < 150)){
    /*  гучний удар, що не пішов у жест (чужий, свій чи іншого виду), — жест
        навколо нього вже не жест: так ударні в музиці не стають командами  */
    _foreignMs = now;
  }
}

/*  Ритм — правила pector: між ударами не більше 0,45 с, для трійки ще й
    рівний крок (розбіжність до 0,1 с). Рішення — через 0,5 с тиші після
    останнього удару, щоб дві й три не плутались.  */
void YoMic::_pattern(uint32_t now){
  if(!onN || now - onT[onN-1] < 500) return;
  /*  Жест — окрема серія: за 0,8 с до першого удару й аж до рішення жодного
      стороннього гучного удару. Ритм барабанів цього не витримує.  */
  uint32_t first = onT[0];
  if(_foreignMs && _foreignMs + 800 > first){
    if(_dbg) Serial.printf("##ONSET#\tрішення: серію з %u ударів відкинуто — поруч інші удари\n", (unsigned)onN);
    onN = 0;
    return;
  }
  uint8_t kind = onKind[onN-1];
  uint8_t cnt = 1;
  for(int i = onN - 1; i > 0; i--){
    if(onKind[i-1] != kind || onT[i] - onT[i-1] > 450) break;
    cnt++;
  }
  if(cnt >= 3){
    uint32_t d1 = onT[onN-1] - onT[onN-2], d2 = onT[onN-2] - onT[onN-3];
    if((d1 > d2 ? d1 - d2 : d2 - d1) > 100) cnt = 2;
  }
  uint8_t g = MG_NONE;
  if(cnt == 2) g = kind == 1 ? MG_CLAP2 : MG_KNOCK2;
  else if(cnt >= 3) g = kind == 1 ? MG_CLAP3 : MG_KNOCK3;
  if(_dbg) Serial.printf("##ONSET#\tрішення: %u удари виду %u -> жест %u\n", (unsigned)cnt, (unsigned)kind, (unsigned)g);
  if(g){ _gesture = g; _heard = g; _heardMs = now; }
  onN = 0;
}

/*  ---------- самоперевірка тракту: динамік → повітря й корпус → мікрофон ---------- */
static const uint16_t SW_HZ[YoMic::SWEEP_N] = {
  63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800, 1000,
  1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000 };

uint16_t YoMic::sweepHz(uint8_t i){ return i < SWEEP_N ? SW_HZ[i] : 0; }

/*  Кличе цикл радіо (там само, де плеєр міняє частоту), тож перемкнути
    такти на 44,1 кГц тут безпечно: після ввімкнення вони на 16 кГц, а з
    ними вище 8 кГц не зміряти.  */
const char* YoMic::sweepStart(int8_t dbfs, bool amp, uint32_t mask, bool dsp){
  if(!_task)            return "мікрофон не запущено";
  if(extras.s.dac)      return "звук іде на зовнішній ЦАП, вбудований динамік вимкнено";
  if(_swState == 1 || _swReq) return "уже міряю";
  if(player.isRunning()) return "спершу зупиніть звук";
  if(dbfs > -6)  dbfs = -6;
  if(dbfs < -40) dbfs = -40;
  _swLevel = dbfs;
  _swAmp = amp;
  _swDsp = dsp;
  _swMask = mask ? mask : 0x1FFFFFF;
  if(player.getSampleRate() != 44100) player.forceSampleRate(44100);
  _swStop = false;
  _swState = 1; _swPos = 0;
  _swReq = true;
  return nullptr;
}

/*  Амплітуда частоти f у записі (Гёрцель із вікном Ганна), частка повної шкали.  */
static float toneAmp(const float* x, size_t n, double wsum, float f, float fs){
  double c = 2.0 * cos(2.0 * M_PI * f / fs), s1 = 0, s2 = 0;
  for(size_t i = 0; i < n; i++){ double s = x[i] + c * s1 - s2; s2 = s1; s1 = s; }
  double p = s1 * s1 + s2 * s2 - c * s1 * s2;
  return (float)(2.0 * sqrt(p > 0 ? p : 0) / wsum);
}

static float toDb(float a){ return a > 1e-7f ? 20.0f * log10(a) : -140.0f; }

void YoMic::_sweep(int16_t* raw){
  const uint32_t MEMPS = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
  const size_t FR = 256;                                   /* кадрів за один обмін */
  uint32_t fs = player.getSampleRate();
  if(fs < 8000) fs = 44100;
  _swRate = fs;
  for(int i = 0; i < SWEEP_N; i++){ _swDb[i] = _swNoise[i] = _swH2[i] = _swH3[i] = NAN; }
  _swPeak = 0;
  size_t capN = fs / 4;                                    /* 250 мс на аналіз */
  float*   cap = (float*)heap_caps_malloc(capN * sizeof(float), MEMPS);
  float*   sil = (float*)heap_caps_malloc(capN * sizeof(float), MEMPS);
  float*   hw  = (float*)heap_caps_malloc(capN * sizeof(float), MEMPS);
  int16_t* out = (int16_t*)heap_caps_malloc(FR * 4, MEMPS);
  if(!cap || !sil || !hw || !out){
    free(cap); free(sil); free(hw); free(out);
    _swState = 3; return;
  }
  double wsum = 0;
  for(size_t i = 0; i < capN; i++){ hw[i] = 0.5f - 0.5f * cos(2.0 * M_PI * i / (capN - 1)); wsum += hw[i]; }

  const float A = 32767.0f * pow(10.0f, _swLevel / 20.0f);
  const size_t fade = fs / 100;                            /* 10 мс наростання й спаду — без клацань */
  double phase = 0;
  bool ok = true;

  /*  Грати frames кадрів тону f (0 — тиша) і водночас записувати мікрофон;
      записані кадри з номерами [from, from+capN) кладемо в dst.  */
  auto pump = [&](float f, size_t frames, float* dst, size_t from) -> bool {
    double step = 2.0 * M_PI * f / fs;
    for(size_t done = 0; done < frames; ){
      if(_swStop || player.isRunning()) return false;
      size_t k = frames - done < FR ? frames - done : FR;
      for(size_t j = 0; j < k; j++){
        size_t t = done + j;
        float g = 1.0f;
        if(t < fade) g = (float)t / fade;
        else if(frames - t < fade) g = (float)(frames - t) / fade;
        int16_t v = f > 0 ? (int16_t)(A * g * sin(phase)) : 0;
        if(f > 0){ phase += step; if(phase > 2.0 * M_PI) phase -= 2.0 * M_PI; }
        if(_swDsp){
          /*  той самий шлях, яким іде радіо: еквалайзер, захист, гучність  */
          int16_t smp[2] = { v, v };
          uint32_t o = yoDsp.process(smp);
          out[2*j] = (int16_t)(o >> 16); out[2*j+1] = (int16_t)(o & 0xFFFF);
        }else{
          out[2*j] = v; out[2*j+1] = v;
        }
      }
      size_t w = 0, got = 0;
      i2s_write(I2S_NUM_0, out, k * 4, &w, pdMS_TO_TICKS(300));
      i2s_read(I2S_NUM_0, raw, k * 4, &got, pdMS_TO_TICKS(300));
      for(size_t j = 0; j < got / 4; j++){
        size_t t = done + j;
        if(dst && t >= from && t < from + capN){
          dst[t - from] = raw[2*j] / 32768.0f * hw[t - from];
          int16_t pk = raw[2*j] < 0 ? -raw[2*j] : raw[2*j];
          if(pk > _swPeak) _swPeak = pk;                       /* упор АЦП — ознака спотворення */
        }
      }
      done += k;
    }
    return true;
  };

  /*  старі записи з буферів приймання — геть  */
  for(int i = 0; i < 16; i++){
    size_t got = 0;
    i2s_read(I2S_NUM_0, raw, FR * 4, &got, 0);
    if(!got) break;
  }
  if(MUTE_PIN != 255) digitalWrite(MUTE_PIN, _swAmp ? !MUTE_VAL : MUTE_VAL);  /* підсилювач */
  ok = pump(0, fs / 5, nullptr, 0)                          /* 200 мс: підсилювач устоявся */
    && pump(0, fs * 3 / 10, sil, fs / 20);                  /* фон кімнати */
  for(uint8_t i = 0; ok && i < SWEEP_N; i++){
    float f = SW_HZ[i];
    if(f < fs * 0.45f && (_swMask >> i & 1)){
      phase = 0;
      /*  600 мс тону; аналізуємо з 300-ї мс — буфери передачі й приймання
          разом затримують звук менш ніж на 200 мс  */
      memset(cap, 0, capN * sizeof(float));
      ok = pump(f, fs * 6 / 10, cap, fs * 3 / 10) && pump(0, fs / 10, nullptr, 0);
      if(!ok) break;
      float a = toneAmp(cap, capN, wsum, f, fs);
      _swDb[i]    = toDb(a);
      _swNoise[i] = toDb(toneAmp(sil, capN, wsum, f, fs));
      if(2 * f < fs * 0.48f) _swH2[i] = toDb(toneAmp(cap, capN, wsum, 2 * f, fs)) - _swDb[i];
      if(3 * f < fs * 0.48f) _swH3[i] = toDb(toneAmp(cap, capN, wsum, 3 * f, fs)) - _swDb[i];
    }
    _swPos = i + 1;
  }
  i2s_zero_dma_buffer(I2S_NUM_0);
  if(MUTE_PIN != 255 && !player.isRunning()) digitalWrite(MUTE_PIN, MUTE_VAL);
  free(cap); free(sil); free(hw); free(out);
  _swState = ok ? 2 : 3;
}

/*  ---------- що робити з почутим (головний цикл) ---------- */

static const char* const ACT_NAME[MA_N] = { "типово", "нічого", "пауза / грати", "наступна", "попередня",
                                            "гучніше", "тихіше", "екран", "обране 1" };

const char* YoMic::actionName(uint8_t a){ return a < MA_N ? ACT_NAME[a] : ""; }

uint8_t YoMic::actionFor(MicGesture g){
  const ExtStore& e = extras.s;
  uint8_t a = g == MG_CLAP2 ? e.clap2 : g == MG_CLAP3 ? e.clap3 : g == MG_KNOCK2 ? e.knock2 : e.knock3;
  if(a == MA_DEFAULT || a >= MA_N) a = (g == MG_CLAP2 || g == MG_KNOCK2) ? MA_TOGGLE : MA_NEXT;
  return a;
}

const char* YoMic::gestureName(uint8_t g){
  switch(g){
    case MG_CLAP2: return "2 хлопки";
    case MG_CLAP3: return "3 хлопки";
    case MG_KNOCK2: return "2 стуки";
    case MG_KNOCK3: return "3 стуки";
  }
  return "";
}

void YoMic::loop(){
  uint32_t now = millis();
  MicGesture g = takeGesture();
  if(g != MG_NONE){
    uint8_t a = actionFor(g);
    Serial.printf("##MIC#\t%s → %s\n", gestureName(g), actionName(a));
    switch(a){
      case MA_TOGGLE: player.toggle(); break;
      case MA_NEXT:   player.next(); break;
      case MA_PREV:   player.prev(); break;
      case MA_VOLUP:  player.stepVol(true); break;
      case MA_VOLDN:  player.stepVol(false); break;
      case MA_SCREEN: if(!extras.touchWake()) extras.screenOff(); break;
      case MA_FAV1:   extras.favPlay(0); break;
      default: break;
    }
  }
  if(!listening()){ extras.setPresenceDark(false); _earFrom = 0; return; }

  /*  Таймер сну слухає: у кімнаті N хвилин тихо — затихаємо, не чекаючи кінця.  */
  const ExtStore& e = extras.s;
  if(e.sleepEar && extras.sleepMinutes()){
    if(!_earFrom) _earFrom = now;
    uint32_t last = _lastRoom > _earFrom ? _lastRoom : _earFrom;
    uint32_t need = (uint32_t)(e.sleepEarMin ? e.sleepEarMin : 10) * 60000UL;
    if(now - last > need && extras.sleepSoon())
      Serial.printf("##MIC#\tу кімнаті тихо %u хв — затихаю\n", (unsigned)(need / 60000));
  }else _earFrom = 0;

  /*  Присутність: заговорили — екран прокидається; довго тихо — гасне.  */
  if(e.presWake && _lastVoice && _lastVoice != _wokeFor && now - _lastVoice < 1500){
    _wokeFor = _lastVoice;
    if(extras.screenDim()){ extras.touchWake(); Serial.println("##MIC#\tголос — будю екран"); }
  }
  if(e.presOff){
    if(!_presFrom) _presFrom = now;
    uint32_t last = _lastRoom;
    if(extras.lastTouchMs() > last) last = extras.lastTouchMs();
    if(_presFrom > last) last = _presFrom;
    extras.setPresenceDark(now - last > (uint32_t)e.presOff * 60000UL);
  }else{ _presFrom = 0; extras.setPresenceDark(false); }
}

/*  ---------- перевірка жестів без рук ----------
    Радіо саме грає через динамік короткі хлопки (шумовий імпульс зі спадом
    ~8 мс, без низу) або стук (затухаючий тон 400 Гц), із заданим кроком, а
    мікрофон ловить їх звичайним шляхом: удари, ритм, дія. Лише коли плеєр
    стоїть — інакше нема чим грати й звук радіо змішався б із перевіркою.  */
const char* YoMic::simStart(uint8_t kind, uint8_t count, uint16_t gapMs){
  if(!listening())       return "мікрофон не слухає";
  if(player.isRunning()) return "спершу зупиніть звук";
  if(_tsKind || _swState == 1) return "уже зайнятий";
  if(count < 1) count = 1;
  if(count > 6) count = 6;
  _tsCount = count; _tsGap = gapMs < 60 ? 60 : gapMs; _tsPos = 0;
  if(MUTE_PIN != 255) digitalWrite(MUTE_PIN, !MUTE_VAL);
  _tsKind = kind ? 2 : 1;
  return nullptr;
}

void YoMic::_simChunk(int16_t* buf){
  const uint32_t FR = 256;
  uint32_t fs = player.getSampleRate(); if(fs < 8000) fs = 44100;
  uint32_t burst = fs * 40 / 1000, period = fs * _tsGap / 1000;
  /*  півтори секунди тиші наприкінці: у буферах I2S іще лежить не зіграний
      хвіст, і вимкнений раніше підсилювач «з'їдав» останній удар  */
  uint32_t lead = fs * 4 / 10;                                  /* 400 мс: підсилювач щойно ввімкнено, він ще прокидається */
  uint32_t total = lead + period * (_tsCount - 1) + burst + fs * 3 / 2;
  static uint32_t rnd = 22222;
  static int16_t prev = 0;
  for(uint32_t j = 0; j < FR; j++){
    uint32_t t = _tsPos + j;
    float v = 0;
    uint32_t n = t >= lead ? (t - lead) / period : 0xFFFF, k = t >= lead ? (t - lead) % period : 0;
    if(n < _tsCount && k < burst){
      float tt = (float)k / fs;
      if(_tsKind == 1){
        rnd = rnd * 1664525UL + 1013904223UL;
        float w = ((int32_t)(rnd >> 16) - 32768) / 32768.0f;
        v = w * exp(-tt / 0.008f) * 0.9f;
      }else{
        v = sin(2.0f * M_PI * 400.0f * tt) * exp(-tt / 0.015f) * 0.9f;
      }
    }
    int16_t x = (int16_t)(v * 32767.0f);
    int16_t y = _tsKind == 1 ? (int16_t)((x - prev) / 2) : x;    /* хлопок — без низу */
    prev = x;
    buf[2*j] = y; buf[2*j+1] = y;
  }
  /*  лише записане рахується: відкинуті кадри раніше «з'їдали» наступний удар  */
  size_t w = 0;
  i2s_write(I2S_NUM_0, buf, FR * 4, &w, pdMS_TO_TICKS(600));
  _tsPos += w / 4;
  if(_tsPos >= total){
    _tsKind = 0;
    if(MUTE_PIN != 255 && !player.isRunning()) digitalWrite(MUTE_PIN, MUTE_VAL);
  }
}
