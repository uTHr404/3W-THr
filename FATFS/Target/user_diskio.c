/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * @file    user_diskio.c
  * @brief   Pilote diskio FatFS pour carte SD en mode SPI (SPI1, CS = SD_CS)
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
 /* USER CODE END Header */

#ifdef USE_OBSOLETE_USER_CODE_SECTION_0
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */
#endif

/* USER CODE BEGIN DECL */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "ff_gen_drv.h"
#include "main.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
extern SPI_HandleTypeDef hspi1;

#define SD_CS_LOW()    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET)
#define SD_CS_HIGH()   HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET)

/* Vitesse SPI : SPI1 sur APB2 = 80 MHz */
#define SD_SPEED_INIT  SPI_BAUDRATEPRESCALER_256   /* 312,5 kHz (< 400 kHz exige pour l'init) */
#define SD_SPEED_RUN   SPI_BAUDRATEPRESCALER_128   /* 625 kHz (80 MHz / 128) : lent et sur avec fils volants */

/* Commandes SD (mode SPI) */
#define CMD0    (0)           /* GO_IDLE_STATE */
#define CMD8    (8)           /* SEND_IF_COND */
#define CMD9    (9)           /* SEND_CSD */
#define CMD12   (12)          /* STOP_TRANSMISSION */
#define CMD16   (16)          /* SET_BLOCKLEN */
#define CMD17   (17)          /* READ_SINGLE_BLOCK */
#define CMD18   (18)          /* READ_MULTIPLE_BLOCK */
#define CMD24   (24)          /* WRITE_BLOCK */
#define CMD25   (25)          /* WRITE_MULTIPLE_BLOCK */
#define CMD55   (55)          /* APP_CMD */
#define CMD58   (58)          /* READ_OCR */
#define ACMD23  (0x80 + 23)   /* SET_WR_BLK_ERASE_COUNT */
#define ACMD41  (0x80 + 41)   /* SD_SEND_OP_COND */

/* Types de carte */
#define CT_SD1    0x01
#define CT_SD2    0x02
#define CT_BLOCK  0x08        /* adressage par blocs (SDHC / SDXC) */

/* Private variables ---------------------------------------------------------*/
/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;
static uint8_t CardType = 0;

/* Diagnostic (lisible depuis main.c avec "extern volatile uint8_t ...") :
 *   sd_dbg_cmd0  : reponse brute a CMD0 (attendu 0x01 ; 0x00 = MISO colle a 0 ; 0xFF = pas de reponse)
 *   sd_dbg_stage : 0 = CMD0 echoue, 1 = CMD0 OK, 2 = CMD8 OK, 3 = carte prete (ACMD41 OK), 4 = init terminee */
volatile uint8_t sd_dbg_cmd0  = 0xEE;
volatile uint8_t sd_dbg_stage = 0;

/* ---- Bas niveau SPI ---- */
static uint8_t spi_rw(uint8_t tx)
{
  uint8_t rx = 0xFF;
  HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1, 50);
  return rx;
}

static void sd_set_speed(uint32_t prescaler)
{
  hspi1.Init.BaudRatePrescaler = prescaler;
  HAL_SPI_Init(&hspi1);
}

static int sd_wait_ready(uint32_t ms)
{
  uint32_t t0 = HAL_GetTick();
  while ((HAL_GetTick() - t0) < ms)
  {
    if (spi_rw(0xFF) == 0xFF) return 1;
  }
  return 0;
}

static void sd_deselect(void)
{
  SD_CS_HIGH();
  spi_rw(0xFF);                 /* 8 coups d'horloge pour liberer MISO */
}

static int sd_select(void)
{
  SD_CS_LOW();
  spi_rw(0xFF);
  if (sd_wait_ready(500)) return 1;
  sd_deselect();
  return 0;
}

static uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg)
{
  uint8_t n, res;

  if (cmd & 0x80)               /* commande ACMD : CMD55 d'abord */
  {
    cmd &= 0x7F;
    res = sd_send_cmd(CMD55, 0);
    if (res > 1) return res;
  }

  if (cmd != CMD12)
  {
    sd_deselect();
    if (!sd_select()) return 0xFF;
  }

  spi_rw(0x40 | cmd);
  spi_rw((uint8_t)(arg >> 24));
  spi_rw((uint8_t)(arg >> 16));
  spi_rw((uint8_t)(arg >> 8));
  spi_rw((uint8_t)arg);
  n = 0x01;                     /* CRC factice + bit de stop */
  if (cmd == CMD0) n = 0x95;
  if (cmd == CMD8) n = 0x87;
  spi_rw(n);

  if (cmd == CMD12) spi_rw(0xFF);   /* octet parasite apres CMD12 */

  n = 10;
  do { res = spi_rw(0xFF); } while ((res & 0x80) && --n);
  return res;
}

