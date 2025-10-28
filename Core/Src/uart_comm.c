/* uart_comm.c
 * UART parsing and callbacks moved out of main.c
 */

#include "main.h"
#include "uart_comm.h"
#include "effects.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Globals are declared in globals.h and included via uart_comm.h
// (uart_rx_buffer, uart_rx_byte, uart_rx_index, uart_command_ready, uart_tx_buffer, command_blink_counter)
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart2;

void Parse_UART_Command(void)
{
  char* cmd = (char*)uart_rx_buffer;
  uint8_t command_received = 0;
  
  if (strncmp(cmd, "VOL:", 4) == 0)
  {
    float vol = atof(cmd + 4);
    if (vol >= 0.0f && vol <= 1.0f)
    {
      output_volume = vol;
      command_received = 1;
      snprintf(uart_tx_buffer, UART_TX_BUFFER_SIZE, "ACK:VOL=%.2f\n", vol);
      if (HAL_UART_Transmit_IT(&huart3, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer)) != HAL_OK)
      {
        // fallback to blocking transmit if IT transmit not available
        HAL_UART_Transmit(&huart3, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer), 100);
      }
    }
  }
  else if (strncmp(cmd, "OVR:", 4) == 0)
  {
    if (strncmp(cmd + 4, "ON", 2) == 0)
    {
      overdrive.enabled = 1;
      command_received = 1;
      const char *msg = "ACK:OVR=ON\n";
      if (HAL_UART_Transmit_IT(&huart3, (uint8_t*)msg, strlen(msg)) != HAL_OK)
      {
        HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), 100);
      }
    }
    else if (strncmp(cmd + 4, "OFF", 3) == 0)
    {
      overdrive.enabled = 0;
      command_received = 1;
      const char *msg = "ACK:OVR=OFF\n";
      if (HAL_UART_Transmit_IT(&huart3, (uint8_t*)msg, strlen(msg)) != HAL_OK)
      {
        HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), 100);
      }
    }
    else
    {
      char params[UART_RX_BUFFER_SIZE];
      strncpy(params, cmd + 4, sizeof(params));
      params[sizeof(params) - 1] = '\0';

      char *saveptr = NULL;
      char *token = strtok_r(params, ",", &saveptr);
      uint8_t parsed = 0;
      float gain = overdrive.gain;
      float threshold = overdrive.threshold;
      float tone = overdrive.tone;
      float mix = overdrive.mix;
      int mode = overdrive.mode;

      if (token)
      {
        gain = atof(token);
        parsed++;
        token = strtok_r(NULL, ",", &saveptr);
      }
      if (token)
      {
        threshold = atof(token);
        parsed++;
        token = strtok_r(NULL, ",", &saveptr);
      }
      if (token)
      {
        tone = atof(token);
        parsed++;
        token = strtok_r(NULL, ",", &saveptr);
      }
      if (token)
      {
        mix = atof(token);
        parsed++;
        token = strtok_r(NULL, ",", &saveptr);
      }
      if (token)
      {
        mode = atoi(token);
        parsed++;
      }

      if (parsed >= 3)
      {
        if (gain >= 1.0f && gain <= 100.0f) overdrive.gain = gain;
        if (threshold >= 0.1f && threshold <= 0.95f) overdrive.threshold = threshold;
        if (tone >= 0.0f && tone <= 1.0f) overdrive.tone = tone;
        if (parsed >= 4 && mix >= 0.0f && mix <= 1.0f) overdrive.mix = mix;
        if (parsed >= 5 && mode >= 0 && mode <= 2) overdrive.mode = mode;

        command_received = 1;
        snprintf(uart_tx_buffer, UART_TX_BUFFER_SIZE,
                 "ACK:OVR=%.1f,%.2f,%.2f,%.2f,%d\n",
                 overdrive.gain, overdrive.threshold, overdrive.tone, overdrive.mix, overdrive.mode);
        if (HAL_UART_Transmit_IT(&huart3, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer)) != HAL_OK)
        {
          HAL_UART_Transmit(&huart3, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer), 100);
        }
      }
    }
  }
  else if (strncmp(cmd, "DLY:", 4) == 0)
  {
    if (strncmp(cmd + 4, "ON", 2) == 0)
    {
      delay_effect.enabled = 1;
      command_received = 1;
      const char *msg = "ACK:DLY=ON\n";
      if (HAL_UART_Transmit_IT(&huart3, (uint8_t*)msg, strlen(msg)) != HAL_OK)
      {
        HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), 100);
      }
    }
    else if (strncmp(cmd + 4, "OFF", 3) == 0)
    {
      delay_effect.enabled = 0;
      command_received = 1;
      const char *msg = "ACK:DLY=OFF\n";
      if (HAL_UART_Transmit_IT(&huart3, (uint8_t*)msg, strlen(msg)) != HAL_OK)
      {
        HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), 100);
      }
    }
    else
    {
      char params[UART_RX_BUFFER_SIZE];
      strncpy(params, cmd + 4, sizeof(params));
      params[sizeof(params) - 1] = '\0';

      char *saveptr = NULL;
      char *token = strtok_r(params, ",", &saveptr);
      uint8_t parsed = 0;
      float time_ms = (float)delay_effect.delay_samples * (1000.0f / SAMPLE_RATE);
      float feedback = delay_effect.feedback;
      float mix = delay_effect.mix;
      float tone = delay_effect.tone;

      if (token)
      {
        time_ms = atof(token);
        parsed++;
        token = strtok_r(NULL, ",", &saveptr);
      }
      if (token)
      {
        feedback = atof(token);
        parsed++;
        token = strtok_r(NULL, ",", &saveptr);
      }
      if (token)
      {
        mix = atof(token);
        parsed++;
        token = strtok_r(NULL, ",", &saveptr);
      }
      if (token)
      {
        tone = atof(token);
        parsed++;
      }

      if (parsed >= 3)
      {
        uint32_t samples = (uint32_t)((time_ms / 1000.0f) * SAMPLE_RATE);
        if (samples > 0 && samples <= DELAY_BUFFER_SIZE)
        {
          delay_effect.delay_samples = samples;
        }
        else
        {
          delay_effect.delay_samples = DELAY_BUFFER_SIZE;
        }
        if (feedback >= 0.0f && feedback <= 0.95f) delay_effect.feedback = feedback;
        if (mix >= 0.0f && mix <= 1.0f) delay_effect.mix = mix;
        if (parsed >= 4 && tone >= 0.0f && tone <= 1.0f) delay_effect.tone = tone;

        command_received = 1;
        snprintf(uart_tx_buffer, UART_TX_BUFFER_SIZE,
                 "ACK:DLY=%.0fms,%.2f,%.2f,%.2f\n",
                 time_ms, delay_effect.feedback, delay_effect.mix, delay_effect.tone);
        if (HAL_UART_Transmit_IT(&huart3, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer)) != HAL_OK)
        {
          HAL_UART_Transmit(&huart3, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer), 100);
        }
      }
    }
  }
  else if (strncmp(cmd, "GATE:", 5) == 0)
  {
    if (strncmp(cmd + 5, "ON", 2) == 0)
    {
      noise_gate.enabled = 1;
      command_received = 1;
      const char *msg = "ACK:GATE=ON\n";
      if (HAL_UART_Transmit_IT(&huart3, (uint8_t*)msg, strlen(msg)) != HAL_OK)
      {
        HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), 100);
      }
    }
    else if (strncmp(cmd + 5, "OFF", 3) == 0)
    {
      noise_gate.enabled = 0;
      command_received = 1;
      const char *msg = "ACK:GATE=OFF\n";
      if (HAL_UART_Transmit_IT(&huart3, (uint8_t*)msg, strlen(msg)) != HAL_OK)
      {
        HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), 100);
      }
    }
    else
    {
      char params[UART_RX_BUFFER_SIZE];
      strncpy(params, cmd + 5, sizeof(params));
      params[sizeof(params) - 1] = '\0';

      char *saveptr = NULL;
      char *token = strtok_r(params, ",", &saveptr);
      uint8_t parsed = 0;
      float threshold = noise_gate.threshold;
      float attack = noise_gate.attack_time;
      float release = noise_gate.release_time;

      if (token)
      {
        threshold = atof(token);
        parsed++;
        token = strtok_r(NULL, ",", &saveptr);
      }
      if (token)
      {
        attack = atof(token);
        parsed++;
        token = strtok_r(NULL, ",", &saveptr);
      }
      if (token)
      {
        release = atof(token);
        parsed++;
      }

      if (parsed >= 1)
      {
        if (threshold >= 0.001f && threshold <= 0.5f) noise_gate.threshold = threshold;
        if (parsed >= 2 && attack >= 0.0001f && attack <= 0.1f) noise_gate.attack_time = attack;
        if (parsed >= 3 && release >= 0.01f && release <= 1.0f) noise_gate.release_time = release;

        command_received = 1;
        snprintf(uart_tx_buffer, UART_TX_BUFFER_SIZE,
                 "ACK:GATE=%.3f,%.4f,%.2f\n",
                 noise_gate.threshold, noise_gate.attack_time, noise_gate.release_time);
        if (HAL_UART_Transmit_IT(&huart3, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer)) != HAL_OK)
        {
          HAL_UART_Transmit(&huart3, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer), 100);
        }
      }
    }
  }
  else if (strncmp(cmd, "STATUS", 6) == 0)
  {
    snprintf(uart_tx_buffer, UART_TX_BUFFER_SIZE,
             "VOL:%.2f,OVR:%d,DLY:%d,GATE:%d\n",
             output_volume, overdrive.enabled, delay_effect.enabled, noise_gate.enabled);
    if (HAL_UART_Transmit_IT(&huart3, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer)) != HAL_OK)
    {
      HAL_UART_Transmit(&huart3, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer), 100);
    }
  }

  if (command_received)
  {
    command_blink_counter = 6;
  }

  memset(uart_rx_buffer, 0, UART_RX_BUFFER_SIZE);
  uart_rx_index = 0;
}

