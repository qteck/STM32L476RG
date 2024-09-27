/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

DAC_HandleTypeDef hdac1;

OPAMP_HandleTypeDef hopamp2;

TIM_HandleTypeDef htim4;
TIM_HandleTypeDef htim15;

UART_HandleTypeDef huart4;
UART_HandleTypeDef huart5;
DMA_HandleTypeDef hdma_uart4_rx;
DMA_HandleTypeDef hdma_uart4_tx;
DMA_HandleTypeDef hdma_uart5_rx;
DMA_HandleTypeDef hdma_uart5_tx;

osThreadId UART_comHandle;
osThreadId UART_com2Handle;
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_UART4_Init(void);
static void MX_UART5_Init(void);
static void MX_TIM15_Init(void);
static void MX_OPAMP2_Init(void);
static void MX_DAC1_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM4_Init(void);
void UARTPathIn(void const * argument);
void UARTPathOut(void const * argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define BUFFER_SIZE_X 1
#define BUFFER_SIZE_Y 255

/*
 * Variables for UART - high speed path
 *
 */

uint8_t dmaBufferHS[BUFFER_SIZE_Y][BUFFER_SIZE_X];
uint8_t rxDataHS[BUFFER_SIZE_X];

int counterBufferCpltHS_tx = 0; // Counter for all transmissions completed
int * counterBufferCpltHS_tx_ptr = &counterBufferCpltHS_tx; //

int counterBufferCpltHS_rx = 0; // Counter for all receptions completed
int * counterBufferCpltHS_rx_ptr = &counterBufferCpltHS_rx; // Pointer type for chasing algorithm and instant access to changed state.

int counterHS_tx = 0; // Counter of transmissions up to buffer size
int * counterHS_tx_ptr = &counterHS_tx;

int counterHS_rx = 0; // Counter of receptions up to buffer size

/*
 * Variables for UART - low speed path
 *
 */

uint8_t dmaBufferLS[BUFFER_SIZE_Y][BUFFER_SIZE_X];
uint8_t rxDataLS[BUFFER_SIZE_X];

int counterBufferCpltLS_tx = 0; // Counter for all transmissions completed
int * counterBufferCpltLS_tx_ptr = &counterBufferCpltLS_tx; //

int counterBufferCpltLS_rx = 0; // Counter for all receptions completed
int * counterBufferCpltLS_rx_ptr = &counterBufferCpltLS_rx; // Pointer type for chasing algorithm and instant access to changed state.

int counterLS_tx = 0; // Counter of transmissions up to buffer size
int * counterLS_tx_ptr = &counterLS_tx;

int counterLS_rx = 0; // Counter of receptions up to buffer size

/**
 * Variables For Command Matching
 *
 */

int counterCommands = 0;
int * counterCommands_ptr = &counterCommands;

/**
 * Variables For ADC
 *
 */
uint32_t value_adc;

/**
 * Variables For Wave Capture
 *
 */
uint32_t t1 = 0;
uint32_t t2 = 0;
int f_clk = 8000000; //8000000 no pre-scale
int f = 0;
uint32_t value = 0;
uint32_t previousf = 0;
uint32_t difference = 0;

uint8_t bf[4];

int f_trigger = 8763;
int f_threshold = 200;
int f_debounce = 30;

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{

  if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
  {
  	value = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

  	if(t1 == 0) {
  		t1 = value;
  	} else {
  		t2 = value;

  		difference = (t2 < t1) ? (htim->Instance->ARR - t1 + t2) : (t2 - t1); // Adjustment if ARR overflow occurs in between the captured edges

  		f = f_clk/difference; // Clock frequency / difference between rising edges

  		if(abs((int)f - (int)previousf) > f_debounce) // Debounce the input signal from the control knob of the oscilloscope
  		{
  	  		// Setting threshold for capturing desired frequency
  	  		if((f < (f_trigger + f_threshold)) && (f > (f_trigger - f_threshold)))
  	  		{
  	  			/**
  	  			 * This is the event triggered when the frequency is within the limits.
  	  			 *
  	  			 */
  	  			sprintf((char *) bf, "%d", f);
  	  			HAL_UART_Transmit(&huart5, (uint8_t*) bf, sizeof(bf), 100);
  	  		}

  	        previousf = f;
  		}

        f = 0;
        difference = 0;
        t1 = 0;
        t2 = 0;
  	}
  }

}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == UART4)
	{
		if(counterLS_tx == (BUFFER_SIZE_Y-1)) {
			counterLS_tx = 0; // Resets counter when number of transmissions exceeds buffer size
		} else {
			counterLS_tx++; // Increments counter when number of transmissions is smaller than buffer size
		}

	}
	else if (huart->Instance == UART5)
	{
		if(counterHS_tx == (BUFFER_SIZE_Y-1)) {
			counterHS_tx = 0; // Resets counter when number of transmissions exceeds buffer size
		} else {
			counterHS_tx++; // Increments counter when number of transmissions is smaller than buffer size
		}
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	// the DMA is quicker than the transmission
	// the buffering needs to slow down
	// the buffer may not be big enough for such baud rate difference
	// the DMA will not wait for the call back to finish

	if (huart->Instance == UART5)
	{
		dmaBufferLS[counterLS_rx][0] = rxDataLS[0];

		if(counterLS_rx == (BUFFER_SIZE_Y-1)) {
			counterLS_rx = 0; // Resets counter when number of receptions exceeds buffer size
		} else {
			counterLS_rx++; // Increments counter when number of receptions is smaller than buffer size
		}

		counterBufferCpltLS_rx++;
	}
	else if (huart->Instance == UART4)
	{
		dmaBufferHS[counterHS_rx][0] = rxDataHS[0];

		if(counterHS_rx == (BUFFER_SIZE_Y-1)) {
			counterHS_rx = 0; // Resets counter when number of receptions exceeds buffer size
		} else {
			counterHS_rx++; // Increments counter when number of receptions is smaller than buffer size
		}

		counterBufferCpltHS_rx++;
	}
}

