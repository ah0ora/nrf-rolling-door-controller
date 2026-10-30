/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : گیرنده Remoot - کنترل رله‌های پارکینگ
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

/* STOP فقط یک وضعیت داخلی گیرنده است و روی هوا ارسال نمی‌شود؛
   گیرنده خودش با تشخیص تغییر جهت، آن را می‌سازد */
#define CMD_STOP  0x03

#define LINK_TIMEOUT_MS  400
#define RELAY_GUARD_MS   50
/* مکث اجباری بین خاموش شدن یک جهت و روشن شدن «جهت مخالف» — حفاظت از موتور
   کرکره در برابر معکوس‌شدن ناگهانی (پلاگینگ). فقط جهت مخالف را محدود می‌کند؛
   روشن شدنِ دوبارهٔ همان جهت فقط RELAY_GUARD_MS صبر دارد. */
#define RELAY_REVERSE_MS 500
#define LED_BLINK_MS     250

/* بازیابی ماژول فقط در صورت قطع بودن پیوند (بی‌پکت بودن به مدت ۵ ثانیه) */
#define RECOVER_INTERVAL_MS  5000
/* Flush FIFO فقط هنگام قطع پیوند */
#define FLUSH_INTERVAL_MS    1000

/* LED: سطح بالا = روشن */
#define LED_ON()   HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, GPIO_PIN_SET)
#define LED_OFF()  HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, GPIO_PIN_RESET)

/* علائم شناخت پکت */
#define PKT_MAGIC0 0x52  /* 'R' */
#define PKT_MAGIC1 0x4D  /* 'M' */
#define PKT_MAGIC2 0x54  /* 'T' */

/* Private variables ---------------------------------------------------------*/
uint8_t  rxBuf[8];

uint8_t  rxCmd      = CMD_NONE;
/* وضعیت فعلی رله‌ها: NONE / UP / DOWN / STOP (ماشین وضعیت) */
uint8_t  relayState = CMD_NONE;
uint32_t lastRxTime = 0;
uint32_t guardUntil = 0;
/* آخرین جهتِ وصل‌شده و لحظهٔ قطع آن — برای قفل معکوس RELAY_REVERSE_MS */
uint8_t  lastDir  = CMD_NONE;
uint32_t dirOffAt = 0;

uint8_t  linkUp     = 0;
uint32_t blinkAt    = 0;
uint8_t  blinkOn    = 0;

