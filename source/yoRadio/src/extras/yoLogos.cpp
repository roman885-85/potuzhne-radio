#include "yoLogos.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include "miniz.h"
#include "yoSermons.h"          /* yoJpegFit */

YoLogos logos;

#define LG_MAXDL   (256 * 1024)          /* найбільший значок, який качаємо */
#define LG_MAXPX   (1024 * 1024)         /* найбільше зображення в пікселях */
#define LG_API     "http://de1.api.radio-browser.info/json/stations/"

uint32_t YoLogos::crc(const char* s){
  uint32_t c = 0xFFFFFFFF;
  for(; *s; s++){ c ^= (uint8_t)*s; for(uint8_t k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320 & (0 - (c & 1))); }
  return ~c;
}

/*  ---------- PNG ---------- */

static uint32_t be32(const uint8_t* p){ return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

static uint8_t paeth(int a, int b, int c){
  int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
  return (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
}

/*  Розпакувати PNG у RGBA8888. Черезрядковий (interlace) не підтримуємо —
    серед значків станцій він трапляється рідко.  */
static bool pngDecode(const uint8_t* d, size_t n, uint8_t** outRGBA, uint32_t* W, uint32_t* H){
  static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
  if(n < 33 || memcmp(d, sig, 8)) return false;
  uint32_t w = 0, h = 0; uint8_t bd = 0, ct = 0, il = 0;
  uint8_t pal[256][4]; uint16_t npal = 0;
  int32_t trGray = -1; int32_t trR = -1, trG = -1, trB = -1;
  for(int i = 0; i < 256; i++){ pal[i][0] = pal[i][1] = pal[i][2] = 0; pal[i][3] = 255; }
  /*  спершу рахуємо загальний розмір IDAT  */
  size_t idatLen = 0;
  for(size_t p = 8; p + 12 <= n; ){
    uint32_t len = be32(d + p); const uint8_t* t = d + p + 4; const uint8_t* c = d + p + 8;
    if(p + 12 + len > n) break;
    if(!memcmp(t, "IHDR", 4) && len >= 13){ w = be32(c); h = be32(c + 4); bd = c[8]; ct = c[9]; il = c[12]; }
    else if(!memcmp(t, "PLTE", 4)){ npal = len / 3; if(npal > 256) npal = 256; for(uint16_t k = 0; k < npal; k++){ pal[k][0] = c[k*3]; pal[k][1] = c[k*3+1]; pal[k][2] = c[k*3+2]; } }
    else if(!memcmp(t, "tRNS", 4)){
      if(ct == 3){ for(uint32_t k = 0; k < len && k < 256; k++) pal[k][3] = c[k]; }
      else if(ct == 0 && len >= 2) trGray = (c[0] << 8) | c[1];
      else if(ct == 2 && len >= 6){ trR = (c[0] << 8) | c[1]; trG = (c[2] << 8) | c[3]; trB = (c[4] << 8) | c[5]; }
    }
    else if(!memcmp(t, "IDAT", 4)) idatLen += len;
    else if(!memcmp(t, "IEND", 4)) break;
    p += 12 + len;
  }
  if(!w || !h || il || (uint64_t)w * h > LG_MAXPX) return false;
  uint8_t ch = ct == 0 ? 1 : ct == 2 ? 3 : ct == 3 ? 1 : ct == 4 ? 2 : ct == 6 ? 4 : 0;
  if(!ch || !(bd == 1 || bd == 2 || bd == 4 || bd == 8 || bd == 16)) return false;
  uint32_t bitsPx = ch * bd, stride = (w * bitsPx + 7) / 8, bpp = bitsPx >= 8 ? bitsPx / 8 : 1;
  size_t rawLen = (size_t)h * (stride + 1);
  uint8_t* idat = (uint8_t*)ps_malloc(idatLen + 1);
  uint8_t* raw  = (uint8_t*)ps_malloc(rawLen);
  uint8_t* rgba = (uint8_t*)ps_malloc((size_t)w * h * 4);
  bool ok = false;
  if(idat && raw && rgba){
    size_t q = 0;
    for(size_t p = 8; p + 12 <= n; ){
      uint32_t len = be32(d + p);
      if(p + 12 + len > n) break;
      if(!memcmp(d + p + 4, "IDAT", 4)){ memcpy(idat + q, d + p + 8, len); q += len; }
      p += 12 + len;
    }
    size_t got = tinfl_decompress_mem_to_mem(raw, rawLen, idat, idatLen, TINFL_FLAG_PARSE_ZLIB_HEADER);
    if(got == rawLen){
      /*  зняти фільтри рядків (None, Sub, Up, Average, Paeth)  */
      uint8_t* prev = nullptr;
      for(uint32_t y = 0; y < h; y++){
        uint8_t* row = raw + (size_t)y * (stride + 1);
        uint8_t f = row[0]; uint8_t* s = row + 1;
        for(uint32_t x = 0; x < stride; x++){
          uint8_t a = x >= bpp ? s[x - bpp] : 0, b = prev ? prev[x] : 0, c = (prev && x >= bpp) ? prev[x - bpp] : 0;
          switch(f){ case 1: s[x] += a; break; case 2: s[x] += b; break; case 3: s[x] += (a + b) >> 1; break; case 4: s[x] += paeth(a, b, c); break; default: break; }
        }
        prev = s;
        /*  у RGBA  */
        for(uint32_t x = 0; x < w; x++){
          uint8_t* o = rgba + ((size_t)y * w + x) * 4;
          auto sample = [&](uint32_t idx)->uint16_t{         /* idx — номер каналу в рядку */
            if(bd == 16) return s[idx * 2];                   /* старший байт */
            if(bd == 8)  return s[idx];
            uint32_t bit = idx * bd; uint8_t v = (s[bit >> 3] >> (8 - bd - (bit & 7))) & ((1 << bd) - 1);
            return v;
          };
          uint16_t v0 = sample(x * ch);
          if(ct == 3){ uint8_t* pp = pal[v0 & 255]; o[0] = pp[0]; o[1] = pp[1]; o[2] = pp[2]; o[3] = pp[3]; continue; }
          uint8_t scale = bd < 8 ? (255 / ((1 << bd) - 1)) : 1;
          if(ct == 0 || ct == 4){
            uint8_t g = (uint8_t)(v0 * scale); o[0] = o[1] = o[2] = g;
            o[3] = ct == 4 ? (uint8_t)sample(x * ch + 1) : ((trGray >= 0 && (int32_t)v0 == (bd == 16 ? (trGray >> 8) : trGray)) ? 0 : 255);
          }else{
            o[0] = (uint8_t)v0; o[1] = (uint8_t)sample(x * ch + 1); o[2] = (uint8_t)sample(x * ch + 2);
            o[3] = ct == 6 ? (uint8_t)sample(x * ch + 3) : ((trR >= 0 && o[0] == (bd == 16 ? trR >> 8 : trR) && o[1] == (bd == 16 ? trG >> 8 : trG) && o[2] == (bd == 16 ? trB >> 8 : trB)) ? 0 : 255);
          }
        }
      }
      ok = true;
    }
  }
  if(idat) free(idat);
  if(raw) free(raw);
  if(!ok){ if(rgba) free(rgba); return false; }
  *outRGBA = rgba; *W = w; *H = h;
  return true;
}

/*  ---------- ICO: беремо найбільший значок, усередині PNG або BMP ---------- */

static bool icoDecode(const uint8_t* d, size_t n, uint8_t** outRGBA, uint32_t* W, uint32_t* H){
  if(n < 22 || d[0] || d[1] || d[2] != 1 || d[3]) return false;
  uint16_t cnt = d[4] | (d[5] << 8);
  int best = -1; uint32_t bestW = 0;
  for(uint16_t i = 0; i < cnt && 6 + i * 16 + 16 <= n; i++){
    const uint8_t* e = d + 6 + i * 16;
    uint32_t w = e[0] ? e[0] : 256;
    if(w > bestW){ bestW = w; best = i; }
  }
  if(best < 0) return false;
  const uint8_t* e = d + 6 + best * 16;
  uint32_t size = e[8] | (e[9] << 8) | (e[10] << 16) | ((uint32_t)e[11] << 24);
  uint32_t off  = e[12] | (e[13] << 8) | (e[14] << 16) | ((uint32_t)e[15] << 24);
  if(off + size > n) return false;
  const uint8_t* b = d + off;
  if(size > 8 && b[0] == 0x89 && b[1] == 'P') return pngDecode(b, size, outRGBA, W, H);
  /*  BMP без файлового заголовка: висота подвоєна (там ще маска)  */
  if(size < 40) return false;
  int32_t w = (int32_t)(b[4] | (b[5] << 8) | (b[6] << 16) | ((uint32_t)b[7] << 24));
  int32_t h = (int32_t)(b[8] | (b[9] << 8) | (b[10] << 16) | ((uint32_t)b[11] << 24)) / 2;
  uint16_t bpp = b[14] | (b[15] << 8);
  uint32_t hs = b[0] | (b[1] << 8);
  if(w <= 0 || h <= 0 || w > 512 || h > 512 || (bpp != 32 && bpp != 24)) return false;
  uint32_t rowB = ((w * bpp / 8) + 3) & ~3u;
  if(hs + rowB * h > size) return false;
  uint8_t* rgba = (uint8_t*)ps_malloc((size_t)w * h * 4);
  if(!rgba) return false;
  for(int32_t y = 0; y < h; y++){
    const uint8_t* s = b + hs + (size_t)(h - 1 - y) * rowB;
    for(int32_t x = 0; x < w; x++){
      uint8_t* o = rgba + ((size_t)y * w + x) * 4;
      o[0] = s[x * bpp / 8 + 2]; o[1] = s[x * bpp / 8 + 1]; o[2] = s[x * bpp / 8];
      o[3] = bpp == 32 ? s[x * 4 + 3] : 255;
    }
  }
  *outRGBA = rgba; *W = w; *H = h;
  return true;
}

/*  RGBA -> W x H RGB565, прозоре на білому, з усередненням пікселів.  */
static void fitRGBA(const uint8_t* rgba, uint32_t sw, uint32_t sh, uint16_t* out, int W, int H){
  float f = fminf((float)W / sw, (float)H / sh);
  int dw = (int)(sw * f), dh = (int)(sh * f);
  if(dw < 1) dw = 1; if(dh < 1) dh = 1;
  int ox = (W - dw) / 2, oy = (H - dh) / 2;
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++){
      uint32_t r = 255, g = 255, b = 255;
      int sx = x - ox, sy = y - oy;
      if(sx >= 0 && sy >= 0 && sx < dw && sy < dh){
        int x0 = sx * sw / dw, x1 = (sx + 1) * sw / dw; if(x1 <= x0) x1 = x0 + 1;
        int y0 = sy * sh / dh, y1 = (sy + 1) * sh / dh; if(y1 <= y0) y1 = y0 + 1;
        uint32_t R = 0, G = 0, B = 0, k = 0;
        for(int yy = y0; yy < y1; yy++)
          for(int xx = x0; xx < x1; xx++){
            const uint8_t* p = rgba + ((size_t)yy * sw + xx) * 4;
            uint32_t a = p[3];
            R += (p[0] * a + 255 * (255 - a)) / 255; G += (p[1] * a + 255 * (255 - a)) / 255; B += (p[2] * a + 255 * (255 - a)) / 255; k++;
          }
        r = R / k; g = G / k; b = B / k;
      }
      out[y * W + x] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }
}

bool yoImageFit(const uint8_t* d, size_t n, uint16_t* out, int W, int H){
  if(n > 3 && d[0] == 0xFF && d[1] == 0xD8) return yoJpegFit(d, n, out, W, H);
  uint8_t* rgba = nullptr; uint32_t w = 0, h = 0;
  bool ok = (n > 8 && d[0] == 0x89 && d[1] == 'P') ? pngDecode(d, n, &rgba, &w, &h) : icoDecode(d, n, &rgba, &w, &h);
  if(!ok) return false;
  fitRGBA(rgba, w, h, out, W, H);
  free(rgba);
  return true;
}

/*  ---------- мережа ---------- */

static void urlEnc(const char* s, char* o, size_t cap){
  static const char* hx = "0123456789ABCDEF";
  size_t k = 0;
  for(; *s && k + 4 < cap; s++){
    uint8_t c = (uint8_t)*s;
    if(isalnum(c) && c < 0x80) o[k++] = c;
    else if(c == '-' || c == '_' || c == '.' || c == '~') o[k++] = c;
    else { o[k++] = '%'; o[k++] = hx[c >> 4]; o[k++] = hx[c & 15]; }
  }
  o[k] = 0;
}

/*  GET у буфер PSRAM. Повертає довжину, -1 — мережа/сервер не відповіли.  */
static int httpGet(const char* url, uint8_t* buf, size_t cap){
  bool tls = !strncmp(url, "https", 5);
  WiFiClient plain; WiFiClientSecure sec; sec.setInsecure();
  HTTPClient http;
  http.setTimeout(6000); http.setConnectTimeout(6000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("PotuzhneRadio/1.0");
  if(!(tls ? http.begin(sec, url) : http.begin(plain, url))) return -1;
  int code = http.GET();
  if(code != 200){ http.end(); return code > 0 ? 0 : -1; }
  WiFiClient* st = http.getStreamPtr();
  int total = http.getSize();
  size_t len = 0;
  uint32_t deadline = millis() + 10000;
  while(len < cap && (int32_t)(deadline - millis()) > 0 && (total < 0 || (int)len < total)){
    int av = st->available();
    if(!av){ if(!st->connected()) break; vTaskDelay(pdMS_TO_TICKS(5)); continue; }
    len += st->read(buf + len, (size_t)av > cap - len ? cap - len : av);
  }
  http.end();
  return (int)len;
}

/*  Перше непорожнє "favicon" у відповіді каталогу; якщо byName — лише у
    станції з точно такою ж назвою (щоб не взяти чужий логотип).  */
static bool findFavicon(char* json, const char* byName, char* out, size_t cap){
  char* p = json;
  while((p = strstr(p, "\"favicon\":\"")) != nullptr){
    p += 11;
    char* e = strchr(p, '"');
    if(!e) break;
    if(e > p){
      if(byName){
        /*  назва — в тому ж об'єкті, перед favicon  */
        char* objStart = p; while(objStart > json && *objStart != '{') objStart--;
        char* nm = strstr(objStart, "\"name\":\"");
        if(!nm || nm > p){ p = e; continue; }
        nm += 8; char* ne = strchr(nm, '"');
        if(!ne || (size_t)(ne - nm) != strlen(byName) || strncmp(nm, byName, ne - nm)){ p = e; continue; }
      }
      size_t k = 0;
      for(char* q = p; q < e && k + 1 < cap; q++){ if(*q == '\\' && q[1] == '/'){ out[k++] = '/'; q++; } else out[k++] = *q; }
      out[k] = 0;
      if(!strncmp(out, "http", 4)) return true;
    }
    p = e;
  }
  return false;
}

void YoLogos::want(const char* url, const char* name){
  if(_busy || WiFi.status() != WL_CONNECTED) return;
  strlcpy(_url, url, sizeof(_url));
  strlcpy(_name, name, sizeof(_name));
  _crc = crc(url);
  _busy = true;
  if(xTaskCreatePinnedToCore(_task, "logo", 24576, this, 1, nullptr, 0) != pdPASS) _busy = false;
}

uint16_t YoLogos::forget(){
  uint16_t n = 0;
  File dir = SPIFFS.open("/");
  if(!dir) return 0;
  char names[64][32]; uint8_t k = 0;
  for(File f = dir.openNextFile(); f && k < 64; f = dir.openNextFile()){
    const char* nm = f.path();
    if(strstr(nm, "/logo/") && strstr(nm, ".no")) strlcpy(names[k++], nm, 32);
    f.close();
  }
  dir.close();
  for(uint8_t i = 0; i < k; i++){ SPIFFS.remove(names[i]); n++; }
  return n;
}

void YoLogos::_task(void* arg){
  YoLogos* s = (YoLogos*)arg;
  vTaskDelay(pdMS_TO_TICKS(3000));            /* нехай станція спершу підключиться */
  uint8_t* buf = (uint8_t*)ps_malloc(LG_MAXDL + 1);
  char fav[256] = {0};
  int result = -1;                            /* -1 мережа, 0 нема логотипа, 1 є */
  if(buf){
    char q[420], enc[380];
    urlEnc(s->_url, enc, sizeof(enc));
    snprintf(q, sizeof(q), LG_API "byurl?url=%s", enc);
    int n = httpGet(q, buf, 64 * 1024);
    if(n >= 0){
      buf[n] = 0;
      bool f = findFavicon((char*)buf, nullptr, fav, sizeof(fav));
      if(!f){
        urlEnc(s->_name, enc, sizeof(enc));
        snprintf(q, sizeof(q), LG_API "byname/%s?limit=10", enc);
        n = httpGet(q, buf, 64 * 1024);
        if(n >= 0){ buf[n] = 0; f = findFavicon((char*)buf, s->_name, fav, sizeof(fav)); }
      }
      if(n < 0) result = -1;
      else if(!f) result = 0;
      else{
        n = httpGet(fav, buf, LG_MAXDL);
        if(n < 0) result = -1;
        else{
          uint16_t* px = (uint16_t*)ps_malloc(LOGO_S * LOGO_S * 2);
          result = 0;
          if(px && n > 0 && yoImageFit(buf, n, px, LOGO_S, LOGO_S)){
            char path[28]; snprintf(path, sizeof(path), "/logo/%08x.565", (unsigned)s->_crc);
            File o = SPIFFS.open(path, "w");
            if(o){ o.write((uint8_t*)px, LOGO_S * LOGO_S * 2); o.close(); result = 1; }
          }
          if(px) free(px);
        }
      }
    }
    free(buf);
  }
  if(result == 0){                             /* нема — не питати щоразу */
    char path[28]; snprintf(path, sizeof(path), "/logo/%08x.no", (unsigned)s->_crc);
    File o = SPIFFS.open(path, "w"); if(o){ o.print("1"); o.close(); }
  }
  Serial.printf("##LOGO#\t%s: %s %s\n", s->_name, result > 0 ? "знайдено" : result == 0 ? "нема" : "мережа", fav);
  if(result > 0) s->_ver++;
  s->_busy = false;
  vTaskDelete(nullptr);
}
