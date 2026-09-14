#include "uicanvas.h"
#include "esp_heap_caps.h"
#include "../core/options.h"
#include "../displays/dspcore.h"
#include "../displays/tools/spidma.h"
#include "../displays/fonts/aa/aaUI8.h"
#include "../displays/fonts/aa/aaUI9.h"
#include "../displays/fonts/aa/aaUI9b.h"
#include "../displays/fonts/aa/aaUI11.h"
#include "../displays/fonts/aa/aaUI12b.h"
#include "../displays/fonts/aa/aaUI13.h"
#include "../displays/fonts/aa/aaUI14b.h"
#include "../displays/fonts/aa/aaUI15.h"
#include "../displays/fonts/aa/aaUI6.h"
#include "../displays/fonts/aa/aaUI26b.h"

extern DspCore dsp;
UiCanvas ui;

static const int16_t CW_ = 320, CH_ = 240;
static const int FX_STACK = 2048;
static const uint32_t FX_MS = 420;               /* життя хвилі від дотику */

UiCanvas::UiCanvas() : GFXcanvas16(CW_, CH_, false) {}

bool UiCanvas::begin(){
  if(buffer) return true;
  buffer = (uint16_t*)heap_caps_calloc((size_t)CW_ * CH_, sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if(!buffer) return false;
  if(!spidmaScratch((size_t)CW_ * TS * 2)){ heap_caps_free(buffer); buffer = nullptr; return false; }
  _seen  = (uint32_t*)heap_caps_malloc(((size_t)CW_ * CH_ + 31) / 32 * 4, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  _stack = (uint16_t*)heap_caps_malloc(FX_STACK * 2 * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return true;
}

void UiCanvas::setActive(bool on){ _active = on && buffer; if(!_active){ _fx.on = false; _fx.req = false; } }

bool UiCanvas::isAA(const GFXfont* f){
  return f == &aaUI8 || f == &aaUI9 || f == &aaUI9b || f == &aaUI11 || f == &aaUI12b ||
         f == &aaUI13 || f == &aaUI14b || f == &aaUI15 || f == &aaUI6 || f == &aaUI26b;
}

/*  ---------- зміни ---------- */
void UiCanvas::_markRaw(int16_t x, int16_t y, int16_t w, int16_t h){
  if(w <= 0 || h <= 0) return;
  int16_t x1 = x + w - 1, y1 = y + h - 1;
  if(x < 0) x = 0;
  if(y < 0) y = 0;
  if(x1 >= CW_) x1 = CW_ - 1;
  if(y1 >= CH_) y1 = CH_ - 1;
  if(x > x1 || y > y1) return;
  int tx0 = x / TS, tx1 = x1 / TS, ty0 = y / TS, ty1 = y1 / TS;
  uint32_t bits = (tx1 - tx0 == 31) ? 0xFFFFFFFFUL : (((1UL << (tx1 - tx0 + 1)) - 1) << tx0);
  portENTER_CRITICAL(&_mux);
  for(int ty = ty0; ty <= ty1; ty++) _dirty[ty] |= bits;
  portEXIT_CRITICAL(&_mux);
}
void UiCanvas::mark(int16_t x, int16_t y, int16_t w, int16_t h){ _markRaw(x, y, w, h); }
void UiCanvas::markAll(){ _markRaw(0, 0, CW_, CH_); }

/*  ---------- малювання в кадр ---------- */
void UiCanvas::drawPixel(int16_t x, int16_t y, uint16_t color){
  if(!buffer || x < 0 || y < 0 || x >= CW_ || y >= CH_) return;
  buffer[(int32_t)y * CW_ + x] = color;
  _markRaw(x, y, 1, 1);
}

void UiCanvas::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color){
  if(!buffer) return;
  if(w < 0){ x += w + 1; w = -w; }
  if(h < 0){ y += h + 1; h = -h; }
  if(x < 0){ w += x; x = 0; }
  if(y < 0){ h += y; y = 0; }
  if(x + w > CW_) w = CW_ - x;
  if(y + h > CH_) h = CH_ - y;
  if(w <= 0 || h <= 0) return;
  for(int16_t yy = y; yy < y + h; yy++){
    uint16_t* p = buffer + (int32_t)yy * CW_ + x;
    for(int16_t i = 0; i < w; i++) p[i] = color;
  }
  _markRaw(x, y, w, h);
}
void UiCanvas::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color){ fillRect(x, y, w, 1, color); }
void UiCanvas::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color){ fillRect(x, y, 1, h, color); }
void UiCanvas::fillScreen(uint16_t color){ fillRect(0, 0, CW_, CH_, color); }

