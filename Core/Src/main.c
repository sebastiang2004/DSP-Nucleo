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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BUFFER_SIZE 128        // Audio buffer size
#define SAMPLE_RATE 48000      // 48kHz sampling rate
#define ADC_MAX_VALUE 4095.0f  // 12-bit ADC max value
#define DAC_MAX_VALUE 4095.0f  // 12-bit DAC max value
#define ADC_VREF 3.3f          // ADC reference voltage
#define DELAY_BUFFER_SIZE 4800 // 100ms max delay at 48kHz (4800 * 4 bytes = 19.2KB)
#define UART_RX_BUFFER_SIZE 64
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

DAC_HandleTypeDef hdac1;

OPAMP_HandleTypeDef hopamp1;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;  // Add USART3 for ESP32 communication

/* USER CODE BEGIN PV */
// Audio processing buffers
uint16_t adc_buffer[BUFFER_SIZE];
uint16_t dac_buffer[BUFFER_SIZE];
float32_t input_signal[BUFFER_SIZE];
float32_t output_signal[BUFFER_SIZE];

// Delay effect buffer
float32_t delay_buffer[DELAY_BUFFER_SIZE];
uint32_t delay_write_index = 0;
uint32_t delay_samples = 2400;  // 50ms default delay at 48kHz
float32_t delay_feedback = 0.3f;
float32_t delay_mix = 0.5f;

// Effect parameters
typedef struct {
  float32_t gain;           // Overdrive/distortion gain (1.0 - 100.0)
  float32_t threshold;      // Clipping threshold (0.3 - 0.9)
  float32_t tone;           // Tone control (0.0 - 1.0) - post-distortion low-pass
  float32_t mix;            // Wet/dry mix (0.0 - 1.0)
  uint8_t mode;             // 0=soft clip, 1=hard clip, 2=asymmetric
  uint8_t enabled;          // Effect on/off
  // State variables for filters
  float32_t hp_state;       // High-pass filter state (removes rumble)
  float32_t lp_state;       // Low-pass filter state (smooths harshness)
} Overdrive_t;

typedef struct {
  uint32_t delay_samples;   // Delay time in samples
  float32_t feedback;       // Feedback amount (0.0 - 0.95)
  float32_t mix;            // Wet/dry mix (0.0 - 1.0)
  float32_t tone;           // Tone/damping (0.0 - 1.0) - filters repeats
  uint8_t enabled;          // Effect on/off
  // State variable for tone filter
  float32_t lp_state;       // Low-pass filter state for damping
} Delay_t;

// NOISE GATE - Removes noise when not playing
typedef struct {
  float32_t threshold;      // Gate opens above this level (0.001 - 0.5)
  float32_t attack_time;    // How fast gate opens in seconds (0.001 - 0.01)
  float32_t release_time;   // How fast gate closes in seconds (0.05 - 0.5)
  float32_t envelope;       // Current envelope follower value
  uint8_t enabled;          // Effect on/off
} NoiseGate_t;

Overdrive_t overdrive = {
  .gain = 20.0f,           // Moderate gain (increased for better effect)
  .threshold = 0.6f,       // Clip at 60% amplitude
  .tone = 0.5f,            // Mid tone (now functional)
  .mix = 0.8f,             // 80% wet / 20% dry
  .mode = 0,               // Soft clipping (smoother)
  .enabled = 0,            // DISABLED by default - enable from web
  .hp_state = 0.0f,
  .lp_state = 0.0f
};

Delay_t delay_effect = {
  .delay_samples = 2400,   // 50ms delay (fits inside 4800-sample buffer)
  .feedback = 0.6f,        // Strong feedback for multiple echoes
  .mix = 0.5f,             // 50% wet signal - balanced mix
  .tone = 0.5f,            // Mid tone (filters high frequencies on repeats)
  .enabled = 0,
  .lp_state = 0.0f
};

NoiseGate_t noise_gate = {
  .threshold = 0.02f,      // Opens at 2% of max signal (~-34dB)
  .attack_time = 0.001f,   // 1ms attack (very fast)
  .release_time = 0.1f,    // 100ms release (smooth fade out)
  .envelope = 0.0f,
  .enabled = 1             // ENABLED by default - helps with noise
};

// Distortion parameters (kept for backward compatibility)
float32_t distortion_gain = 3.0f;      // Distortion drive (1.0 = clean, higher = more distortion)
float32_t distortion_threshold = 0.7f; // Soft clipping threshold
float32_t output_volume = 0.8f;        // Output volume control

