/* www_rtc.c - RTC */
#include "www.h"

/* ------------------------------ RTC interne ------------------------- */
HAL_StatusTypeDef RTC_Read(RTC_TimeTypeDef *t, RTC_DateTypeDef *d)
{
  HAL_StatusTypeDef a = HAL_RTC_GetTime(&hrtc, t, RTC_FORMAT_BIN);   /* heure AVANT date */
  HAL_StatusTypeDef b = HAL_RTC_GetDate(&hrtc, d, RTC_FORMAT_BIN);
  return (a == HAL_OK && b == HAL_OK) ? HAL_OK : HAL_ERROR;
}

/* Regle le RTC avec la date/heure de compilation, une seule fois */
void RTC_SetFromBuild(void)
{
  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == RTC_MAGIC) return;

  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mon[4] = {0};
  int day = 1, year = 2026, h = 0, m = 0, sec = 0;

  sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
  sscanf(__TIME__, "%d:%d:%d", &h, &m, &sec);

  const char *p = strstr(months, mon);
  int month = p ? (int)((p - months) / 3) + 1 : 1;

  RTC_TimeTypeDef t = {0};
  RTC_DateTypeDef d = {0};
  t.Hours = h; t.Minutes = m; t.Seconds = sec;
  t.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  t.StoreOperation = RTC_STOREOPERATION_RESET;
  d.WeekDay = RTC_WEEKDAY_MONDAY;
  d.Month = month; d.Date = day; d.Year = year - 2000;

  HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN);
  HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN);
  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, RTC_MAGIC);
}

/* 1 si le RTC ne repond pas ou ne compte plus (quartz LSE absent) */
uint8_t RTC_Stalled(void)
{
  static uint32_t last_s = 0xFFFFFFFFUL, last_tick = 0;
  RTC_TimeTypeDef t; RTC_DateTypeDef d;

  if (RTC_Read(&t, &d) != HAL_OK) return 1;
  uint32_t s = t.Hours * 3600UL + t.Minutes * 60UL + t.Seconds;
  uint32_t now = HAL_GetTick();
  uint8_t stalled = (last_s != 0xFFFFFFFFUL) && (s == last_s) && ((now - last_tick) >= 1500UL);
  last_s = s; last_tick = now;
  return stalled;
}