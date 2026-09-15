#include "m2gfx.h"

namespace m2 {

/*  ---------- кодування ---------- */
static uint8_t cpOf(uint32_t u){
  if(u < 0x80) return (uint8_t)u;
  if(u >= 0x410 && u <= 0x44F) return (uint8_t)(0xC0 + (u - 0x410));
  switch(u){
    case 0x401: return 0xA8; case 0x451: return 0xB8;
    case 0x404: return 0xAA; case 0x454: return 0xBA;
    case 0x406: return 0xB2; case 0x456: return 0xB3;
    case 0x407: return 0xAF; case 0x457: return 0xBF;
    case 0x490: return 0xA5; case 0x491: return 0xB4;
    case 0xA0: case 0xA4: case 0xA6: case 0xA7: case 0xA9: case 0xAB: case 0xAC: case 0xAE:
    case 0xB0: case 0xB1: case 0xB5: case 0xB6: case 0xB7: case 0xBB: return (uint8_t)u;
    case 0x2013: return 0x96; case 0x2014: return 0x97;
    case 0x2018: return 0x91; case 0x2019: return 0x92; case 0x201C: return 0x93; case 0x201D: return 0x94;
    case 0x2022: return 0x95; case 0x2026: return 0x85; case 0x2116: return 0xB9; case 0x20AC: return 0x88;
    case 0x2122: return 0x99;
    case 0x2192: return 0xBB;                  /* → — у CP1251 немає, показуємо «»» */
    case 0x203A: return 0xBB; case 0x2039: return 0xAB;   /* › ‹ — показуємо » « */
  }
  return '?';
}

uint16_t toCp1251(const char* s, uint8_t* out, uint16_t cap){
  uint16_t n = 0;
  if(!s || !cap) return 0;
  while(*s && n + 1 < cap){
    uint8_t c = (uint8_t)*s;
    uint32_t u; uint8_t len;
    if(c < 0x80){ u = c; len = 1; }
    else if((c & 0xE0) == 0xC0){ u = c & 0x1F; len = 2; }
    else if((c & 0xF0) == 0xE0){ u = c & 0x0F; len = 3; }
    else if((c & 0xF8) == 0xF0){ u = c & 0x07; len = 4; }
    else { s++; continue; }
    uint8_t k = 1;
    for(; k < len && s[k]; k++) u = (u << 6) | ((uint8_t)s[k] & 0x3F);
    s += k;
    if(k < len) break;
    out[n++] = cpOf(u);
  }
  out[n] = 0;
  return n;
}

/*  ---------- основа ---------- */
void Gfx::target(uint16_t* px, int16_t sx, int16_t sy, int16_t sw, int16_t sh){
  _px = px; _tx = sx; _ty = sy; _tw = sw; _th = sh;
  _ox = 0; _oy = 0;
  unclip();
}

void Gfx::clip(int16_t x, int16_t y, int16_t w, int16_t h){
  int16_t x0 = x, y0 = y, x1 = x + w, y1 = y + h;
  if(x0 < _tx) x0 = _tx; if(y0 < _ty) y0 = _ty;
  if(x1 > _tx + _tw) x1 = _tx + _tw; if(y1 > _ty + _th) y1 = _ty + _th;
  if(x1 < x0) x1 = x0; if(y1 < y0) y1 = y0;
  _cx0 = x0; _cy0 = y0; _cx1 = x1; _cy1 = y1;
}

/*  Ділення на 255 — множенням і зсувом: (x·257 + 257) >> 16 дає те саме
    ціле для всього діапазону. Змішування — найчастіша дія під час прокрутки,
    а ділення в процесорі в рази повільніше за множення.  */
static inline uint32_t div255(uint32_t x){ return (x * 257 + 257) >> 16; }

uint16_t Gfx::blend(uint16_t bg, uint16_t fg, uint8_t a){
  if(a >= 252) return fg;
  if(a <= 3) return bg;
  uint32_t ia = 255 - a;
  uint32_t r = div255(((fg >> 11) & 31) * a + ((bg >> 11) & 31) * ia + 127);
  uint32_t g = div255(((fg >> 5) & 63) * a + ((bg >> 5) & 63) * ia + 127);
  uint32_t b = div255((fg & 31) * a + (bg & 31) * ia + 127);
  return (uint16_t)((r << 11) | (g << 5) | b);
}

void Gfx::fill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c){
  int16_t x0 = x + _ox, y0 = y + _oy, x1 = x0 + w, y1 = y0 + h;
  if(x0 < _cx0) x0 = _cx0; if(y0 < _cy0) y0 = _cy0;
  if(x1 > _cx1) x1 = _cx1; if(y1 > _cy1) y1 = _cy1;
  if(x1 <= x0 || y1 <= y0) return;
  for(int16_t yy = y0; yy < y1; yy++){
    uint16_t* p = _px + (int32_t)(yy - _ty) * _tw + (x0 - _tx);
    for(int16_t i = x1 - x0; i > 0; i--) *p++ = c;
  }
}

