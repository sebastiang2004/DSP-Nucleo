/* dsp_core.h
 * DSP core declarations (buffers and processing entrypoints)
 */
#ifndef DSP_CORE_H
#define DSP_CORE_H

#include "main.h"
#include "globals.h"

extern uint16_t adc_buffer[BUFFER_SIZE];
extern uint16_t dac_buffer[BUFFER_SIZE];
extern volatile uint8_t process_audio_flag;
extern volatile uint16_t buffer_index;

void Process_Guitar_Signal(void);

#endif // DSP_CORE_H
