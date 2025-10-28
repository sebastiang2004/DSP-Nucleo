/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "arm_math.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Modularized headers
#include "peripherals.h"
#include "dsp_core.h"
#include "effects.h"
#include "uart_comm.h"
#include "io.h"
/* USER CODE END Includes */

/*
 * Define HAL handle instances here (single-authority definitions).
 * These are declared as extern in `peripherals.h` so other modules can use them.
 */
ADC_HandleTypeDef hadc1;
DAC_HandleTypeDef hdac1;
OPAMP_HandleTypeDef hopamp1;
TIM_HandleTypeDef htim1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE BEGIN 4 */
// Implementations moved to modules:
// - peripherals.c (MX_* init, TIM1_Config_For_Sampling)
// - effects.c (Apply_Distortion, Apply_Overdrive, Apply_Delay, Apply_NoiseGate)
// - dsp_core.c (Process_Guitar_Signal, HAL_TIM_PeriodElapsedCallback)
// - uart_comm.c (Parse_UART_Command, Send_UART_Response, HAL_UART_RxCpltCallback)

/* USER CODE END 4 */
// ADC initialization moved to Core/Src/peripherals.c (MX_ADC1_Init)


/* DAC initialization implemented in Core/Src/peripherals.c */

/* OPAMP1 initialization implemented in Core/Src/peripherals.c */

/* TIM1 initialization implemented in Core/Src/peripherals.c */

/* USART2 init implemented in Core/Src/peripherals.c */

/* USART3 init implemented in Core/Src/peripherals.c */

/* GPIO init implemented in Core/Src/peripherals.c */

/* USER CODE BEGIN 4 */

/**
  * @brief  Configure Timer1 for audio sampling rate (48kHz)
  * @param  None
  * @retval None
  */
/* TIM1 sampling configuration implemented in Core/Src/peripherals.c */

/**
  * @brief  Apply distortion effect to input signal
  * @param  input: Input sample value (-1.0 to 1.0)
  * @retval Distorted output sample
  */
/* Apply_Distortion implementation moved to Core/Src/effects.c */

/**
  * @brief  Apply overdrive effect to input signal (OPTIMIZED)
  * @param  input: Input sample value (-1.0 to 1.0)
  * @retval Overdrive output sample
  * @note   Implements:
  *         - Input high-pass filter (removes rumble/DC offset)
  *         - Multiple clipping modes (soft/hard/asymmetric)
  *         - Tone control with adjustable low-pass filter
  *         - Wet/dry mix for parallel processing
  */
/* Apply_Overdrive implementation moved to Core/Src/effects.c */

/**
  * @brief  Apply delay effect to input signal (OPTIMIZED)
  * @param  input: Input sample value (-1.0 to 1.0)
  * @retval Delayed output sample (mixed with input)
  * @note   Improvements:
  *         - Added tone/damping control for tape-like delay
  *         - Better feedback limiting to prevent runaway
  *         - Interpolation for smoother delay changes (future)
  */
/* Apply_Delay implementation moved to Core/Src/effects.c */

/**
  * @brief  Apply noise gate to input signal
  * @param  input: Input sample value (-1.0 to 1.0)
  * @retval Gated output sample
  * @note   This should be the FIRST effect in the chain
  *         Prevents noise from being amplified by other effects
  */
/* Apply_NoiseGate implementation moved to Core/Src/effects.c */

/* Parse_UART_Command implementation moved to Core/Src/uart_comm.c */
/* Send_UART_Response implementation moved to Core/Src/uart_comm.c */

/**
  * @brief  Process guitar signal with distortion effect
  * @param  None
  * @retval None
  */
/* Process_Guitar_Signal implementation moved to Core/Src/dsp_core.c */

/**
  * @brief  Timer period elapsed callback
  * @param  htim: Timer handle
  * @retval None
  */
/* HAL_TIM_PeriodElapsedCallback implementation moved to Core/Src/dsp_core.c */

/**
  * @brief  UART Receive Complete Callback
  * @param  huart: UART handle
  * @retval None
  */
/* HAL_UART_RxCpltCallback implementation moved to Core/Src/uart_comm.c */

/**
  * @brief System Clock Configuration
  * The system Clock is configured as follows:
  *            System Clock source            = PLL (HSI)
  *            SYSCLK(Hz)                     = 170000000
  *            HCLK(Hz)                       = 170000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 1
  *            APB2 Prescaler                 = 1
  *            HSI Frequency(Hz)              = 16000000
  *            PLL_M                          = 4
  *            PLL_N                          = 85
  *            PLL_P                          = 10
  *            PLL_Q                          = 2
  *            PLL_R                          = 2
  *            Flash Latency(WS)              = 4
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV10;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_DAC1_Init();
  MX_OPAMP1_Init();
  MX_TIM1_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();

  // Start DAC and OPAMP
  HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
  HAL_OPAMP_Start(&hopamp1);

  TIM1_Config_For_Sampling();
  HAL_TIM_Base_Start_IT(&htim1);

  HAL_UART_Receive_IT(&huart3, &uart_rx_byte, 1);

  while (1)
  {
    if (uart_command_ready)
    {
      Parse_UART_Command();
      uart_command_ready = 0;
    }

    if (process_audio_flag)
    {
      Process_Guitar_Signal();
      process_audio_flag = 0;
    }

    if (command_blink_counter)
    {
      HAL_Delay(50);
      HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
      command_blink_counter--;
    }
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
