/* dsp_core.c
 * Real-time audio processing and timer callback
 */

#include "main.h"
#include "dsp_core.h"
#include "effects.h"
#include "peripherals.h"
#include <math.h>

// Bring in globals
extern ADC_HandleTypeDef hadc1;
extern DAC_HandleTypeDef hdac1;

extern uint16_t adc_buffer[];
extern uint16_t dac_buffer[];
extern volatile uint8_t process_audio_flag;
extern volatile uint16_t buffer_index;

void Process_Guitar_Signal(void)
{
  uint16_t i;
  float32_t normalized_input;
  float32_t processed_signal;
  
  for (i = 0; i < BUFFER_SIZE; i++)
  {
    normalized_input = ((float32_t)adc_buffer[i] / (ADC_MAX_VALUE / 2.0f)) - 1.0f;
    processed_signal = Apply_Distortion(normalized_input);
    processed_signal *= output_volume;
    if (processed_signal > 1.0f) processed_signal = 1.0f;
    if (processed_signal < -1.0f) processed_signal = -1.0f;
    dac_buffer[i] = (uint16_t)((processed_signal + 1.0f) * (DAC_MAX_VALUE / 2.0f));
  }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM1)
  {
    HAL_ADC_Start(&hadc1);

    if (HAL_ADC_PollForConversion(&hadc1, 1) == HAL_OK)
    {
      uint16_t adc_value = HAL_ADC_GetValue(&hadc1);

      float32_t normalized_input = ((float32_t)adc_value - 2048.0f) / 2048.0f;
      float32_t processed_signal = Apply_NoiseGate(normalized_input);
      processed_signal = Apply_Overdrive(processed_signal);
      processed_signal = Apply_Delay(processed_signal);
      processed_signal *= output_volume;
      if (processed_signal > 1.0f) processed_signal = 1.0f;
      if (processed_signal < -1.0f) processed_signal = -1.0f;

      uint16_t dac_value = (uint16_t)((processed_signal * 2048.0f) + 2048.0f);
      if (dac_value > 4095) dac_value = 4095;
      if (dac_value < 0) dac_value = 0;

      HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, dac_value);

      adc_buffer[buffer_index] = adc_value;
      buffer_index++;
      if (buffer_index >= BUFFER_SIZE)
      {
        buffer_index = 0;
      }
    }

    HAL_ADC_Stop(&hadc1);
  }
}
