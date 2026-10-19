/* www_gps.c - GPS */
#include "www.h"

static char     gps_line[GPS_LINE_MAX];
static uint8_t  gps_len = 0;
Gps_t gps;

/* -------------------------------- GPS (USART3) ---------------------- */
static uint8_t nmea_checksum_ok(const char *s)    /* s commence par '$' */
{
  uint8_t cs = 0;
  const char *p = s + 1;
  while (*p && *p != '*') cs ^= (uint8_t)*p++;
  if (*p != '*') return 0;
  return cs == (uint8_t)strtoul(p + 1, NULL, 16);
}

/* "ddmm.mmmm" ou "dddmm.mmmm" -> micro-degres (calcul entier, sans float) */
static int32_t nmea_to_udeg(const char *s, char hemi)
{
  const char *dot = strchr(s, '.');
  if (!dot || (dot - s) < 3 || (dot - s) > 5) return INT32_MIN;
  int dlen = (int)(dot - s) - 2;
  int32_t deg = 0;
  for (int i = 0; i < dlen; i++)
  {
    if (s[i] < '0' || s[i] > '9') return INT32_MIN;
    deg = deg * 10 + (s[i] - '0');
  }
  if (s[dlen] < '0' || s[dlen] > '9' || s[dlen + 1] < '0' || s[dlen + 1] > '9') return INT32_MIN;
  int32_t min_e5 = ((s[dlen] - '0') * 10 + (s[dlen + 1] - '0')) * 100000L;
  int32_t mult = 10000;
  for (int i = 1; i <= 5 && dot[i] >= '0' && dot[i] <= '9'; i++)
  {
    min_e5 += (dot[i] - '0') * mult;
    mult /= 10;
  }
  int32_t u = deg * 1000000L + min_e5 / 6;
  return (hemi == 'S' || hemi == 'W') ? -u : u;
}

/* $GPRMC,hhmmss,A,lat,N,lon,E,... */
static void GPS_ParseLine(char *s)
{
  if (s[0] != '$' || !nmea_checksum_ok(s)) return;
  if (strncmp(s + 3, "RMC", 3) != 0) return;

  char *f[8]; uint8_t n = 0;
  f[n++] = s;
  for (char *p = s; *p && n < 8; p++)
    if (*p == ',') { *p = '\0'; f[n++] = p + 1; }
  if (n < 7) return;

  if (f[2][0] != 'A') { gps.valid = 0; return; }            /* pas de fix */
  int32_t la = nmea_to_udeg(f[3], f[4][0]);
  int32_t lo = nmea_to_udeg(f[5], f[6][0]);
  if (la == INT32_MIN || lo == INT32_MIN) return;
  gps.lat = la; gps.lon = lo; gps.tick = HAL_GetTick(); gps.valid = 1;
}

/* Lit les octets recus sans bloquer (appele en continu) */
void GPS_Feed(void)
{
  while (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE))
  {
    char c = (char)(huart3.Instance->RDR & 0xFF);
    if (c == '$') gps_len = 0;
    if (c == '\n' || c == '\r')
    {
      if (gps_len > 6) { gps_line[gps_len] = '\0'; GPS_ParseLine(gps_line); }
      gps_len = 0;
    }
    else if (gps_len < GPS_LINE_MAX - 1) gps_line[gps_len++] = c;
    else gps_len = 0;
  }
  if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_ORE)) __HAL_UART_CLEAR_OREFLAG(&huart3);
}