#include "m2icons.h"

namespace m2 {

static void star(Gfx& g, float cx, float cy, float R, uint16_t c){
  float xy[20];
  for(uint8_t i = 0; i < 10; i++){
    float a = -1.5708f + i * 0.6283f, r = (i & 1) ? R * 0.45f : R;
    xy[2 * i] = cx + r * cosf(a); xy[2 * i + 1] = cy + r * sinf(a);
  }
  g.poly(xy, 10, c);
}

void signalBars(Gfx& g, float x, float bottom, uint8_t level, uint16_t on, uint16_t off){
  for(uint8_t k = 0; k < 4; k++){
    float h = 3 + k * 2.3f;
    float bx = x + k * 4.2f;
    g.line(bx + 1.2f, bottom - 1.2f, bx + 1.2f, bottom - h + 1.2f, 2.4f, k < level ? on : off);
  }
}

void icon(Gfx& g, uint8_t id, float cx, float cy, uint16_t c, uint16_t bg){
  switch(id){
    case IC_BACK:
      g.line(cx + 3, cy - 6, cx - 3, cy, 2.3f, c); g.line(cx - 3, cy, cx + 3, cy + 6, 2.3f, c); break;
    case IC_CHEV:
      g.line(cx - 2, cy - 4.5f, cx + 2.5f, cy, 1.9f, c); g.line(cx + 2.5f, cy, cx - 2, cy + 4.5f, 1.9f, c); break;
    case IC_DOWN:
      g.line(cx - 5, cy - 2.5f, cx, cy + 2.5f, 2.2f, c); g.line(cx, cy + 2.5f, cx + 5, cy - 2.5f, 2.2f, c); break;
    case IC_MOON:
      g.circle(cx, cy, 8, c); g.circle(cx + 4.6f, cy - 3.6f, 7, bg); break;
    case IC_ALARM:
      g.arc(cx, cy + 1, 7.3f, 2, c);
      g.line(cx, cy + 1, cx, cy - 3, 1.9f, c); g.line(cx, cy + 1, cx + 3, cy + 2.6f, 1.9f, c);
      g.line(cx - 8.2f, cy - 5.5f, cx - 5.2f, cy - 8.3f, 2.1f, c); g.line(cx + 8.2f, cy - 5.5f, cx + 5.2f, cy - 8.3f, 2.1f, c);
      break;
    case IC_BELL: {
      g.circle(cx, cy - 2.5f, 6.2f, c);
      const float xy[8] = { cx - 6.2f, cy - 2.5f, cx + 6.2f, cy - 2.5f, cx + 8.5f, cy + 5, cx - 8.5f, cy + 5 };
      g.poly(xy, 4, c);
      g.line(cx - 9, cy + 5.4f, cx + 9, cy + 5.4f, 2, c); g.circle(cx, cy + 8.3f, 2.2f, c);
      break; }
    case IC_REC:
      g.arc(cx, cy, 8, 2, c); g.circle(cx, cy, 4.3f, c); break;
    case IC_CARD: {
      const float xy[10] = { cx - 6.5f, cy - 8.5f, cx + 3, cy - 8.5f, cx + 7, cy - 4.5f, cx + 7, cy + 8.5f, cx - 6.5f, cy + 8.5f };
      g.poly(xy, 5, c);
      for(uint8_t k = 0; k < 3; k++) g.line(cx - 3.5f + k * 3, cy - 6.3f, cx - 3.5f + k * 3, cy - 3, 1.3f, bg);
      break; }
    case IC_RADIO:
      g.circle(cx, cy - 1.5f, 2.5f, c); g.line(cx, cy + 1, cx, cy + 8.5f, 2, c);
      g.arc(cx, cy - 1.5f, 6, 1.8f, c, 50, 130); g.arc(cx, cy - 1.5f, 6, 1.8f, c, 230, 310);
      g.arc(cx, cy - 1.5f, 10, 1.8f, c, 55, 125); g.arc(cx, cy - 1.5f, 10, 1.8f, c, 235, 305);
      break;
    case IC_STAR: star(g, cx, cy + 0.5f, 9, c); break;
    case IC_CROSS:
      g.line(cx, cy - 8, cx, cy + 8.5f, 2.7f, c); g.line(cx - 6, cy - 3, cx + 6, cy - 3, 2.7f, c); break;
    case IC_EQ:
      g.line(cx - 6, cy + 7, cx - 6, cy - 1, 2.5f, c); g.line(cx, cy + 7, cx, cy - 8, 2.5f, c); g.line(cx + 6, cy + 7, cx + 6, cy - 3, 2.5f, c);
      break;
    case IC_SUN:
      g.circle(cx, cy, 3.9f, c);
      for(uint8_t k = 0; k < 8; k++){ float a = k * 0.7854f; g.line(cx + 6.4f * cosf(a), cy + 6.4f * sinf(a), cx + 8.6f * cosf(a), cy + 8.6f * sinf(a), 1.8f, c); }
      break;
    case IC_GEAR:
      g.arc(cx, cy, 4.7f, 2.3f, c);
      for(uint8_t k = 0; k < 8; k++){ float a = k * 0.7854f; g.line(cx + 6.8f * cosf(a), cy + 6.8f * sinf(a), cx + 8.8f * cosf(a), cy + 8.8f * sinf(a), 2.5f, c); }
      break;
    case IC_LIST:
      for(uint8_t k = 0; k < 3; k++){ g.circle(cx - 6.5f, cy - 5 + k * 5, 1.5f, c); g.line(cx - 2.5f, cy - 5 + k * 5, cx + 7.5f, cy - 5 + k * 5, 2.1f, c); }
      break;
    case IC_MENU:
      for(uint8_t k = 0; k < 3; k++) g.line(cx - 7, cy - 5 + k * 5, cx + 7, cy - 5 + k * 5, 2.1f, c);
      break;
    case IC_WIFI:
      for(uint8_t k = 1; k <= 3; k++) g.arc(cx, cy + 6.5f, 4.2f * k, 2, c, -45, 45);
      g.circle(cx, cy + 6.5f, 1.9f, c);
      break;
    case IC_MIC:
      g.box((int16_t)(cx - 3.5f), (int16_t)(cy - 9), 7, 12, 3, c);
      g.arc(cx, cy - 1.5f, 6.3f, 1.8f, c, 95, 265);
      g.line(cx, cy + 5, cx, cy + 8.5f, 1.8f, c);
      break;
    case IC_CLOCK:
      g.arc(cx, cy, 7.8f, 2, c); g.line(cx, cy, cx, cy - 4.6f, 1.9f, c); g.line(cx, cy, cx + 3.6f, cy + 1.2f, 1.9f, c); break;
    case IC_INFO:
      g.arc(cx, cy, 7.8f, 2, c); g.circle(cx, cy - 3.7f, 1.35f, c); g.line(cx, cy - 0.6f, cx, cy + 4.3f, 2.1f, c); break;
    case IC_POWER:
      g.arc(cx, cy + 0.8f, 7.2f, 2.1f, c, 38, 322); g.line(cx, cy - 8.4f, cx, cy - 1.2f, 2.1f, c); break;
    case IC_RESTART:
      g.arc(cx, cy, 7.2f, 2.1f, c, 60, 350);
      { const float xy[6] = { cx - 1.2f, cy - 11, cx - 1.2f, cy - 3.6f, cx + 4.6f, cy - 7.3f }; g.poly(xy, 3, c); }
      break;
    case IC_CODE:
      g.line(cx - 4, cy - 5, cx - 8, cy, 1.9f, c); g.line(cx - 8, cy, cx - 4, cy + 5, 1.9f, c);
      g.line(cx + 4, cy - 5, cx + 8, cy, 1.9f, c); g.line(cx + 8, cy, cx + 4, cy + 5, 1.9f, c);
      g.line(cx + 1.6f, cy - 6.5f, cx - 1.6f, cy + 6.5f, 1.7f, c);
      break;
    case IC_SPEAKER: {
      const float xy[12] = { cx - 8.5f, cy - 3.2f, cx - 4.2f, cy - 3.2f, cx + 0.5f, cy - 7.8f, cx + 0.5f, cy + 7.8f, cx - 4.2f, cy + 3.2f, cx - 8.5f, cy + 3.2f };
      g.poly(xy, 6, c);
      g.arc(cx + 0.5f, cy, 4.8f, 1.7f, c, 50, 130); g.arc(cx + 0.5f, cy, 8.6f, 1.7f, c, 55, 125);
      break; }
    case IC_HAND:
      g.arc(cx - 3, cy + 1, 5, 1.9f, c, 200, 40); g.arc(cx + 3, cy + 1, 5, 1.9f, c, 320, 160);
      g.line(cx - 6, cy - 7.5f, cx - 8, cy - 9.5f, 1.6f, c); g.line(cx, cy - 8.5f, cx, cy - 11, 1.6f, c); g.line(cx + 6, cy - 7.5f, cx + 8, cy - 9.5f, 1.6f, c);
      break;
    case IC_EYE:
      g.arc(cx, cy + 8, 11, 1.8f, c, -48, 48); g.arc(cx, cy - 8, 11, 1.8f, c, 132, 228); g.circle(cx, cy, 2.8f, c); break;
    case IC_LOCK:
      g.box((int16_t)(cx - 4.5f), (int16_t)(cy - 1), 9, 7, 2, c); g.arc(cx, cy - 1.5f, 3, 1.5f, c, 270, 90); break;
    case IC_PLUS:
      g.line(cx - 6.5f, cy, cx + 6.5f, cy, 2.3f, c); g.line(cx, cy - 6.5f, cx, cy + 6.5f, 2.3f, c); break;
    case IC_CLOSE:
      g.line(cx - 5.5f, cy - 5.5f, cx + 5.5f, cy + 5.5f, 2.2f, c); g.line(cx - 5.5f, cy + 5.5f, cx + 5.5f, cy - 5.5f, 2.2f, c); break;
    case IC_UP:
      g.line(cx, cy + 7, cx, cy - 6, 2.2f, c); g.line(cx - 5.5f, cy - 1, cx, cy - 6.5f, 2.2f, c); g.line(cx, cy - 6.5f, cx + 5.5f, cy - 1, 2.2f, c); break;
    case IC_CHECK:
      g.line(cx - 6, cy + 0.5f, cx - 2, cy + 4.5f, 2.3f, c); g.line(cx - 2, cy + 4.5f, cx + 6.5f, cy - 4.5f, 2.3f, c); break;
    case IC_REFRESH:
      g.arc(cx, cy, 7, 2, c, 30, 300);
      { const float xy[6] = { cx + 2.5f, cy - 10.5f, cx + 2.5f, cy - 3.5f, cx + 8.5f, cy - 7 }; g.poly(xy, 3, c); }
      break;
    case IC_KEYS:
      g.frame((int16_t)(cx - 9), (int16_t)(cy - 6), 18, 12, 3, c, 2);
      for(uint8_t k = 0; k < 3; k++) g.circle(cx - 4 + k * 4, cy - 1.5f, 1.1f, c);
      g.line(cx - 4, cy + 2.5f, cx + 4, cy + 2.5f, 1.4f, c);
      break;
    case IC_BATTERY:
      g.frame((int16_t)(cx - 9), (int16_t)(cy - 5), 16, 10, 2, c, 2); g.box((int16_t)(cx + 7), (int16_t)(cy - 2), 2, 4, 1, c);
      g.box((int16_t)(cx - 6), (int16_t)(cy - 2), 7, 4, 1, c);
      break;
    case IC_LED:
      g.circle(cx, cy - 1, 5, c); g.line(cx - 3, cy + 6.5f, cx + 3, cy + 6.5f, 1.8f, c);
      g.line(cx - 9, cy - 1, cx - 7, cy - 1, 1.6f, c); g.line(cx + 7, cy - 1, cx + 9, cy - 1, 1.6f, c); g.line(cx, cy - 9, cx, cy - 7.5f, 1.6f, c);
      break;
    case IC_NOTE:
      g.circle(cx - 4, cy + 5, 3.2f, c); g.line(cx - 1.4f, cy + 5, cx - 1.4f, cy - 8, 1.9f, c);
      g.line(cx - 1.4f, cy - 8, cx + 6, cy - 5.5f, 2.2f, c);
      break;
    case IC_ROOM: {
      const float xy[10] = { cx - 9, cy - 1, cx, cy - 9, cx + 9, cy - 1, cx + 9, cy + 8, cx - 9, cy + 8 };
      g.poly(xy, 5, c);
      g.line(cx - 4, cy + 4, cx - 4, cy + 1, 1.5f, bg); g.line(cx, cy + 4, cx, cy - 1, 1.5f, bg); g.line(cx + 4, cy + 4, cx + 4, cy + 2, 1.5f, bg);
      break; }
    case IC_PERSON:
      g.circle(cx, cy - 4.5f, 3.8f, c); g.arc(cx, cy + 9, 8, 2.6f, c, 300, 60); break;
    case IC_WAVE:
      for(uint8_t k = 0; k < 5; k++){ static const float H[5] = { 3, 7, 10, 6, 3 }; g.line(cx - 8 + k * 4, cy - H[k], cx - 8 + k * 4, cy + H[k], 2, c); }
      break;
    case IC_CHIP:
      g.frame((int16_t)(cx - 6), (int16_t)(cy - 6), 12, 12, 2, c, 2);
      for(uint8_t k = 0; k < 3; k++){
        float o = -3.5f + k * 3.5f;
        g.line(cx + o, cy - 9, cx + o, cy - 7, 1.3f, c); g.line(cx + o, cy + 7, cx + o, cy + 9, 1.3f, c);
        g.line(cx - 9, cy + o, cx - 7, cy + o, 1.3f, c); g.line(cx + 7, cy + o, cx + 9, cy + o, 1.3f, c);
      }
      break;
    case IC_PLAY: { const float xy[6] = { cx - 4.5f, cy - 7, cx - 4.5f, cy + 7, cx + 7, cy }; g.poly(xy, 3, c); break; }
    case IC_STOP: g.box((int16_t)(cx - 6), (int16_t)(cy - 6), 12, 12, 2, c); break;
    case IC_PENCIL:
      g.line(cx - 4, cy + 4, cx + 6, cy - 6, 4.2f, c);
      { const float xy[6] = { cx - 8.5f, cy + 8.5f, cx - 7, cy + 2, cx - 2, cy + 7 }; g.poly(xy, 3, c); }
      break;
    case IC_TRASH:
      g.line(cx - 7.5f, cy - 5.5f, cx + 7.5f, cy - 5.5f, 2, c); g.line(cx - 2.5f, cy - 8, cx + 2.5f, cy - 8, 2, c);
      { const float xy[8] = { cx - 6, cy - 3.5f, cx + 6, cy - 3.5f, cx + 5, cy + 8.5f, cx - 5, cy + 8.5f }; g.poly(xy, 4, c); }
      break;
    case IC_SPLASH:
      g.circle(cx, cy - 2, 2.6f, c); g.line(cx, cy, cx, cy + 8, 2, c);
      g.arc(cx, cy - 2, 6, 1.8f, c, 50, 130); g.arc(cx, cy - 2, 6, 1.8f, c, 230, 310);
      break;
    case IC_GLOBE:
      g.arc(cx, cy, 8, 1.8f, c); g.line(cx - 8, cy, cx + 8, cy, 1.5f, c);
      g.arc(cx + 9, cy, 12, 1.5f, c, 222, 318); g.arc(cx - 9, cy, 12, 1.5f, c, 42, 138);
      break;
    case IC_START:
      g.arc(cx, cy, 8, 1.8f, c); { const float xy[6] = { cx - 2.5f, cy - 4.5f, cx - 2.5f, cy + 4.5f, cx + 5, cy }; g.poly(xy, 3, c); } break;
    case IC_BACKSPACE: {
      const float xy[10] = { cx - 10, cy, cx - 5, cy - 6.5f, cx + 9, cy - 6.5f, cx + 9, cy + 6.5f, cx - 5, cy + 6.5f };
      g.poly(xy, 5, c);
      g.line(cx - 1.5f, cy - 3, cx + 4.5f, cy + 3, 1.8f, bg); g.line(cx - 1.5f, cy + 3, cx + 4.5f, cy - 3, 1.8f, bg);
      break; }
    case IC_SHIFT: {
      const float xy[14] = { cx, cy - 8, cx + 8, cy, cx + 3.5f, cy, cx + 3.5f, cy + 7, cx - 3.5f, cy + 7, cx - 3.5f, cy, cx - 8, cy };
      g.poly(xy, 7, c);
      break; }
    default: break;
  }
}

}  // namespace m2
