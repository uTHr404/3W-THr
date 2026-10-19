/* www_sd.c - Carte SD */
#include "www.h"

/* ------------------------------ Carte SD ---------------------------- */
#if USE_FATFS
static FATFS fs;
static uint8_t sd_mounted = 0;

static void SD_Name(char *out)                    /* ex : 0:/261012.LOG (AAMMJJ.LOG, un seul fichier par jour) */
{
  RTC_TimeTypeDef t; RTC_DateTypeDef d;
  RTC_Read(&t, &d);
  snprintf(out, 24, "0:/%02d%02d%02d.LOG", d.Year, d.Month, d.Date);
}

void SD_Unmount(void)
{
  if (sd_mounted) { f_mount(NULL, "0:/", 0); sd_mounted = 0; }
}

extern volatile uint8_t sd_dbg_cmd0, sd_dbg_stage;
extern DSTATUS USER_initialize(BYTE pdrv);        /* pilote bas niveau (user_diskio.c) */

static const char *sd_step = "";                  /* derniere etape en echec (diagnostic) */
static int         sd_fr   = 0;

#define SD_FAIL(step, fr)  do { sd_step = (step); sd_fr = (int)(fr); sd_mounted = 0; return 2; } while (0)

/* 0 = OK, 1 = carte pleine, 2 = erreur d'acces / d'ecriture */
static int SD_LogOnce(const char *line, uint16_t len)
{
  FIL f; UINT bw; FRESULT r;
  char name[24];

  if (!sd_mounted)
  {
    FRESULT rm = FR_DISK_ERR;
    for (uint8_t i = 0; i < 6 && rm != FR_OK; i++)     /* 6 essais : tolere un contact incertain */
    {
      if (i) { USER_initialize(0); HAL_Delay(150 + 50 * i); }   /* delai croissant + reinit SPI a chaque essai */
      rm = f_mount(&fs, "0:/", 1);
    }
    if (rm != FR_OK)
    {
      uart_printf("[SD] f_mount failed: %d (CMD0=0x%02X stage=%d)\r\n", (int)rm, (unsigned)sd_dbg_cmd0, (int)sd_dbg_stage);
      SD_FAIL("f_mount", rm);
    }
    sd_mounted = 1;
    uart_printf("[SD] f_mount OK\r\n");
  }

  FATFS *pfs; DWORD fre;
  if (f_getfree("0:/", &fre, &pfs) == FR_OK)
  {
    uint64_t free_b = (uint64_t)fre * pfs->csize * 512ULL;
    if (free_b < (uint64_t)cfg.file_max_size + 1024ULL) return 1;
  }

  SD_Name(name);
  r = f_open(&f, name, FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
  if (r != FR_OK) SD_FAIL("f_open", r);

  if (f_size(&f) + len > cfg.file_max_size)      /* fichier du jour plein (> 2 Mo) : securite, on arrete d'ecrire */
  {
    f_close(&f);
    return 1;
  }

  f_lseek(&f, f_size(&f));
  r = f_write(&f, line, len, &bw);
  FRESULT rc = f_close(&f);
  if (r != FR_OK || rc != FR_OK || bw != len) SD_FAIL("f_write", (r != FR_OK) ? r : rc);
  return 0;
}

int SD_Log(const char *line, uint16_t len)
{
  int r = SD_LogOnce(line, len);
  if (r == 2)        /* panne passagere possible : on reinitialise la carte et on reessaie une fois */
  {
    uart_printf("[SD] echec (%s, code %d) : reinitialisation de la carte\r\n", sd_step, sd_fr);
    f_mount(NULL, "0:/", 0);
    sd_mounted = 0;
    USER_initialize(0);                          /* force la reinitialisation SPI de la carte */
    HAL_Delay(50);
    r = SD_LogOnce(line, len);
    if (r == 2) uart_printf("[SD] echec persistant (%s, code %d)\r\n", sd_step, sd_fr);
  }
  return r;
}
#else
void SD_Unmount(void) {}
int  SD_Log(const char *line, uint16_t len) { (void)line; (void)len; return 0; }
#endif

/* Diagnostic : affiche ce que la carte renvoie reellement (CS bas, puis reponse a CMD0).
   - tout a 00      : MISO colle a 0 (module non alimente, carte absente, fil MISO mal place)
   - tout a FF      : la carte ne repond pas (fil CS/SCK/MOSI, alimentation, carte)
   - un 01 present  : la carte repond, le montage doit fonctionner */
static void SD_RawProbe(void)
{
  uint8_t ff = 0xFF, rx = 0, i;
  const uint8_t cmd0[6] = { 0x40, 0, 0, 0, 0, 0x95 };

  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;   /* lent : < 400 kHz */
  HAL_SPI_Init(&hspi1);

  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
  for (i = 0; i < 10; i++) HAL_SPI_TransmitReceive(&hspi1, &ff, &rx, 1, 50);   /* >= 74 coups d'horloge */

  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET);
  uart_print("[SD] CS bas, 8 octets lus : ");
  for (i = 0; i < 8; i++) { HAL_SPI_TransmitReceive(&hspi1, &ff, &rx, 1, 50); uart_printf("%02X ", rx); }
  uart_print("\r\n");

  for (i = 0; i < 6; i++) HAL_SPI_TransmitReceive(&hspi1, (uint8_t *)&cmd0[i], &rx, 1, 50);
  uart_print("[SD] reponse a CMD0       : ");
  for (i = 0; i < 10; i++) { HAL_SPI_TransmitReceive(&hspi1, &ff, &rx, 1, 50); uart_printf("%02X ", rx); }
  uart_print("\r\n");
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
}

