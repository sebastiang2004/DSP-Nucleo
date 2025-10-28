/* effects.h
 * Declarations for audio effects and their state
 */
#ifndef EFFECTS_H
#define EFFECTS_H

#include "main.h"
#include "globals.h"

typedef struct {
  float32_t gain;
  float32_t threshold;
  float32_t tone;
  float32_t mix;
  uint8_t mode;
  uint8_t enabled;
  float32_t hp_state;
  float32_t lp_state;
} Overdrive_t;

typedef struct {
  uint32_t delay_samples;
  float32_t feedback;
  float32_t mix;
  float32_t tone;
  uint8_t enabled;
  float32_t lp_state;
} Delay_t;

typedef struct {
  float32_t threshold;
  float32_t attack_time;
  float32_t release_time;
  float32_t envelope;
  uint8_t enabled;
} NoiseGate_t;

extern Overdrive_t overdrive;
extern Delay_t delay_effect;
extern NoiseGate_t noise_gate;

// Distortion/backwards compatibility
extern float32_t distortion_gain;
extern float32_t distortion_threshold;
extern float32_t output_volume;

// Effect processing functions
float32_t Apply_Distortion(float32_t input);
float32_t Apply_Overdrive(float32_t input);
float32_t Apply_Delay(float32_t input);
float32_t Apply_NoiseGate(float32_t input);

#endif // EFFECTS_H
