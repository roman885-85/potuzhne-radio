/*  Генератор GFXfont для yoRadio у розкладці CP1251.
 *
 *  Adafruit fontconvert вміє лише неперервний діапазон Unicode, а текст у
 *  yoRadio після utf8Rus() — це однобайтові коди CP1251. Тому тут кожен байт
 *  0x20..0xFF рендериться зі свого місця в Unicode за таблицею нижче, і на
 *  виході виходить шрифт, індекс у якому дорівнює байту рядка.
 *
 *  Збірка:  cc cp1251fontconvert.c -o cp1251fontconvert $(pkg-config --cflags --libs freetype2)
 *  Виклик:  ./cp1251fontconvert <файл.ttf> <розмір_pt> <ім'я> > шрифт.h
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

static const unsigned short cp1251[128] = {
0x402,0x403,0x201A,0x453,0x201E,0x2026,0x2020,0x2021,0x20AC,0x2030,0x409,0x2039,0x40A,0x40C,0x40B,0x40F,
0x452,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,0x0020,0x2122,0x459,0x203A,0x45A,0x45C,0x45B,0x45F,
0x00A0,0x40E,0x45E,0x408,0x00A4,0x490,0x00A6,0x00A7,0x401,0x00A9,0x404,0x00AB,0x00AC,0x00AD,0x00AE,0x407,
0x00B0,0x00B1,0x406,0x456,0x491,0x00B5,0x00B6,0x00B7,0x451,0x2116,0x454,0x00BB,0x458,0x405,0x455,0x457,
0x410,0x411,0x412,0x413,0x414,0x415,0x416,0x417,0x418,0x419,0x41A,0x41B,0x41C,0x41D,0x41E,0x41F,
0x420,0x421,0x422,0x423,0x424,0x425,0x426,0x427,0x428,0x429,0x42A,0x42B,0x42C,0x42D,0x42E,0x42F,
0x430,0x431,0x432,0x433,0x434,0x435,0x436,0x437,0x438,0x439,0x43A,0x43B,0x43C,0x43D,0x43E,0x43F,
0x440,0x441,0x442,0x443,0x444,0x445,0x446,0x447,0x448,0x449,0x44A,0x44B,0x44C,0x44D,0x44E,0x44F };

#define FIRST 0x20
#define LAST  0xFF

int main(int argc, char **argv){
  if(argc < 4){ fprintf(stderr,"usage: %s font.ttf size name [mono_advance]\n", argv[0]); return 1; }
  const char *path=argv[1]; int size=atoi(argv[2]); const char *name=argv[3];
  int mono = (argc > 4) ? atoi(argv[4]) : 0;   /* 0 = пропорційний */
  int maxh = (argc > 5) ? atoi(argv[5]) : 0;   /* обмеження висоти рядка */

  FT_Library lib; FT_Face face;
  if(FT_Init_FreeType(&lib)){ fprintf(stderr,"freetype init failed\n"); return 1; }
  if(FT_New_Face(lib, path, 0, &face)){ fprintf(stderr,"cannot open %s\n", path); return 1; }
  FT_Set_Char_Size(face, size*64, 0, 141, 141);   /* 141 dpi — як в Adafruit fontconvert */

  /*  Моноширинний режим: підбираємо кегль так, щоб найширша літера влізла
      в заданий крок, інакше широкі Ш або М обрізало б.  */
  if(mono > 0){
    int pt = size*64;
    for(;;){
      FT_Set_Char_Size(face, pt, 0, 141, 141);
      int maxw = 0;
      for(int c=FIRST;c<=LAST;c++){
        unsigned long u = (c<0x80) ? (unsigned long)c : cp1251[c-0x80];
        if(FT_Load_Char(face, u, FT_LOAD_TARGET_MONO|FT_LOAD_RENDER|FT_LOAD_MONOCHROME)) continue;
        if((int)face->glyph->bitmap.width > maxw) maxw = face->glyph->bitmap.width;
      }
      /*  Висоту рахуємо з надрядковими знаками і нижніми виносними: інакше
          літери на кшталт «й» вилазять за рядок і їх зрізає.  */
      int asc = 0, desc = 0;
      for(int c=FIRST;c<=LAST;c++){
        /*  Висоту міряємо лише по літерах і цифрах. Рідкісні знаки на кшталт
            «‰» чи «№» вищі за все інше й даремно тиснули б кегль униз.  */
        if(!((c>=0x20 && c<=0x7E) || c==0xA5 || c==0xAA || c==0xAF || c==0xB2 ||
             c==0xB3 || c==0xB4 || c==0xBA || c==0xBF || c>=0xC0)) continue;
        unsigned long u = (c<0x80) ? (unsigned long)c : cp1251[c-0x80];
        if(FT_Load_Char(face, u, FT_LOAD_TARGET_MONO|FT_LOAD_RENDER|FT_LOAD_MONOCHROME)) continue;
        int a = face->glyph->bitmap_top;
        int d = (int)face->glyph->bitmap.rows - a;
        if(a > asc) asc = a;
        if(d > desc) desc = d;
      }
      if((maxw <= mono) && (maxh <= 0 || asc + desc <= maxh)) break;
      if(pt < 4*64) break;
      pt -= 32;                                  /* пів пункта вниз */
    }
    fprintf(stderr,"моноширинний крок %d, підібраний кегль %.1f pt\n", mono, pt/64.0);
  }

  unsigned char *bits=NULL; long nbits=0;
  int gw[256], gh[256], gx[256], gy[256], ga[256]; long goff[256];

  for(int c=FIRST;c<=LAST;c++){
    unsigned long uni = (c<0x80) ? (unsigned long)c : cp1251[c-0x80];
    if(FT_Load_Char(face, uni, FT_LOAD_TARGET_MONO|FT_LOAD_RENDER|FT_LOAD_MONOCHROME)){
      gw[c]=gh[c]=gx[c]=gy[c]=0; ga[c]=size/2; goff[c]=nbits; continue;
    }
    FT_Bitmap *bm=&face->glyph->bitmap;
    goff[c]=nbits;
    gw[c]=bm->width; gh[c]=bm->rows;
    gx[c]=face->glyph->bitmap_left; gy[c]=-face->glyph->bitmap_top;
    ga[c]=(int)(face->glyph->advance.x >> 6);
    if(mono > 0){ ga[c] = mono; gx[c] = (mono - (int)bm->width)/2; }   /* по центру кроку */
    long need=((long)bm->width*bm->rows+7)/8;
    bits=realloc(bits, nbits+need+1); memset(bits+nbits, 0, need+1);
    long bit=0;
    for(unsigned r=0;r<bm->rows;r++)
      for(unsigned x=0;x<bm->width;x++){
        if(bm->buffer[r*bm->pitch + x/8] & (0x80>>(x&7)))
          bits[nbits + bit/8] |= 0x80 >> (bit&7);
        bit++;
      }
    nbits += need;
  }

  printf("#ifndef %s_h\n#define %s_h\n#include <Adafruit_GFX.h>\n\n", name, name);
  printf("/*  %s %dpt, розкладка CP1251 (0x20..0xFF) — під вивід yoRadio через utf8Rus().  */\n", path, size);
  printf("const uint8_t %sBitmaps[] PROGMEM = {\n ", name);
  for(long i=0;i<nbits;i++){ printf(" 0x%02X%s", bits[i], (i+1<nbits?",":"")); if(i%16==15) printf("\n "); }
  printf(" };\n\nconst GFXglyph %sGlyphs[] PROGMEM = {\n", name);
  for(int c=FIRST;c<=LAST;c++)
    printf("  { %ld, %d, %d, %d, %d, %d }%s\n", goff[c], gw[c], gh[c], ga[c], gx[c], gy[c], (c<LAST?",":""));
  printf("};\n\nconst GFXfont %s PROGMEM = {\n  (uint8_t*)%sBitmaps, (GFXglyph*)%sGlyphs, 0x%02X, 0x%02X, %d };\n\n#endif\n",
         name, name, name, FIRST, LAST, (int)(face->size->metrics.height>>6));
  fprintf(stderr,"%s %dpt: %ld байт растру, висота рядка %d\n", name, size, nbits, (int)(face->size->metrics.height>>6));
  return 0;
}
