/* www_app.c - initialisation et boucle principale de l'application */
#include "www.h"

void App_Init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  USART2_Init_Manual();            /* sortie vers le PC (COM du ST-LINK) */
  Config_Load();                   /* parametres sauvegardes en flash */
  RTC_SetFromBuild();              /* regle l'heure une seule fois */

  /* Bouton rouge (PC13) presse au demarrage -> mode configuration */
  HAL_Delay(50);
  if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) mode = MODE_CONFIG;

  uart_printf("\r\n=== Worldwide Weather Watcher | %s | %s ===\r\n", FW_VERSION, LOT_NUMBER);

  if (BH1750_Init() != HAL_OK) uart_print("BH1750 non detecte\r\n");
  HAL_Delay(180);
  LCD_Begin();

#if USE_FATFS
  MX_FATFS_Init();                 /* a retirer si CubeMX l'appelle deja plus haut */
  uart_print("FatFS actif\r\n");
  SD_SpiSelfTest();
#else
  uart_print("FatFS absent : mesures affichees sur le port serie, pas d'ecriture SD\r\n");
#endif

  cfg_activity = HAL_GetTick();
  next_log = HAL_GetTick();        /* premiere mesure immediate */
  Mode_Announce();
}

void App_Loop(void)
{
  Background_Tasks();

  switch (mode)
  {
    case MODE_CONFIG:
      if ((HAL_GetTick() - cfg_activity) >= CONFIG_IDLE_MS)
      {
        uart_print("Inactivite 30 min : retour en mode standard\r\n");
        Mode_Set(MODE_STANDARD);
      }
      break;

    case MODE_MAINT:
      if ((HAL_GetTick() - last_maint) >= MAINT_PERIOD_MS)
      {
        last_maint = HAL_GetTick();
        Maintenance_Measure();
      }
      break;

    default:                       /* STANDARD et ECONOMIQUE */
      if ((int32_t)(HAL_GetTick() - next_log) >= 0) Log_Cycle();
      break;
  }
}