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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
    APP_IDLE = 0,
    APP_TEMP_MESSAGE,
    APP_LOCKOUT
} app_state_t;

typedef enum {
    EV_KEY = 1,
    EV_LOCKOUT_TICK,
    EV_UI_TIMEOUT
} event_type_t;

typedef enum {
    UI_IDLE = 1,
    UI_PIN_MASK,
    UI_GRANTED,
    UI_DENIED,
    UI_LOCKOUT,
    UI_NEED4
} ui_cmd_t;

typedef enum {
    OUT_IDLE = 1,
    OUT_KEY,
    OUT_SUCCESS,
    OUT_FAIL,
    OUT_LOCKOUT
} output_cmd_t;

typedef struct {
    char text[64];
} log_msg_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LCD_I2C_ADDR        (0x27 << 1)

#define LCD_BACKLIGHT       0x08
#define LCD_ENABLE_BIT      0x04
#define LCD_RW_BIT          0x02
#define LCD_RS_BIT          0x01

#define PACK_MSG(type, value)   ((((uint32_t)(type)) << 16) | ((uint16_t)(value)))
#define MSG_TYPE(msg)           ((uint16_t)(((msg) >> 16) & 0xFFFF))
#define MSG_VALUE(msg)          ((uint16_t)((msg) & 0xFFFF))
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityLow,
  .stack_size = 128 * 4
};
/* Definitions for InputTask */
osThreadId_t InputTaskHandle;
const osThreadAttr_t InputTask_attributes = {
  .name = "InputTask",
  .priority = (osPriority_t) osPriorityAboveNormal,
  .stack_size = 512 * 4
};
/* Definitions for AuthTask */
osThreadId_t AuthTaskHandle;
const osThreadAttr_t AuthTask_attributes = {
  .name = "AuthTask",
  .priority = (osPriority_t) osPriorityHigh,
  .stack_size = 768 * 4
};
/* Definitions for UiTask */
osThreadId_t UiTaskHandle;
const osThreadAttr_t UiTask_attributes = {
  .name = "UiTask",
  .priority = (osPriority_t) osPriorityLow,
  .stack_size = 768 * 4
};
/* Definitions for OutputTask */
osThreadId_t OutputTaskHandle;
const osThreadAttr_t OutputTask_attributes = {
  .name = "OutputTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 768 * 4
};
/* Definitions for LoggerTask */
osThreadId_t LoggerTaskHandle;
const osThreadAttr_t LoggerTask_attributes = {
  .name = "LoggerTask",
  .priority = (osPriority_t) osPriorityLow,
  .stack_size = 768 * 4
};
/* Definitions for eventQueue */
osMessageQueueId_t eventQueueHandle;
const osMessageQueueAttr_t eventQueue_attributes = {
  .name = "eventQueue"
};
/* Definitions for uiQueue */
osMessageQueueId_t uiQueueHandle;
const osMessageQueueAttr_t uiQueue_attributes = {
  .name = "uiQueue"
};
/* Definitions for outputQueue */
osMessageQueueId_t outputQueueHandle;
const osMessageQueueAttr_t outputQueue_attributes = {
  .name = "outputQueue"
};
/* Definitions for logQueue */
osMessageQueueId_t logQueueHandle;
const osMessageQueueAttr_t logQueue_attributes = {
  .name = "logQueue"
};
/* Definitions for lockoutTimer */
osTimerId_t lockoutTimerHandle;
const osTimerAttr_t lockoutTimer_attributes = {
  .name = "lockoutTimer"
};
/* Definitions for uiReturnTimer */
osTimerId_t uiReturnTimerHandle;
const osTimerAttr_t uiReturnTimer_attributes = {
  .name = "uiReturnTimer"
};
/* USER CODE BEGIN PV */
/* Keypad matrix mapping after software remapping */
char keymap[4][4] = {
    {'D', 'C', 'B', 'A'},
    {'#', '9', '6', '3'},
    {'0', '8', '5', '2'},
    {'*', '7', '4', '1'}
};

