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
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
/* 功能模块使能宏：在 main.c 的 USER CODE BEGIN PD 中定义。
 * 此处不定义，避免与 main.c 的 #define 冲突。
 * 其他文件（如 usart.c）若需使用这些宏，应包含 main.h 且确保 main.c
 * 的定义在 main.h 之后（由编译顺序保证）。 */
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define AD2_Pin GPIO_PIN_2
#define AD2_GPIO_Port GPIOE
#define AD3_Pin GPIO_PIN_3
#define AD3_GPIO_Port GPIOE
#define AD4_Pin GPIO_PIN_4
#define AD4_GPIO_Port GPIOE
#define AD5_Pin GPIO_PIN_5
#define AD5_GPIO_Port GPIOE
#define AD6_Pin GPIO_PIN_6
#define AD6_GPIO_Port GPIOE
#define AD7_Pin GPIO_PIN_7
#define AD7_GPIO_Port GPIOE
#define AD8_Pin GPIO_PIN_8
#define AD8_GPIO_Port GPIOE
#define AD9_Pin GPIO_PIN_9
#define AD9_GPIO_Port GPIOE
#define AD10_Pin GPIO_PIN_10
#define AD10_GPIO_Port GPIOE
#define AD11_Pin GPIO_PIN_11
#define AD11_GPIO_Port GPIOE
#define NRF_CSN_Pin GPIO_PIN_5
#define NRF_CSN_GPIO_Port GPIOB
#define NRF_CE_Pin GPIO_PIN_6
#define NRF_CE_GPIO_Port GPIOB
#define NRF_IRQ_Pin GPIO_PIN_7
#define NRF_IRQ_GPIO_Port GPIOB
#define AD0_Pin GPIO_PIN_0
#define AD0_GPIO_Port GPIOE
#define AD1_Pin GPIO_PIN_1
#define AD1_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
