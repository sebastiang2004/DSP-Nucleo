/* effects.c
 * Implementations of distortion, overdrive, delay and noise gate
 */

#include "main.h"
#include "effects.h"
#include <math.h>

// Default effect states (moved from main.c)
Overdrive_t overdrive = {
  .gain = 20.0f,
  .threshold = 0.6f,
  .tone = 0.5f,
  .mix = 0.8f,
  .mode = 0,
  .enabled = 0,
  .hp_state = 0.0f,
  .lp_state = 0.0f
};

Delay_t delay_effect = {
  .delay_samples = 2400,
  .feedback = 0.6f,
  .mix = 0.5f,
  .tone = 0.5f,
  .enabled = 0,
  .lp_state = 0.0f
};

NoiseGate_t noise_gate = {
  .threshold = 0.02f,
  .attack_time = 0.001f,
  .release_time = 0.1f,
  .envelope = 0.0f,
  .enabled = 1
};

float32_t distortion_gain = 3.0f;
float32_t distortion_threshold = 0.7f;
float32_t output_volume = 0.8f;

float32_t Apply_Distortion(float32_t input)
{
  float32_t output;
  output = input * distortion_gain;
  if (output > distortion_threshold)
  {
    output = distortion_threshold + (output - distortion_threshold) / (1.0f + fabsf(output - distortion_threshold));
  }
  else if (output < -distortion_threshold)
  {
    output = -distortion_threshold + (output + distortion_threshold) / (1.0f + fabsf(output + distortion_threshold));
  }
  return output;
}

float32_t Apply_Overdrive(float32_t input)
{
  if (!overdrive.enabled) return input;

  float32_t hp_alpha = 0.99f;
  float32_t hp_output = input - overdrive.hp_state;
  overdrive.hp_state = input - hp_alpha * hp_output;

  float32_t gained = hp_output * overdrive.gain;
  float32_t clipped;

  if (overdrive.mode == 0)
  {
    float32_t abs_gained = fabsf(gained);
    if (abs_gained < 0.001f)
    {
      clipped = gained;
    }
    else
    {
      if (gained > 3.0f)
        clipped = 1.0f;
      else if (gained < -3.0f)
        clipped = -1.0f;
      else
        clipped = gained / (1.0f + fabsf(gained) * 0.3f);
    }
  }
  else if (overdrive.mode == 1)
  {
    float32_t abs_gained = fabsf(gained);
    float32_t sign = (gained >= 0.0f) ? 1.0f : -1.0f;
    float32_t th = overdrive.threshold;
    
    if (abs_gained < th)
      clipped = 2.0f * gained;
    else if (abs_gained < 2.0f * th)
    {
      float32_t x = (2.0f - 3.0f * abs_gained / th);
      clipped = sign * (3.0f - x * x) / 3.0f;
    }
    else
      clipped = sign;
  }
  else
  {
    if (gained > 0.0f)
    {
      if (gained > overdrive.threshold)
        clipped = overdrive.threshold + (gained - overdrive.threshold) * 0.1f;
      else
        clipped = gained;
    }
    else
    {
      if (gained < -overdrive.threshold * 1.5f)
        clipped = -overdrive.threshold * 1.5f + (gained + overdrive.threshold * 1.5f) * 0.3f;
      else
        clipped = gained;
    }
  }

  float32_t lp_alpha = 0.3f + overdrive.tone * 0.6f;
  overdrive.lp_state = lp_alpha * clipped + (1.0f - lp_alpha) * overdrive.lp_state;

  float32_t output = overdrive.mix * overdrive.lp_state + (1.0f - overdrive.mix) * input;

  if (output > 0.95f)
    output = 0.95f + (output - 0.95f) * 0.1f;
  else if (output < -0.95f)
    output = -0.95f + (output + 0.95f) * 0.1f;

  if (output > 1.0f) output = 1.0f;
  if (output < -1.0f) output = -1.0f;

  return output;
}

float32_t Apply_Delay(float32_t input)
{
  if (!delay_effect.enabled) return input;

  int32_t delay_read_index = (int32_t)delay_write_index - (int32_t)delay_effect.delay_samples;
  while (delay_read_index < 0)
  {
    delay_read_index += DELAY_BUFFER_SIZE;
  }

  float32_t delayed_sample = delay_buffer[delay_read_index];

  float32_t tone_alpha = 0.2f + delay_effect.tone * 0.7f;
  delay_effect.lp_state = tone_alpha * delayed_sample + (1.0f - tone_alpha) * delay_effect.lp_state;

  float32_t filtered_delay = delay_effect.lp_state;

  float32_t feedback_signal = filtered_delay * delay_effect.feedback;
  if (feedback_signal > 0.95f)
    feedback_signal = 0.95f;
  else if (feedback_signal < -0.95f)
    feedback_signal = -0.95f;

  delay_buffer[delay_write_index] = input + feedback_signal;

  if (delay_buffer[delay_write_index] > 1.0f)
    delay_buffer[delay_write_index] = 1.0f;
  else if (delay_buffer[delay_write_index] < -1.0f)
    delay_buffer[delay_write_index] = -1.0f;

  delay_write_index++;
  if (delay_write_index >= DELAY_BUFFER_SIZE)
  {
    delay_write_index = 0;
  }

  float32_t dry_gain = 1.0f - delay_effect.mix;
  float32_t wet_gain = delay_effect.mix;

  return (input * dry_gain) + (delayed_sample * wet_gain);
}

float32_t Apply_NoiseGate(float32_t input)
{
  if (!noise_gate.enabled) return input;

  float32_t input_level = fabsf(input);

  float32_t attack_coeff = 1.0f - (1.0f / (noise_gate.attack_time * SAMPLE_RATE));
  float32_t release_coeff = 1.0f - (1.0f / (noise_gate.release_time * SAMPLE_RATE));

  if (attack_coeff < 0.0f) attack_coeff = 0.0f;
  if (attack_coeff >= 1.0f) attack_coeff = 0.999f;
  if (release_coeff < 0.0f) release_coeff = 0.0f;
  if (release_coeff >= 1.0f) release_coeff = 0.999f;

  if (input_level > noise_gate.envelope)
  {
    noise_gate.envelope += (input_level - noise_gate.envelope) * (1.0f - attack_coeff);
  }
  else
  {
    noise_gate.envelope += (input_level - noise_gate.envelope) * (1.0f - release_coeff);
  }

  float32_t gate_gain;

  if (noise_gate.envelope > noise_gate.threshold * 1.2f)
  {
    gate_gain = 1.0f;
  }
  else if (noise_gate.envelope < noise_gate.threshold * 0.8f)
  {
    gate_gain = 0.0f;
  }
  else
  {
    float32_t range = noise_gate.threshold * 0.4f;
    float32_t position = (noise_gate.envelope - noise_gate.threshold * 0.8f) / range;
    gate_gain = position * position * (3.0f - 2.0f * position);
  }

  return input * gate_gain;
}
