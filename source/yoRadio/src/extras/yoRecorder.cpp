#include "yoRecorder.h"
#include "../core/options.h"
#include "../core/config.h"
#include "../core/player.h"
#include "../core/network.h"
#include "../core/sdmanager.h"
#include "yoExtras.h"

YoRecorder recorder;

#define REC_BUF    (64*1024)       /* запас у PSRAM: 2 с навіть на 256 кбіт/с */
#define REC_CHUNK  (8*1024)        /* скидаємо на картку такими шматками */
#define REC_SYNC   30000UL         /* раз на 30 с оновлюємо довжину файлу в FAT */
#define REC_MINFREE (50ULL*1024*1024)

/*  Викликається з Audio::processWebStream() — див. Audio.cpp.  */
void yoRecTap(const uint8_t* p, size_t n){ if(recorder.active()) recorder.tap(p, n); }

/*  Ім'я файлу лише з латиниці й цифр: кирилиця в іменах на FAT залежить
    від налаштувань збірки й на іншому пристрої може стати знаками питання.  */
static void asciiName(const char* src, char* dst, size_t cap){
  size_t o = 0; bool us = false;
  for(const char* q = src; *q && o + 1 < cap; q++){
    char c = *q;
    if(isalnum((unsigned char)c) && (unsigned char)c < 0x80){ dst[o++] = c; us = false; }
    else if((c == ' ' || c == '-' || c == '_' || c == '.') && o && !us){ dst[o++] = '_'; us = true; }
  }
  while(o && dst[o-1] == '_') o--;
  dst[o] = '\0';
}

bool YoRecorder::start(){
  if(_on) return true;
  _err = "";
  if(extras.s.noSd){ _err = "картку вимкнено (розробник)"; return false; }
  if(config.getMode() != PM_WEB){ _err = "запис лише з радіо"; return false; }
  if(player.status() != PLAYING){ _err = "нічого не грає"; return false; }
  if(!sdman.ready && !sdman.start()){ _err = "нема картки"; return false; }
  uint64_t freeB = sdman.totalBytes() - sdman.usedBytes();
  if(freeB < REC_MINFREE){ _err = "на картці мало місця"; return false; }
  if(!_buf){
    _buf = (uint8_t*)ps_malloc(REC_BUF);
    if(!_buf){ _err = "нема пам'яті"; return false; }
  }
  if(!sdman.exists("/records")) sdman.mkdir("/records");

  char st[32];
  asciiName(config.station.name, st, sizeof(st));
  if(!st[0]) snprintf(st, sizeof(st), "st%u", (unsigned)config.lastStation());
  const char* c = player.getCodecname();
  const char* ext = "bin";
  if(!strcmp(c, "MP3")) ext = "mp3";
  else if(!strcmp(c, "AAC")) ext = "aac";
  else if(!strcmp(c, "FLAC")) ext = "flac";
  else if(!strcmp(c, "OGG") || !strcmp(c, "VORBIS") || !strcmp(c, "OPUS")) ext = "ogg";
  const struct tm& t = network.timeinfo;
  snprintf(_name, sizeof(_name), "/records/%04d%02d%02d-%02d%02d%02d_%s.%s",
           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, st, ext);
  _f = sdman.open(_name, FILE_WRITE);
  if(!_f){ _err = "файл не створився"; return false; }
  _len = 0; _written = 0; _dropped = 0;
  _t0 = millis();
  _station = config.lastStation();
  _on = true;
  Serial.printf("##REC#\tзапис у %s (%s)\n", _name, c);
  return true;
}

void YoRecorder::stop(){
  if(!_on) return;
  _on = false;
  if(_len && _f){ _written += _f.write(_buf, _len); _len = 0; }
  if(_f) _f.close();
  /*  Індекс картки будується лише тоді, коли його файлу немає, — тож
      нові записи з'являлись би в списку картки хіба після ручного
      перебудування. Прибираємо індекс: при переході на картку він
      збудується наново, уже з записами з /records.  */
  if(sdman.ready){ sdman.remove(INDEX_SD_PATH); sdman.remove(PLAYLIST_SD_PATH); }
  Serial.printf("##REC#\tзупинено: %u байт за %u с, загублено %u\n",
                (unsigned)_written, (unsigned)((millis() - _t0) / 1000), (unsigned)_dropped);
}

void YoRecorder::tap(const uint8_t* p, size_t n){
  if(!_on || !_buf) return;
  size_t room = REC_BUF - _len;
  if(n > room){ _dropped += n - room; n = room; }
  memcpy(_buf + _len, p, n);
  _len += n;
}

void YoRecorder::_flush(){
  if(!_len) return;
  size_t w = _f.write(_buf, _len);
  _written += w;
  bool ok = (w == _len);
  _len = 0;
  if(!ok){ _err = "помилка запису на картку"; stop(); }
}

void YoRecorder::loop(){
  if(!_on) return;
  /*  Станцію перемкнули, зупинили, пішли на картку — запис закінчено:
      інакше в один файл злиплися б дві станції.  */
  if(config.getMode() != PM_WEB || !sdman.ready || player.status() != PLAYING ||
     config.lastStation() != _station){
    stop();
    return;
  }
  if(_len >= REC_CHUNK) _flush();
  static uint32_t syncT = 0;
  if(_on && millis() - syncT > REC_SYNC){ syncT = millis(); _f.flush(); }
}
