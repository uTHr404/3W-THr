/* www_led.c - LED RGB */
#include "www.h"

Err_t err = ERR_NONE;

/* ------------------------------ LED RGB ----------------------------- */
enum { C_OFF = 0, C_RED = 1, C_GREEN = 2, C_BLUE = 4, C_YELLOW = 3, C_WHITE = 7 };

static uint8_t           led_last   = 0xFF;   /* derniere couleur ecrite (0xFF = a reecrire) */
static volatile uint8_t  led_orange = 0;      /* 1 = l'orange est genere par l'interruption SysTick */

/* Remplace la version "weak" du HAL : en plus de compter les ms, genere l'orange du mode
   maintenance (rouge fixe + vert 1 ms sur 4). Ainsi l'orange reste stable meme si la boucle
   principale est bloquee (lecture DHT11, I2C...). */
void HAL_IncTick(void)
{
  uwTick += uwTickFreq;
  if (led_orange)
  {
    const GPIO_PinState on  = LED_COMMON_ANODE ? GPIO_PIN_RESET : GPIO_PIN_SET;
    const GPIO_PinState off = LED_COMMON_ANODE ? GPIO_PIN_SET   : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, on);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, ((uwTick & 3U) == 0U) ? on : off);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, off);
  }
}

static void LED_Write(uint8_t c)
{
  if (c == led_last) return;                   /* n'ecrit que si ca change */
  led_last = c;
  const GPIO_PinState on  = LED_COMMON_ANODE ? GPIO_PIN_RESET : GPIO_PIN_SET;
  const GPIO_PinState off = LED_COMMON_ANODE ? GPIO_PIN_SET   : GPIO_PIN_RESET;
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, (c & C_RED)   ? on : off);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, (c & C_GREEN) ? on : off);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, (c & C_BLUE)  ? on : off);
}

/* Couleur du mode, ou clignotement 1 Hz de l'erreur (cf. tableau du sujet) */
void LED_Update(void)
{
  uint32_t now = HAL_GetTick();
  uint8_t c;

  if (err == ERR_NONE && mode == MODE_MAINT)   /* orange stable : gere par HAL_IncTick */
  {
    led_orange = 1;
    return;
  }
  if (led_orange) { led_orange = 0; led_last = 0xFF; }   /* on quitte l'orange : forcer la reecriture */

  if (err != ERR_NONE)
  {
    uint8_t a = C_RED, b = C_BLUE;
    uint32_t split = 500;                      /* ms de la 1re couleur dans la seconde */
    switch (err)
    {
      case ERR_RTC:        b = C_BLUE;                  break;
      case ERR_GPS:        b = C_YELLOW;                break;
      case ERR_SENSOR:     b = C_GREEN;                 break;
      case ERR_INCOHERENT: b = C_GREEN;  split = 333;   break;   /* vert 2x plus long */
      case ERR_SD_FULL:    b = C_WHITE;                 break;
      default:             b = C_WHITE;  split = 333;   break;   /* ERR_SD_IO : blanc 2x plus long */
    }
    c = ((now % 1000UL) < split) ? a : b;
  }
  else
  {
    switch (mode)
    {
      case MODE_STANDARD: c = C_GREEN;  break;
      case MODE_CONFIG:   c = C_YELLOW; break;
      case MODE_ECO:      c = C_BLUE;   break;
      default:            c = C_RED | (((now & 3U) == 0U) ? C_GREEN : 0);   /* orange (PWM logiciel) */
    }
  }
  LED_Write(c);
}