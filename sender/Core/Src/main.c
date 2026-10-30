/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : فرستنده Remoot - کنترل گیت پارکینگ از راه دور
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "nRF24l01.h"

/* Private define ------------------------------------------------------------*/
#define CMD_NONE  0x00
#define CMD_UP    0x01
#define CMD_DOWN  0x02

#define DEBOUNCE_MS      30
#define TX_INTERVAL_MS   50
#define LINK_TIMEOUT_MS  400
#define TX_ACK_WAIT_MS   40
#define LED_BLINK_MS     250

/* بازیابی ماژول nRF فقط در صورت قطع بودن پیوند (بی‌ACK بودن به مدت ۵ ثانیه) */
#define RECOVER_INTERVAL_MS  5000

/* LED: سطح بالا = روشن */
#define LED_ON()   HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, GPIO_PIN_SET)
#define LED_OFF()  HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, GPIO_PIN_RESET)

/* Private typedef -----------------------------------------------------------*/
typedef struct
{
  uint8_t  raw;
  uint8_t  state;
  uint8_t  prev;
  uint32_t tick;
} Button_t;

/* Private variables ---------------------------------------------------------*/
Button_t btnKey    = {0, 0, 0, 0};
Button_t btnSensor = {0, 0, 0, 0};

uint8_t  txCmd     = CMD_NONE;
/* دکمه‌ای که دیرتر فشرده شده — وقتی هر دو فشرده‌اند، فرمانِ آن ارسال می‌شود
   تا فشردنِ دکمهٔ مخالفِ دکمهٔ نگه‌داشته‌شده در هر دو جهت به گیرنده برسد و STOP فعال شود */
uint8_t  lastPressed = CMD_UP;
uint8_t  txSeq     = 0;
uint32_t lastTx    = 0;

uint8_t  txPending = 0;
uint32_t txStart   = 0;
uint32_t lastAck   = 0;
uint8_t  linkUp    = 0;
uint32_t blinkAt   = 0;
uint8_t  blinkOn   = 0;

/* شمارنده خطاهای متوالی برای بازیابی */
uint8_t  failCount    = 0;
uint32_t lastRecover  = 0;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
uint8_t Button_Update(Button_t *btn, GPIO_TypeDef *port, uint16_t pin, uint8_t activeHigh);
void    SendCommand(uint8_t cmd);
void    Tx_CheckLink(uint32_t now);
void    Tx_Recover(void);

