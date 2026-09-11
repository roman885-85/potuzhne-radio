#include "spidma.h"
#if CONFIG_IDF_TARGET_ESP32S3
#include "esp_private/gdma.h"
#include "hal/spi_ll.h"
#include "hal/dma_types.h"
#include "soc/gdma_channel.h"
#include "esp_heap_caps.h"

static gdma_channel_handle_t s_chan = nullptr;
static dma_descriptor_t*     s_desc = nullptr;
static const int   SPIDMA_DESC  = 9;          /* 9 x 4092 = 36 КБ за один захід */
static const size_t SPIDMA_CHUNK = 4092;
static const size_t SPIDMA_MAX   = 32768;     /* межа довжини передачі в регістрі SPI */

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

static void spidmaOnce(const uint8_t* p, size_t len){
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
  uint32_t userSave = hw->user.val;
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
  while(!hw->dma_int_raw.trans_done) { }
  hw->dma_int_clr.trans_done = 1;
  spi_ll_dma_tx_enable(hw, false);
  hw->user.val = userSave;
}

void spidmaWrite(const void* buf, size_t len){
  if(!s_chan || !buf || !len) return;
  const uint8_t* p = (const uint8_t*)buf;
  while(len){                                    /* довше за межу — кількома заходами */
    size_t c = len > SPIDMA_MAX ? SPIDMA_MAX : len;
    spidmaOnce(p, c);
    p += c; len -= c;
  }
}
#else
bool spidmaBegin(){ return false; }
void spidmaWrite(const void* buf, size_t len){ (void)buf; (void)len; }
#endif
