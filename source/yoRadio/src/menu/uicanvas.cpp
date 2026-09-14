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

extern DspCore dsp;
UiCanvas ui;

static const int16_t CW_ = 320, CH_ = 240;

UiCanvas::UiCanvas() : GFXcanvas16(CW_, CH_, false) {}

bool UiCanvas::begin(){
  if(buffer) return true;
  buffer = (uint16_t*)heap_caps_calloc((size_t)CW_ * CH_, sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if(!buffer) return false;
  _dma = (uint8_t*)heap_caps_malloc((size_t)CW_ * TS * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if(!_dma){ heap_caps_free(buffer); buffer = nullptr; return false; }
  return true;
}

void UiCanvas::setActive(bool on){ _active = on && buffer; }

bool UiCanvas::isAA(const GFXfont* f){
  return f == &aaUI8 || f == &aaUI9 || f == &aaUI9b || f == &aaUI11 || f == &aaUI12b ||
         f == &aaUI13 || f == &aaUI14b || f == &aaUI15;
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

void UiCanvas::fillRoundRectAA(int16_t x, int16_t y, int16_t w, int16_t h, float r, uint16_t color){
  if(!buffer || w <= 0 || h <= 0) return;
  if(r > w / 2.0f) r = w / 2.0f;
  if(r > h / 2.0f) r = h / 2.0f;
  int16_t ri = (int16_t)ceilf(r);
  /*  середина — звичайними прямокутниками  */
  fillRect(x + ri, y, w - 2 * ri, h, color);
  fillRect(x, y + ri, ri, h - 2 * ri, color);
  fillRect(x + w - ri, y + ri, ri, h - 2 * ri, color);
  /*  кути — із покриттям  */
  const float cxl = x + r, cxr = x + w - r, cyt = y + r, cyb = y + h - r;
  for(int16_t yy = 0; yy < ri; yy++){
    for(int16_t xx = 0; xx < ri; xx++){
      for(uint8_t k = 0; k < 4; k++){
        int16_t px = (k & 1) ? (x + w - ri + xx) : (x + xx);
        int16_t py = (k & 2) ? (y + h - ri + yy) : (y + yy);
        if(px < 0 || py < 0 || px >= CW_ || py >= CH_) continue;
        float ccx = (k & 1) ? cxr : cxl, ccy = (k & 2) ? cyb : cyt;
        float dx = px + 0.5f - ccx, dy = py + 0.5f - ccy;
        bool inCornerX = (k & 1) ? (px + 0.5f > ccx) : (px + 0.5f < ccx);
        bool inCornerY = (k & 2) ? (py + 0.5f > ccy) : (py + 0.5f < ccy);
        float cov = 1.0f;
        if(inCornerX && inCornerY) cov = r - sqrtf(dx * dx + dy * dy) + 0.5f;
        if(cov <= 0) continue;
        uint16_t* p = buffer + (int32_t)py * CW_ + px;
        *p = cov >= 1 ? color : blend(*p, color, (uint8_t)(cov * 255));
      }
    }
  }
  _markRaw(x, y, w, h);
}

/*  ---------- на екран ---------- */
void UiCanvas::flush(){
  if(!buffer || !_active) return;
  uint32_t rows[TH];
  portENTER_CRITICAL(&_mux);
  memcpy(rows, _dirty, sizeof(rows));
  memset(_dirty, 0, sizeof(_dirty));
  portEXIT_CRITICAL(&_mux);
  bool any = false;
  for(int r = 0; r < TH; r++) if(rows[r]) { any = true; break; }
  if(!any) return;
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