/* Private user code ---------------------------------------------------------*/

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();

  /* ماژول nRF در حالت TX */
  nRF_Init_Pin();
  nRF_Init();
  lastTx     = HAL_GetTick();
  lastAck    = HAL_GetTick();
  lastRecover = HAL_GetTick();

  while (1)
  {
    uint32_t now = HAL_GetTick();

    /* خواندن دکمه‌ها با debounce — خروجی تابع = لبهٔ فشردن (۰→۱) */
    uint8_t keyPressed    = Button_Update(&btnKey,    KEY_GPIO_Port,    KEY_Pin,    KEY_ACTIVE_HIGH);
    uint8_t sensorPressed = Button_Update(&btnSensor, SENSOR_GPIO_Port, SENSOR_Pin, SENSOR_ACTIVE_HIGH);

    /* ثبت دکمه‌ای که دیرتر فشرده شده است */
    if (keyPressed)    lastPressed = CMD_UP;
    if (sensorPressed) lastPressed = CMD_DOWN;

    /* تعیین فرمان
       اگر هر دو دکمه فشرده باشند، دکمه‌ای که دیرتر فشرده شده فرستاده می‌شود
       (قبلاً UP اولویت مطلق داشت و DOWN هرگز ارسال نمی‌شد) */
    if (btnKey.state && btnSensor.state)
    {
      txCmd = lastPressed;
    }
    else if (btnKey.state)
    {
      txCmd = CMD_UP;
    }
    else if (btnSensor.state)
    {
      txCmd = CMD_DOWN;
    }
    else
    {
      txCmd = CMD_NONE;
    }

    /* ارسال دوره‌ای فرمان */
    if ((now - lastTx) >= TX_INTERVAL_MS)
    {
      lastTx = now;
      SendCommand(txCmd);
      txPending = 1;
      txStart   = now;
    }

    /* بررسی ACK */
    Tx_CheckLink(now);

    /* بازیابی ماژول فقط وقتی پیوند واقعاً قطع است (۵ ثانیه بدون ACK)
       تا اتصال سالم هرگز با ریست ماژول قطع نشود */
    if (((now - lastAck) >= RECOVER_INTERVAL_MS) &&
        ((now - lastRecover) >= RECOVER_INTERVAL_MS))
    {
      lastRecover = now;
      Tx_Recover();
    }

    /* LED وضعیت */
    if (linkUp && ((now - lastAck) <= LINK_TIMEOUT_MS))
    {
      LED_ON();
      blinkAt = now;
    }
    else if ((now - blinkAt) >= LED_BLINK_MS)
    {
      blinkAt  = now;
      blinkOn ^= 1;
      if (blinkOn) LED_ON(); else LED_OFF();
    }

    HAL_Delay(2);
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* فعال‌سازی کلاک GPIO */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /* خاموش کردن JTAG (بدون خاموش کردن SWD): PB4/JTDO/JTDI آزاد می‌مانند
     و SWD برای فلش/دیباگ باقی است. (دکمهٔ SENSOR دیگر روی PB4 نیست.) */
  __HAL_RCC_AFIO_CLK_ENABLE();
  __HAL_AFIO_REMAP_SWJ_NOJTAG();

  /* CE, CSN خروجی */
  HAL_GPIO_WritePin(GPIOA, CE_Pin|CSN_Pin, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = CE_Pin|CSN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* KEY: ورودی با Pull-Up — فشردن = سطح پایین (Active-Low) */
  GPIO_InitStruct.Pin = KEY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* SENSOR (PB7): ورودی با Pull-Down — فشردن = سطح بالا (Active-High) */
  GPIO_InitStruct.Pin = SENSOR_Pin;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* LED وضعیت */
  HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin  = LED_STATUS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_STATUS_GPIO_Port, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */

/**
  * @brief  خواندن دکمه با debounce
  * @param  activeHigh 1 = فشردن یعنی سطح بالا (High)، 0 = فشردن یعنی سطح پایین (Low)
  */
uint8_t Button_Update(Button_t *btn, GPIO_TypeDef *port, uint16_t pin, uint8_t activeHigh)
{
  GPIO_PinState level = HAL_GPIO_ReadPin(port, pin);
  uint8_t raw = (activeHigh ? (level == GPIO_PIN_SET)
                            : (level == GPIO_PIN_RESET)) ? 1 : 0;

  if (raw != btn->raw)
  {
    btn->raw  = raw;
    btn->tick = HAL_GetTick();
  }

  if ((HAL_GetTick() - btn->tick) >= DEBOUNCE_MS)
  {
    btn->state = btn->raw;
  }

  if (btn->state && !btn->prev)
  {
    btn->prev = 1;
    return 1;
  }
  if (!btn->state)
  {
    btn->prev = 0;
  }
  return 0;
}

/**
  * @brief  ساخت و ارسال پکت 8 بایتی
  */
void SendCommand(uint8_t cmd)
{
  uint8_t txBuf[8] = {0};

  txBuf[0] = cmd;
  txBuf[1] = txSeq++;
  txBuf[2] = 0x52;  /* 'R' */
  txBuf[3] = 0x4D;  /* 'M' */
  txBuf[4] = 0x54;  /* 'T' */
  txBuf[5] = (uint8_t)~cmd;

  nRF_SendPacket(txBuf);
}

/**
  * @brief  بررسی وضعیت ارسال و به‌روزرسانی linkUp
  */
void Tx_CheckLink(uint32_t now)
{
  if (!txPending) return;

  nRF24L01_Transmit_Status_t st = nRF_GetTransmissionStatus();

  if (st == nRF24L01_Transmit_Status_Ok)
  {
    /* ACK دریافت شد */
    nRF_spi_RWreg(nRF24L01_WRITE_RE + nRF24L01_STATUS, (1 << NRF24L01_TX_DS));
    lastAck   = now;
    linkUp    = 1;
    txPending = 0;
    failCount = 0;
  }
  else if (st == nRF24L01_Transmit_Status_Lost)
  {
    /* MAX_RT: گیرنده در دسترس نیست */
    nRF_spi_RWreg(nRF24L01_WRITE_RE + nRF24L01_STATUS, (1 << NRF24L01_MAX_RT));
    NRF24L01_CSN_LOW;
    nRF_spi_RW(NRF24L01_FLUSH_TX_MASK);
    NRF24L01_CSN_HIGH;
    txPending = 0;
    failCount++;

    /* اگر خطاها زیاد شد، بازیابی کامل */
    if (failCount >= 3)
    {
      failCount = 0;
      Tx_Recover();
    }
  }
  else if ((now - txStart) > TX_ACK_WAIT_MS)
  {
    /* تایم‌اوت */
    txPending = 0;
    failCount++;
  }
}

/**
  * @brief  بازیابی کامل ماژول nRF (Power Cycle)
  */
void Tx_Recover(void)
{
  nRF_Recover();
  linkUp    = 0;
  txPending = 0;
  failCount = 0;
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { }
#endif
