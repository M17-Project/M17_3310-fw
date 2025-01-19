/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define RF_PWR_Pin GPIO_PIN_13
#define RF_PWR_GPIO_Port GPIOC
#define RF_PTT_Pin GPIO_PIN_0
#define RF_PTT_GPIO_Port GPIOC
#define COL_3_Pin GPIO_PIN_1
#define COL_3_GPIO_Port GPIOC
#define COL_2_Pin GPIO_PIN_3
#define COL_2_GPIO_Port GPIOC
#define BSB_IN_Pin GPIO_PIN_0
#define BSB_IN_GPIO_Port GPIOA
#define U_BATT_IN_Pin GPIO_PIN_1
#define U_BATT_IN_GPIO_Port GPIOA
#define BLIGHT_Pin GPIO_PIN_2
#define BLIGHT_GPIO_Port GPIOA
#define VIBR_Pin GPIO_PIN_3
#define VIBR_GPIO_Port GPIOA
#define BSB_OUT_Pin GPIO_PIN_4
#define BSB_OUT_GPIO_Port GPIOA
#define CHG_Pin GPIO_PIN_5
#define CHG_GPIO_Port GPIOA
#define COL_1_Pin GPIO_PIN_12
#define COL_1_GPIO_Port GPIOB
#define BUZZ_Pin GPIO_PIN_13
#define BUZZ_GPIO_Port GPIOB
#define PWR_OFF_Pin GPIO_PIN_15
#define PWR_OFF_GPIO_Port GPIOB
#define ROW_5_Pin GPIO_PIN_8
#define ROW_5_GPIO_Port GPIOC
#define ROW_4_Pin GPIO_PIN_9
#define ROW_4_GPIO_Port GPIOC
#define ROW_3_Pin GPIO_PIN_8
#define ROW_3_GPIO_Port GPIOA
#define ROW_2_Pin GPIO_PIN_9
#define ROW_2_GPIO_Port GPIOA
#define ROW_1_Pin GPIO_PIN_10
#define ROW_1_GPIO_Port GPIOA
#define RF_TX_Pin GPIO_PIN_10
#define RF_TX_GPIO_Port GPIOC
#define RF_RX_Pin GPIO_PIN_11
#define RF_RX_GPIO_Port GPIOC
#define BTN_OK_Pin GPIO_PIN_2
#define BTN_OK_GPIO_Port GPIOD
#define DISP_CE_Pin GPIO_PIN_6
#define DISP_CE_GPIO_Port GPIOB
#define DISP_DC_Pin GPIO_PIN_7
#define DISP_DC_GPIO_Port GPIOB
#define DISP_RST_Pin GPIO_PIN_8
#define DISP_RST_GPIO_Port GPIOB
#define RF_ENA_Pin GPIO_PIN_9
#define RF_ENA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