void Send_UART_Response(const char* msg)
{
  /* Prefer sending responses back to the ESP32 on USART3 (huart3).
   * huart2 is left available for host/console messages if needed. */
  snprintf(uart_tx_buffer, UART_TX_BUFFER_SIZE, "%s\n", msg);
  if (HAL_UART_Transmit_IT(&huart3, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer)) != HAL_OK)
  {
    HAL_UART_Transmit(&huart3, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer), 100);
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART3)
  {
    if (uart_rx_byte == '\n' || uart_rx_byte == '\r')
    {
      if (uart_rx_index > 0)
      {
        uart_rx_buffer[uart_rx_index] = '\0';
        uart_command_ready = 1;
      }
    }
    else if (uart_rx_index < UART_RX_BUFFER_SIZE - 1)
    {
      uart_rx_buffer[uart_rx_index++] = uart_rx_byte;
    }
    HAL_UART_Receive_IT(&huart3, &uart_rx_byte, 1);
  }
}

/**
 * @brief Called when UART transmit completes (ACK sent)
 * Disabled: TX callback LED blinks were causing spurious LED activity
 * when powered via USB (likely from noise/enumeration signals).
 * Keeping callback present but empty for future diagnostics if needed.
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART3)
  {
    /* TX complete - no action */
  }
}

/**
 * @brief Called on UART error (useful for diagnostics)
 * Disabled: Error callback LED blinks were causing spurious LED activity
 * when powered via USB. Keeping callback present but empty for future use.
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART3)
  {
    /* UART error - no action for now */
  }
}
