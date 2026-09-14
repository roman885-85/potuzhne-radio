#ifndef common_gfx_h
#define common_gfx_h
#include "../widgets/widgetsconfig.h" // displayXXXDDDDconf.h
#include "utf8Rus.h"
#define ADAFRUIT_CLIPPING !defined(DSP_LCD) && DSP_MODEL!=DSP_ILI9225

typedef struct clipArea {
  uint16_t left; 
  uint16_t top; 
  uint16_t width;  
  uint16_t height;
} clipArea;

class psFrameBuffer;

extern volatile bool g_dspWatch;       /* налагодження: хто малює в дисплей поза новим екраном */
extern volatile bool g_m2Draw;         /* зараз малює новий екран / нове меню */

class DspCore: public yoDisplay {
#ifdef YO_DEBUG
  public:
    void setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) override {
      if(g_dspWatch && !g_m2Draw) Serial.printf("##DSPW#\t%u,%u %ux%u\n", x, y, w, h);
      yoDisplay::setAddrWindow(x, y, w, h);
    }
    /*  Читання відеопам'яті ILI9341: CASET/PASET, далі RAMRD і по три байти
        на піксель. Потрібен доступ до writeCommand()/spiRead(), які в
        Adafruit_SPITFT захищені, тому метод живе всередині класу.  */
    void dbgReadRow(uint16_t y, uint16_t w, uint16_t* out){
      setSPISpeed(4000000);          // читання не терпить високої частоти
      startWrite();
      writeCommand(0x2A); SPI_WRITE16(0); SPI_WRITE16(w-1);
      writeCommand(0x2B); SPI_WRITE16(y); SPI_WRITE16(y);
      writeCommand(0x2E);
      spiRead();                     // порожній байт після команди
      for(uint16_t i=0;i<w;i++){
        uint8_t r=spiRead(), g=spiRead(), b=spiRead();
        out[i] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
      }
      endWrite();
      setSPISpeed(60000000);   /* повертаємо робочу частоту */
      /*  Adafruit пам'ятає останнє вікно стовпців і не шле його вдруге, якщо
          воно не змінилось. Ми щойно задали вікно напряму, в обхід цієї
          пам'яті, — тож після знімка бібліотека вважала б, що вікно старе, а
          контролер уже мав 0..319, і рядки списку лягали б у стовпчик кнопок.
          Задаємо свідомо інше вікно через саму бібліотеку: пам'ять і
          контролер знову збігаються.  */
      startWrite();
      setAddrWindow(0, 0, 1, 1);
      endWrite();
    }
#endif

  public:
    DspCore();
    void initDisplay();
    void clearDsp(bool black=false);
    void printClock(){}
    #ifdef DSP_OLED
    inline void loop(bool force=false){
      #if DSP_MODEL==DSP_NOKIA5110
        if(digitalRead(TFT_CS)==LOW) return;
        display();
      #else
        display();
        //delay(DSP_MODEL==DSP_ST7920?20:5);
        vTaskDelay(DSP_MODEL==DSP_ST7920?10:0);
      #endif
    }
    inline void drawLogo(uint16_t top) {
      #if DSP_MODEL!=DSP_SSD1306x32
        drawBitmap((width()  - LOGO_WIDTH ) / 2, top, logo, LOGO_WIDTH, LOGO_HEIGHT, 1);
      #else
        setTextSize(1); setCursor((width() - 6*CHARWIDTH) / 2, 0); setTextColor(TFT_FG, TFT_BG); print(utf8Rus("ПОТУЖНЕ", false));
      #endif
      display();
    }
    #else
      #ifndef DSP_LCD
      inline void loop(bool force=false){}
      inline void drawLogo(uint16_t top){ drawRGBBitmap((width() - LOGO_WIDTH) / 2, top, logo, LOGO_WIDTH, LOGO_HEIGHT); }
      #endif
    #endif
    #ifdef DSP_LCD
      uint16_t width();
      uint16_t height();
      void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
      void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color){}
      void setTextSize(uint8_t s){}
      void setTextSize(uint8_t sx, uint8_t sy){}
      void setTextColor(uint16_t c, uint16_t bg){}
      void setFont(){}
      void apScreen();
      void drawLogo(uint16_t top){}
      void loop(bool force=false){}
    #endif
    void flip();
    void invert();
    void sleep();
    void wake();
    void setScrollId(void * scrollid) { _scrollid = scrollid; }
    void * getScrollId() { return _scrollid; }
    uint16_t textWidth(const char *txt);
    #if DSP_MODEL==DSP_ILI9225
      uint16_t width(void) { return (int16_t)maxX(); }
      uint16_t height(void) { return (int16_t)maxY(); }
      inline void drawRGBBitmap(int16_t x, int16_t y, const uint16_t *bitmap, int16_t w, int16_t h){ drawBitmap(x, y, bitmap, w, h); }
      uint16_t print(const char* s);
      void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
      void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
      void setFont(const GFXfont *f = NULL);
      void setFont(uint8_t* font, bool monoSp=false );
      void setTextColor(uint16_t fg, uint16_t bg);
      void setCursor(int16_t x, int16_t y);
      void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
      void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
      inline uint16_t drawChar(uint16_t x, uint16_t y, uint16_t ch, uint16_t color = COLOR_WHITE){
        if(_clipping){
          if ((x < _cliparea.left) || (x >= _cliparea.left+_cliparea.width) || (y < _cliparea.top) || (y > _cliparea.top + _cliparea.height))  {
            return cfont.width;
          }
        }
        uint16_t ret=TFT_22_ILI9225::drawChar(x, y, ch, color);
        return ret;
      }
      void setTextSize(uint8_t s);
    #endif
    #if ADAFRUIT_CLIPPING
      inline void writePixel(int16_t x, int16_t y, uint16_t color) {
        if(_clipping){
          if ((x < _cliparea.left) || (x > _cliparea.left+_cliparea.width) || (y < _cliparea.top) || (y > _cliparea.top + _cliparea.height)) return;
        }
        yoDisplay::writePixel(x, y, color);
      }
      inline void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
        if(_clipping){
          if ((x < _cliparea.left) || (x >= _cliparea.left+_cliparea.width) || (y < _cliparea.top) || (y > _cliparea.top + _cliparea.height))  return;
        }
        yoDisplay::writeFillRect(x, y, w, h, color);
      }
    #else
      inline void writePixel(int16_t x, int16_t y, uint16_t color) { }
      inline void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { }
    #endif
    inline void setClipping(clipArea ca){
      _cliparea = ca;
      _clipping = true;
    }
    inline void clearClipping(){
      _clipping = false;
      #ifdef DSP_LCD
      setClipping({0, 0, width(), height()});
      #endif
    }
  private:
    bool _clipping;
    clipArea _cliparea;
    void * _scrollid;
    #ifdef PSFBUFFER
    psFrameBuffer* _fb=nullptr;
    #endif
    #if DSP_MODEL==DSP_ILI9225
      uint16_t _bgcolor, _fgcolor;
      int16_t  _cursorx, _cursory;
      bool _gFont/*, _started*/;
    #endif
};

extern DspCore dsp;
#endif
