/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
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
#include <stdio.h>
#include "stm32h7xx_hal_tim.h"
#include "stm32h7xx_ll_exti.h"
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

I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int _write(int file, char* ptr, int len) {
    HAL_UART_Transmit(&huart3, ptr, len, HAL_MAX_DELAY);
    return len;
}

void regVersion(void) {
    MX_USART3_UART_Init();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    uint32_t moder = GPIOB->MODER;
    moder &= ~(1 << 29);
    moder |= 1 << 28;
    GPIOB->MODER = moder;

    moder = GPIOC->MODER;
    moder &= ~(1 << 27);
    moder &= ~(1 << 26);
    GPIOC->MODER = moder;

    moder = GPIOE->MODER;
    moder &= ~(1 << 3);
    moder |= 1 << 2;
    GPIOE->MODER = moder;

    uint32_t lastTime = 0;
    int lastState = 0;

    while(1) {
        if(HAL_GetTick() - lastTime > 1000) {
            lastTime = HAL_GetTick();
            if(lastState) GPIOB->ODR |= 1 << 14;
            else GPIOB->ODR &= ~(1 << 14);
            lastState = !lastState;

            printf("Hello World\n");
        }

        if(GPIOC->IDR & (1 << 13)) GPIOE->ODR |= 1 << 1;
        else GPIOE->ODR &= ~(1 << 1);
    }
}

const uint8_t MCPADD = 0x60 << 1;
const uint8_t MCP_T_HOT = 0x00;
const uint8_t MCP_CONF = 0x06;
const uint8_t MCP_CONF_VAL = 0x20;
const uint8_t MCP_REV = 0x20;
const uint8_t MCP_REV_VALUE = 0x40;

int setupMCP(void) {
    // Check that its responding
    HAL_StatusTypeDef t;
    if((t = HAL_I2C_IsDeviceReady(&hi2c1, MCPADD, 3, 100)) != HAL_OK) {
        printf("Could not initialize MCP: %d\n", t);
        return 1;
    }

    // Validate the value of the revision register
    uint8_t data;
    HAL_I2C_Mem_Read(&hi2c1, MCPADD, MCP_REV, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);

    if(data != MCP_REV_VALUE) {
        printf("MCP returned wrong revision: %d\n", data);
        return 2;
    }

    // Write the correct value for the thermocouple type we are using into the configuration register
    data = MCP_CONF_VAL;
    HAL_I2C_Mem_Write(&hi2c1, MCPADD, MCP_CONF, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);

    return 0;
}

float getMCPValue(void) {
    uint8_t data[2];
    HAL_I2C_Mem_Read(&hi2c1, MCPADD, MCP_T_HOT, I2C_MEMADD_SIZE_8BIT, data, 2, 10);
    int16_t raw = (int16_t) ((data[0] << 8) | data[1]);
    float reading = ((float) (raw / 16)) + 0.0625f * ((float) (raw % 16));
    return reading;
}

// Load Cell Code
TIM_TypeDef* const timer = TIM8;
const uint32_t TIMCHANNEL = TIM_CHANNEL_1;
GPIO_TypeDef* const clkport = GPIOC;
uint16_t const clkpin = GPIO_PIN_6;
volatile GPIO_TypeDef* const dinport = CELL_DIN_GPIO_Port;
uint16_t const dinpin = CELL_DIN_Pin;

uint32_t inProgressReading = 0;
uint32_t mask = 0;

float offset = 0;
const float scale = 0.0001532810009f;

volatile float latestReading = 0;