/* RTOS-owned application state */
static app_state_t appState = APP_IDLE;

static char appPinBuffer[5] = {0};          // 4 digits + '\0'
static uint8_t appPinLen = 0;
static const char appCorrectPin[5] = "1234";

static uint8_t appFailedStreak = 0;
static uint32_t appLockoutRemainingSec = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
void StartDefaultTask(void *argument);
void StartTask02(void *argument);
void StartTask03(void *argument);
void StartTask04(void *argument);
void StartTask05(void *argument);
void StartTask06(void *argument);
void Callback01(void *argument);
void Callback02(void *argument);

/* USER CODE BEGIN PFP */
/* Low-level hardware helpers */
void uart_print(const char *msg);
char keypad_scan(void);

void lcd_write_nibble(uint8_t nibble, uint8_t rs);
void lcd_send_cmd(uint8_t cmd);
void lcd_send_data(uint8_t data);
void lcd_init_custom(void);
void lcd_clear(void);
void lcd_set_cursor(uint8_t row, uint8_t col);
void lcd_send_string(const char *str);
void lcd_print_line(uint8_t row, const char *text);

void leds_off(void);
void buzzer_tone_ms(uint32_t duration_ms);

/* RTOS application helpers */
static void app_reset_pin(void);
static uint8_t app_is_digit(char key);
static uint32_t app_calc_lockout_seconds(uint8_t streak);

static void send_ui(uint16_t cmd, uint16_t value);
static void send_output(uint16_t cmd, uint16_t value);
static void send_event(uint16_t type, uint16_t value);
static void send_logf(const char *fmt, ...);

static void ui_show_pin_mask(uint8_t len);

static void output_idle(void);
static void output_key(void);
static void output_success(void);
static void output_fail(void);
static void output_lockout(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void uart_print(const char *msg)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
}

char keypad_scan(void)
{
    GPIO_TypeDef* row_ports[4] = {GPIOA, GPIOA, GPIOA, GPIOB};
    uint16_t row_pins[4]       = {GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_4, GPIO_PIN_0};

    GPIO_TypeDef* col_ports[4] = {GPIOA, GPIOB, GPIOA, GPIOC};
    uint16_t col_pins[4]       = {GPIO_PIN_8, GPIO_PIN_10, GPIO_PIN_9, GPIO_PIN_7};

    // Set all rows HIGH
    for (int r = 0; r < 4; r++) {
        HAL_GPIO_WritePin(row_ports[r], row_pins[r], GPIO_PIN_SET);
    }

    // Scan each row
    for (int r = 0; r < 4; r++) {
        // Drive one row LOW
        HAL_GPIO_WritePin(row_ports[r], row_pins[r], GPIO_PIN_RESET);

        // Small settling delay
        HAL_Delay(1);

        // Read columns
        for (int c = 0; c < 4; c++) {
            if (HAL_GPIO_ReadPin(col_ports[c], col_pins[c]) == GPIO_PIN_RESET) {
                // Restore this row before returning
                HAL_GPIO_WritePin(row_ports[r], row_pins[r], GPIO_PIN_SET);
                return keymap[r][c];
            }
        }

        // Restore row HIGH
        HAL_GPIO_WritePin(row_ports[r], row_pins[r], GPIO_PIN_SET);
    }

    return 0;   // No key pressed
}


void lcd_write_nibble(uint8_t nibble, uint8_t rs)
{
    uint8_t data = 0;

    // Put nibble on D4-D7 => backpack P4-P7
    data |= (nibble & 0x0F) << 4;

    // Backlight on
    data |= LCD_BACKLIGHT;

    // RS if sending character data
    if (rs)
    {
        data |= LCD_RS_BIT;
    }

    // Write with EN low
    HAL_I2C_Master_Transmit(&hi2c1, LCD_I2C_ADDR, &data, 1, HAL_MAX_DELAY);

    // Pulse EN high
    data |= LCD_ENABLE_BIT;
    HAL_I2C_Master_Transmit(&hi2c1, LCD_I2C_ADDR, &data, 1, HAL_MAX_DELAY);

    // Then EN low again
    data &= ~LCD_ENABLE_BIT;
    HAL_I2C_Master_Transmit(&hi2c1, LCD_I2C_ADDR, &data, 1, HAL_MAX_DELAY);

    HAL_Delay(1);
}

