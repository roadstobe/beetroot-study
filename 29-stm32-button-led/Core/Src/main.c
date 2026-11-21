/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * Урок 29 / Модуль 2.8 — STM32 знайомство
  *
  * Плата: WeAct Black Pill (STM32F411CEU6)
  *   LED  — PC13, активний у нулі (катод через резистор на пін, анод на 3V3)
  *   KEY  — PA0,  кнопка замикає пін на GND, потрібен внутрішній pull-up
  *
  * Поведінка: поки кнопка натиснута — світлодіод блимає (0.5 с),
  *            коли відпущена — вимкнений.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

/* USER CODE BEGIN PD */
#define LED_GPIO_Port GPIOC
#define LED_Pin       GPIO_PIN_13
#define KEY_GPIO_Port GPIOA
#define KEY_Pin       GPIO_PIN_0

/* PC13: LED активний у нулі, тому RESET = світить, SET = погашений */
#define LED_ON        GPIO_PIN_RESET
#define LED_OFF       GPIO_PIN_SET
/* USER CODE END PD */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();

  while (1)
  {
    /* USER CODE BEGIN WHILE */
    if (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_RESET)
    {
      /* кнопка натиснута — код з уроку */
      HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
      HAL_Delay(500);
    }
    else
    {
      HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, LED_OFF);
    }
    /* USER CODE END WHILE */
  }
}

/**
  * @brief Тактування від внутрішнього HSI (16 МГц), без PLL.
  *        Для блимання швидкість не потрібна, а HSI не залежить від кварцу на платі.
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* світлодіод погашений ще до налаштування піна, щоб не блимнув на старті */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, LED_OFF);

  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /* кнопка замикає пін на GND, тому підтяжка до 3V3 — внутрішня */
  GPIO_InitStruct.Pin = KEY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(KEY_GPIO_Port, &GPIO_InitStruct);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