/* Test du fil MISO (PA6 / D12) : la broche est lue comme simple entree, avec pull-up puis pull-down.
   Resultat affiche : [MISO] CS haut : haut=x bas=y | CS bas : haut=x bas=y   (haut = avec pull-up, bas = avec pull-down)
   Le test decisif est "CS bas" (la carte pilote MISO) :
   - haut=1 bas=1 : la carte (ou le module) pilote la ligne a 1  -> MISO relie et carte vivante, c'est bon
   - haut=1 bas=0 : la ligne suit le pull : fil MISO debranche, module non alimente, ou carte absente
   - haut=0 bas=0 : ligne forcee a 0 : fil MISO en court-circuit vers GND, ou module defectueux */
static uint8_t SD_MisoRead(uint32_t pull)
{
  GPIO_InitTypeDef g = {0};
  g.Pin = GPIO_PIN_6; g.Mode = GPIO_MODE_INPUT; g.Pull = pull;
  HAL_GPIO_Init(GPIOA, &g);
  HAL_Delay(2);
  return (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == GPIO_PIN_SET) ? 1 : 0;
}

static void SD_MisoPinTest(void)
{
  GPIO_InitTypeDef g = {0};
  uint8_t hi_h, lo_h, hi_l, lo_l;

  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
  hi_h = SD_MisoRead(GPIO_PULLUP);  lo_h = SD_MisoRead(GPIO_PULLDOWN);
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET);
  hi_l = SD_MisoRead(GPIO_PULLUP);  lo_l = SD_MisoRead(GPIO_PULLDOWN);
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);

  g.Pin = GPIO_PIN_6; g.Mode = GPIO_MODE_AF_PP; g.Pull = GPIO_NOPULL;     /* remet PA6 en MISO du SPI1 */
  g.Speed = GPIO_SPEED_FREQ_VERY_HIGH; g.Alternate = GPIO_AF5_SPI1;
  HAL_GPIO_Init(GPIOA, &g);

  uart_printf("[MISO] CS haut : haut=%u bas=%u | CS bas : haut=%u bas=%u\r\n", hi_h, lo_h, hi_l, lo_l);
}

/* Auto-test SPI (CS haut) : envoie 0xA5 puis 0xFF et affiche ce qui revient sur MISO.
   - fil MISO(D12) relie a MOSI(D11), module SD debranche : on doit lire A5 puis FF
   - module SD branche : lecture attendue FF FF (ligne MISO libre) */
void SD_SpiSelfTest(void)
{
  uint8_t t1 = 0, t2 = 0, a = 0xA5, b = 0xFF;
  SD_MisoPinTest();
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
  HAL_SPI_TransmitReceive(&hspi1, &a, &t1, 1, 50);
  HAL_SPI_TransmitReceive(&hspi1, &b, &t2, 1, 50);
  uart_printf("[SPI] test: envoye A5 -> recu %02X ; envoye FF -> recu %02X\r\n", t1, t2);
  SD_RawProbe();
}