/* بازیابی ماژول و Flush FIFO — فقط هنگام قطع پیوند */
uint32_t lastRecover = 0;
uint32_t lastFlush   = 0;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void Relays_Off(void);
uint8_t Next_State(uint8_t state, uint8_t cmd);
void Apply_Command(uint8_t cmd);
uint8_t Packet_Valid(uint8_t *buf);
void Rx_Recover(void);
void Rx_Periodic(uint32_t now);

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

  nRF_Init_Pin();
  nRF_Init();
  nRF_SetRX();
  Relays_Off();

  lastRxTime  = HAL_GetTick();
  lastRecover = HAL_GetTick();
  lastFlush   = HAL_GetTick();

  while (1)
  {
    uint32_t now = HAL_GetTick();

    /* خواندن تمام پکت‌های موجود در FIFO */
    for (uint8_t i = 0; i < 4; i++)
    {
      if (!nRF_GetPacket(rxBuf)) break;
      if (Packet_Valid(rxBuf))
      {
        rxCmd      = rxBuf[0];
        lastRxTime = now;
        linkUp     = 1;
      }
    }

    /* ایمنی: اگر پیوند قطع شد، فرمان لغو و رله‌ها خاموش */
    if ((rxCmd == CMD_UP || rxCmd == CMD_DOWN) && ((now - lastRxTime) > LINK_TIMEOUT_MS))
    {
      rxCmd = CMD_NONE;
    }

    /* اعمال فرمان با ماشین وضعیت STOP و رعایت زمان مرده
       - خاموش شدن هر رله‌ای همیشه فوری است
       - روشن شدن، از guardUntil به بعد مجاز است (RELAY_GUARD_MS)
       - روشن شدنِ جهتِ مخالفِ آخرین جهتِ وصل‌شده تا پایان RELAY_REVERSE_MS
         ممنوع است تا موتورِ در حال چرخش، ناگهان معکوس نشود */
    uint8_t nextState = Next_State(relayState, rxCmd);
    if (nextState != relayState)
    {
      uint8_t nextIsDir  = (nextState  == CMD_UP) || (nextState  == CMD_DOWN);
      uint8_t relayIsDir = (relayState == CMD_UP) || (relayState == CMD_DOWN);
      uint8_t allowed;

      if (nextState == CMD_NONE)
      {
        allowed = 1;    /* خاموش‌شدن فوری (فشردنِ رها شده / قطع پیوند) */
      }
      else
      {
        allowed = ((int32_t)(now - guardUntil) >= 0);

        /* قفل معکوس: جهتِ مخالف باید صبر کند — STOP مستثنی است چون باید فوری وصل شود */
        if (allowed && nextIsDir &&
            (lastDir != CMD_NONE) && (lastDir != nextState) &&
            ((int32_t)(now - dirOffAt) < RELAY_REVERSE_MS))
        {
          allowed = 0;
        }
      }

      if (allowed)
      {
        /* لحظهٔ قطعِ آخرین جهت را ثبت کن تا مکث معکوس از همان لحظه شمرده شود */
        if (relayIsDir && !nextIsDir) dirOffAt = now;
        if (nextIsDir)                lastDir  = nextState;

        Apply_Command(nextState);
        relayState = nextState;
        guardUntil = now + RELAY_GUARD_MS;
      }
    }

    /* بازیابی ماژول / Flush — فقط هنگام قطع پیوند */
    Rx_Periodic(now);

    /* LED وضعیت */
    if (linkUp && ((now - lastRxTime) <= LINK_TIMEOUT_MS))
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

    HAL_Delay(5);
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
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) Error_Handler();
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /* LED status on PD2 */
  HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = LED_STATUS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_STATUS_GPIO_Port, &GPIO_InitStruct);

  /* CE, CSN on PA2, PA3 */
  HAL_GPIO_WritePin(GPIOA, CE_Pin|CSN_Pin, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = CE_Pin|CSN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Relay pins: UP=PB5, STOP=PB3, DOWN=PB4
     (PB3/PB4 آزاد شده‌اند چون در HAL_MspInit سویچ NOJTAG زده شده است) */
  HAL_GPIO_WritePin(GPIOB, RELAY_UP_Pin|RELAY_DOWN_Pin|RELAY_STOP_Pin, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = RELAY_UP_Pin|RELAY_DOWN_Pin|RELAY_STOP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */

/**
  * @brief  خاموش کردن همه رله‌ها
  */
void Relays_Off(void)
{
  HAL_GPIO_WritePin(RELAY_UP_GPIO_Port,   RELAY_UP_Pin,   GPIO_PIN_RESET);
  HAL_GPIO_WritePin(RELAY_DOWN_GPIO_Port, RELAY_DOWN_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(RELAY_STOP_GPIO_Port, RELAY_STOP_Pin, GPIO_PIN_RESET);
}

/**
  * @brief  ماشین وضعیت رله‌ها: تعیین وضعیت بعدی از وضعیت فعلی و فرمان دریافتی
  * @param state وضعیت فعلی: CMD_NONE / CMD_UP / CMD_DOWN / CMD_STOP
  * @param cmd   فرمان روی هوا: CMD_NONE / CMD_UP / CMD_DOWN
  * @retval وضعیت بعدی
  *
  * قواعد:
  *  - رها کردن دکمه (CMD_NONE)        → همه رله‌ها خاموش (idle)
  *  - از حالت ساکن (NONE)             → مستقیم همان جهت وصل شود
  *  - از حالت در حال حرکت، فرمانِ جهتِ مخالف → اول STOP، نه تغییر جهت
  *  - در حالت STOP                    → تا رها کردن دکمه در STOP می‌مانیم؛
  *                                       فشار بعدی (دومین فرمان) جهت جدید را وصل می‌کند
  */
uint8_t Next_State(uint8_t state, uint8_t cmd)
{
  if (cmd == CMD_NONE)   return CMD_NONE;   /* رها کردن → خاموش */
  if (cmd == state)      return state;      /* بدون تغییر */
  if (state == CMD_NONE) return cmd;        /* از حالت ساکن: مستقیم همان جهت */
  return CMD_STOP;                          /* تغییر جهت در حال حرکت یا ادامهٔ فشار در STOP */
}

/**
  * @brief  اعمال وضعیت روی رله‌ها (فقط یکی روشن است)
  */
void Apply_Command(uint8_t cmd)
{
  Relays_Off();

  if (cmd == CMD_UP)
  {
    HAL_GPIO_WritePin(RELAY_UP_GPIO_Port, RELAY_UP_Pin, GPIO_PIN_SET);
  }
  else if (cmd == CMD_DOWN)
  {
    HAL_GPIO_WritePin(RELAY_DOWN_GPIO_Port, RELAY_DOWN_Pin, GPIO_PIN_SET);
  }
  else if (cmd == CMD_STOP)
  {
    HAL_GPIO_WritePin(RELAY_STOP_GPIO_Port, RELAY_STOP_Pin, GPIO_PIN_SET);
  }
  /* CMD_NONE: همه خاموش */
}

/**
  * @brief  اعتبارسنجی پکت (فقط فرمان‌های NONE/UP/DOWN روی هوا معتبرند؛
  *         CMD_STOP وضعیت داخلی است و از فرستنده نمی‌آید)
  * @retval 1 = معتبر، 0 = نامعتبر
  */
uint8_t Packet_Valid(uint8_t *buf)
{
  if (buf[0] > CMD_DOWN)           return 0;
  if (buf[2] != PKT_MAGIC0)        return 0;
  if (buf[3] != PKT_MAGIC1)        return 0;
  if (buf[4] != PKT_MAGIC2)        return 0;
  if (buf[5] != (uint8_t)(~buf[0])) return 0;
  return 1;
}

/**
  * @brief  بازیابی کامل ماژول nRF (ریست + بازنویسی پیکربندی + ورود به RX)
  *         فقط هنگامی صدا زده می‌شود که پیوند واقعاً قطع است،
  *         بنابراین رله‌های گیت از کار نمی‌افتند.
  */
void Rx_Recover(void)
{
  /* بازیابی کامل: ریست نرم‌افزاری ماژول، سپس بازنویسی همهٔ رجیسترهای
     (کانال، آدرس، طول پلود، سرعت) و در نهایت ورود به حالت گوش‌دادن.
     فقط Power-Cycle کافی نیست: اگر افت ولتاژ (مثلاً هنگام کار موتور گیت)
     رجیسترها را پاک کرده باشد، بدون بازنویسی، گیرنده تا ریست MCU کور می‌ماند. */
  nRF_Recover();
  nRF_Init();      /* پیکربندی کامل — CE را پایین می‌آورد */
  nRF_SetRX();     /* حالت گوش‌دادن + CE بالا */

  /* اگر رله‌ای روشن بود، لحظهٔ قطعش را ثبت می‌کنیم تا قفل معکوس از همان لحظه شمرده شود */
  if ((relayState == CMD_UP) || (relayState == CMD_DOWN)) dirOffAt = HAL_GetTick();

  linkUp     = 0;
  rxCmd      = CMD_NONE;
  relayState = CMD_NONE;
  Relays_Off();
  /* lastRxTime دست‌نخورده می‌ماند تا وضعیت پیوند واقعی بماند */
}

/**
  * @brief  بازیابی دوره‌ای ماژول و Flush FIFO — فقط هنگام قطع پیوند
  * @param  now  زمان فعلی (ms)
  */
void Rx_Periodic(uint32_t now)
{
  uint8_t linkDown = ((now - lastRxTime) > LINK_TIMEOUT_MS);

  /* Flush دوره‌ای FIFO دریافت (پکت معتبر هنگام اتصال سالم دور ریخته نشود) */
  if (linkDown && ((now - lastFlush) >= FLUSH_INTERVAL_MS))
  {
    lastFlush = now;
    nRF_FlushRx();
  }

  /* بازیابی ماژول: فقط اگر بیش از RECOVER_INTERVAL_MS پکت معتبر نرسیده باشد
     و آخرین بازیابی هم قدیمی باشد — نه به‌صورت کور هر ۵ ثانیه */
  if (((now - lastRxTime) >= RECOVER_INTERVAL_MS) &&
      ((now - lastRecover) >= RECOVER_INTERVAL_MS))
  {
    lastRecover = now;
    Rx_Recover();
  }
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
