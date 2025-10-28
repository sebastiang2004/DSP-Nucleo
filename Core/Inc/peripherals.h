/*
 * peripherals.h
 * Declarations for peripheral initialization moved out of main.c
 */
#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include "main.h"

// Export HAL handles declared in main.c so modules can reference them
extern ADC_HandleTypeDef hadc1;
extern DAC_HandleTypeDef hdac1;
extern OPAMP_HandleTypeDef hopamp1;
extern TIM_HandleTypeDef htim1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;

void MX_GPIO_Init(void);
void MX_ADC1_Init(void);
void MX_DAC1_Init(void);
void MX_OPAMP1_Init(void);
void MX_TIM1_Init(void);
void MX_USART2_UART_Init(void);
void MX_USART3_UART_Init(void);
void TIM1_Config_For_Sampling(void);

#endif // PERIPHERALS_H
