#include "spidma.h"
#if CONFIG_IDF_TARGET_ESP32S3
#include "esp_private/gdma.h"
#include "hal/spi_ll.h"
#include "hal/dma_types.h"
#include "soc/gdma_channel.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

static gdma_channel_handle_t s_chan = nullptr;
static dma_descriptor_t*     s_desc = nullptr;
static const int   SPIDMA_DESC  = 9;          /* 9 x 4092 = 36 КБ за один захід */
static const size_t SPIDMA_CHUNK = 4092;
static const size_t SPIDMA_MAX   = 32768;     /* межа довжини передачі в регістрі SPI */
static bool s_broken = false;                /* передача раз не завершилась — далі без DMA */
uint32_t spidmaBytes = 0;                        /* налагодження: скільки пішло в екран */
uint32_t spidmaHz = 40000000;                 /* частота шини дисплея — щоб знати, скільки спати під час передачі */

bool spidmaBegin(){
  if(s_chan) return true;
  /*  Дескриптори мусять лежати у внутрішній пам'яті, доступній для DMA.  */
  s_desc = (dma_descriptor_t*)heap_caps_calloc(SPIDMA_DESC, sizeof(dma_descriptor_t),
                                                MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if(!s_desc) return false;
  gdma_channel_alloc_config_t cfg = {};
  cfg.direction = GDMA_CHANNEL_DIRECTION_TX;
  if(gdma_new_ahb_channel(&cfg, &s_chan) != ESP_OK){ s_chan = nullptr; return false; }
  if(gdma_connect(s_chan, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_SPI, 2)) != ESP_OK){
    gdma_del_channel(s_chan); s_chan = nullptr; return false;
  }
  gdma_strategy_config_t st = {};
  st.owner_check = false;
  st.auto_update_desc = false;
  gdma_apply_strategy(s_chan, &st);
  return true;
}

static uint32_t s_userSave = 0;
static size_t   s_len = 0;
static int64_t  s_t0 = 0;
static bool     s_busy = false;

/*  Почати передачу й одразу повернутись: поки байти йдуть шиною, процесор
    малює наступну смугу в інший буфер.  */
static void spidmaKick(const uint8_t* p, size_t len){
  spi_dev_t* hw = &GPSPI2;
  int n = 0; size_t left = len;
  while(left && n < SPIDMA_DESC){
    size_t c = left > SPIDMA_CHUNK ? SPIDMA_CHUNK : left;
    s_desc[n].dw0.size    = c;
    s_desc[n].dw0.length  = c;
    s_desc[n].dw0.suc_eof = 0;
    s_desc[n].dw0.owner   = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
    s_desc[n].buffer = (void*)p;
    s_desc[n].next   = nullptr;
    if(n) s_desc[n-1].next = &s_desc[n];
    p += c; left -= c; n++;
  }
  s_desc[n-1].dw0.suc_eof = 1;

  /*  Ядро Arduino тримає SPI у повнодуплексному режимі з фазою прийому.
      На час передачі прийом вимикаємо, а після — повертаємо як було, щоб
      його власні записи через FIFO працювали далі без змін.  */
  s_userSave = hw->user.val;
  hw->user.usr_miso = 0;
  hw->user.usr_mosi = 1;
  gdma_reset(s_chan);
  spi_ll_dma_tx_fifo_reset(hw);
  spi_ll_outfifo_empty_clr(hw);
  spi_ll_dma_tx_enable(hw, true);
  gdma_start(s_chan, (intptr_t)&s_desc[0]);
  spi_ll_set_mosi_bitlen(hw, len * 8);
  hw->dma_int_clr.trans_done = 1;
  spi_ll_apply_config(hw);
  spi_ll_user_start(hw);
  s_len = len; s_t0 = esp_timer_get_time(); s_busy = true;
}

