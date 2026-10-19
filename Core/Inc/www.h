/* www.h - Worldwide Weather Watcher : types, constantes et prototypes communs */
#ifndef WWW_H
#define WWW_H

#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>

/* FatFS est utilise automatiquement s'il est genere par CubeMX (fatfs.h present) */
#if defined(__has_include)
# if __has_include("fatfs.h")
#  include "fatfs.h"
#  define USE_FATFS 1
# endif
#endif
#ifndef USE_FATFS
# define USE_FATFS 0
#endif

/* ---------------------------- Constantes ---------------------------- */
#define FW_VERSION       "WWW-1.0"
#define LOT_NUMBER       "LOT-0001"

#define LOG_UNIT_MS      60000UL   /* 60000 : LOG_INTERVAL en minutes. Mettre 1000UL pour tester en secondes */
#define LONG_PRESS_MS    5000UL
#define CONFIG_IDLE_MS   (30UL * 60UL * 1000UL)
#define MAINT_PERIOD_MS  2000UL
#define ECO_RED_TO_MAINT 1         /* 0 : en eco, rouge 5 s -> standard (sujet) ; 1 : -> maintenance */

#define USE_GPS          0         /* 0 : GPS desactive (pas d'attente, positions = NA) ; 1 : GPS actif */

#define LED_COMMON_ANODE 1         /* 1 : LED RGB a anode commune (allumee = broche a 0) */

#define BH1750_ADDR      (0x23 << 1)

#define LCD_ADDR_1       (0x27 << 1)
#define LCD_ADDR_2       (0x3F << 1)
#define LCD_BL           0x08
#define LCD_EN           0x04
#define LCD_RS           0x01

#define RTC_MAGIC        0x32F2
#define CFG_MAGIC        0x57575702UL
#define CFG_FLASH_ADDR   0x08040000UL   /* page 128 : valable en simple ou double banque */
#define CFG_FLASH_PAGE   128U

#define GPS_LINE_MAX     100U
#define CMD_MAX          64U

#define E_SENSOR         0x01
#define E_INCOH          0x02
#define E_GPS            0x04

/* ------------------------------ Types ------------------------------- */
typedef enum { MODE_STANDARD = 0, MODE_CONFIG, MODE_ECO, MODE_MAINT } Mode_t;

typedef enum { ERR_NONE = 0, ERR_RTC, ERR_GPS, ERR_SENSOR,
               ERR_INCOHERENT, ERR_SD_FULL, ERR_SD_IO } Err_t;

/* Parametres du systeme (sauvegardes en flash, page 128) */
typedef struct {
  uint32_t magic;
  uint32_t log_interval;      /* LOG_INTERVAL  (minutes)            */
  uint32_t file_max_size;     /* FILE_MAX_SIZE (octets)             */
  uint32_t timeout_s;         /* TIMEOUT       (secondes)           */
  int16_t  min_temp, max_temp;
  int16_t  hygr_mint, hygr_maxt;
  int16_t  pressure_min, pressure_max;
  uint16_t lumin_low, lumin_high;
  uint8_t  lumin_en, temp_en, hygr_en, press_en;
  uint32_t reserved;
} Config_t;
_Static_assert(sizeof(Config_t) % 8 == 0, "Config_t doit etre multiple de 8 octets");

enum { T_U8, T_U16, T_U32, T_I16 };
typedef struct { const char *name; void *ptr; uint8_t type; int32_t min, max; } Param_t;

/* Une ligne de mesures */
typedef struct {
  uint8_t  temp_ok, hum_ok, lux_ok, gps_ok;
  int16_t  temp;
  uint8_t  hum;
  uint32_t lux;
  int32_t  lat, lon;          /* micro-degres */
} Meas_t;

typedef struct { GPIO_TypeDef *port; uint16_t pin; uint32_t t0; uint8_t down, done; } Btn_t;
typedef struct { uint8_t valid; uint32_t tick; int32_t lat, lon; } Gps_t;

/* ------------------- Peripheriques HAL (definis dans main.c) --------- */
extern I2C_HandleTypeDef  hi2c1;
extern RTC_HandleTypeDef  hrtc;
extern SPI_HandleTypeDef  hspi1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;

/* --------------------- Variables partagees entre modules ------------- */
extern Config_t        cfg;                 /* www_config.c  */
extern const Param_t   params[];            /* www_config.c  */
extern const unsigned  params_count;
#define N_PARAMS       params_count
extern Mode_t          mode;                /* www_modes.c   */
extern const char *const mode_name[];
extern uint32_t        next_log, cfg_activity, last_maint;
extern Err_t           err;                 /* www_led.c     */
extern Gps_t           gps;                 /* www_gps.c     */

/* --------------------------- Prototypes ------------------------------ */
/* UART / outils */
void USART2_Init_Manual(void);
void uart_print(const char *s);
void uart_printf(const char *fmt, ...);
void delay_us(uint32_t us);

/* LED RGB */
void LED_Update(void);

/* DHT11 */
int DHT11_Read(uint8_t *hum, uint8_t *temp);

/* BH1750 */
HAL_StatusTypeDef BH1750_Init(void);
HAL_StatusTypeDef BH1750_ReadLux(float *lux);

/* RTC */
HAL_StatusTypeDef RTC_Read(RTC_TimeTypeDef *t, RTC_DateTypeDef *d);
void RTC_SetFromBuild(void);
uint8_t RTC_Stalled(void);

/* LCD 16x2 */
void LCD_Begin(void);
void LCD_ShowMode(void);
void LCD_ShowMeas(const Meas_t *m);

/* Carte SD */
void SD_Unmount(void);
int SD_Log(const char *line, uint16_t len);

/* GPS */
void GPS_Feed(void);

/* Configuration (flash) */
void Config_Default(void);
void Config_Load(void);
HAL_StatusTypeDef Config_Save(void);
long Param_Get(const Param_t *p);
int Param_Set(const char *name, const char *val);

/* Console */
void Console_Feed(void);

/* Modes et boutons */
void Mode_Announce(void);
void Mode_Set(Mode_t m);
void Background_Tasks(void);
void Wait_ms(uint32_t ms);

/* Mesures */
void Log_Cycle(void);
void Maintenance_Measure(void);

void SD_SpiSelfTest(void);
void App_Init(void);
void App_Loop(void);

#endif /* WWW_H */