void lcd_send_cmd(uint8_t cmd)
{
    lcd_write_nibble(cmd >> 4, 0);
    lcd_write_nibble(cmd & 0x0F, 0);
}

void lcd_send_data(uint8_t data)
{
    lcd_write_nibble(data >> 4, 1);
    lcd_write_nibble(data & 0x0F, 1);
}

void lcd_init_custom(void)
{
    HAL_Delay(50);

    // Force 4-bit init sequence
    lcd_write_nibble(0x03, 0);
    HAL_Delay(5);

    lcd_write_nibble(0x03, 0);
    HAL_Delay(5);

    lcd_write_nibble(0x03, 0);
    HAL_Delay(1);

    lcd_write_nibble(0x02, 0);   // 4-bit mode
    HAL_Delay(1);

    lcd_send_cmd(0x28);   // 4-bit, 2 lines, 5x8 font
    lcd_send_cmd(0x0C);   // display on, cursor off
    lcd_send_cmd(0x06);   // entry mode: increment cursor
    lcd_send_cmd(0x01);   // clear
    HAL_Delay(2);
}

void lcd_clear(void)
{
    lcd_send_cmd(0x01);
    HAL_Delay(2);
}

void lcd_set_cursor(uint8_t row, uint8_t col)
{
    uint8_t address;

    if (row == 0)
    {
        address = 0x80 + col;
    }
    else
    {
        address = 0xC0 + col;
    }

    lcd_send_cmd(address);
}

void lcd_send_string(const char *str)
{
    while (*str)
    {
        lcd_send_data((uint8_t)(*str));
        str++;
    }
}

void lcd_print_line(uint8_t row, const char *text)
{
    char buf[17];
    snprintf(buf, sizeof(buf), "%-16.16s", text);   // pad/truncate to 16 chars
    lcd_set_cursor(row, 0);
    lcd_send_string(buf);
}


void leds_off(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET); // Red LED off
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET); // Green LED off
}

void buzzer_tone_ms(uint32_t duration_ms)
{
    uint32_t cycles = duration_ms * 2;   // rough tone timing

    for (uint32_t i = 0; i < cycles; i++)
    {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
        HAL_Delay(1);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
        HAL_Delay(1);
    }
}

/* ---------- RTOS application helpers ---------- */
static void app_reset_pin(void)
{
    appPinLen = 0;
    memset(appPinBuffer, 0, sizeof(appPinBuffer));
}

static uint8_t app_is_digit(char key)
{
    return (key >= '0' && key <= '9');
}

static uint32_t app_calc_lockout_seconds(uint8_t streak)
{
    if (streak < 5)
    {
        return 0;
    }

    uint32_t seconds = 60;

    for (uint8_t i = 5; i < streak; i++)
    {
        seconds *= 2;
        if (seconds >= 1800)
        {
            seconds = 1800;
            break;
        }
    }

    return seconds;
}

static void send_ui(uint16_t cmd, uint16_t value)
{
    uint32_t msg = PACK_MSG(cmd, value);
    osMessageQueuePut(uiQueueHandle, &msg, 0, 0);
}

static void send_output(uint16_t cmd, uint16_t value)
{
    uint32_t msg = PACK_MSG(cmd, value);
    osMessageQueuePut(outputQueueHandle, &msg, 0, 0);
}

static void send_event(uint16_t type, uint16_t value)
{
    uint32_t msg = PACK_MSG(type, value);
    osMessageQueuePut(eventQueueHandle, &msg, 0, 0);
}

