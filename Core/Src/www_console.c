/* www_console.c - Console */
#include "www.h"

static char     cmd_buf[CMD_MAX];
static uint8_t  cmd_len = 0;

/* ------------------------------ Console (config) -------------------- */
static void Cmd_Reply(int ok) { uart_print(ok ? "OK\r\n" : "ERREUR\r\n"); }

static int Cmd_Clock(const char *v)                 /* HH:MM:SS */
{
  int h, m, s;
  if (sscanf(v, "%d:%d:%d", &h, &m, &s) != 3 || h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59) return 0;
  RTC_TimeTypeDef t = {0};
  t.Hours = h; t.Minutes = m; t.Seconds = s;
  t.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  t.StoreOperation = RTC_STOREOPERATION_RESET;
  return HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN) == HAL_OK;
}

static int Cmd_Date(const char *v)                  /* MOIS,JOUR,ANNEE */
{
  int mo, dd, yy;
  if (sscanf(v, "%d,%d,%d", &mo, &dd, &yy) != 3 || mo < 1 || mo > 12 || dd < 1 || dd > 31 || yy < 2000 || yy > 2099) return 0;
  RTC_TimeTypeDef t; RTC_DateTypeDef d;
  RTC_Read(&t, &d);
  d.Month = mo; d.Date = dd; d.Year = yy - 2000;
  return HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN) == HAL_OK;
}

static int Cmd_Day(const char *v)                   /* MON..SUN */
{
  static const char *const days[7] = { "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN" };
  for (uint8_t i = 0; i < 7; i++)
  {
    if (strcmp(v, days[i]) != 0) continue;
    RTC_TimeTypeDef t; RTC_DateTypeDef d;
    RTC_Read(&t, &d);
    d.WeekDay = (uint8_t)(i + 1);
    return HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN) == HAL_OK;
  }
  return 0;
}

static void Params_Print(void)
{
  for (size_t i = 0; i < N_PARAMS; i++)
  {
    if (strcmp(params[i].name, "LOG_INTERVALL") == 0) continue;     /* alias */
    uart_printf("%s=%ld\r\n", params[i].name, Param_Get(&params[i]));
  }
}

static void Help_Print(void)
{
  uart_print("Commandes : VERSION, RESET, PARAMS, HELP\r\n"
             "  LOG_INTERVAL=n  FILE_MAX_SIZE=n  TIMEOUT=n\r\n"
             "  LUMIN=0|1  LUMIN_LOW=n  LUMIN_HIGH=n\r\n"
             "  TEMP_AIR=0|1  MIN_TEMP_AIR=n  MAX_TEMP_AIR=n\r\n"
             "  HYGR=0|1  HYGR_MINT=n  HYGR_MAXT=n\r\n"
             "  PRESSURE=0|1  PRESSURE_MIN=n  PRESSURE_MAX=n\r\n"
             "  CLOCK=HH:MM:SS  DATE=MM,JJ,AAAA  DAY=MON..SUN\r\n");
}

static void Command_Exec(char *s)
{
  char *p;
  for (p = s; *p; p++) *p = (char)toupper((unsigned char)*p);
  while (p > s && p[-1] == ' ') *--p = '\0';

  char *val = strchr(s, '=');
  if (val) *val++ = '\0';

  if      (!strcmp(s, "VERSION")) uart_printf("%s - %s\r\n", FW_VERSION, LOT_NUMBER);
  else if (!strcmp(s, "RESET"))   { Config_Default(); Cmd_Reply(Config_Save() == HAL_OK); }
  else if (!strcmp(s, "PARAMS"))  Params_Print();
  else if (!strcmp(s, "HELP"))    Help_Print();
  else if (val && !strcmp(s, "CLOCK")) Cmd_Reply(Cmd_Clock(val));
  else if (val && !strcmp(s, "DATE"))  Cmd_Reply(Cmd_Date(val));
  else if (val && !strcmp(s, "DAY"))   Cmd_Reply(Cmd_Day(val));
  else if (val)
  {
    int r = Param_Set(s, val);
    if (r < 0)       uart_print("Commande inconnue (HELP)\r\n");
    else if (r == 0) uart_print("ERREUR : valeur invalide\r\n");
    else             Cmd_Reply(Config_Save() == HAL_OK);
  }
  else if (*s) uart_print("Commande inconnue (HELP)\r\n");
}

/* Lit la console (USART2) sans bloquer ; n'execute les commandes qu'en mode configuration */
void Console_Feed(void)
{
  while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE))
  {
    char c = (char)(huart2.Instance->RDR & 0xFF);
    if (mode != MODE_CONFIG) continue;

    cfg_activity = HAL_GetTick();
    if (c == '\r' || c == '\n')
    {
      uart_print("\r\n");
      if (cmd_len) { cmd_buf[cmd_len] = '\0'; cmd_len = 0; Command_Exec(cmd_buf); }
    }
    else if (c == 0x08 || c == 0x7F) { if (cmd_len) cmd_len--; }
    else if (cmd_len < CMD_MAX - 1)   { cmd_buf[cmd_len++] = c; HAL_UART_Transmit(&huart2, (uint8_t *)&c, 1, 10); }
  }
  if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE)) __HAL_UART_CLEAR_OREFLAG(&huart2);
}