/* www_modes.c - Modes et boutons */
#include "www.h"

Mode_t   mode = MODE_STANDARD;
static Mode_t prev_mode = MODE_STANDARD;
uint32_t next_log = 0, cfg_activity = 0, last_maint = 0;

const char *const mode_name[] = { "STANDARD", "CONFIGURATION", "ECONOMIQUE", "MAINTENANCE" };

static Btn_t btn_red   = { GPIOC, GPIO_PIN_13, 0, 0, 0 };   /* bouton rouge = B1 (PC13) */
static Btn_t btn_green = { GPIOC, GPIO_PIN_3,  0, 0, 0 };   /* bouton vert  = PC3 externe */

/* ------------------------- Modes de fonctionnement ------------------ */
void Mode_Announce(void)
{
  uart_printf("[MODE] %s\r\n", mode_name[mode]);
  if (mode == MODE_CONFIG) uart_print("Console de configuration active : tapez HELP\r\n");
  if (mode == MODE_MAINT)  uart_print("Carte SD demontee : retrait sans risque\r\n");
  LCD_ShowMode();
}

void Mode_Set(Mode_t m)
{
  if (m == mode) return;
  if (m == MODE_MAINT) { prev_mode = mode; SD_Unmount(); }
  mode = m;
  cfg_activity = HAL_GetTick();
  next_log = HAL_GetTick();
  last_maint = 0;
  Mode_Announce();
}

/* 1 une seule fois quand le bouton est maintenu LONG_PRESS_MS (actif a l'etat bas) */
static uint8_t Btn_LongPress(Btn_t *b)
{
  uint32_t now = HAL_GetTick();
  if (HAL_GPIO_ReadPin(b->port, b->pin) == GPIO_PIN_RESET)
  {
    if (!b->down) { b->down = 1; b->t0 = now; b->done = 0; }
    else if (!b->done && (now - b->t0) >= LONG_PRESS_MS) { b->done = 1; return 1; }
  }
  else b->down = 0;
  return 0;
}

static void Buttons_Poll(void)
{
  uint8_t red = Btn_LongPress(&btn_red);
  uint8_t green = Btn_LongPress(&btn_green);

  if (red || green) cfg_activity = HAL_GetTick();
  if (mode == MODE_CONFIG) return;

  if (red)
  {
    if (mode == MODE_MAINT)                      Mode_Set(prev_mode);
    else if (mode == MODE_ECO && !ECO_RED_TO_MAINT) Mode_Set(MODE_STANDARD);
    else                                         Mode_Set(MODE_MAINT);
  }
  else if (green && mode == MODE_STANDARD) Mode_Set(MODE_ECO);
}

/* Taches de fond : a appeler dans toute attente */
void Background_Tasks(void)
{
  LED_Update();
  GPS_Feed();
  Console_Feed();
  Buttons_Poll();
}

void Wait_ms(uint32_t ms)
{
  uint32_t t0 = HAL_GetTick();
  Mode_t m0 = mode;
  while ((HAL_GetTick() - t0) < ms && mode == m0) Background_Tasks();
}