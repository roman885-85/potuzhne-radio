#include "yoSpectrum.h"
#include "esp_heap_caps.h"

YoSpectrum yoSpec;

static void fft(float* re, float* im, uint16_t n){
  for(uint16_t i = 1, j = 0; i < n; i++){
    uint16_t bit = n >> 1;
    for(; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if(i < j){ float t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
  }
  for(uint16_t len = 2; len <= n; len <<= 1){
    const float ang = -2.0f * (float)M_PI / len;
    const float wr = cosf(ang), wi = sinf(ang);
    for(uint16_t i = 0; i < n; i += len){
      float cr = 1, ci = 0;
      for(uint16_t k = 0; k < len / 2; k++){
        uint16_t a = i + k, b = a + len / 2;
        const float tr = re[b] * cr - im[b] * ci, ti = re[b] * ci + im[b] * cr;
        re[b] = re[a] - tr; im[b] = im[a] - ti;
        re[a] += tr; im[a] += ti;
        const float nr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = nr;
      }
    }
  }
}

bool YoSpectrum::bands(float* out, uint8_t n, uint32_t sr){
  if(n > 64) n = 64;
  uint32_t now = millis();
  float dt = _lastT ? (now - _lastT) / 1000.0f : 0.05f;
  _lastT = now;
  if(dt > 0.3f) dt = 0.3f;
  if(!_buf){
    int16_t* b = (int16_t*)heap_caps_calloc(N, sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if(!b) return false;
    _buf = b;
  }
  if(!_re){
    _re  = (float*)heap_caps_malloc(sizeof(float) * N, MALLOC_CAP_SPIRAM);
    _im  = (float*)heap_caps_malloc(sizeof(float) * N, MALLOC_CAP_SPIRAM);
    _win = (float*)heap_caps_malloc(sizeof(float) * N, MALLOC_CAP_SPIRAM);
    if(!_re || !_im || !_win) return false;
    for(uint16_t i = 0; i < N; i++) _win[i] = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * i / (N - 1));
  }
  const bool live = (now - _lastMs < 200) && sr >= 8000;
  float lv[64] = { 0 };
  if(live){
    const uint16_t w = _w;
    for(uint16_t i = 0; i < N; i++){ _re[i] = _buf[(w + i) & (N - 1)] / 32768.0f * _win[i]; _im[i] = 0; }
    fft(_re, _im, N);
    const float fmax = sr / 2.0f < 16000.0f ? sr / 2.0f : 16000.0f;
    const float fmin = 50.0f;
    for(uint8_t b = 0; b < n; b++){
      /*  смуга — від f0 до f1 у логарифмічному масштабі  */
      const float f0 = fmin * powf(fmax / fmin, (float)b / n), f1 = fmin * powf(fmax / fmin, (float)(b + 1) / n);
      const float fc = sqrtf(f0 * f1);
      const float b0 = f0 * N / sr, b1 = f1 * N / sr;
      float p = 0;
      if(b1 - b0 < 1.5f){
        /*  вузька (низи): значення між сусідніми відліками на середині смуги —
            інакше кілька смуг брали б той самий відлік і стояли однаково  */
        const float fb = fc * N / sr;
        int i = (int)fb; float a = fb - i;
        if(i < 1) i = 1; if(i >= N / 2 - 1) i = N / 2 - 2;
        const float q0 = sqrtf(_re[i] * _re[i] + _im[i] * _im[i]), q1 = sqrtf(_re[i + 1] * _re[i + 1] + _im[i + 1] * _im[i + 1]);
        const float m = q0 + (q1 - q0) * a;
        p = m * m;
      }else{
        int i0 = (int)b0, i1 = (int)b1;
        if(i0 < 1) i0 = 1;
        if(i1 > N / 2) i1 = N / 2;
        for(int i = i0; i < i1; i++){ float q = _re[i] * _re[i] + _im[i] * _im[i]; if(q > p) p = q; }
      }
      /*  синус повної шкали дає |X| = N/4 (вікно Ганна)  */
      const float amp = sqrtf(p) / (N / 4.0f);
      /*  Нахил 4 дБ на октаву відносно 1 кГц, як у звичайних аналізаторах: у музиці
          енергія спадає до верхів, і без нахилу низи й середина стояли під стелею, а
          верхи ледь ворушились. Шкала: −55 дБ — нуль, +5 дБ — повна риска; гучна
          музика дає піки на 70–80 % висоти.  */
      const float db = 20.0f * log10f(amp + 1e-9f) + 4.0f * log2f(fc / 1000.0f);
      float l = (db + 55.0f) / 60.0f;
      if(l < 0) l = 0; if(l > 1) l = 1;
      lv[b] = l;
    }
  }
  bool any = false;
  for(uint8_t b = 0; b < n; b++){
    /*  Спершу рівень смуги усереднюємо по кількох вікнах, потім риска їде до
        нього так само м'яко, як у попередньому варіанті: угору 0,6, униз 0,25
        відстані за крок у 45 мс (крок перераховано під справжній проміжок).  */
    const float k = dt / 0.045f;
    float av = 0.45f * k; if(av > 1) av = 1;
    _avg[b] += (lv[b] - _avg[b]) * av;
    float up = 0.6f * k, dn = 0.25f * k;
    if(up > 1) up = 1; if(dn > 1) dn = 1;
    _smooth[b] += (_avg[b] - _smooth[b]) * (_avg[b] > _smooth[b] ? up : dn);
    if(_smooth[b] < 0.01f) _smooth[b] = 0;
    out[b] = _smooth[b];
    if(out[b] > 0) any = true;
  }
  return any;
}