// Processing flags
volatile uint8_t process_audio_flag = 0;
volatile uint16_t buffer_index = 0;
volatile uint8_t command_blink_counter = 0;  // For LED blinking on command

// LED blink counter
volatile uint32_t led_blink_counter = 0;

// Signal detection
volatile uint8_t signal_detected = 0;
volatile uint16_t last_adc_value = 0;
volatile uint16_t adc_baseline = 2048;  // Will be calibrated at startup
volatile uint16_t adc_noise_level = 0;
volatile uint16_t current_adc_value = 0;  // For monitoring
volatile uint16_t max_adc_deviation = 0;  // Track maximum deviation
volatile uint32_t interrupt_counter = 0;  // Count timer interrupts
#define ADC_THRESHOLD 150  // Increased threshold to avoid false triggers

// UART communication
uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
uint8_t uart_rx_byte;
uint8_t uart_rx_index = 0;
volatile uint8_t uart_command_ready = 0;
char uart_tx_buffer[128];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_DAC1_Init(void);
static void MX_OPAMP1_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);  // Add USART3 init
/* USER CODE BEGIN PFP */
void Process_Guitar_Signal(void);
float32_t Apply_Distortion(float32_t input);
float32_t Apply_Overdrive(float32_t input);
float32_t Apply_Delay(float32_t input);
void TIM1_Config_For_Sampling(void);
void Parse_UART_Command(void);
void Send_UART_Response(const char* msg);
float32_t Apply_NoiseGate(float32_t input);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_DAC1_Init();
  MX_OPAMP1_Init();
  MX_TIM1_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();  // Initialize USART3 for ESP32
  /* USER CODE BEGIN 2 */
  
  // **Clear UART buffers and state variables**
  memset(uart_rx_buffer, 0, UART_RX_BUFFER_SIZE);
  uart_rx_index = 0;
  uart_rx_byte = 0;
  uart_command_ready = 0;
  
  // Initialize delay buffer to zero
  memset(delay_buffer, 0, sizeof(delay_buffer));
  delay_write_index = 0;
  
  // **ENSURE CLEAN SIGNAL ON STARTUP**
  // Make sure all effects are disabled and filter states are zeroed
  overdrive.enabled = 0;
  overdrive.hp_state = 0.0f;
  overdrive.lp_state = 0.0f;
  delay_effect.enabled = 0;
  delay_effect.lp_state = 0.0f;
  noise_gate.enabled = 1;  // Only gate is enabled
  noise_gate.envelope = 0.0f;
  output_volume = 0.8f;
  
  // **STARTUP LED PATTERN - 5 fast blinks to show STM32 is running**
  for(int i = 0; i < 5; i++)
  {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    HAL_Delay(100);
  }
  
  // **Enable USART3 interrupt in NVIC for ESP32 communication**
  // CRITICAL: UART must have HIGHER priority than audio processing timer
  // so it can respond to commands even during heavy DSP load
  HAL_NVIC_SetPriority(USART3_IRQn, 0, 0);  // Highest priority
  HAL_NVIC_EnableIRQ(USART3_IRQn);
  
  // **Flush any garbage in UART before starting**
  HAL_Delay(100);
  __HAL_UART_FLUSH_DRREGISTER(&huart3);
  __HAL_UART_CLEAR_FLAG(&huart3, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_PEF | UART_CLEAR_FEF);
  
  // Start UART reception interrupt on USART3
  HAL_UART_Receive_IT(&huart3, &uart_rx_byte, 1);
  
  // Start OPAMP
  HAL_OPAMP_Start(&hopamp1);
  
  // Calibrate ADC (use DIFFERENTIAL mode since ADC is configured as differential)
  HAL_ADCEx_Calibration_Start(&hadc1, ADC_DIFFERENTIAL_ENDED);
  
  // Start DAC
  HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
  
  // Calibrate baseline ADC value (read several samples)
  HAL_ADC_Start(&hadc1);
  HAL_Delay(10);
  uint32_t adc_sum = 0;
  uint16_t adc_min = 4095;
  uint16_t adc_max = 0;
  
  for(int i = 0; i < 32; i++)
  {
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint16_t sample = HAL_ADC_GetValue(&hadc1);
    adc_sum += sample;
    
    if(sample < adc_min) adc_min = sample;
    if(sample > adc_max) adc_max = sample;
    
    HAL_ADC_Stop(&hadc1);
    HAL_Delay(1);
  }
  adc_baseline = adc_sum / 32;
  adc_noise_level = (adc_max - adc_min) / 2;  // Measure noise
  HAL_ADC_Stop(&hadc1);
  
  // Configure Timer for 48kHz sampling rate
  TIM1_Config_For_Sampling();
  
  // Enable TIM1 interrupt in NVIC
  // IMPORTANT: Timer has LOWER priority than UART so commands can be processed
  // even during heavy audio processing
  HAL_NVIC_SetPriority(TIM1_UP_TIM16_IRQn, 1, 0);  // Lower priority than UART
  HAL_NVIC_EnableIRQ(TIM1_UP_TIM16_IRQn);
  
  // Start Timer interrupt
  HAL_TIM_Base_Start_IT(&htim1);  // Re-enabled
  
  // Simple LED test - blink 3 times at startup to show baseline calibration done
  for(int i = 0; i < 6; i++)  // 6 toggles = 3 blinks
  {
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    HAL_Delay(200);
  }
  
  // Turn LED OFF initially - will be toggled by timer interrupt
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
  
  // Small delay before starting interrupts
  HAL_Delay(1000);

  // **Send READY signal to ESP32 once initialization is truly complete**
  const char *ready_msg = "STM32_READY\n";
  HAL_UART_Transmit(&huart3, (uint8_t*)ready_msg, strlen(ready_msg), 100);

  // Brief LED pulse to show comms are live
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
  HAL_Delay(100);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    // Process UART commands
    if (uart_command_ready)
    {
      uart_command_ready = 0;
      Parse_UART_Command();
    }
    
    // Handle command confirmation blinks (non-blocking)
    if (command_blink_counter > 0)
    {
      static uint32_t last_blink_time = 0;
      if (HAL_GetTick() - last_blink_time >= 100)  // Toggle every 100ms
      {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        command_blink_counter--;
        last_blink_time = HAL_GetTick();
      }
    }
    
    // Process audio when flag is set (if needed)
    if (process_audio_flag)
    {
      process_audio_flag = 0;
      // Process_Guitar_Signal();  // Not needed, processing is in interrupt
    }
    
    // LED is controlled by timer interrupt now (no delay here!)
    
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
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
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
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

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.GainCompensation = 0;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_DIFFERENTIAL_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief DAC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC1_Init(void)
{

  /* USER CODE BEGIN DAC1_Init 0 */

  /* USER CODE END DAC1_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC1_Init 1 */

  /* USER CODE END DAC1_Init 1 */

  /** DAC Initialization
  */
  hdac1.Instance = DAC1;
  if (HAL_DAC_Init(&hdac1) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT1 config
  */
  sConfig.DAC_HighFrequency = DAC_HIGH_FREQUENCY_INTERFACE_MODE_AUTOMATIC;
  sConfig.DAC_DMADoubleDataMode = DISABLE;
  sConfig.DAC_SignedFormat = DISABLE;
  sConfig.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
  sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
  sConfig.DAC_Trigger2 = DAC_TRIGGER_NONE;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_EXTERNAL;
  sConfig.DAC_UserTrimming = DAC_TRIMMING_FACTORY;
  if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC1_Init 2 */

  /* USER CODE END DAC1_Init 2 */

}

/**
  * @brief OPAMP1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_OPAMP1_Init(void)
{

  /* USER CODE BEGIN OPAMP1_Init 0 */

  /* USER CODE END OPAMP1_Init 0 */

  /* USER CODE BEGIN OPAMP1_Init 1 */

  /* USER CODE END OPAMP1_Init 1 */
  hopamp1.Instance = OPAMP1;
  hopamp1.Init.PowerMode = OPAMP_POWERMODE_NORMALSPEED;
  hopamp1.Init.Mode = OPAMP_PGA_MODE;
  hopamp1.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO2;
  hopamp1.Init.InternalOutput = ENABLE;
  hopamp1.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp1.Init.PgaConnect = OPAMP_PGA_CONNECT_INVERTINGINPUT_NO;
  hopamp1.Init.PgaGain = OPAMP_PGA_GAIN_32_OR_MINUS_31;
  hopamp1.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP1_Init 2 */

  /* USER CODE END OPAMP1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief USART3 Initialization Function (for ESP32)
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  
  // LED is already configured by CubeMX above as PA5 (GPIO_PIN_5)
  // USART3 pins (PC10/PC11) are configured automatically in HAL_UART_MspInit()

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/**
  * @brief  Configure Timer1 for audio sampling rate (48kHz)
  * @param  None
  * @retval None
  */
void TIM1_Config_For_Sampling(void)
{
  // System clock is 170MHz (from HSI->PLL configuration)
  // Timer prescaler and period calculation for 48kHz:
  // Timer frequency = 170MHz
  // Desired interrupt frequency = 48kHz
  // ARR = (170,000,000 / 48,000) - 1 = 3541.67 ≈ 3541
  
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 3541;  // 48kHz sampling rate
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  Apply distortion effect to input signal
  * @param  input: Input sample value (-1.0 to 1.0)
  * @retval Distorted output sample
  */
float32_t Apply_Distortion(float32_t input)
{
  float32_t output;
  
  // Apply gain for distortion drive
  output = input * distortion_gain;
  
  // Soft clipping (hyperbolic tangent approximation)
  // This creates a smooth saturation curve
  if (output > distortion_threshold)
  {
    output = distortion_threshold + (output - distortion_threshold) / (1.0f + fabsf(output - distortion_threshold));
  }
  else if (output < -distortion_threshold)
  {
    output = -distortion_threshold + (output + distortion_threshold) / (1.0f + fabsf(output + distortion_threshold));
  }
  
  // Alternative: Hard clipping (more aggressive)
  // arm_clip_f32(&output, &output, -1.0f, 1.0f, 1);
  
  return output;
}

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
float32_t Apply_Overdrive(float32_t input)
{
  if (!overdrive.enabled) return input;

  // 1. INPUT HIGH-PASS FILTER (removes DC offset and rumble below 80Hz)
  //    This prevents "muddy" bass from distorting
  //    Formula: y[n] = alpha * (y[n-1] + x[n] - x[n-1])
  //    alpha = 0.99 gives ~80Hz cutoff at 48kHz sample rate
  float32_t hp_alpha = 0.99f;
  float32_t hp_output = input - overdrive.hp_state;
  overdrive.hp_state = input - hp_alpha * hp_output;
  
  // 2. APPLY GAIN
  float32_t gained = hp_output * overdrive.gain;
  
  // 3. CLIPPING STAGE - Multiple modes for different tones
  float32_t clipped;
  
  if (overdrive.mode == 0)  // SOFT CLIPPING (smooth, tube-like)
  {
    // Fast soft clipping using tanh approximation
    // Much faster than expf() while still smooth
    float32_t abs_gained = fabsf(gained);
    if (abs_gained < 0.001f)
    {
      clipped = gained;  // Linear region for small signals
    }
    else
    {
      // Fast tanh approximation: tanh(x) ≈ x / (1 + |x|)
      // Even faster: use simple rational function
      if (gained > 3.0f)
        clipped = 1.0f;
      else if (gained < -3.0f)
        clipped = -1.0f;
      else
        clipped = gained / (1.0f + fabsf(gained) * 0.3f);
    }
  }
  else if (overdrive.mode == 1)  // HARD CLIPPING (aggressive, solid-state)
  {
    // Simplified hard clipping without powf() - MUCH faster
    float32_t abs_gained = fabsf(gained);
    float32_t sign = (gained >= 0.0f) ? 1.0f : -1.0f;
    float32_t th = overdrive.threshold;
    
    if (abs_gained < th)
      clipped = 2.0f * gained;
    else if (abs_gained < 2.0f * th)
    {
      // Replace powf() with fast quadratic approximation
      float32_t x = (2.0f - 3.0f * abs_gained / th);
      clipped = sign * (3.0f - x * x) / 3.0f;  // x^2 is just multiplication
    }
    else
      clipped = sign;
  }
  else  // ASYMMETRIC CLIPPING (diode-like, classic overdrive)
  {
    // Different clipping for positive and negative - sounds more "organic"
    if (gained > 0.0f)
    {
      // Positive side: harder clipping (like forward-biased diode)
      if (gained > overdrive.threshold)
        clipped = overdrive.threshold + (gained - overdrive.threshold) * 0.1f;
      else
        clipped = gained;
    }
    else
    {
      // Negative side: softer clipping (like reverse-biased diode)
      if (gained < -overdrive.threshold * 1.5f)
        clipped = -overdrive.threshold * 1.5f + (gained + overdrive.threshold * 1.5f) * 0.3f;
      else
        clipped = gained;
    }
  }
  
  // 4. TONE CONTROL - Adjustable low-pass filter
  //    Removes harsh high-frequency harmonics created by clipping
  //    tone=0 (dark), tone=0.5 (balanced), tone=1 (bright)
  //    Frequency range: ~1kHz to ~8kHz
  float32_t lp_alpha = 0.3f + overdrive.tone * 0.6f;  // Maps 0-1 to 0.3-0.9
  overdrive.lp_state = lp_alpha * clipped + (1.0f - lp_alpha) * overdrive.lp_state;
  
  // 5. WET/DRY MIX - Parallel processing for more natural sound
  //    Preserves some clean signal for clarity
  float32_t output = overdrive.mix * overdrive.lp_state + (1.0f - overdrive.mix) * input;
  
  // 6. SOFT LIMITER - Prevent output from exceeding ±1.0
  //    Smoother than hard clipping
  if (output > 0.95f)
    output = 0.95f + (output - 0.95f) * 0.1f;
  else if (output < -0.95f)
    output = -0.95f + (output + 0.95f) * 0.1f;
  
  // Final hard limit
  if (output > 1.0f) output = 1.0f;
  if (output < -1.0f) output = -1.0f;
  
  return output;
}

/**
  * @brief  Apply delay effect to input signal (OPTIMIZED)
  * @param  input: Input sample value (-1.0 to 1.0)
  * @retval Delayed output sample (mixed with input)
  * @note   Improvements:
  *         - Added tone/damping control for tape-like delay
  *         - Better feedback limiting to prevent runaway
  *         - Interpolation for smoother delay changes (future)
  */
float32_t Apply_Delay(float32_t input)
{
  if (!delay_effect.enabled) return input;
  
  // Calculate read position with proper wraparound
  int32_t delay_read_index = (int32_t)delay_write_index - (int32_t)delay_effect.delay_samples;
  while (delay_read_index < 0)
  {
    delay_read_index += DELAY_BUFFER_SIZE;
  }
  
  // Read delayed sample
  float32_t delayed_sample = delay_buffer[delay_read_index];
  
  // TONE CONTROL - Low-pass filter on feedback path (tape-like delay)
  // This makes each repeat progressively darker, more natural
  // tone=0 (very dark repeats), tone=0.5 (balanced), tone=1 (bright repeats)
  float32_t tone_alpha = 0.2f + delay_effect.tone * 0.7f;  // Maps to 0.2-0.9
  delay_effect.lp_state = tone_alpha * delayed_sample + (1.0f - tone_alpha) * delay_effect.lp_state;
  
  // Use filtered signal for feedback
  float32_t filtered_delay = delay_effect.lp_state;
  
  // FEEDBACK LIMITING - Prevent runaway oscillation
  // Soft limit feedback to prevent clicks/distortion
  float32_t feedback_signal = filtered_delay * delay_effect.feedback;
  if (feedback_signal > 0.95f)
    feedback_signal = 0.95f;
  else if (feedback_signal < -0.95f)
    feedback_signal = -0.95f;
  
  // Write to delay buffer (input + limited feedback)
  delay_buffer[delay_write_index] = input + feedback_signal;
  
  // Additional safety: limit buffer writes
  if (delay_buffer[delay_write_index] > 1.0f)
    delay_buffer[delay_write_index] = 1.0f;
  else if (delay_buffer[delay_write_index] < -1.0f)
    delay_buffer[delay_write_index] = -1.0f;
  
  // Increment write index
  delay_write_index++;
  if (delay_write_index >= DELAY_BUFFER_SIZE)
  {
    delay_write_index = 0;
  }
  
  // Mix dry and wet signals
  // Simple linear mix instead of equal-power crossfade (much faster)
  float32_t dry_gain = 1.0f - delay_effect.mix;
  float32_t wet_gain = delay_effect.mix;
  
  return (input * dry_gain) + (delayed_sample * wet_gain);
}

/**
  * @brief  Apply noise gate to input signal
  * @param  input: Input sample value (-1.0 to 1.0)
  * @retval Gated output sample
  * @note   This should be the FIRST effect in the chain
  *         Prevents noise from being amplified by other effects
  */
float32_t Apply_NoiseGate(float32_t input)
{
  if (!noise_gate.enabled) return input;
  
  // 1. ENVELOPE FOLLOWER - Track signal level
  //    Calculate absolute value (rectification)
  float32_t input_level = fabsf(input);
  
  // 2. ATTACK/RELEASE COEFFICIENTS
  //    Use fast approximations instead of expensive expf()
  //    Original: exp(-1 / (time * sample_rate))
  //    For typical values (attack=0.001, release=0.1, fs=48000):
  //    attack_coeff ≈ 0.9792, release_coeff ≈ 0.9998
  //    
  // Fast approximation: coeff ≈ 1 - (1 / (time * fs))
  float32_t attack_coeff = 1.0f - (1.0f / (noise_gate.attack_time * SAMPLE_RATE));
  float32_t release_coeff = 1.0f - (1.0f / (noise_gate.release_time * SAMPLE_RATE));
  
  // Clamp to valid range [0, 1)
  if (attack_coeff < 0.0f) attack_coeff = 0.0f;
  if (attack_coeff >= 1.0f) attack_coeff = 0.999f;
  if (release_coeff < 0.0f) release_coeff = 0.0f;
  if (release_coeff >= 1.0f) release_coeff = 0.999f;
  
  // 3. UPDATE ENVELOPE with attack/release
  if (input_level > noise_gate.envelope)
  {
    // Attack phase (signal rising) - fast
    noise_gate.envelope += (input_level - noise_gate.envelope) * (1.0f - attack_coeff);
  }
  else
  {
    // Release phase (signal falling) - slow
    noise_gate.envelope += (input_level - noise_gate.envelope) * (1.0f - release_coeff);
  }
  
  // 4. CALCULATE GATE GAIN
  //    Creates smooth transition to avoid clicks
  float32_t gate_gain;
  
  if (noise_gate.envelope > noise_gate.threshold * 1.2f)
  {
    // Well above threshold - gate fully open
    gate_gain = 1.0f;
  }
  else if (noise_gate.envelope < noise_gate.threshold * 0.8f)
  {
    // Well below threshold - gate fully closed
    gate_gain = 0.0f;
  }
  else
  {
    // Transition region - smooth crossfade
    // Linear interpolation between 0.8*threshold and 1.2*threshold
    float32_t range = noise_gate.threshold * 0.4f;  // 1.2 - 0.8 = 0.4
    float32_t position = (noise_gate.envelope - noise_gate.threshold * 0.8f) / range;
    
    // Smooth S-curve for even better transition
    gate_gain = position * position * (3.0f - 2.0f * position);  // Smoothstep
  }
  
  // 5. APPLY GATE
  return input * gate_gain;
}

/**
  * @brief  Parse UART command from ESP32
  * @param  None
  * @retval None
  */
void Parse_UART_Command(void)
{
  // Command format: CMD:PARAM1,PARAM2,PARAM3\n
  // Examples:
  // VOL:0.8\n         - Set volume to 0.8
  // OVR:5.0,0.7,0.5\n - Set overdrive gain=5.0, threshold=0.7, tone=0.5
  // OVR:ON\n          - Enable overdrive
  // OVR:OFF\n         - Disable overdrive
  // DLY:200,0.3,0.5\n - Set delay time=200ms, feedback=0.3, mix=0.5
  // DLY:ON\n          - Enable delay
  // DLY:OFF\n         - Disable delay
  // GATE:0.02,0.001,0.1\n - Set gate threshold=0.02, attack=0.001s, release=0.1s
  // GATE:ON\n         - Enable noise gate
  // GATE:OFF\n        - Disable noise gate
  
  char* cmd = (char*)uart_rx_buffer;
  uint8_t command_received = 0;
  
  // LED blink to show command was received (visual feedback)
  HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
  
  if (strncmp(cmd, "VOL:", 4) == 0)
  {
    float vol = atof(cmd + 4);
    if (vol >= 0.0f && vol <= 1.0f)
    {
      output_volume = vol;
      command_received = 1;
      // Send confirmation back to ESP32
      snprintf(uart_tx_buffer, sizeof(uart_tx_buffer), "ACK:VOL=%.2f\n", vol);
      HAL_UART_Transmit(&huart3, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer), 100);
    }
  }
  else if (strncmp(cmd, "OVR:", 4) == 0)
  {
    if (strncmp(cmd + 4, "ON", 2) == 0)
    {
      overdrive.enabled = 1;
      command_received = 1;
      // Send confirmation
      const char *msg = "ACK:OVR=ON\n";
      HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), 100);
    }
    else if (strncmp(cmd + 4, "OFF", 3) == 0)
    {
      overdrive.enabled = 0;
      command_received = 1;
      // Send confirmation
      const char *msg = "ACK:OVR=OFF\n";
      HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), 100);
    }
    else
    {
      // Parse parameters: gain,threshold,tone,mix,mode
      // Example: OVR:20.0,0.6,0.5,0.8,0
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

      if (parsed >= 3)  // At least gain, threshold, tone
      {
        if (gain >= 1.0f && gain <= 100.0f) overdrive.gain = gain;
        if (threshold >= 0.1f && threshold <= 0.95f) overdrive.threshold = threshold;
        if (tone >= 0.0f && tone <= 1.0f) overdrive.tone = tone;
        if (parsed >= 4 && mix >= 0.0f && mix <= 1.0f) overdrive.mix = mix;
        if (parsed >= 5 && mode >= 0 && mode <= 2) overdrive.mode = mode;

        // **DON'T auto-enable - let the user control on/off explicitly**
        // Parameters can be set while effect is disabled (for preset loading)
        
        command_received = 1;
        // Send confirmation
        snprintf(uart_tx_buffer, sizeof(uart_tx_buffer), 
                 "ACK:OVR=%.1f,%.2f,%.2f,%.2f,%d\n", 
                 overdrive.gain, overdrive.threshold, overdrive.tone, overdrive.mix, overdrive.mode);
        HAL_UART_Transmit(&huart3, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer), 100);
      }
    }
  }
  else if (strncmp(cmd, "DLY:", 4) == 0)
  {
    if (strncmp(cmd + 4, "ON", 2) == 0)
    {
      delay_effect.enabled = 1;
      command_received = 1;
      // Send confirmation
      const char *msg = "ACK:DLY=ON\n";
      HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), 100);
    }
    else if (strncmp(cmd + 4, "OFF", 3) == 0)
    {
      delay_effect.enabled = 0;
      command_received = 1;
      // Send confirmation
      const char *msg = "ACK:DLY=OFF\n";
      HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), 100);
    }
    else
    {
      // Parse parameters: time_ms,feedback,mix,tone
      // Example: DLY:250,0.5,0.6,0.4
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

      if (parsed >= 3)  // At least time, feedback, mix
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
        
        // **DON'T auto-enable - let the user control on/off explicitly**
        
        command_received = 1;
        // Send confirmation
        snprintf(uart_tx_buffer, sizeof(uart_tx_buffer), 
                 "ACK:DLY=%.0fms,%.2f,%.2f,%.2f\n", 
                 time_ms, delay_effect.feedback, delay_effect.mix, delay_effect.tone);
        HAL_UART_Transmit(&huart3, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer), 100);
      }
    }
  }
  else if (strncmp(cmd, "GATE:", 5) == 0)
  {
    if (strncmp(cmd + 5, "ON", 2) == 0)
    {
      noise_gate.enabled = 1;
      command_received = 1;
      // Send confirmation
      const char *msg = "ACK:GATE=ON\n";
      HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), 100);
    }
    else if (strncmp(cmd + 5, "OFF", 3) == 0)
    {
      noise_gate.enabled = 0;
      command_received = 1;
      // Send confirmation
      const char *msg = "ACK:GATE=OFF\n";
      HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), 100);
    }
    else
    {
      // Parse parameters: threshold,attack,release
      // Example: GATE:0.02,0.001,0.1
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
        // Send confirmation
        snprintf(uart_tx_buffer, sizeof(uart_tx_buffer), 
                 "ACK:GATE=%.3f,%.4f,%.2f\n", 
                 noise_gate.threshold, noise_gate.attack_time, noise_gate.release_time);
        HAL_UART_Transmit(&huart3, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer), 100);
      }
    }
  }
  else if (strncmp(cmd, "STATUS", 6) == 0)
  {
    // Only respond to explicit STATUS request
    snprintf(uart_tx_buffer, sizeof(uart_tx_buffer),
             "VOL:%.2f,OVR:%d,DLY:%d,GATE:%d\n",
             output_volume, overdrive.enabled, delay_effect.enabled, noise_gate.enabled);
    HAL_UART_Transmit(&huart3, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer), 100);
  }
  
  // **BLINK LED TO CONFIRM COMMAND RECEIVED**
  if (command_received)
  {
    // Set counter for 6 toggles (3 blinks: ON-OFF-ON-OFF-ON-OFF)
    command_blink_counter = 6;
  }
  
  // Clear buffer
  memset(uart_rx_buffer, 0, UART_RX_BUFFER_SIZE);
  uart_rx_index = 0;
}

