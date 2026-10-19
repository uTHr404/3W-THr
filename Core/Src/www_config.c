/* www_config.c - Configuration (flash) */
#include "www.h"

Config_t cfg __attribute__((aligned(8)));

/* ---------------------------- Configuration (flash) ----------------- */
void Config_Default(void)
{
  memset(&cfg, 0, sizeof(cfg));
  cfg.magic = CFG_MAGIC;
  cfg.log_interval = 10;
  cfg.file_max_size = 2097152;
  cfg.timeout_s = 30;
  cfg.lumin_en = 1;  cfg.lumin_low = 255;  cfg.lumin_high = 768;
  cfg.temp_en = 1;   cfg.min_temp = -10;   cfg.max_temp = 60;
  cfg.hygr_en = 1;   cfg.hygr_mint = 0;    cfg.hygr_maxt = 50;
  cfg.press_en = 1;  cfg.pressure_min = 850; cfg.pressure_max = 1080;
}

void Config_Load(void)
{
  const Config_t *f = (const Config_t *)CFG_FLASH_ADDR;
  if (f->magic == CFG_MAGIC) cfg = *f; else Config_Default();
}

HAL_StatusTypeDef Config_Save(void)
{
  FLASH_EraseInitTypeDef ei;
  uint32_t page_err = 0;
  ei.TypeErase = FLASH_TYPEERASE_PAGES;
  ei.Banks = FLASH_BANK_1;
  ei.Page = CFG_FLASH_PAGE;
  ei.NbPages = 1;
  cfg.magic = CFG_MAGIC;

  HAL_FLASH_Unlock();
  HAL_StatusTypeDef st = HAL_FLASHEx_Erase(&ei, &page_err);
  if (st == HAL_OK)
  {
    const uint64_t *src = (const uint64_t *)&cfg;
    for (uint32_t i = 0; i < sizeof(cfg) / 8U && st == HAL_OK; i++)
      st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, CFG_FLASH_ADDR + 8U * i, src[i]);
  }
  HAL_FLASH_Lock();
  return st;
}

const Param_t params[] = {
  { "LOG_INTERVAL",  &cfg.log_interval,  T_U32, 1, 1440 },
  { "LOG_INTERVALL", &cfg.log_interval,  T_U32, 1, 1440 },
  { "FILE_MAX_SIZE", &cfg.file_max_size, T_U32, 128, 2097152 },
  { "TIMEOUT",       &cfg.timeout_s,     T_U32, 1, 600 },
  { "LUMIN",         &cfg.lumin_en,      T_U8,  0, 1 },
  { "LUMIN_LOW",     &cfg.lumin_low,     T_U16, 0, 1023 },
  { "LUMIN_HIGH",    &cfg.lumin_high,    T_U16, 0, 1023 },
  { "TEMP_AIR",      &cfg.temp_en,       T_U8,  0, 1 },
  { "MIN_TEMP_AIR",  &cfg.min_temp,      T_I16, -40, 85 },
  { "MAX_TEMP_AIR",  &cfg.max_temp,      T_I16, -40, 85 },
  { "HYGR",          &cfg.hygr_en,       T_U8,  0, 1 },
  { "HYGR_MINT",     &cfg.hygr_mint,     T_I16, -40, 85 },
  { "HYGR_MAXT",     &cfg.hygr_maxt,     T_I16, -40, 85 },
  { "PRESSURE",      &cfg.press_en,      T_U8,  0, 1 },
  { "PRESSURE_MIN",  &cfg.pressure_min,  T_I16, 300, 1100 },
  { "PRESSURE_MAX",  &cfg.pressure_max,  T_I16, 300, 1100 },
};
const unsigned params_count = sizeof(params) / sizeof(params[0]);

long Param_Get(const Param_t *p)
{
  switch (p->type)
  {
    case T_U8:  return *(const uint8_t  *)p->ptr;
    case T_U16: return *(const uint16_t *)p->ptr;
    case T_U32: return (long)*(const uint32_t *)p->ptr;
    default:    return *(const int16_t  *)p->ptr;
  }
}

/* 1 = OK, 0 = valeur invalide, -1 = parametre inconnu */
int Param_Set(const char *name, const char *val)
{
  for (size_t i = 0; i < params_count; i++)
  {
    if (strcmp(name, params[i].name) != 0) continue;
    char *end;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') return 0;
    if (v < params[i].min || v > params[i].max) return 0;
    switch (params[i].type)
    {
      case T_U8:  *(uint8_t  *)params[i].ptr = (uint8_t)v;  break;
      case T_U16: *(uint16_t *)params[i].ptr = (uint16_t)v; break;
      case T_U32: *(uint32_t *)params[i].ptr = (uint32_t)v; break;
      default:    *(int16_t  *)params[i].ptr = (int16_t)v;  break;
    }
    return 1;
  }
  return -1;
}