void setupCell(void) {
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_TIM8_CLK_ENABLE();

    // Set up the clock pin to be connected to the timer
    HAL_GPIO_DeInit(clkport, clkpin);
    GPIO_InitTypeDef init;
    init.Pin = clkpin;
    init.Mode = GPIO_MODE_AF_PP;
    init.Pull = GPIO_NOPULL;
    init.Alternate = GPIO_AF3_TIM8;
    HAL_GPIO_Init(clkport, &init);

    // Disable timer
    timer->CR1 &= ~TIM_CR1_CEN;

    // Clears:
    // - ARPE: We do not want ARR preloading, we want to be able to load directly into the registers
    // - CMS: Put counter in up/down counting mode
    // - DIR: Put counter in upcounting mode
    // - Sets OPM: Counter stops after an update event (which occurs after RCR overflows of CNT)
    timer->CR1 &= ~(TIM_CR1_ARPE | TIM_CR1_CMS | TIM_CR1_DIR);
    timer->CR1 |= TIM_CR1_OPM;

    // Results in a timer clock of 10MHz (t=0.1micros)
    timer->PSC = (240000000 / 10000000) - 1;

    // 20 * 0.1micros = 2micro period (t3 + t4 in datasheet)
    timer->ARR = 20;

    // We need 25 pulses, so RCR = 25-1
    timer->RCR = 24;

    // Reset counter
    timer->CNT = 0;

    // Enable the master output
    timer->BDTR |= TIM_BDTR_MOE;

    // Set counting mode to PWM
    timer->CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;

    // Set CCR to half of ARR, for 50% duty cycle
    timer->CCR1 = 10;

    // Set CCR2 as the time to actually read the GPIO data. The reading is triggered via a interrupt
    timer->CCR2 = 20;

    // Enable capture/compare and set polarity to make output normally low
    timer->CCER |= TIM_CCER_CC1E | TIM_CCER_CC1P | TIM_CCER_CC2E;

    // Cause an update so that all the fun stuff gets written to the shadow registers
    timer->EGR |= TIM_EGR_UG;

    // Enable capture interrupts
    timer->DIER |= TIM_DIER_UIE | TIM_DIER_CC2IE;

    // Enable capture interrupt
    HAL_NVIC_SetPriority(TIM8_CC_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM8_CC_IRQn);

    // Enable Update interrupt
    HAL_NVIC_SetPriority(TIM8_UP_TIM13_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM8_UP_TIM13_IRQn);
    LL_EXTI_EnableIT_0_31(dinpin);
}

// This interrupt is fired when the DIN line transitions from HIGH to LOW
extern void DIN_IRQ(void) {
    // Check this is actually the intererupt we're looking for
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
    if(__HAL_GPIO_EXTI_GET_IT(dinpin)) {
        // Acknowledge the interrupt so it doesn't re-trigger
        __HAL_GPIO_EXTI_CLEAR_IT(dinpin);

        // Disable the interrupt for now, since we dont want extra interrupts triggering when we read data
        LL_EXTI_DisableIT_0_31(dinpin);

        // Set up the variables for tracking the input
        inProgressReading = 0;
        timer->CNT = 0;
        mask = 0x00800000;

        // Start the timer
        timer->CR1 |= TIM_CR1_CEN;
    }
}

// This fires in the middle of each clock pulse
extern void CC_IRQ(void) {
    // Acknowledge the interrupt
    timer->SR &= ~TIM_SR_CC2IF;

    // If we still have bits left to read (we will have one extra clock pulse per datasheet)
    if(mask > 0) {
        // If the din-pin is set high, we read in a 1
        if(dinport->IDR & dinpin) inProgressReading |= mask;

        // Proceed to the next bit position
        mask >>= 1;
    }
}

// This fires at the end of the 25 pulses
extern void UPDATE_IRQ(void) {
    // Acknowledge
    timer->SR &= ~TIM_SR_UIF;

    // Convert raw data to the final reading
    if(inProgressReading & 0x00800000) inProgressReading += 0xFF000000;
    int32_t rawReading = (int32_t) inProgressReading;
    latestReading = (((float) rawReading) * scale) + offset;

    // Re-enable din interrupts for the next conversion
    LL_EXTI_EnableIT_0_31(dinpin);
}