/**
  * @brief  Send UART response to ESP32
  * @param  msg: Message to send
  * @retval None
  */
void Send_UART_Response(const char* msg)
{
  snprintf(uart_tx_buffer, sizeof(uart_tx_buffer), "%s\n", msg);
  HAL_UART_Transmit(&huart2, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer), 100);
}

/**
  * @brief  Process guitar signal with distortion effect
  * @param  None
  * @retval None
  */
void Process_Guitar_Signal(void)
{
  uint16_t i;
  float32_t normalized_input;
  float32_t processed_signal;
  
  // Convert ADC values to normalized float (-1.0 to 1.0)
  // ADC is differential mode, so center around 0
  for (i = 0; i < BUFFER_SIZE; i++)
  {
    // Normalize ADC input (0-4095) to (-1.0 to 1.0)
    normalized_input = ((float32_t)adc_buffer[i] / (ADC_MAX_VALUE / 2.0f)) - 1.0f;
    
    // Apply distortion effect
    processed_signal = Apply_Distortion(normalized_input);
    
    // Apply output volume control
    processed_signal *= output_volume;
    
    // Ensure signal is within bounds (clamp to -1.0 to 1.0)
    if (processed_signal > 1.0f) processed_signal = 1.0f;
    if (processed_signal < -1.0f) processed_signal = -1.0f;
    
    // Convert back to DAC value (0-4095)
    // Map -1.0 to 1.0 => 0 to 4095
    dac_buffer[i] = (uint16_t)((processed_signal + 1.0f) * (DAC_MAX_VALUE / 2.0f));
  }
  
  // Output processed audio to DAC (this will be done in real-time in the interrupt)
}