void transmitData(UART_HandleTypeDef * huart_f, uint8_t dmaBuffer_f[BUFFER_SIZE_Y][BUFFER_SIZE_X], int *counterBufferCplt_rx_ptr_f, int *counterBufferCplt_tx_ptr_f, int *counter_tx_ptr_f)
{
		if(*counterBufferCplt_tx_ptr_f < *counterBufferCplt_rx_ptr_f)
		{
			for(int index = *counterBufferCplt_tx_ptr_f; index < *counterBufferCplt_rx_ptr_f && (huart_f->gState == HAL_UART_STATE_READY); index++)
			{
					//HAL_Delay(500);
					if(HAL_UART_Transmit_DMA(huart_f, dmaBuffer_f[*counter_tx_ptr_f], 1) == HAL_OK)
					{
						(*counterBufferCplt_tx_ptr_f)++;
					}
			}
		}
}

/*
int commandRecognition(const char code[], int *counterCommands_f_ptr, int *counterBufferCplt_f_rx_ptr, uint8_t (*dmaBuffer_f)[BUFFER_SIZE_Y][BUFFER_SIZE_X]) {

	int command_length = 5;

    int command_matched = 0;

    for (int index = *counterCommands_f_ptr;
        (index <= *counterBufferCplt_f_rx_ptr) && (*counterBufferCplt_f_rx_ptr >= command_length); index++)
    {
  	  HAL_Delay(500);


  	  if ((*dmaBuffer_f[(index - command_length)][0] == code[0])  // Corrected
  	      && (*dmaBuffer_f[(index - command_length + 1)][0] == code[1])
  	      && (*dmaBuffer_f[(index - command_length + 2)][0] == code[2])
  	      && (*dmaBuffer_f[(index - command_length + 3)][0] == code[3])
  	      && (*dmaBuffer_f[(index - command_length + 4)][0] == code[4]))
  	  {
  		  command_matched = 1;
  		  break;
  	  }

  	  //restarting the index count when reaching the end of the buffer
  	  if(*counterCommands_f_ptr <= BUFFER_SIZE_Y)
  	  {
  		  (*counterCommands_f_ptr)++;
  	  } else {
  		  *counterCommands_f_ptr = 0;
  	  }

    }
    //uint8_t dd = dmaBufferHS[2][0];
    //uint8_t dd2 = *dmaBuffer_f[2][0];

    return command_matched;
}

dmaBufferLS[0][0] = 'C';
dmaBufferLS[1][0] = 'O';
dmaBufferLS[2][0] = 'D';
dmaBufferLS[3][0] = 'E';
dmaBufferLS[4][0] = '1';

*/

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
  MX_DMA_Init();
  MX_UART4_Init();
  MX_UART5_Init();
  MX_TIM15_Init();
  MX_OPAMP2_Init();
  MX_DAC1_Init();
  MX_ADC1_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */

  /***
   * Start off the ADC Channel and calibrate it
   * When testing the ADC, it is best to connect it to a DC supply first.
   */
  HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
  HAL_ADC_Start_DMA(&hadc1, &value_adc, 1);

  /***
   * Start OPAMP2
   *
   */
  HAL_OPAMP_Start(&hopamp2);

  /***
   * PWM
   * Use Timer 8 for testing.
   * HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);
   *
   */
  HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_1); // Solder Bridge on Nucleo, test it on TIM2

  /***
   * Start Wave Capture
   *
   */
  HAL_TIM_IC_Start_IT(&htim4, TIM_CHANNEL_1);
  /***
   * UART Communication DMA
   *
   */
  HAL_UART_Receive_DMA(&huart4, rxDataHS, BUFFER_SIZE_X);
  HAL_UART_Receive_DMA(&huart5, rxDataLS, BUFFER_SIZE_X);


  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of UART_com */
  osThreadDef(UART_com, UARTPathIn, osPriorityNormal, 0, 128);
  UART_comHandle = osThreadCreate(osThread(UART_com), NULL);

  /* definition and creation of UART_com2 */
  osThreadDef(UART_com2, UARTPathOut, osPriorityLow, 0, 128);
  UART_com2Handle = osThreadCreate(osThread(UART_com2), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {


	/*

	if(commandRecognition("CODE1", counterCommandsHS_ptr, counterBufferCpltHS_rx_ptr, dmaBufferLS) == 1) {
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
		HAL_Delay(500);
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
		HAL_Delay(500);
	}
	*/




    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 2;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV8;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
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
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
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
  sConfig.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
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
  sConfig.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
  sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_DISABLE;
  sConfig.DAC_UserTrimming = DAC_TRIMMING_FACTORY;
  if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC1_Init 2 */

  /* USER CODE END DAC1_Init 2 */

}

/**
  * @brief OPAMP2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_OPAMP2_Init(void)
{

  /* USER CODE BEGIN OPAMP2_Init 0 */

  /* USER CODE END OPAMP2_Init 0 */

  /* USER CODE BEGIN OPAMP2_Init 1 */

  /* USER CODE END OPAMP2_Init 1 */
  hopamp2.Instance = OPAMP2;
  hopamp2.Init.PowerSupplyRange = OPAMP_POWERSUPPLY_LOW;
  hopamp2.Init.Mode = OPAMP_PGA_MODE;
  hopamp2.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO0;
  hopamp2.Init.InvertingInput = OPAMP_INVERTINGINPUT_IO0;
  hopamp2.Init.PgaGain = OPAMP_PGA_GAIN_2;
  hopamp2.Init.PowerMode = OPAMP_POWERMODE_NORMALPOWER;
  hopamp2.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP2_Init 2 */

  /* USER CODE END OPAMP2_Init 2 */

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 65535;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_IC_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim4, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

}