static int sd_rcvr_block(uint8_t *buff, uint32_t len)
{
  uint8_t token;
  uint32_t t0 = HAL_GetTick();

  do { token = spi_rw(0xFF); } while (token == 0xFF && (HAL_GetTick() - t0) < 200);
  if (token != 0xFE) return 0;

  while (len--) *buff++ = spi_rw(0xFF);
  spi_rw(0xFF);                 /* CRC ignore */
  spi_rw(0xFF);
  return 1;
}

#if _USE_WRITE == 1
static int sd_xmit_block(const uint8_t *buff, uint8_t token)
{
  uint8_t resp, n;
  uint32_t i;

  if (!sd_wait_ready(500)) return 0;

  spi_rw(token);
  if (token != 0xFD)            /* 0xFD = jeton de fin (pas de donnees) */
  {
    for (i = 0; i < 512; i++) spi_rw(buff[i]);
    spi_rw(0xFF);               /* CRC factice */
    spi_rw(0xFF);

    /* Reponse de la carte : certaines cartes mettent plusieurs octets a repondre (0xFF = pas encore) */
    n = 10;
    do { resp = spi_rw(0xFF); } while (resp == 0xFF && --n);
    if ((resp & 0x1F) != 0x05) return 0;   /* donnees refusees */
  }
  else
  {
    spi_rw(0xFF);               /* octet de garde apres le jeton de fin, avant le test d'occupation */
  }

  /* Attendre la fin de l'ecriture interne (MISO reste a 0 tant que la carte est occupee) */
  return sd_wait_ready(500);
}
#endif

/* USER CODE END DECL */

/* Private function prototypes -----------------------------------------------*/
DSTATUS USER_initialize (BYTE pdrv);
DSTATUS USER_status (BYTE pdrv);
DRESULT USER_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
#if _USE_WRITE == 1
  DRESULT USER_write (BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
#endif /* _USE_WRITE == 1 */
#if _USE_IOCTL == 1
  DRESULT USER_ioctl (BYTE pdrv, BYTE cmd, void *buff);
#endif /* _USE_IOCTL == 1 */

Diskio_drvTypeDef  USER_Driver =
{
  USER_initialize,
  USER_status,
  USER_read,
#if  _USE_WRITE
  USER_write,
#endif  /* _USE_WRITE == 1 */
#if  _USE_IOCTL == 1
  USER_ioctl,
#endif /* _USE_IOCTL == 1 */
};

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes a Drive
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_initialize (
	BYTE pdrv           /* Physical drive nmuber to identify the drive */
)
{
  /* USER CODE BEGIN INIT */
    uint8_t n, r, ocr[4];
    uint8_t type = 0;
    uint32_t t0;

    if (pdrv) return STA_NOINIT;

    sd_dbg_stage = 0;
    sd_set_speed(SD_SPEED_INIT);

    SD_CS_HIGH();
    for (n = 10; n; n--) spi_rw(0xFF);      /* >= 74 coups d'horloge, CS haut */
    t0 = HAL_GetTick();
    r = sd_send_cmd(CMD0, 0);
    sd_dbg_cmd0 = r;

    if (r == 1)                              /* carte en etat "idle" */
    {
      sd_dbg_stage = 1;

      if (sd_send_cmd(CMD8, 0x1AA) == 1)     /* SD version 2 ou plus */
      {
        for (n = 0; n < 4; n++) ocr[n] = spi_rw(0xFF);
        if (ocr[2] == 0x01 && ocr[3] == 0xAA)
        {
          sd_dbg_stage = 2;
          do { r = sd_send_cmd(ACMD41, 1UL << 30); }
          while (r && (HAL_GetTick() - t0) < 1500);

          if (r == 0 && sd_send_cmd(CMD58, 0) == 0)
          {
            for (n = 0; n < 4; n++) ocr[n] = spi_rw(0xFF);
            type = (ocr[0] & 0x40) ? (CT_SD2 | CT_BLOCK) : CT_SD2;
          }
        }
      }
      else                                   /* SD version 1 */
      {
        do { r = sd_send_cmd(ACMD41, 0); }
        while (r && (HAL_GetTick() - t0) < 1500);

        if (r == 0 && sd_send_cmd(CMD16, 512) == 0) type = CT_SD1;
      }
    }

    CardType = type;
    sd_deselect();

    if (type)
    {
      sd_dbg_stage = 3;
      sd_set_speed(SD_SPEED_RUN);            /* on accelere apres l'init */
      Stat &= ~STA_NOINIT;
      sd_dbg_stage = 4;
    }
    else
    {
      Stat = STA_NOINIT;
    }
    return Stat;
  /* USER CODE END INIT */
}

/**
  * @brief  Gets Disk Status
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_status (
	BYTE pdrv       /* Physical drive number to identify the drive */
)
{
  /* USER CODE BEGIN STATUS */
    if (pdrv) return STA_NOINIT;
    return Stat;
  /* USER CODE END STATUS */
}