/**
  * @brief  Timer period elapsed callback
  * @param  htim: Timer handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM1)
  {
    static uint32_t signal_timeout = 0;
    static uint32_t signal_detected = 0;
    static uint16_t min_value = 4095;
    static uint16_t max_value = 0;
    static uint32_t range_counter = 0;

    HAL_ADC_Start(&hadc1);

    if (HAL_ADC_PollForConversion(&hadc1, 1) == HAL_OK)
    {
      uint16_t adc_value = HAL_ADC_GetValue(&hadc1);

      // Signal detection for LED
      if (adc_value < min_value) min_value = adc_value;
      if (adc_value > max_value) max_value = adc_value;

      range_counter++;
      if (range_counter >= 4800)
      {
        uint16_t signal_range = max_value - min_value;

        if (signal_range > 100)
        {
          signal_detected = 1;
          signal_timeout = 48000;
        }

        min_value = 4095;
        max_value = 0;
        range_counter = 0;
      }

      if (signal_detected)
      {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);

        if (signal_timeout > 0)
        {
          signal_timeout--;
        }
        else
        {
          signal_detected = 0;
          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
        }
      }
      else
      {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
      }

      // **APPLY EFFECTS IN REAL-TIME**
      // Normalize ADC input (0-4095) to (-1.0 to 1.0)
      // Since ADC is in differential mode, center at 2048
      float32_t normalized_input = ((float32_t)adc_value - 2048.0f) / 2048.0f;
      
      // EFFECT CHAIN - Order matters!
      // 1. NOISE GATE (first - prevents noise from being amplified)
      float32_t processed_signal = Apply_NoiseGate(normalized_input);
      
      // 2. OVERDRIVE (distortion/gain stage)
      processed_signal = Apply_Overdrive(processed_signal);
      
      // 3. DELAY (time-based effect, after distortion)
      processed_signal = Apply_Delay(processed_signal);
      
      // Apply output volume control
      processed_signal *= output_volume;
      
      // Ensure signal is within bounds (clamp to -1.0 to 1.0)
      if (processed_signal > 1.0f) processed_signal = 1.0f;
      if (processed_signal < -1.0f) processed_signal = -1.0f;
      
      // Convert back to DAC value (0-4095)
      // Map -1.0 to 1.0 => 0 to 4095 (centered at 2048)
      uint16_t dac_value = (uint16_t)((processed_signal * 2048.0f) + 2048.0f);
      
      // Clamp DAC output to valid range
      if (dac_value > 4095) dac_value = 4095;
      if (dac_value < 0) dac_value = 0;
      
      // Output to DAC
      HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, dac_value);

      // Store in buffer for monitoring
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

/**
  * @brief  UART Receive Complete Callback
  * @param  huart: UART handle
  * @retval None
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART3)  // ESP32 communication
  {
    // Check for newline (command complete)
    if (uart_rx_byte == '\n' || uart_rx_byte == '\r')
    {
      if (uart_rx_index > 0)
      {
        uart_rx_buffer[uart_rx_index] = '\0';  // Null terminate
        uart_command_ready = 1;
      }
    }
    else if (uart_rx_index < UART_RX_BUFFER_SIZE - 1)
    {
      // Add byte to buffer
      uart_rx_buffer[uart_rx_index++] = uart_rx_byte;
    }
    
    // Re-enable UART receive interrupt for next byte
    HAL_UART_Receive_IT(&huart3, &uart_rx_byte, 1);
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
