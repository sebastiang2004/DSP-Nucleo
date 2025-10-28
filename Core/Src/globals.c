/* globals.c
 * Definitions for shared globals used across dsp/effects/uart modules
 */

#include "globals.h"
#include "arm_math.h"

/* Audio buffers */
uint16_t adc_buffer[BUFFER_SIZE];
uint16_t dac_buffer[BUFFER_SIZE];

/* Audio processing control flags and indices */
volatile uint8_t process_audio_flag = 0;
volatile uint16_t buffer_index = 0;

/* ADC monitoring */
volatile uint16_t max_adc_deviation = 0;
volatile uint16_t current_adc_value = 0;

/* Delay buffer and index */
float32_t delay_buffer[DELAY_BUFFER_SIZE];
uint32_t delay_write_index = 0;

/* UART comm buffers */
uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
char uart_tx_buffer[UART_TX_BUFFER_SIZE];
volatile uint8_t uart_rx_index = 0;
volatile uint8_t command_blink_counter = 0;
/* Per-byte UART temporary storage and command-ready flag */
uint8_t uart_rx_byte = 0;
volatile uint8_t uart_command_ready = 0;
