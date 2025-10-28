/* io.h - simple IO helpers (LED) */
#ifndef IO_H
#define IO_H

#include "main.h"

static inline void LED_On(void) { HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_SET); }
static inline void LED_Off(void) { HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_RESET); }
static inline void LED_Toggle(void) { HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN); }

#endif // IO_H
