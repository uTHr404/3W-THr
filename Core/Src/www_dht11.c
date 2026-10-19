/* www_dht11.c - DHT11 */
#include "www.h"

/* ------------------------------ DHT11 (PB0) ------------------------- */
static void DHT_PinOutput(void)
{
  GPIO_InitTypeDef g = {0};
  g.Pin = GPIO_PIN_0;
  g.Mode = GPIO_MODE_OUTPUT_OD;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &g);
}

static void DHT_PinInput(void)
{
  GPIO_InitTypeDef g = {0};
  g.Pin = GPIO_PIN_0;
  g.Mode = GPIO_MODE_INPUT;
  g.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &g);
}

static int wait_pin(GPIO_PinState state, uint32_t timeout_us)
{
  while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) != state)
  {
    if (timeout_us-- == 0) return 0;
    delay_us(1);
  }
  return 1;
}

static int DHT11_Bits(uint8_t d[5])
{
  DHT_PinInput();
  if (!wait_pin(GPIO_PIN_RESET, 100)) return -1;
  if (!wait_pin(GPIO_PIN_SET,   100)) return -1;
  if (!wait_pin(GPIO_PIN_RESET, 100)) return -1;

  for (int i = 0; i < 40; i++)
  {
    if (!wait_pin(GPIO_PIN_SET, 100)) return -1;
    delay_us(40);
    d[i / 8] <<= 1;
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_SET) d[i / 8] |= 1;
    if (i < 39 && !wait_pin(GPIO_PIN_RESET, 100)) return -1;
  }
  return 0;
}

/* 0 = OK, -1 = pas de reponse, -2 = checksum faux */
int DHT11_Read(uint8_t *hum, uint8_t *temp)
{
  uint8_t d[5] = {0};

  DHT_PinOutput();
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
  HAL_Delay(20);                   /* start : > 18 ms a l'etat bas */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
  delay_us(30);

  uint32_t pm = __get_PRIMASK();
  __disable_irq();                 /* timing critique : pas d'interruption */
  int r = DHT11_Bits(d);
  __set_PRIMASK(pm);

  if (r != 0) return r;
  if ((uint8_t)(d[0] + d[1] + d[2] + d[3]) != d[4]) return -2;

  *hum  = d[0];
  *temp = d[2];
  return 0;
}