/*  Дочекатися кінця передачі, але не вічно. Раніше тут був порожній цикл без
    межі: при швидкій прокрутці довгого списку «кінець» одного разу не
    прийшов, задача екрана стала, і сторож задач перезавантажував радіо.
    Поки лишається більше 2 мс — спимо: на тому ж ядрі мікрофон і Wi-Fi.  */
static bool spidmaFinish(){
  if(!s_busy) return true;
  spi_dev_t* hw = &GPSPI2;
  const int64_t need = (int64_t)s_len * 8 * 1000000LL / spidmaHz;      /* мікросекунд на всю передачу */
  bool done = true;
  while(!hw->dma_int_raw.trans_done){
    const int64_t el = esp_timer_get_time() - s_t0;
    if(el > 300000){ done = false; break; }
    /*  спимо навіть коротко: порожнє очікування по кілька мілісекунд на смугу
        забирало ядро 0 цілком, і задача простою не діставала часу  */
    if(need - el > 1200) vTaskDelay(pdMS_TO_TICKS(need - el > 3000 ? (need - el) / 1000 - 1 : 1));
  }
  if(!done){
    Serial.printf("##DSP#\tDMA: передача %u байт не завершилась (usr=%u raw=0x%08x) — далі без DMA\n",
                  (unsigned)s_len, (unsigned)hw->cmd.usr, (unsigned)hw->dma_int_raw.val);
    gdma_stop(s_chan);
    gdma_reset(s_chan);
  }
  hw->dma_int_clr.trans_done = 1;
  spi_ll_dma_tx_enable(hw, false);
  hw->user.val = s_userSave;
  s_busy = false;
  return done;
}

static bool spidmaOnce(const uint8_t* p, size_t len){
  spidmaKick(p, len);
  return spidmaFinish();
}

bool spidmaStart(const void* buf, size_t len){
  if(!s_chan || s_broken || !buf || !len || len > SPIDMA_MAX) return false;
  if(s_busy && !spidmaFinish()){ s_broken = true; return false; }
  spidmaBytes += len;
  spidmaKick((const uint8_t*)buf, len);
  return true;
}

bool spidmaWait(){
  if(!s_busy) return true;
  if(!spidmaFinish()){ s_broken = true; return false; }
  return true;
}

bool spidmaOk(){ return s_chan && !s_broken; }

void* spidmaScratch(size_t len){
  static void* s_buf = nullptr; static size_t s_len = 0;
  if(len > s_len){
    /*  Перший запит беремо із запасом під найбільшу смугу (рядок списку 254×32),
        щоб потім не перевиділяти й не дробити пам'ять.  */
    size_t want = len < 16384 ? 16384 : len;
    void* nb = heap_caps_malloc(want, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if(!nb) return s_len >= len ? s_buf : nullptr;
    if(s_buf) heap_caps_free(s_buf);
    s_buf = nb; s_len = want;
  }
  return s_buf;
}

bool spidmaWrite(const void* buf, size_t len){
  if(!s_chan || s_broken || !buf || !len) return false;
  if(s_busy && !spidmaFinish()){ s_broken = true; return false; }
  spidmaBytes += len;
  const uint8_t* p = (const uint8_t*)buf;
  while(len){                                    /* довше за межу — кількома заходами */
    size_t c = len > SPIDMA_MAX ? SPIDMA_MAX : len;
    if(!spidmaOnce(p, c)){ s_broken = true; return false; }
    p += c; len -= c;
  }
  return true;
}
#else
uint32_t spidmaHz = 40000000;
bool spidmaBegin(){ return false; }
bool spidmaOk(){ return false; }
void* spidmaScratch(size_t len){
  static void* s_buf = nullptr; static size_t s_len = 0;
  if(len > s_len){ if(s_buf) free(s_buf); s_buf = malloc(len); s_len = s_buf ? len : 0; }
  return s_buf;
}
bool spidmaWrite(const void* buf, size_t len){ (void)buf; (void)len; return false; }
bool spidmaStart(const void* buf, size_t len){ (void)buf; (void)len; return false; }
bool spidmaWait(){ return true; }
#endif
