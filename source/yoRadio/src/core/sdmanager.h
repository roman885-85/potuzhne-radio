#ifndef sdmanager_h
#define sdmanager_h

#include "options.h"

#if SD_SDMMC
  #include <SD_MMC.h>
  /*  SD.h подтягивает "using namespace fs;" сам, а SD_MMC.h — нет.  */
  using namespace fs;
  #define SDBASEFS fs::SDMMCFS
#else
  #define SDBASEFS SDFS
#endif

class SDManager : public SDBASEFS {
  public:
    bool ready;
  public:
    SDManager(FSImplPtr impl) : SDBASEFS(impl) {}
    bool start();
    void stop();
    bool cardPresent();
    void listSD(File &plSDfile, File &plSDindex, const char * dirname, uint8_t levels);
    void indexSDPlaylist();
  private:
    uint32_t _sdFCount;
  private:
    bool _checkNoMedia(const char* path);
    bool _endsWith (const char* base, const char* str);
};

extern SDManager sdman;
#if defined(SD_SPIPINS) || SD_HSPI
extern SPIClass  SDSPI;
#endif
#endif
