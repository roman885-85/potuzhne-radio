#ifndef dspcore_h
#define dspcore_h
#pragma once
/*  Екран плати ES3C28P — ILI9341 2,8″ 320×240. Драйвери інших екранів yoRadio
    прибрано: прошивка — для однієї плати.  */
#if DSP_MODEL==DSP_ILI9341
  #define TIME_SIZE           52
  #include "displayILI9341.h"
#else
  #error "ПОТУЖНЕ РАДІО зібране під екран ILI9341 (DSP_MODEL=DSP_ILI9341)"
#endif

#endif