void Gfx::blit(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* px){
  if(!px) return;
  const int16_t sx0 = x + _ox, sy0 = y + _oy;
  int16_t x0 = sx0, y0 = sy0, x1 = sx0 + w, y1 = sy0 + h;
  if(x0 < _cx0) x0 = _cx0; if(y0 < _cy0) y0 = _cy0;
  if(x1 > _cx1) x1 = _cx1; if(y1 > _cy1) y1 = _cy1;
  if(x1 <= x0 || y1 <= y0) return;
  const size_t n = (size_t)(x1 - x0) * 2;
  for(int16_t yy = y0; yy < y1; yy++)
    memcpy(_px + (int32_t)(yy - _ty) * _tw + (x0 - _tx), px + (int32_t)(yy - sy0) * w + (x0 - sx0), n);
}

void Gfx::fillA(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c, uint8_t a){
  int16_t x0 = x + _ox, y0 = y + _oy, x1 = x0 + w, y1 = y0 + h;
  if(x0 < _cx0) x0 = _cx0; if(y0 < _cy0) y0 = _cy0;
  if(x1 > _cx1) x1 = _cx1; if(y1 > _cy1) y1 = _cy1;
  if(x1 <= x0 || y1 <= y0) return;
  if(c == 0){
    /*  затемнення чорним (перехід між сторінками, тінь під карткою) — лише множення  */
    const uint32_t k = 256 - a;
    for(int16_t yy = y0; yy < y1; yy++){
      uint16_t* p = _px + (int32_t)(yy - _ty) * _tw + (x0 - _tx);
      for(int16_t i = x1 - x0; i > 0; i--, p++){
        const uint32_t v = *p;
        *p = (uint16_t)(((((v >> 11) & 31) * k >> 8) << 11) | ((((v >> 5) & 63) * k >> 8) << 5) | (((v & 31) * k) >> 8));
      }
    }
    return;
  }
  for(int16_t yy = y0; yy < y1; yy++){
    uint16_t* p = _px + (int32_t)(yy - _ty) * _tw + (x0 - _tx);
    for(int16_t i = x1 - x0; i > 0; i--, p++) *p = blend(*p, c, a);
  }
}