/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    /* MPU Configuration--------------------------------------------------------*/
    MPU_Config();

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    // Uncomment this function to run the register-direct based prorgam instead
    // regVersion();

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_USART3_UART_Init();
    MX_I2C1_Init();
    /* USER CODE BEGIN 2 */

    int mcpstat = setupMCP();
    setupCell();

    uint32_t lastTime = 0;
    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while(1) {
        if(HAL_GetTick() - lastTime > 1000) {
            lastTime = HAL_GetTick();
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
            printf("Hello World!\n");

            if(mcpstat == 0) {
                float value = getMCPValue();
                printf("Read temperature: %f C\n", value);
            }

            printf("Load Cell weight: %fkg\n", latestReading);
            printf("raw: %ld\n", inProgressReading);
        }

        if(HAL_GPIO_ReadPin(USRBTN_GPIO_Port, USRBTN_Pin) == GPIO_PIN_SET) {
            HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
            offset -= latestReading;
            latestReading = 0;
        }
        else HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Supply configuration update enable
     */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

    /** Configure the main internal regulator output voltage
     */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

    while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 4;
    RCC_OscInitStruct.PLL.PLLN = 60;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 2;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;
    if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

    if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {

    /* USER CODE BEGIN I2C1_Init 0 */

    /* USER CODE END I2C1_Init 0 */

    /* USER CODE BEGIN I2C1_Init 1 */

    /* USER CODE END I2C1_Init 1 */
    hi2c1.Instance = I2C1;
    hi2c1.Init.Timing = 0x307075B1;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if(HAL_I2C_Init(&hi2c1) != HAL_OK) {
        Error_Handler();
    }

    /** Configure Analogue filter
     */
    if(HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
        Error_Handler();
    }

    /** Configure Digital filter
     */
    if(HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) {
        Error_Handler();
    }
    /* USER CODE BEGIN I2C1_Init 2 */

    /* USER CODE END I2C1_Init 2 */
}

/**
 * @brief USART3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART3_UART_Init(void) {

    /* USER CODE BEGIN USART3_Init 0 */

    /* USER CODE END USART3_Init 0 */

    /* USER CODE BEGIN USART3_Init 1 */

    /* USER CODE END USART3_Init 1 */
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
    if(HAL_UART_Init(&huart3) != HAL_OK) {
        Error_Handler();
    }
    if(HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK) {
        Error_Handler();
    }
    if(HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) {
        Error_Handler();
    }
    if(HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK) {
        Error_Handler();
    }
    /* USER CODE BEGIN USART3_Init 2 */

    /* USER CODE END USART3_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    /* USER CODE BEGIN MX_GPIO_Init_1 */

    /* USER CODE END MX_GPIO_Init_1 */

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin : USRBTN_Pin */
    GPIO_InitStruct.Pin = USRBTN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(USRBTN_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pin : LED_Pin */
    GPIO_InitStruct.Pin = LED_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pin : CELL_DIN_Pin */
    GPIO_InitStruct.Pin = CELL_DIN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(CELL_DIN_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pin : LED2_Pin */
    GPIO_InitStruct.Pin = LED2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED2_GPIO_Port, &GPIO_InitStruct);

    /* EXTI interrupt init*/
    HAL_NVIC_SetPriority(CELL_DIN_EXTI_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(CELL_DIN_EXTI_IRQn);

    /* USER CODE BEGIN MX_GPIO_Init_2 */

    /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* MPU Configuration */

void MPU_Config(void) {
    MPU_Region_InitTypeDef MPU_InitStruct = {0};

    /* Disables the MPU */
    HAL_MPU_Disable();

    /** Initializes and configures the Region and the memory to be protected
     */
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress = 0x0;
    MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
    MPU_InitStruct.SubRegionDisable = 0x87;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

    HAL_MPU_ConfigRegion(&MPU_InitStruct);
    /* Enables the MPU */
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while(1) {}
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
void assert_failed(uint8_t* file, uint32_t line) {
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