static void send_logf(const char *fmt, ...)
{
    log_msg_t msg;
    va_list args;

    memset(&msg, 0, sizeof(msg));

    va_start(args, fmt);
    vsnprintf(msg.text, sizeof(msg.text), fmt, args);
    va_end(args);

    osMessageQueuePut(logQueueHandle, &msg, 0, 0);
}

static void ui_show_pin_mask(uint8_t len)
{
    char stars[5] = {0};
    char line[17];

    for (uint8_t i = 0; i < len && i < 4; i++)
    {
        stars[i] = '*';
    }

    snprintf(line, sizeof(line), "PIN: %s", stars);
    lcd_print_line(0, "Enter PIN");
    lcd_print_line(1, line);
}

static void output_idle(void)
{
    leds_off();
}

static void output_key(void)
{
    buzzer_tone_ms(30);
}

static void output_success(void)
{
    leds_off();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
    buzzer_tone_ms(60);
    osDelay(60);
    buzzer_tone_ms(60);
}

static void output_fail(void)
{
    leds_off();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
    buzzer_tone_ms(150);
}

static void output_lockout(void)
{
    leds_off();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
    buzzer_tone_ms(200);
    osDelay(80);
    buzzer_tone_ms(200);
    osDelay(80);
    buzzer_tone_ms(200);
}
static void task_debug(const char *msg)
{
    uart_print(msg);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  char msg[64];
  osStatus_t status;

  HAL_Init();

  SystemClock_Config();

  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();

  uart_print("Before osKernelInitialize\r\n");
  status = osKernelInitialize();
  snprintf(msg, sizeof(msg), "osKernelInitialize=%d\r\n", (int)status);
  uart_print(msg);

  MX_FREERTOS_Init();

  uart_print("Secure Access RTOS boot\r\n");
  leds_off();

  uart_print("Before osKernelStart\r\n");
  status = osKernelStart();

  /* If we get here, scheduler did NOT start properly */
  snprintf(msg, sizeof(msg), "osKernelStart returned %d\r\n", (int)status);
  uart_print(msg);

  while (1)
  {
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    HAL_Delay(250);
  }
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
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00503D58;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_5
                          |GPIO_PIN_10, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_4|GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA0 PA1 PA4 PA5
                           PA10 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_5
                          |GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB4 PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_4|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB10 */
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA8 PA9 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void MX_FREERTOS_Init(void)
{
  char msg[80];

  uart_print("MX_FREERTOS_Init start\r\n");

  /* Create RTOS queues */
  eventQueueHandle = osMessageQueueNew(16, sizeof(uint32_t), &eventQueue_attributes);
  snprintf(msg, sizeof(msg), "eventQueueHandle=%p\r\n", eventQueueHandle);
  uart_print(msg);

  uiQueueHandle = osMessageQueueNew(8, sizeof(uint32_t), &uiQueue_attributes);
  snprintf(msg, sizeof(msg), "uiQueueHandle=%p\r\n", uiQueueHandle);
  uart_print(msg);

  outputQueueHandle = osMessageQueueNew(8, sizeof(uint32_t), &outputQueue_attributes);
  snprintf(msg, sizeof(msg), "outputQueueHandle=%p\r\n", outputQueueHandle);
  uart_print(msg);

  logQueueHandle = osMessageQueueNew(16, sizeof(log_msg_t), &logQueue_attributes);
  snprintf(msg, sizeof(msg), "logQueueHandle=%p\r\n", logQueueHandle);
  uart_print(msg);

  /* Create RTOS timers */
  lockoutTimerHandle = osTimerNew(Callback01, osTimerPeriodic, NULL, &lockoutTimer_attributes);
  snprintf(msg, sizeof(msg), "lockoutTimerHandle=%p\r\n", lockoutTimerHandle);
  uart_print(msg);

  uiReturnTimerHandle = osTimerNew(Callback02, osTimerOnce, NULL, &uiReturnTimer_attributes);
  snprintf(msg, sizeof(msg), "uiReturnTimerHandle=%p\r\n", uiReturnTimerHandle);
  uart_print(msg);

  /* Create RTOS threads */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
  snprintf(msg, sizeof(msg), "defaultTaskHandle=%p\r\n", defaultTaskHandle);
  uart_print(msg);

  InputTaskHandle = osThreadNew(StartTask02, NULL, &InputTask_attributes);
  snprintf(msg, sizeof(msg), "InputTaskHandle=%p\r\n", InputTaskHandle);
  uart_print(msg);

  AuthTaskHandle = osThreadNew(StartTask03, NULL, &AuthTask_attributes);
  snprintf(msg, sizeof(msg), "AuthTaskHandle=%p\r\n", AuthTaskHandle);
  uart_print(msg);

  UiTaskHandle = osThreadNew(StartTask04, NULL, &UiTask_attributes);
  snprintf(msg, sizeof(msg), "UiTaskHandle=%p\r\n", UiTaskHandle);
  uart_print(msg);

  OutputTaskHandle = osThreadNew(StartTask05, NULL, &OutputTask_attributes);
  snprintf(msg, sizeof(msg), "OutputTaskHandle=%p\r\n", OutputTaskHandle);
  uart_print(msg);

  LoggerTaskHandle = osThreadNew(StartTask06, NULL, &LoggerTask_attributes);
  snprintf(msg, sizeof(msg), "LoggerTaskHandle=%p\r\n", LoggerTaskHandle);
  uart_print(msg);

  uart_print("MX_FREERTOS_Init end\r\n");
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
	/* Idle background task; application logic lives in the dedicated RTOS tasks */
  for(;;)
  {
    osDelay(1000);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartTask02 */
/**
* @brief Function implementing the InputTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask02 */
void StartTask02(void *argument)
{
  /* USER CODE BEGIN StartTask02 */
	  char lastKey = 0;
	  char key;
	  char msg[32];

	  task_debug("InputTask alive\r\n");

	  for(;;)
	  {
		  /* Periodically scan the keypad and send key events to AuthTask */
	    key = keypad_scan();

	    if (key != 0 && key != lastKey)
	    {
	      snprintf(msg, sizeof(msg), "Input key: %c\r\n", key);
	      task_debug(msg);

	      send_event(EV_KEY, (uint16_t)key);
	      lastKey = key;
	    }

	    if (key == 0)
	    {
	      lastKey = 0;
	    }

	    osDelay(20);
	  }

  /* USER CODE END StartTask02 */
}

/* USER CODE BEGIN Header_StartTask03 */
/**
* @brief Function implementing the AuthTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask03 */
void StartTask03(void *argument)
{
  /* USER CODE BEGIN StartTask03 */
	  uint32_t msg;
	  char dbg[40];

	  task_debug("AuthTask alive\r\n");

	  /* Initialize application state and request the idle UI */
	  app_reset_pin();
	  appState = APP_IDLE;

	  send_ui(UI_IDLE, 0);
	  send_output(OUT_IDLE, 0);
	  send_logf("RTOS auth start\r\n");

	  for(;;)
	  {
	    osMessageQueueGet(eventQueueHandle, &msg, NULL, osWaitForever);

	    snprintf(dbg, sizeof(dbg), "Auth got event type=%u val=%u\r\n",
	             (unsigned)MSG_TYPE(msg), (unsigned)MSG_VALUE(msg));
	    task_debug(dbg);

	    switch (MSG_TYPE(msg))
	    {
	      case EV_KEY:
	      {
	        char key = (char)MSG_VALUE(msg);

	        snprintf(dbg, sizeof(dbg), "Auth key=%c\r\n", key);
	        task_debug(dbg);

	        if (appState == APP_LOCKOUT)
	        {
	          break;
	        }

	        if (appState == APP_TEMP_MESSAGE)
	        {
	          break;
	        }

	        if (app_is_digit(key))
	        {
	          if (appPinLen < 4)
	          {
	            appPinBuffer[appPinLen] = key;
	            appPinLen++;
	            appPinBuffer[appPinLen] = '\0';

	            send_ui(UI_PIN_MASK, appPinLen);
	            send_output(OUT_KEY, 0);
	            send_logf("PIN: %s\r\n", appPinBuffer);
	          }
	        }
	        else if (key == '*')
	        {
	          app_reset_pin();
	          send_ui(UI_IDLE, 0);
	          send_logf("PIN cleared\r\n");
	        }
	        else if (key == '#')
	        {
	          if (appPinLen != 4)
	          {
	            send_ui(UI_NEED4, 0);
	            osTimerStart(uiReturnTimerHandle, 800);
	            appState = APP_TEMP_MESSAGE;
	            send_logf("Need exactly 4 digits before submit\r\n");
	          }
	          else if (strcmp(appPinBuffer, appCorrectPin) == 0)
	          {
	            appFailedStreak = 0;
	            app_reset_pin();
	            appState = APP_TEMP_MESSAGE;

	            send_ui(UI_GRANTED, 0);
	            send_output(OUT_SUCCESS, 0);
	            send_logf("ACCESS GRANTED\r\n");

	            osTimerStart(uiReturnTimerHandle, 800);
	          }
	          else
	          {
	            appFailedStreak++;
	            app_reset_pin();

	            send_logf("ACCESS DENIED\r\nFailed streak: %u\r\n", appFailedStreak);

	            if (appFailedStreak >= 5)
	            {
	              appState = APP_LOCKOUT;
	              appLockoutRemainingSec = app_calc_lockout_seconds(appFailedStreak);

	              send_ui(UI_LOCKOUT, appLockoutRemainingSec);
	              send_output(OUT_LOCKOUT, 0);
	              send_logf("LOCKOUT %lu seconds\r\n", (unsigned long)appLockoutRemainingSec);

	              osTimerStart(lockoutTimerHandle, 1000);
	            }
	            else
	            {
	              appState = APP_TEMP_MESSAGE;
	              send_ui(UI_DENIED, appFailedStreak);
	              send_output(OUT_FAIL, 0);
	              osTimerStart(uiReturnTimerHandle, 800);
	            }
	          }
	        }
	        else
	        {
	          send_logf("Ignored key: %c\r\n", key);
	        }

	        break;
	      }

	      case EV_UI_TIMEOUT:
	        task_debug("Auth: UI timeout\r\n");
	        if (appState == APP_TEMP_MESSAGE)
	        {
	          appState = APP_IDLE;
	          send_ui(UI_IDLE, 0);
	          send_output(OUT_IDLE, 0);
	        }
	        break;

	      case EV_LOCKOUT_TICK:
	        task_debug("Auth: lockout tick\r\n");
	        if (appState == APP_LOCKOUT)
	        {
	          if (appLockoutRemainingSec > 0)
	          {
	            appLockoutRemainingSec--;
	          }

	          if (appLockoutRemainingSec == 0)
	          {
	            osTimerStop(lockoutTimerHandle);
	            appState = APP_IDLE;
	            send_ui(UI_IDLE, 0);
	            send_output(OUT_IDLE, 0);
	            send_logf("LOCKOUT ENDED\r\n");
	          }
	          else
	          {
	            send_ui(UI_LOCKOUT, appLockoutRemainingSec);
	          }
	        }
	        break;

	      default:
	        break;
	    }
	  }
  /* USER CODE END StartTask03 */
}

/* USER CODE BEGIN Header_StartTask04 */
/**
* @brief Function implementing the UiTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask04 */
void StartTask04(void *argument)
{
  /* USER CODE BEGIN StartTask04 */
	  uint32_t msg;
	  char dbg[40];
	  /* UiTask owns LCD initialization and all display updates */
	  task_debug("UiTask alive\r\n");

	  lcd_init_custom();
	  lcd_clear();
	  lcd_print_line(0, "Starting...");
	  lcd_print_line(1, "RTOS UI");
	  osDelay(200);

	  for(;;)
	  {
	    osMessageQueueGet(uiQueueHandle, &msg, NULL, osWaitForever);

	    snprintf(dbg, sizeof(dbg), "UI msg type=%u val=%u\r\n",
	             (unsigned)MSG_TYPE(msg), (unsigned)MSG_VALUE(msg));
	    task_debug(dbg);

	    switch (MSG_TYPE(msg))
	    {
	      case UI_IDLE:
	        lcd_print_line(0, "Enter PIN");
	        lcd_print_line(1, "PIN:");
	        break;

	      case UI_PIN_MASK:
	        ui_show_pin_mask((uint8_t)MSG_VALUE(msg));
	        break;

	      case UI_GRANTED:
	        lcd_print_line(0, "Access Granted");
	        lcd_print_line(1, "Welcome");
	        break;

	      case UI_DENIED:
	      {
	        char line[17];
	        snprintf(line, sizeof(line), "Fails: %u", (unsigned)MSG_VALUE(msg));
	        lcd_print_line(0, "Access Denied");
	        lcd_print_line(1, line);
	        break;
	      }

	      case UI_LOCKOUT:
	      {
	        char line[17];
	        snprintf(line, sizeof(line), "Try in %us", (unsigned)MSG_VALUE(msg));
	        lcd_print_line(0, "LOCKED OUT");
	        lcd_print_line(1, line);
	        break;
	      }

	      case UI_NEED4:
	        lcd_print_line(0, "Enter 4 digits");
	        lcd_print_line(1, "Then press #");
	        break;

	      default:
	        break;
	    }
	  }

  /* USER CODE END StartTask04 */
}

/* USER CODE BEGIN Header_StartTask05 */
/**
* @brief Function implementing the OutputTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask05 */
void StartTask05(void *argument)
{
  /* USER CODE BEGIN StartTask05 */
	  uint32_t msg;
	  /* OutputTask owns the LEDs and buzzer */

	  output_idle();

	  for(;;)
	  {
	    osMessageQueueGet(outputQueueHandle, &msg, NULL, osWaitForever);

	    switch (MSG_TYPE(msg))
	    {
	      case OUT_IDLE:
	        output_idle();
	        break;

	      case OUT_KEY:
	        output_key();
	        break;

	      case OUT_SUCCESS:
	        output_success();
	        break;

	      case OUT_FAIL:
	        output_fail();
	        break;

	      case OUT_LOCKOUT:
	        output_lockout();
	        break;

	      default:
	        break;
	    }
	  }
  /* USER CODE END StartTask05 */
}

/* USER CODE BEGIN Header_StartTask06 */
/**
* @brief Function implementing the LoggerTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask06 */
void StartTask06(void *argument)
{
  /* USER CODE BEGIN StartTask06 */
	  log_msg_t msg;

	  /* LoggerTask owns UART output for queued logs */
	  for(;;)
	  {
	    osMessageQueueGet(logQueueHandle, &msg, NULL, osWaitForever);
	    uart_print(msg.text);
	  }
  /* USER CODE END StartTask06 */
}

/* Callback01 function */
void Callback01(void *argument)
{
  /* USER CODE BEGIN Callback01 */
	  /* Periodic lockout timer callback: send one lockout tick event */
	  send_event(EV_LOCKOUT_TICK, 0);
  /* USER CODE END Callback01 */
}

/* Callback02 function */
void Callback02(void *argument)
{
  /* USER CODE BEGIN Callback02 */
	  /* One-shot UI timer callback: return temporary screens back to idle */
	  send_event(EV_UI_TIMEOUT, 0);
  /* USER CODE END Callback02 */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
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