void Gfx::vgrad(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c0, uint16_t c1){
  if(h <= 0) return;
  int16_t x0 = x + _ox, y0 = y + _oy, x1 = x0 + w, y1 = y0 + h;
  int16_t top = y0;
  if(x0 < _cx0) x0 = _cx0; if(y0 < _cy0) y0 = _cy0;
  if(x1 > _cx1) x1 = _cx1; if(y1 > _cy1) y1 = _cy1;
  if(x1 <= x0 || y1 <= y0) return;
  /*  рахуємо в 8 бітах на канал і розсіюємо матрицею 4×4: у 16 бітах
      плавний перехід інакше розпадається на смуги  */
  static const uint8_t BAY[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
  int r0 = ((c0 >> 11) & 31) * 255 / 31, g0 = ((c0 >> 5) & 63) * 255 / 63, b0 = (c0 & 31) * 255 / 31;
  int r1 = ((c1 >> 11) & 31) * 255 / 31, g1 = ((c1 >> 5) & 63) * 255 / 63, b1 = (c1 & 31) * 255 / 31;
  for(int16_t yy = y0; yy < y1; yy++){
    int k = (int)(yy - top) * 256 / h;
    int r = r0 + (r1 - r0) * k / 256, g = g0 + (g1 - g0) * k / 256, b = b0 + (b1 - b0) * k / 256;
    uint16_t* p = _px + (int32_t)(yy - _ty) * _tw + (x0 - _tx);
    for(int16_t xx = x0; xx < x1; xx++){
      int d = BAY[((yy & 3) << 2) | (xx & 3)];
      int rr = div255(r * 31 + d * 16), gg = div255(g * 63 + d * 16), bb = div255(b * 31 + d * 16);
      if(rr > 31) rr = 31; if(gg > 63) gg = 63; if(bb > 31) bb = 31;
      *p++ = (uint16_t)((rr << 11) | (gg << 5) | bb);
    }
  }
}

/*  Кут заокруглення: покриття пікселів r×r, рахується раз на радіус.  */
static const uint8_t* cornerTable(uint8_t r){
  static uint8_t* tab[33] = { nullptr };
  if(r > 32) r = 32;
  if(!r) return nullptr;
  if(tab[r]) return tab[r];
  uint8_t* t = (uint8_t*)malloc((size_t)r * r);
  if(!t) return nullptr;
  for(uint8_t yy = 0; yy < r; yy++)
    for(uint8_t xx = 0; xx < r; xx++){
      /*  4×4 підпікселі — точно й без коренів  */
      uint8_t n = 0;
      for(uint8_t sy = 0; sy < 4; sy++) for(uint8_t sx = 0; sx < 4; sx++){
        float dx = r - (xx + (sx + 0.5f) / 4), dy = r - (yy + (sy + 0.5f) / 4);
        if(dx * dx + dy * dy <= (float)r * r) n++;
      }
      t[(size_t)yy * r + xx] = (uint8_t)(n * 255 / 16);
    }
  tab[r] = t;
  return t;
}

void Gfx::box(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint16_t c){
  if(w <= 0 || h <= 0) return;
  if(r > w / 2) r = w / 2;
  if(r > h / 2) r = h / 2;
  if(!visible(x, y, w, h)) return;
  if(!r){ fill(x, y, w, h, c); return; }
  fill(x, y + r, w, h - 2 * r, c);
  fill(x + r, y, w - 2 * r, r, c);
  fill(x + r, y + h - r, w - 2 * r, r, c);
  const uint8_t* t = cornerTable(r);
  if(!t) return;
  for(uint8_t yy = 0; yy < r; yy++){
    for(uint8_t xx = 0; xx < r; xx++){
      uint8_t a = t[(size_t)yy * r + xx];
      if(!a) continue;
      int16_t lx = x + _ox + xx, rx = x + _ox + w - 1 - xx;
      int16_t ty = y + _oy + yy, by = y + _oy + h - 1 - yy;
      _cover(lx, ty, a / 255.0f, c); _cover(rx, ty, a / 255.0f, c);
      _cover(lx, by, a / 255.0f, c); _cover(rx, by, a / 255.0f, c);
    }
  }
}

void Gfx::frame(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint16_t c, uint8_t t){
  if(!visible(x, y, w, h)) return;
  /*  рамка: зовнішній і внутрішній контур як дві дуги по кутах і лінії по сторонах  */
  float rr = r, tt = t;
  float hw = tt / 2;
  line(x + rr, y + hw, x + w - rr, y + hw, tt, c);
  line(x + rr, y + h - hw, x + w - rr, y + h - hw, tt, c);
  line(x + hw, y + rr, x + hw, y + h - rr, tt, c);
  line(x + w - hw, y + rr, x + w - hw, y + h - rr, tt, c);
  if(r){
    arc(x + rr, y + rr, rr - hw, tt, c, 270, 360);
    arc(x + w - rr, y + rr, rr - hw, tt, c, 0, 90);
    arc(x + w - rr, y + h - rr, rr - hw, tt, c, 90, 180);
    arc(x + rr, y + h - rr, rr - hw, tt, c, 180, 270);
  }
}

void Gfx::circle(float cx, float cy, float r, uint16_t c){
  float scx = cx + _ox, scy = cy + _oy;
  int16_t x0 = (int16_t)floorf(scx - r - 1), x1 = (int16_t)ceilf(scx + r + 1);
  int16_t y0 = (int16_t)floorf(scy - r - 1), y1 = (int16_t)ceilf(scy + r + 1);
  if(x0 < _cx0) x0 = _cx0; if(y0 < _cy0) y0 = _cy0;
  if(x1 >= _cx1) x1 = _cx1 - 1; if(y1 >= _cy1) y1 = _cy1 - 1;
  const float rin = r - 0.75f > 0 ? (r - 0.75f) * (r - 0.75f) : 0, rout = (r + 0.75f) * (r + 0.75f);
  for(int16_t yy = y0; yy <= y1; yy++){
    float dy = yy + 0.5f - scy;
    uint16_t* row = _px + (int32_t)(yy - _ty) * _tw;
    for(int16_t xx = x0; xx <= x1; xx++){
      float dx = xx + 0.5f - scx, d2 = dx * dx + dy * dy;
      if(d2 >= rout) continue;
      uint16_t* p = row + (xx - _tx);
      if(d2 <= rin){ *p = c; continue; }
      float cov = r - sqrtf(d2) + 0.5f;
      if(cov <= 0) continue;
      *p = cov >= 1 ? c : blend(*p, c, (uint8_t)(cov * 255));
    }
  }
}

void Gfx::line(float x0, float y0, float x1, float y1, float wd, uint16_t c){
  x0 += _ox; x1 += _ox; y0 += _oy; y1 += _oy;
  const float hw = wd / 2;
  int16_t bx0 = (int16_t)floorf((x0 < x1 ? x0 : x1) - hw - 1), bx1 = (int16_t)ceilf((x0 > x1 ? x0 : x1) + hw + 1);
  int16_t by0 = (int16_t)floorf((y0 < y1 ? y0 : y1) - hw - 1), by1 = (int16_t)ceilf((y0 > y1 ? y0 : y1) + hw + 1);
  if(bx0 < _cx0) bx0 = _cx0; if(by0 < _cy0) by0 = _cy0;
  if(bx1 >= _cx1) bx1 = _cx1 - 1; if(by1 >= _cy1) by1 = _cy1 - 1;
  const float vx = x1 - x0, vy = y1 - y0, l2 = vx * vx + vy * vy;
  for(int16_t py = by0; py <= by1; py++){
    for(int16_t px = bx0; px <= bx1; px++){
      const float qx = px + 0.5f - x0, qy = py + 0.5f - y0;
      float k = l2 > 0 ? (qx * vx + qy * vy) / l2 : 0;
      if(k < 0) k = 0; else if(k > 1) k = 1;
      const float dx = qx - k * vx, dy = qy - k * vy;
      const float d2 = dx * dx + dy * dy;
      if(d2 > (hw + 1) * (hw + 1)) continue;
      _cover(px, py, hw - sqrtf(d2) + 0.5f, c);
    }
  }
}

void Gfx::arc(float cx, float cy, float r, float wd, uint16_t c, float a0, float a1){
  cx += _ox; cy += _oy;
  if(a1 < a0) a1 += 360;                             /* дуга через «вгору» */
  const float hw = wd / 2, span = a1 - a0, d2r = 0.0174533f;
  const bool full = span >= 359.9f;
  const float e0x = cx + r * sinf(a0 * d2r), e0y = cy - r * cosf(a0 * d2r);
  const float e1x = cx + r * sinf(a1 * d2r), e1y = cy - r * cosf(a1 * d2r);
  int16_t bx0 = (int16_t)floorf(cx - r - hw - 1), bx1 = (int16_t)ceilf(cx + r + hw + 1);
  int16_t by0 = (int16_t)floorf(cy - r - hw - 1), by1 = (int16_t)ceilf(cy + r + hw + 1);
  if(bx0 < _cx0) bx0 = _cx0; if(by0 < _cy0) by0 = _cy0;
  if(bx1 >= _cx1) bx1 = _cx1 - 1; if(by1 >= _cy1) by1 = _cy1 - 1;
  for(int16_t py = by0; py <= by1; py++){
    for(int16_t px = bx0; px <= bx1; px++){
      const float dx = px + 0.5f - cx, dy = py + 0.5f - cy;
      const float dist = sqrtf(dx * dx + dy * dy);
      if(fabsf(dist - r) > hw + 1) continue;
      bool in = full;
      if(!full){
        float ang = atan2f(dx, -dy) / d2r;
        in = fmodf(ang - a0 + 720.0f, 360.0f) <= span;
      }
      float cov;
      if(in) cov = hw - fabsf(dist - r) + 0.5f;
      else{
        float ax = px + 0.5f - e0x, ay = py + 0.5f - e0y, bx = px + 0.5f - e1x, by = py + 0.5f - e1y;
        float d0 = sqrtf(ax * ax + ay * ay), d1 = sqrtf(bx * bx + by * by);
        cov = hw - (d0 < d1 ? d0 : d1) + 0.5f;
      }
      _cover(px, py, cov, c);
    }
  }
}

void Gfx::poly(const float* xy, uint8_t n, uint16_t c){
  if(n < 3 || n > 24) return;
  float p[48];
  float mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f;
  for(uint8_t i = 0; i < n; i++){
    p[2 * i] = xy[2 * i] + _ox; p[2 * i + 1] = xy[2 * i + 1] + _oy;
    if(p[2 * i] < mnx) mnx = p[2 * i]; if(p[2 * i] > mxx) mxx = p[2 * i];
    if(p[2 * i + 1] < mny) mny = p[2 * i + 1]; if(p[2 * i + 1] > mxy) mxy = p[2 * i + 1];
  }
  int16_t bx0 = (int16_t)floorf(mnx) - 1, bx1 = (int16_t)ceilf(mxx) + 1, by0 = (int16_t)floorf(mny) - 1, by1 = (int16_t)ceilf(mxy) + 1;
  if(bx0 < _cx0) bx0 = _cx0; if(by0 < _cy0) by0 = _cy0;
  if(bx1 >= _cx1) bx1 = _cx1 - 1; if(by1 >= _cy1) by1 = _cy1 - 1;
  for(int16_t py = by0; py <= by1; py++){
    const float qy = py + 0.5f;
    for(int16_t px = bx0; px <= bx1; px++){
      const float qx = px + 0.5f;
      bool inside = false; float dmin = 1e9f;
      for(uint8_t i = 0, j = n - 1; i < n; j = i++){
        const float ax = p[2 * i], ay = p[2 * i + 1], bx = p[2 * j], by = p[2 * j + 1];
        if(((ay > qy) != (by > qy)) && (qx < (bx - ax) * (qy - ay) / (by - ay) + ax)) inside = !inside;
        const float vx = bx - ax, vy = by - ay, l2 = vx * vx + vy * vy;
        float k = l2 > 0 ? ((qx - ax) * vx + (qy - ay) * vy) / l2 : 0;
        if(k < 0) k = 0; else if(k > 1) k = 1;
        const float dx = qx - ax - k * vx, dy = qy - ay - k * vy, d2 = dx * dx + dy * dy;
        if(d2 < dmin) dmin = d2;
      }
      const float d = sqrtf(dmin);
      _cover(px, py, inside ? 0.5f + d : 0.5f - d, c);
    }
  }
}

void Gfx::lighten(const Rect& area, uint8_t r, float cx, float cy, float rad, uint8_t a, uint16_t c){
  if(!a || rad <= 0) return;
  int16_t x0 = area.x + _ox, y0 = area.y + _oy, x1 = x0 + area.w, y1 = y0 + area.h;
  float scx = cx + _ox, scy = cy + _oy;
  int16_t bx0 = x0 > _cx0 ? x0 : _cx0, by0 = y0 > _cy0 ? y0 : _cy0;
  int16_t bx1 = x1 < _cx1 ? x1 : _cx1, by1 = y1 < _cy1 ? y1 : _cy1;
  const uint8_t* t = cornerTable(r);
  for(int16_t yy = by0; yy < by1; yy++){
    const float dy = yy + 0.5f - scy;
    uint16_t* row = _px + (int32_t)(yy - _ty) * _tw;
    for(int16_t xx = bx0; xx < bx1; xx++){
      const float dx = xx + 0.5f - scx, d2 = dx * dx + dy * dy;
      if(d2 >= (rad + 1) * (rad + 1)) continue;
      float k = d2 <= (rad - 1) * (rad - 1) ? 1.0f : (rad + 1 - sqrtf(d2)) / 2;
      /*  кути прямокутника — заокруглені  */
      if(t){
        int16_t lx = xx - x0, ly = yy - y0, rx = x1 - 1 - xx, ry = y1 - 1 - yy;
        int16_t cxk = lx < r ? lx : (rx < r ? rx : -1), cyk = ly < r ? ly : (ry < r ? ry : -1);
        if(cxk >= 0 && cyk >= 0) k *= t[(size_t)cyk * r + cxk] / 255.0f;
      }
      uint16_t* p = row + (xx - _tx);
      *p = blend(*p, c, (uint8_t)(a * k));
    }
  }
}

void Gfx::image(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* px, uint8_t r){
  if(!px || !visible(x, y, w, h)) return;
  const uint8_t* t = r ? cornerTable(r) : nullptr;
  int16_t sx0 = x + _ox, sy0 = y + _oy;
  for(int16_t yy = 0; yy < h; yy++){
    int16_t sy = sy0 + yy;
    if(sy < _cy0 || sy >= _cy1) continue;
    uint16_t* row = _px + (int32_t)(sy - _ty) * _tw;
    for(int16_t xx = 0; xx < w; xx++){
      int16_t sx = sx0 + xx;
      if(sx < _cx0 || sx >= _cx1) continue;
      uint16_t v = px[(int32_t)yy * w + xx];
      if(t){
        int16_t cx = xx < r ? xx : (w - 1 - xx < r ? w - 1 - xx : -1);
        int16_t cy = yy < r ? yy : (h - 1 - yy < r ? h - 1 - yy : -1);
        if(cx >= 0 && cy >= 0){
          uint8_t a = t[(size_t)cy * r + cx];
          if(!a) continue;
          row[sx - _tx] = blend(row[sx - _tx], v, a);
          continue;
        }
      }
      row[sx - _tx] = v;
    }
  }
}

/*  ---------- текст ---------- */
void Gfx::_glyphs(int16_t x, int16_t baseline, const uint8_t* cp, uint16_t n, const GFXfont* f, uint16_t c){
  int16_t sx = x + _ox, sb = baseline + _oy;
  /*  рядок цілком поза смугою — не рахуємо навіть гліфів  */
  if(sb - (int16_t)f->yAdvance > _cy1 || sb + (int16_t)f->yAdvance / 2 < _cy0) return;
  for(uint16_t i = 0; i < n; i++){
    uint8_t ch = cp[i];
    if(ch < f->first || ch > f->last){ continue; }
    const GFXglyph* g = f->glyph + (ch - f->first);
    const int16_t w = g->width, h = g->height;
    const int16_t gx = sx + g->xOffset, gy = sb + g->yOffset;
    if(w && h && gx < _cx1 && gx + w > _cx0 && gy < _cy1 && gy + h > _cy0){
      const uint8_t* bm = f->bitmap + g->bitmapOffset;
      const uint16_t fr = (c >> 11) & 31, fg = (c >> 5) & 63, fb = c & 31;
      for(int16_t yy = 0; yy < h; yy++){
        const int16_t py = gy + yy;
        if(py < _cy0 || py >= _cy1) continue;
        uint16_t* line = _px + (int32_t)(py - _ty) * _tw;
        uint32_t idx = (uint32_t)yy * w;
        for(int16_t xx = 0; xx < w; xx++, idx++){
          const uint8_t a = (idx & 1) ? (bm[idx >> 1] & 0x0F) : (bm[idx >> 1] >> 4);
          if(!a) continue;
          const int16_t px = gx + xx;
          if(px < _cx0 || px >= _cx1) continue;
          uint16_t* p = line + (px - _tx);
          if(a == 15){ *p = c; continue; }
          const uint32_t b = *p, ia = 15 - a;
          /*  /15 — множенням: (x·4370) >> 16 збігається для x до 945  */
          *p = (uint16_t)((((fr * a + ((b >> 11) & 31) * ia) * 4370 >> 16) << 11) |
                          (((fg * a + ((b >> 5) & 63) * ia) * 4370 >> 16) << 5) |
                           ((fb * a + (b & 31) * ia) * 4370 >> 16));
        }
      }
    }
    sx += g->xAdvance;
    if(sx >= _cx1) break;
  }
}

static int16_t cpWidth(const uint8_t* cp, uint16_t n, const GFXfont* f){
  int16_t w = 0;
  for(uint16_t i = 0; i < n; i++){
    uint8_t ch = cp[i];
    if(ch < f->first || ch > f->last) continue;
    w += f->glyph[ch - f->first].xAdvance;
  }
  return w;
}

int16_t Gfx::textW(const char* s, const GFXfont* f){
  uint8_t cp[160];
  uint16_t n = toCp1251(s, cp, sizeof(cp));
  return cpWidth(cp, n, f);
}

int16_t Gfx::text(int16_t x, int16_t baseline, const char* s, const GFXfont* f, uint16_t c, uint8_t align, int16_t maxw){
  if(!s || !f) return 0;
  uint8_t cp[160];
  uint16_t n = toCp1251(s, cp, sizeof(cp) - 2);
  int16_t w = cpWidth(cp, n, f);
  /*  рядок цілком поза смугою (сторінка малюється смугами, і більшість викликів
      сюди не влучає) — ширину вже знаємо, решту не рахуємо  */
  const int16_t sb = baseline + _oy;
  if(sb - (int16_t)f->yAdvance > _cy1 || sb + (int16_t)f->yAdvance / 2 < _cy0) return maxw > 0 && w > maxw ? maxw : w;
  if(maxw > 0 && w > maxw){
    /*  обрізаємо по місцю й ставимо «…» (ширину віднімаємо по літері, а не рахуємо щоразу наново)  */
    const int16_t ell = cpWidth((const uint8_t*)"\x85", 1, f);
    auto adv = [&](uint8_t ch) -> int16_t { return (ch < f->first || ch > f->last) ? 0 : f->glyph[ch - f->first].xAdvance; };
    while(n > 0 && w + ell > maxw){ n--; w -= adv(cp[n]); }
    while(n > 0 && cp[n - 1] == ' '){ n--; w -= adv(cp[n]); }
    cp[n++] = 0x85; cp[n] = 0;
    w += ell;
  }
  int16_t xx = align == AL_C ? x - w / 2 : align == AL_R ? x - w : x;
  _glyphs(xx, baseline, cp, n, f, c);
  return w;
}

}  // namespace m2