uint16_t UiCanvas::blend(uint16_t bg, uint16_t fg, uint8_t a) const {
  if(a >= 250) return fg;
  if(a <= 4) return bg;
  uint32_t ia = 255 - a;
  uint32_t r = (((fg >> 11) & 31) * a + ((bg >> 11) & 31) * ia + 127) / 255;
  uint32_t g = (((fg >> 5) & 63) * a + ((bg >> 5) & 63) * ia + 127) / 255;
  uint32_t b = ((fg & 31) * a + (bg & 31) * ia + 127) / 255;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

/*  ---------- згладжений текст ---------- */
size_t UiCanvas::write(uint8_t c){
  if(!gfxFont || !isAA(gfxFont)) return GFXcanvas16::write(c);
  if(c == '\n'){ cursor_x = 0; cursor_y += (int16_t)textsize_y * gfxFont->yAdvance; return 1; }
  if(c == '\r') return 1;
  if(c < gfxFont->first || c > gfxFont->last) return 1;
  const GFXglyph* g = gfxFont->glyph + (c - gfxFont->first);
  if(g->width && g->height) _drawAAGlyph(cursor_x, cursor_y, c);
  cursor_x += (int16_t)g->xAdvance * textsize_x;
  return 1;
}

void UiCanvas::_drawAAGlyph(int16_t x, int16_t y, uint8_t c){
  if(!buffer) return;
  const GFXglyph* g = gfxFont->glyph + (c - gfxFont->first);
  const uint8_t* bm = gfxFont->bitmap + g->bitmapOffset;
  uint8_t s = textsize_x ? textsize_x : 1;
  int16_t w = g->width, h = g->height;
  int16_t x0 = x + g->xOffset * s, y0 = y + g->yOffset * s;
  uint16_t fg = textcolor;
  for(int16_t yy = 0; yy < h; yy++){
    for(int16_t xx = 0; xx < w; xx++){
      uint32_t idx = (uint32_t)yy * w + xx;
      uint8_t nib = (idx & 1) ? (bm[idx >> 1] & 0x0F) : (bm[idx >> 1] >> 4);
      if(!nib) continue;
      uint8_t a = nib * 17;
      for(uint8_t sy = 0; sy < s; sy++){
        int16_t py = y0 + yy * s + sy;
        if(py < 0 || py >= CH_) continue;
        uint16_t* row = buffer + (int32_t)py * CW_;
        for(uint8_t sx = 0; sx < s; sx++){
          int16_t px = x0 + xx * s + sx;
          if(px < 0 || px >= CW_) continue;
          row[px] = nib == 15 ? fg : blend(row[px], fg, a);
        }
      }
    }
  }
  _markRaw(x0, y0, w * s, h * s);
}

/*  ---------- згладжені фігури ---------- */
void UiCanvas::fillCircleAA(float cx, float cy, float r, uint16_t color){
  if(!buffer) return;
  int16_t x0 = (int16_t)floorf(cx - r - 1), x1 = (int16_t)ceilf(cx + r + 1);
  int16_t y0 = (int16_t)floorf(cy - r - 1), y1 = (int16_t)ceilf(cy + r + 1);
  for(int16_t y = y0; y <= y1; y++){
    if(y < 0 || y >= CH_) continue;
    for(int16_t x = x0; x <= x1; x++){
      if(x < 0 || x >= CW_) continue;
      float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
      float cov = r - sqrtf(dx * dx + dy * dy) + 0.5f;
      if(cov <= 0) continue;
      uint16_t* p = buffer + (int32_t)y * CW_ + x;
      *p = cov >= 1 ? color : blend(*p, color, (uint8_t)(cov * 255));
    }
  }
  _markRaw(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
}

void UiCanvas::_cover(int16_t px, int16_t py, float cov, uint16_t color){
  if(cov <= 0 || px < 0 || py < 0 || px >= CW_ || py >= CH_) return;
  uint16_t* p = buffer + (int32_t)py * CW_ + px;
  *p = cov >= 1 ? color : blend(*p, color, (uint8_t)(cov * 255));
}

static void roundRect(UiCanvas& c, uint16_t* buf, int16_t x, int16_t y, int16_t w, int16_t h, float r,
                      uint16_t color, int32_t bg){
  if(!buf || w <= 0 || h <= 0) return;
  if(r > w / 2.0f) r = w / 2.0f;
  if(r > h / 2.0f) r = h / 2.0f;
  if(r < 0) r = 0;
  int16_t ri = (int16_t)ceilf(r);
  /*  середина — звичайними прямокутниками  */
  c.fillRect(x + ri, y, w - 2 * ri, h, color);
  c.fillRect(x, y + ri, ri, h - 2 * ri, color);
  c.fillRect(x + w - ri, y + ri, ri, h - 2 * ri, color);
  if(!ri) return;
  /*  кути — із покриттям  */
  const float cxl = x + r, cxr = x + w - r, cyt = y + r, cyb = y + h - r;
  for(int16_t yy = 0; yy < ri; yy++){
    for(int16_t xx = 0; xx < ri; xx++){
      for(uint8_t k = 0; k < 4; k++){
        int16_t px = (k & 1) ? (x + w - ri + xx) : (x + xx);
        int16_t py = (k & 2) ? (y + h - ri + yy) : (y + yy);
        if(px < 0 || py < 0 || px >= 320 || py >= 240) continue;
        float ccx = (k & 1) ? cxr : cxl, ccy = (k & 2) ? cyb : cyt;
        float dx = px + 0.5f - ccx, dy = py + 0.5f - ccy;
        bool inX = (k & 1) ? (dx > 0) : (dx < 0);
        bool inY = (k & 2) ? (dy > 0) : (dy < 0);
        float cov = 1.0f;
        if(inX && inY) cov = r - sqrtf(dx * dx + dy * dy) + 0.5f;
        uint16_t* p = buf + (int32_t)py * 320 + px;
        uint16_t under = bg >= 0 ? (uint16_t)bg : *p;
        if(cov <= 0){ if(bg >= 0) *p = under; continue; }
        *p = cov >= 1 ? color : c.blend(under, color, (uint8_t)(cov * 255));
      }
    }
  }
  c.mark(x, y, w, h);
}

void UiCanvas::fillRoundRectAA(int16_t x, int16_t y, int16_t w, int16_t h, float r, uint16_t color){
  roundRect(*this, buffer, x, y, w, h, r, color, -1);
}

void UiCanvas::box(int16_t x, int16_t y, int16_t w, int16_t h, float r, uint16_t color, uint16_t bg){
  roundRect(*this, buffer, x, y, w, h, r, color, bg);
}

void UiCanvas::frame(int16_t x, int16_t y, int16_t w, int16_t h, float r, float t, uint16_t line, uint16_t fill, uint16_t bg){
  roundRect(*this, buffer, x, y, w, h, r, line, bg);
  int16_t ti = (int16_t)lroundf(t);
  float ri = r - t; if(ri < 0.5f) ri = 0.5f;
  roundRect(*this, buffer, x + ti, y + ti, w - 2 * ti, h - 2 * ti, ri, fill, line);
}

void UiCanvas::lineAA(float x0, float y0, float x1, float y1, float wd, uint16_t color){
  if(!buffer) return;
  const float hw = wd / 2.0f;
  int16_t bx0 = (int16_t)floorf((x0 < x1 ? x0 : x1) - hw - 1), bx1 = (int16_t)ceilf((x0 > x1 ? x0 : x1) + hw + 1);
  int16_t by0 = (int16_t)floorf((y0 < y1 ? y0 : y1) - hw - 1), by1 = (int16_t)ceilf((y0 > y1 ? y0 : y1) + hw + 1);
  const float vx = x1 - x0, vy = y1 - y0, len2 = vx * vx + vy * vy;
  for(int16_t py = by0; py <= by1; py++){
    if(py < 0 || py >= CH_) continue;
    for(int16_t px = bx0; px <= bx1; px++){
      if(px < 0 || px >= CW_) continue;
      const float cx = px + 0.5f - x0, cy = py + 0.5f - y0;
      float k = len2 > 0 ? (cx * vx + cy * vy) / len2 : 0;
      if(k < 0) k = 0; else if(k > 1) k = 1;
      const float dx = cx - k * vx, dy = cy - k * vy;
      _cover(px, py, hw - sqrtf(dx * dx + dy * dy) + 0.5f, color);
    }
  }
  _markRaw(bx0, by0, bx1 - bx0 + 1, by1 - by0 + 1);
}

void UiCanvas::arcAA(float cx, float cy, float r, float wd, uint16_t color, float a0, float a1){
  if(!buffer) return;
  const float hw = wd / 2.0f;
  const bool full = (a1 - a0) >= 359.9f;
  const float span = a1 - a0;
  const float d2r = 0.0174533f;
  const float e0x = cx + r * sinf(a0 * d2r), e0y = cy - r * cosf(a0 * d2r);
  const float e1x = cx + r * sinf(a1 * d2r), e1y = cy - r * cosf(a1 * d2r);
  int16_t bx0 = (int16_t)floorf(cx - r - hw - 1), bx1 = (int16_t)ceilf(cx + r + hw + 1);
  int16_t by0 = (int16_t)floorf(cy - r - hw - 1), by1 = (int16_t)ceilf(cy + r + hw + 1);
  for(int16_t py = by0; py <= by1; py++){
    if(py < 0 || py >= CH_) continue;
    for(int16_t px = bx0; px <= bx1; px++){
      if(px < 0 || px >= CW_) continue;
      const float dx = px + 0.5f - cx, dy = py + 0.5f - cy;
      const float dist = sqrtf(dx * dx + dy * dy);
      if(fabsf(dist - r) > hw + 1) continue;
      bool in = full;
      if(!full){
        float ang = atan2f(dx, -dy) / d2r;
        float rel = fmodf(ang - a0 + 720.0f, 360.0f);
        in = rel <= span;
      }
      float cov;
      if(in) cov = hw - fabsf(dist - r) + 0.5f;
      else{                                         /* круглі кінці */
        float ax = px + 0.5f - e0x, ay = py + 0.5f - e0y, bx = px + 0.5f - e1x, by = py + 0.5f - e1y;
        float d = sqrtf(ax * ax + ay * ay), d1 = sqrtf(bx * bx + by * by);
        cov = hw - (d < d1 ? d : d1) + 0.5f;
      }
      _cover(px, py, cov, color);
    }
  }
  _markRaw(bx0, by0, bx1 - bx0 + 1, by1 - by0 + 1);
}

void UiCanvas::triAA(float x0, float y0, float x1, float y1, float x2, float y2, uint16_t color){
  const float xy[6] = { x0, y0, x1, y1, x2, y2 };
  polyAA(xy, 3, color);
}

/*  Покриття — за відстанню до найближчого краю: усередині 0,5 + d, зовні 0,5 − d.
    Годиться й для увігнутих фігур (зірка), без швів між частинами.  */
void UiCanvas::polyAA(const float* xy, uint8_t n, uint16_t color){
  if(!buffer || n < 3) return;
  float mnx = xy[0], mxx = xy[0], mny = xy[1], mxy = xy[1];
  for(uint8_t i = 1; i < n; i++){
    float x = xy[2 * i], y = xy[2 * i + 1];
    if(x < mnx) mnx = x; if(x > mxx) mxx = x; if(y < mny) mny = y; if(y > mxy) mxy = y;
  }
  int16_t bx0 = (int16_t)floorf(mnx) - 1, bx1 = (int16_t)ceilf(mxx) + 1, by0 = (int16_t)floorf(mny) - 1, by1 = (int16_t)ceilf(mxy) + 1;
  for(int16_t py = by0; py <= by1; py++){
    if(py < 0 || py >= CH_) continue;
    const float qy = py + 0.5f;
    for(int16_t px = bx0; px <= bx1; px++){
      if(px < 0 || px >= CW_) continue;
      const float qx = px + 0.5f;
      bool inside = false;
      float dmin = 1e9f;
      for(uint8_t i = 0, j = n - 1; i < n; j = i++){
        const float ax = xy[2 * i], ay = xy[2 * i + 1], bx = xy[2 * j], by = xy[2 * j + 1];
        if(((ay > qy) != (by > qy)) && (qx < (bx - ax) * (qy - ay) / (by - ay) + ax)) inside = !inside;
        const float vx = bx - ax, vy = by - ay, l2 = vx * vx + vy * vy;
        float k = l2 > 0 ? ((qx - ax) * vx + (qy - ay) * vy) / l2 : 0;
        if(k < 0) k = 0; else if(k > 1) k = 1;
        const float dx = qx - ax - k * vx, dy = qy - ay - k * vy, d2 = dx * dx + dy * dy;
        if(d2 < dmin) dmin = d2;
      }
      const float d = sqrtf(dmin);
      _cover(px, py, inside ? 0.5f + d : 0.5f - d, color);
    }
  }
  _markRaw(bx0, by0, bx1 - bx0 + 1, by1 - by0 + 1);
}

/*  ---------- відгук на дотик ---------- */
void UiCanvas::press(int16_t x, int16_t y, uint16_t glow){
  if(!buffer || !_active) return;
  portENTER_CRITICAL(&_mux);
  _fx.req = true; _fx.x = x; _fx.y = y; _fx.glow = glow; _fx.t0 = millis();
  portEXIT_CRITICAL(&_mux);
}

/*  Кнопка під пальцем — суцільна пляма одного кольору навколо точки дотику
    (текст на ній — дірки, рамка від того не міняється). Чорне тло кнопкою
    не вважаємо, як і пляму на пів екрана.  */
bool UiCanvas::_region(int16_t x, int16_t y, int16_t& bx, int16_t& by, int16_t& bw, int16_t& bh){
  if(!_seen || !_stack) return false;
  /*  колір — найчастіший у квадраті 7×7: палець міг лягти на літеру  */
  uint16_t cols[49]; uint8_t cnt[49]; uint8_t nc = 0;
  for(int16_t dy = -3; dy <= 3; dy++) for(int16_t dx = -3; dx <= 3; dx++){
    int16_t px = x + dx, py = y + dy;
    if(px < 0 || py < 0 || px >= CW_ || py >= CH_) continue;
    uint16_t v = buffer[(int32_t)py * CW_ + px];
    uint8_t k = 0; while(k < nc && cols[k] != v) k++;
    if(k == nc){ cols[nc] = v; cnt[nc] = 0; nc++; }
    cnt[k]++;
  }
  if(!nc) return false;
  uint8_t best = 0; for(uint8_t k = 1; k < nc; k++) if(cnt[k] > cnt[best]) best = k;
  const uint16_t c = cols[best];
  if(c == 0x0000) return false;
  /*  стартова точка — найближчий піксель цього кольору  */
  int16_t sx = -1, sy = -1;
  for(int16_t dy = -3; dy <= 3 && sx < 0; dy++) for(int16_t dx = -3; dx <= 3; dx++){
    int16_t px = x + dx, py = y + dy;
    if(px >= 0 && py >= 0 && px < CW_ && py < CH_ && buffer[(int32_t)py * CW_ + px] == c){ sx = px; sy = py; break; }
  }
  if(sx < 0) return false;
  memset(_seen, 0, ((size_t)CW_ * CH_ + 31) / 32 * 4);
  int16_t x0 = sx, x1 = sx, y0 = sy, y1 = sy;
  int sp = 0;
  _stack[0] = sx; _stack[1] = sy; sp = 1;
  #define SEEN(px, py) (_seen[((int32_t)(py) * CW_ + (px)) >> 5] & (1UL << ((((int32_t)(py) * CW_ + (px))) & 31)))
  #define SETSEEN(px, py) (_seen[((int32_t)(py) * CW_ + (px)) >> 5] |= (1UL << ((((int32_t)(py) * CW_ + (px))) & 31)))
  uint32_t guard = 0;
  while(sp > 0 && ++guard < 20000){
    sp--;
    int16_t px = _stack[sp * 2], py = _stack[sp * 2 + 1];
    if(SEEN(px, py) || buffer[(int32_t)py * CW_ + px] != c) continue;
    int16_t l = px, r = px;
    const uint16_t* row = buffer + (int32_t)py * CW_;
    while(l > 0 && row[l - 1] == c && !SEEN(l - 1, py)) l--;
    while(r < CW_ - 1 && row[r + 1] == c && !SEEN(r + 1, py)) r++;
    for(int16_t i = l; i <= r; i++) SETSEEN(i, py);
    if(l < x0) x0 = l; if(r > x1) x1 = r; if(py < y0) y0 = py; if(py > y1) y1 = py;
    if((x1 - x0) > 300 || (y1 - y0) > 190) return false;       /* це вже тло, а не кнопка */
    for(int8_t d = -1; d <= 1; d += 2){
      int16_t ny = py + d;
      if(ny < 0 || ny >= CH_) continue;
      const uint16_t* nr = buffer + (int32_t)ny * CW_;
      bool inRun = false;
      for(int16_t i = l; i <= r; i++){
        bool ok = nr[i] == c && !SEEN(i, ny);
        if(ok && !inRun && sp < FX_STACK){ _stack[sp * 2] = i; _stack[sp * 2 + 1] = ny; sp++; }
        inRun = ok;
      }
    }
  }
  #undef SEEN
  #undef SETSEEN
  if(x1 - x0 < 10 || y1 - y0 < 10) return false;               /* смужка, а не кнопка */
  bx = x0; by = y0; bw = x1 - x0 + 1; bh = y1 - y0 + 1;
  return true;
}

/*  ---------- на екран ---------- */
void UiCanvas::flush(){
  if(!buffer || !_active) return;
  /*  хвиля від дотику: знайти кнопку (тут, у задачі дисплея) і позначити її  */
  bool fxReq = false; int16_t fxX = 0, fxY = 0;
  portENTER_CRITICAL(&_mux);
  if(_fx.req){ fxReq = true; _fx.req = false; fxX = _fx.x; fxY = _fx.y; }
  portEXIT_CRITICAL(&_mux);
  if(fxReq){
    if(_fx.on) _markRaw(_fx.bx, _fx.by, _fx.bw, _fx.bh);        /* стара хвиля — прибрати */
    int16_t bx, by, bw, bh;
    _fx.panel = _region(fxX, fxY, bx, by, bw, bh);
    if(!_fx.panel){ const int16_t R = 26; bx = fxX - R; by = fxY - R; bw = bh = 2 * R + 1; _fx.rmax = R; }
    else{
      /*  до найдальшого кута кнопки  */
      float dx = (fxX - bx) > (bx + bw - fxX) ? (fxX - bx) : (bx + bw - fxX);
      float dy = (fxY - by) > (by + bh - fxY) ? (fxY - by) : (by + bh - fxY);
      _fx.rmax = sqrtf(dx * dx + dy * dy) + 2;
    }
    _fx.bx = bx; _fx.by = by; _fx.bw = bw; _fx.bh = bh;
    _fx.on = true;
  }
  bool fxOn = false; float fxR = 0; uint8_t fxA = 0;
  if(_fx.on){
    uint32_t age = _fxFreeze >= 0 ? (uint32_t)_fxFreeze : millis() - _fx.t0;
    _markRaw(_fx.bx, _fx.by, _fx.bw, _fx.bh);
    if(age >= FX_MS) _fx.on = false;                              /* останній кадр — уже чистий */
    else{
      float k = age / 240.0f; if(k > 1) k = 1;
      float e = 1 - (1 - k) * (1 - k) * (1 - k);
      float fade = age < 140 ? 1.0f : 1.0f - (age - 140) / (float)(FX_MS - 140);
      fxOn = true;
      if(_fx.panel){ fxR = 6 + (_fx.rmax - 6) * e; fxA = (uint8_t)(70 * fade); }
      else{ fxR = _fx.rmax * (0.55f + 0.45f * e); fxA = (uint8_t)(110 * fade); }
    }
  }
  uint32_t rows[TH];
  portENTER_CRITICAL(&_mux);
  memcpy(rows, _dirty, sizeof(rows));
  memset(_dirty, 0, sizeof(_dirty));
  portEXIT_CRITICAL(&_mux);
  bool any = false;
  for(int r = 0; r < TH; r++) if(rows[r]) { any = true; break; }
  if(!any) return;
  _dma = (uint8_t*)spidmaScratch((size_t)CW_ * TS * 2);        /* спільна смуга: беремо щоразу */
  if(!_dma) return;
  bool dma = spidmaOk() || spidmaBegin();
  dsp.startWrite();
  for(int r = 0; r < TH; r++){
    while(rows[r]){
      uint32_t m = rows[r];
      int s = __builtin_ctz(m), e = s;
      while(e < TW && ((m >> e) & 1)) e++;
      uint32_t run = (((1UL << (e - s)) - 1) << s);
      rows[r] &= ~run;
      int16_t x0 = s * TS, w = (e - s) * TS;
      if(x0 + w > CW_) w = CW_ - x0;
      int16_t y0 = r * TS, h = TS;
      /*  ті самі квадрати в наступних рядках — однією смугою вниз, поки влазить у буфер  */
      for(int rr = r + 1; rr < TH && (rows[rr] & run) == run && (int32_t)w * (h + TS) <= (int32_t)CW_ * TS; rr++){
        rows[rr] &= ~run;
        h += TS;
      }
      if(y0 + h > CH_) h = CH_ - y0;
      uint32_t n = 0;
      for(int16_t yy = y0; yy < y0 + h; yy++){
        const uint16_t* src = buffer + (int32_t)yy * CW_ + x0;
        if(fxOn && yy >= _fx.by && yy < _fx.by + _fx.bh && x0 < _fx.bx + _fx.bw && x0 + w > _fx.bx){
          const float r2in = (fxR - 1) > 0 ? (fxR - 1) * (fxR - 1) : 0, r2out = (fxR + 1) * (fxR + 1);
          for(int16_t i = 0; i < w; i++){
            uint16_t v = src[i];
            int16_t px = x0 + i;
            if(px >= _fx.bx && px < _fx.bx + _fx.bw){
              const float dx = px - _fx.x, dy = yy - _fx.y, d2 = dx * dx + dy * dy;
              if(_fx.panel){
                /*  по кнопці — світла хвиля; чорні літери лишаються чіткими  */
                if(v != 0x0000 && d2 < r2out){
                  uint8_t a = fxA;
                  if(d2 > r2in) a = (uint8_t)(fxA * (fxR + 1 - sqrtf(d2)) / 2);
                  v = blend(v, 0xFFFF, a);
                }
              }else if(d2 < fxR * fxR){
                const float q = 1 - sqrtf(d2) / fxR;
                v = blend(v, _fx.glow, (uint8_t)(fxA * q * q));
              }
            }
            _dma[n++] = v >> 8; _dma[n++] = (uint8_t)v;
          }
          continue;
        }
        for(int16_t i = 0; i < w; i++){ uint16_t v = src[i]; _dma[n++] = v >> 8; _dma[n++] = (uint8_t)v; }
      }
      dsp.setAddrWindow(x0, y0, w, h);
      if(!(dma && spidmaWrite(_dma, n))) dsp.writePixels((uint16_t*)_dma, (uint32_t)w * h, true, true);
    }
  }
  dsp.endWrite();
}

/*  ---------- дзеркало прямого виводу ---------- */
void UiCanvas::mirror(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* px, bool be){
  if(!buffer || !_active || !px) return;
  for(int16_t yy = 0; yy < h; yy++){
    int16_t py = y + yy;
    if(py < 0 || py >= CH_) continue;
    for(int16_t xx = 0; xx < w; xx++){
      int16_t qx = x + xx;
      if(qx < 0 || qx >= CW_) continue;
      uint16_t v = px[(int32_t)yy * w + xx];
      buffer[(int32_t)py * CW_ + qx] = be ? (uint16_t)((v >> 8) | (v << 8)) : v;
    }
  }
}

void UiCanvas::mirrorFill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color){
  if(!buffer || !_active) return;
  for(int16_t yy = y; yy < y + h; yy++){
    if(yy < 0 || yy >= CH_) continue;
    for(int16_t xx = x; xx < x + w; xx++) if(xx >= 0 && xx < CW_) buffer[(int32_t)yy * CW_ + xx] = color;
  }
}

void uiMirror(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* px, bool be){ ui.mirror(x, y, w, h, px, be); }
void uiMirrorFill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color){ ui.mirrorFill(x, y, w, h, color); }
