#include "options.h"
#if SDC_CS!=255
#include <Arduino.h>
#include <SPI.h>
#if SD_SDMMC
  #include <SD_MMC.h>
#else
  #include <SD.h>
  #include "sd_diskio.h"
#endif
#include "vfs_api.h"
//#define USE_SD
#include "config.h"
#include "sdmanager.h"
#include "display.h"
#include "player.h"

#if defined(SD_SPIPINS) || SD_HSPI
SPIClass  SDSPI(HSPI);
#define SDREALSPI SDSPI
#else
  #define SDREALSPI SPI
#endif

#ifndef SDSPISPEED
  #define SDSPISPEED 20000000
#endif

SDManager sdman(FSImplPtr(new VFSImpl()));

#if SD_SDMMC
/*  Карта на ES3C28P сидить не на SPI, а на чотирибітній шині SDIO.
 *
 *  Важливо: невдалий begin() лишає всередині SDMMCFS ненульовий _card, а
 *  setPins() у такому стані відмовляється працювати («must be called before
 *  begin»). Тому перед кожною спробою робимо end() — інакше одна невдача
 *  назавжди закриває доступ до картки до перезавантаження.                  */
bool SDManager::start(){
  if(ready) return true;

  end();                                   /* скидаємо _card від попередніх спроб */
  if(!setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_D0, SDMMC_D1, SDMMC_D2, SDMMC_D3)){
    Serial.println("##[ERROR]#\tSD_MMC: не вдалося призначити виводи");
    return false;
  }
  ready = begin("/sdcard", false, false, BOARD_MAX_SDMMC_FREQ, 5);

  if(!ready){                              /* один біт і менша частота — терпиміше */
    end();
    setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_D0, SDMMC_D1, SDMMC_D2, SDMMC_D3);
    ready = begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 5);
    if(ready) Serial.println("##[BOOT]#\tSD: однобітний режим");
  }
  if(!ready){
    end();
    Serial.println("##[ERROR]#\tSD: картку не знайдено");
  }
  return ready;
}

#else
bool SDManager::start(){
  ready = begin(SDC_CS, SDREALSPI, SDSPISPEED);
  vTaskDelay(10);
  if(!ready) ready = begin(SDC_CS, SDREALSPI, SDSPISPEED);
  vTaskDelay(20);
  if(!ready) ready = begin(SDC_CS, SDREALSPI, SDSPISPEED);
  vTaskDelay(50);
  if(!ready) ready = begin(SDC_CS, SDREALSPI, SDSPISPEED);
  return ready;
}
#endif

void SDManager::stop(){
  end();
  ready = false;
}
#if !SD_SDMMC
#include "diskio_impl.h"
#endif
bool SDManager::cardPresent() {

  if(!ready) return false;
  if(sectorSize()<1) {
    return false;
  }
  uint8_t buff[sectorSize()] = { 0 };
  bool bread = readRAW(buff, 1);
  if(sectorSize()>0 && !bread) return false;
  return bread;
}

bool SDManager::_checkNoMedia(const char* path){
  if (path[strlen(path) - 1] == '/')
    snprintf(config.tmpBuf, sizeof(config.tmpBuf), "%s%s", path, ".nomedia");
  else
    snprintf(config.tmpBuf, sizeof(config.tmpBuf), "%s/%s", path, ".nomedia");
  bool nm = exists(config.tmpBuf);
  return nm;
}

bool SDManager::_endsWith (const char* base, const char* str) {
  int slen = strlen(str) - 1;
  const char *p = base + strlen(base) - 1;
  while(p > base && isspace(*p)) p--;
  p -= slen;
  if (p < base) return false;
  return (strncmp(p, str, slen) == 0);
}

void SDManager::listSD(File &plSDfile, File &plSDindex, const char* dirname, uint8_t levels) {
    File root = sdman.open(dirname);
    if (!root) {
        Serial.println("##[ERROR]#\tFailed to open directory");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println("##[ERROR]#\tNot a directory");
        return;
    }

    uint32_t pos = 0;
    char* filePath;
    while (true) {
        vTaskDelay(2);
        player.loop();
        bool isDir;
        String fileName = root.getNextFileName(&isDir);
        if (fileName.isEmpty()) break;
        filePath = (char*)malloc(fileName.length() + 1);
        if (filePath == NULL) {
            Serial.println("Memory allocation failed");
            break;
        }
        strcpy(filePath, fileName.c_str());
        const char* fn = strrchr(filePath, '/') + 1;
        /*  Службові файли й теки, що лишають на флешці macOS і Windows.
            «._назва.m4a» — це метадані Finder, а не музика, але розширення в
            них те саме, і плейлист брав їх за треки: перемикання на такий
            «трек» давало тишу. Приховані теки (.Trashes, .Spotlight-V100)
            теж пропускаємо.  */
        if (fn[0] == '.' || strcmp(fn, "System Volume Information") == 0 || strcmp(fn, "$RECYCLE.BIN") == 0) {
          free(filePath);
          continue;
        }
        if (isDir) {
            if (levels && !_checkNoMedia(filePath)) {
                listSD(plSDfile, plSDindex, filePath, levels - 1);
            }
        } else {
            if (_endsWith(strlwr((char*)fn), ".mp3") || _endsWith(fn, ".m4a") || _endsWith(fn, ".aac") ||
                _endsWith(fn, ".wav") || _endsWith(fn, ".flac")) {
                pos = plSDfile.position();
                plSDfile.printf("%s\t%s\t0\n", fn, filePath);
                plSDindex.write((uint8_t*)&pos, 4);
                Serial.print(".");
                if(display.mode()==SDCHANGE) display.putRequest(SDFILEINDEX, _sdFCount+1);
                _sdFCount++;
                if (_sdFCount % 64 == 0) Serial.println();
            }
        }
        free(filePath);
    }
    root.close();
}

void SDManager::indexSDPlaylist() {
  _sdFCount = 0;
  if(exists(PLAYLIST_SD_PATH)) remove(PLAYLIST_SD_PATH);
  if(exists(INDEX_SD_PATH)) remove(INDEX_SD_PATH);
  File playlist = open(PLAYLIST_SD_PATH, "w", true);
  if (!playlist) {
    return;
  }
  File index = open(INDEX_SD_PATH, "w", true);
  listSD(playlist, index, "/", SD_MAX_LEVELS);
  index.flush();
  index.close();
  playlist.flush();
  playlist.close();
  Serial.println();
  delay(50);
}
#endif


