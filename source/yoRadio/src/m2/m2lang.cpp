#include "m2lang.h"
#include <string.h>

namespace m2 {

uint8_t g_lang = LANG_UK;

struct TrRow { const char* uk; const char* en; };

/*  Таблиця перекладу. Її не пишуть руками: вона складається з tools/lang/en.json
    командою `python3 tools/mklang.py` і відсортована за українським рядком
    побайтово — саме так, як порівнює strcmp, щоб шукати навпіл.  */
#include "m2lang_en.h"

static const uint16_t TR_N = sizeof(TR_EN) / sizeof(TR_EN[0]);

void langSet(uint8_t l){ g_lang = l < LANG_N ? l : LANG_UK; }

const char* tr(const char* s){
  if(g_lang == LANG_UK || !s || !*s) return s;
  uint16_t lo = 0, hi = TR_N;
  while(lo < hi){
    const uint16_t mid = (uint16_t)((lo + hi) >> 1);
    const int c = strcmp(s, TR_EN[mid].uk);
    if(c == 0) return TR_EN[mid].en;
    if(c < 0) hi = mid; else lo = mid + 1;
  }
  return s;                       /* назви станцій, числа, адреси — як є */
}

}  // namespace m2
