/* globals.h
 * Centralized project-wide constants and shared buffers
 */
#ifndef GLOBALS_H
#define GLOBALS_H

/* Centralized project-wide constants and shared buffers
 * This header intentionally avoids including effect headers to
 * prevent circular includes. Put shared sizes and externs here.
 */

#include "main.h"
#include <stdint.h>
#include <arm_math.h>

/* Sample/config constants */
#ifndef SAMPLE_RATE
#define SAMPLE_RATE 48000
#endif

/* ADC/DAC full scale (12-bit by default) */
#ifndef ADC_MAX_VALUE
#define ADC_MAX_VALUE 4095
#endif
#ifndef DAC_MAX_VALUE
#define DAC_MAX_VALUE 4095
#endif

/* Shared buffer sizes */
#ifndef BUFFER_SIZE
#define BUFFER_SIZE 128
#endif

#ifndef DELAY_BUFFER_SIZE
#define DELAY_BUFFER_SIZE 4800
#endif

#ifndef UART_RX_BUFFER_SIZE
#define UART_RX_BUFFER_SIZE 64
#endif

#ifndef UART_TX_BUFFER_SIZE
#define UART_TX_BUFFER_SIZE 128
#endif

/* Externs for audio buffers and control flags */
extern uint16_t adc_buffer[BUFFER_SIZE];
extern uint16_t dac_buffer[BUFFER_SIZE];
extern volatile uint8_t process_audio_flag;
extern volatile uint16_t buffer_index;

/* ADC monitoring (used in dsp_core.c) */
extern volatile uint16_t max_adc_deviation;
extern volatile uint16_t current_adc_value;

/* Delay buffer storage */
extern float32_t delay_buffer[DELAY_BUFFER_SIZE];
extern uint32_t delay_write_index;

/* UART communication buffers and counters */
extern uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
extern char uart_tx_buffer[UART_TX_BUFFER_SIZE];
extern volatile uint8_t uart_rx_index;
extern volatile uint8_t command_blink_counter;
/* Per-byte UART temporary storage and command-ready flag */
extern uint8_t uart_rx_byte;
extern volatile uint8_t uart_command_ready;

#endif /* GLOBALS_H */
