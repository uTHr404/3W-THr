/* www_bh1750.c - BH1750 */
#include "www.h"

/* ------------------------------ BH1750 (I2C1) ----------------------- */
HAL_StatusTypeDef BH1750_Init(void)
{
  uint8_t cmd = 0x01;              /* Power on */
  if (HAL_I2C_Master_Transmit(&hi2c1, BH1750_ADDR, &cmd, 1, 100) != HAL_OK)
    return HAL_ERROR;
  cmd = 0x10;                      /* Mesure continue, haute resolution */
  return HAL_I2C_Master_Transmit(&hi2c1, BH1750_ADDR, &cmd, 1, 100);
}

HAL_StatusTypeDef BH1750_ReadLux(float *lux)
{
  uint8_t buf[2];
  if (HAL_I2C_Master_Receive(&hi2c1, BH1750_ADDR, buf, 2, 100) != HAL_OK)
    return HAL_ERROR;
  *lux = ((buf[0] << 8) | buf[1]) / 1.2f;
  return HAL_OK;
}