/**
  * @brief TIM15 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM15_Init(void)
{

  /* USER CODE BEGIN TIM15_Init 0 */

  /* USER CODE END TIM15_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM15_Init 1 */

  /* USER CODE END TIM15_Init 1 */
  htim15.Instance = TIM15;
  htim15.Init.Prescaler = 0;
  htim15.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim15.Init.Period = 913;
  htim15.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim15.Init.RepetitionCounter = 0;
  htim15.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim15) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim15, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 457;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim15, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim15, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM15_Init 2 */

  /* USER CODE END TIM15_Init 2 */
  HAL_TIM_MspPostInit(&htim15);

}

/**
  * @brief UART4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART4_Init(void)
{

  /* USER CODE BEGIN UART4_Init 0 */

  /* USER CODE END UART4_Init 0 */

  /* USER CODE BEGIN UART4_Init 1 */

  /* USER CODE END UART4_Init 1 */
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 220;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART4_Init 2 */

  /* USER CODE END UART4_Init 2 */

}

/**
  * @brief UART5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART5_Init(void)
{

  /* USER CODE BEGIN UART5_Init 0 */

  /* USER CODE END UART5_Init 0 */

  /* USER CODE BEGIN UART5_Init 1 */

  /* USER CODE END UART5_Init 1 */
  huart5.Instance = UART5;
  huart5.Init.BaudRate = 250000;
  huart5.Init.WordLength = UART_WORDLENGTH_8B;
  huart5.Init.StopBits = UART_STOPBITS_1;
  huart5.Init.Parity = UART_PARITY_NONE;
  huart5.Init.Mode = UART_MODE_TX_RX;
  huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart5.Init.OverSampling = UART_OVERSAMPLING_16;
  huart5.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart5.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART5_Init 2 */

  /* USER CODE END UART5_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA2_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Channel1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Channel1_IRQn);
  /* DMA2_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Channel2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Channel2_IRQn);
  /* DMA2_Channel3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Channel3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Channel3_IRQn);
  /* DMA2_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Channel5_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Channel5_IRQn);

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_UARTPathIn */
/**
  * @brief  Function implementing the UART_com thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_UARTPathIn */
void UARTPathIn(void const * argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
		transmitData(&huart5, dmaBufferHS, counterBufferCpltHS_rx_ptr, counterBufferCpltHS_tx_ptr, counterHS_tx_ptr);

    osDelay(1);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_UARTPathOut */
/**
* @brief Function implementing the UART_com2 thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_UARTPathOut */
void UARTPathOut(void const * argument)
{
  /* USER CODE BEGIN UARTPathOut */
  /* Infinite loop */
  for(;;)
  {
	  transmitData(&huart4, dmaBufferLS, counterBufferCpltLS_rx_ptr, counterBufferCpltLS_tx_ptr, counterLS_tx_ptr);
    osDelay(1);
  }
  /* USER CODE END UARTPathOut */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

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

#ifdef  USE_FULL_ASSERT
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
