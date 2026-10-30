/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define CE_Pin GPIO_PIN_2
#define CE_GPIO_Port GPIOA
#define CSN_Pin GPIO_PIN_3
#define CSN_GPIO_Port GPIOA
#define KEY_Pin GPIO_PIN_4
#define KEY_GPIO_Port GPIOC
#define SENSOR_Pin GPIO_PIN_7
#define SENSOR_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define LED_STATUS_Pin GPIO_PIN_2       // نت STATUS طبق شماتیک (PD2 = پین 54)
#define LED_STATUS_GPIO_Port GPIOD

/* حالت فعال‌سازی دکمه‌ها: 1 = فشردن = سطح بالا (Active-High)
                                 0 = فشردن = سطح پایین (Active-Low) */
#define KEY_ACTIVE_HIGH     0   /* KEY:    PC4 با Pull-Up   → فشردن = صفر */
#define SENSOR_ACTIVE_HIGH  1   /* SENSOR: PB7 با Pull-Down → فشردن = یک */
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
