/* www_lcd.c - LCD 16x2 */
#include "www.h"

static uint8_t lcd_ok = 0;
static uint8_t lcd_addr = LCD_ADDR_1;
static const char *const mode_short[] = { "STD", "CFG", "ECO", "MNT" };

/* ------------------------- LCD 16x2 I2C (optionnel) ----------------- */
static uint8_t LCD_Detect(void)
{
  if (HAL_I2C_IsDeviceReady(&hi2c1, LCD_ADDR_1, 2, 20) == HAL_OK) { lcd_addr = LCD_ADDR_1; return 1; }
  if (HAL_I2C_IsDeviceReady(&hi2c1, LCD_ADDR_2, 2, 20) == HAL_OK) { lcd_addr = LCD_ADDR_2; return 1; }
  return 0;
}

static void LCD_Write4(uint8_t nibble, uint8_t rs)
{
  uint8_t d = (nibble & 0xF0) | LCD_BL | rs;
  uint8_t t[2] = { (uint8_t)(d | LCD_EN), d };
  HAL_I2C_Master_Transmit(&hi2c1, lcd_addr, t, 2, 100);
  delay_us(50);
}

static void LCD_Send(uint8_t v, uint8_t rs)
{
  LCD_Write4(v & 0xF0, rs);
  LCD_Write4((uint8_t)(v << 4), rs);
}

static void LCD_Cmd(uint8_t c)  { LCD_Send(c, 0); }
static void LCD_Data(uint8_t c) { LCD_Send(c, LCD_RS); }

static void LCD_Init(void)
{
  HAL_Delay(50);
  LCD_Write4(0x30, 0); HAL_Delay(5);
  LCD_Write4(0x30, 0); HAL_Delay(1);
  LCD_Write4(0x30, 0); HAL_Delay(1);
  LCD_Write4(0x20, 0);
  LCD_Cmd(0x28);
  LCD_Cmd(0x0C);
  LCD_Cmd(0x06);
  LCD_Cmd(0x01); HAL_Delay(2);
}

/* Ecrit exactement 16 caracteres (complete avec des espaces) */
static void LCD_PrintLine(uint8_t row, const char *s)
{
  if (!lcd_ok) return;
  LCD_Cmd((uint8_t)(0x80 | (row ? 0x40 : 0x00)));
  for (uint8_t i = 0; i < 16; i++)
    LCD_Data((*s) ? (uint8_t)*s++ : ' ');
}

void LCD_Begin(void)
{
  lcd_ok = LCD_Detect();
  if (lcd_ok)
  {
    LCD_Init();
    LCD_PrintLine(0, "WWW Station");
    LCD_PrintLine(1, FW_VERSION);
    uart_printf("LCD detecte a 0x%02X\r\n", lcd_addr >> 1);
  }
  else
    uart_print("LCD absent (ignore)\r\n");
}

void LCD_ShowMode(void)
{
  LCD_PrintLine(0, mode_name[mode]);
  LCD_PrintLine(1, "");
}

void LCD_ShowMeas(const Meas_t *m)
{
  if (!lcd_ok) return;
  char t[8], h[8], l[12], l1[24], l2[24];
  if (m->temp_ok) snprintf(t, sizeof(t), "%dC", m->temp);        else strcpy(t, "--");
  if (m->hum_ok)  snprintf(h, sizeof(h), "%d", m->hum);          else strcpy(h, "--");
  if (m->lux_ok)  snprintf(l, sizeof(l), "%lu", (unsigned long)m->lux); else strcpy(l, "--");
  snprintf(l1, sizeof(l1), "%s %s %s%%", mode_short[mode], t, h);
  snprintf(l2, sizeof(l2), "%slx %s", l, m->gps_ok ? "GPS" : "noGPS");
  LCD_PrintLine(0, l1);
  LCD_PrintLine(1, l2);
}