/**
  * @brief  Reads Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT USER_read (
	BYTE pdrv,      /* Physical drive nmuber to identify the drive */
	BYTE *buff,     /* Data buffer to store read data */
	DWORD sector,   /* Sector address in LBA */
	UINT count      /* Number of sectors to read */
)
{
  /* USER CODE BEGIN READ */
    if (pdrv || !count) return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;

    if (!(CardType & CT_BLOCK)) sector *= 512;   /* SDSC : adresse en octets */

    if (count == 1)
    {
      if (sd_send_cmd(CMD17, sector) == 0 && sd_rcvr_block(buff, 512)) count = 0;
    }
    else
    {
      if (sd_send_cmd(CMD18, sector) == 0)
      {
        do
        {
          if (!sd_rcvr_block(buff, 512)) break;
          buff += 512;
        } while (--count);
        sd_send_cmd(CMD12, 0);
      }
    }
    sd_deselect();
    return count ? RES_ERROR : RES_OK;
  /* USER CODE END READ */
}

/**
  * @brief  Writes Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
DRESULT USER_write (
	BYTE pdrv,          /* Physical drive nmuber to identify the drive */
	const BYTE *buff,   /* Data to be written */
	DWORD sector,       /* Sector address in LBA */
	UINT count          /* Number of sectors to write */
)
{
  /* USER CODE BEGIN WRITE */
  /* USER CODE HERE */
    if (pdrv || !count) return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;
    if (Stat & STA_PROTECT) return RES_WRPRT;

    if (!(CardType & CT_BLOCK)) sector *= 512;

    if (count == 1)
    {
      if (sd_send_cmd(CMD24, sector) == 0 && sd_xmit_block(buff, 0xFE)) count = 0;
    }
    else
    {
      if (CardType & CT_SD1 || CardType & CT_SD2) sd_send_cmd(ACMD23, count);
      if (sd_send_cmd(CMD25, sector) == 0)
      {
        do
        {
          if (!sd_xmit_block(buff, 0xFC)) break;
          buff += 512;
        } while (--count);
        if (!sd_xmit_block(0, 0xFD)) count = 1;    /* jeton de fin */
      }
    }
    sd_deselect();
    return count ? RES_ERROR : RES_OK;
  /* USER CODE END WRITE */
}
#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O control operation
  * @param  pdrv: Physical drive number (0..)
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT USER_ioctl (
	BYTE pdrv,      /* Physical drive nmuber (0..) */
	BYTE cmd,       /* Control code */
	void *buff      /* Buffer to send/receive control data */
)
{
  /* USER CODE BEGIN IOCTL */
    DRESULT res = RES_ERROR;
    uint8_t csd[16];
    uint32_t csize;
    uint8_t n;

    if (pdrv) return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;

    switch (cmd)
    {
      case CTRL_SYNC:                       /* attendre la fin des ecritures */
        if (sd_select()) { sd_deselect(); res = RES_OK; }
        break;

      case GET_SECTOR_COUNT:                /* nombre de secteurs (via le CSD) */
        if (sd_send_cmd(CMD9, 0) == 0 && sd_rcvr_block(csd, 16))
        {
          if ((csd[0] >> 6) == 1)           /* CSD v2 (SDHC/SDXC) */
          {
            csize = csd[9] + ((uint32_t)csd[8] << 8) + ((uint32_t)(csd[7] & 63) << 16) + 1;
            *(DWORD *)buff = csize << 10;
          }
          else                              /* CSD v1 */
          {
            n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
            csize = (csd[8] >> 6) + ((uint32_t)csd[7] << 2) + ((uint32_t)(csd[6] & 3) << 10) + 1;
            *(DWORD *)buff = csize << (n - 9);
          }
          res = RES_OK;
        }
        sd_deselect();
        break;

      case GET_SECTOR_SIZE:
        *(WORD *)buff = 512;
        res = RES_OK;
        break;

      case GET_BLOCK_SIZE:
        *(DWORD *)buff = 1;
        res = RES_OK;
        break;

      default:
        res = RES_PARERR;
        break;
    }
    return res;
  /* USER CODE END IOCTL */
}
#endif /* _USE_IOCTL == 1 */