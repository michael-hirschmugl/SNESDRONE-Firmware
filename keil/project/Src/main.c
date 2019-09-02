
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  ** This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * COPYRIGHT(c) 2019 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f4xx_hal.h"
#include "adc.h"
#include "dma.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include "rom_bank_0.h"

#define SNES_IRQ_OFF 0xFFFF0008U
#define TRANS_SNES_2_CART 0xFFFF000CU
#define SNES_CART_OFF 0xFFFF0010U

#define MISC_PORT_REG_WR GPIOA->BSRR
#define MISC_PORT_REG_RD GPIOA->IDR
#define ADDR_PORT_REG_RD GPIOF->IDR
#define DATA_PORT_REG_WR GPIOD->BSRR

#define RST_VEC_LOC_LO 0x0000FFFCU
#define RST_VEC_LOC_HI 0x0000FFFDU
#define RST_VEC_BYT_LO 0xFFFF0000U
#define RST_VEC_BYT_HI 0xFFFF0088U
#define NMI_VEC_LOC_LO 0x0000FFEAU
#define NMI_VEC_LOC_HI 0x0000FFEBU
#define BANK0_FUL_SIZE 0x00007FFFU
#define BANK0_MAX_SIZE 0x00004FFFU

#define PORT_WRIT_MASK 0xFFFF0000U

/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

uint16_t temp = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
volatile uint32_t adc_32[8];

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  *
  * @retval None
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

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
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
	
	HAL_ADC_Start_DMA(&hadc1,adc_32,8);  // Enable ADC DMA
	
	__disable_irq();
	
	IWDG->KR = ((uint16_t)0xAAAA);	// Watchdog no bark

  MISC_PORT_REG_WR = SNES_IRQ_OFF; // TRANSCEIVER CART TO SNES (and disable IRQ line)

  // Provide Reset Vector to SNES CPU
	// Vector defined as 0x8800 at compile time
  while(GPIOF->IDR != RST_VEC_LOC_LO){}  // Wait for Reset Vector query
  DATA_PORT_REG_WR = RST_VEC_BYT_LO;
  while(GPIOF->IDR != RST_VEC_LOC_HI){}  // Wait for 2nd byte
  DATA_PORT_REG_WR = RST_VEC_BYT_HI;  // write three times for timing
	DATA_PORT_REG_WR = RST_VEC_BYT_HI;
	DATA_PORT_REG_WR = RST_VEC_BYT_HI;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */

		if((MISC_PORT_REG_RD & SNES_CART_OFF))  // No Cart read access means transceiver direction change
    {
      MISC_PORT_REG_WR = TRANS_SNES_2_CART;  // TRANSCEIVER SNES TO CART
      while(MISC_PORT_REG_RD & SNES_CART_OFF){}  // Wait for it...
      MISC_PORT_REG_WR = SNES_IRQ_OFF;  // TRANSCEIVER CART TO SNES
    }
		else
		{
			temp = (ADDR_PORT_REG_RD & BANK0_FUL_SIZE);  // Assuming Cart read access (cut all above bank 0)
			
			if(temp < BANK0_MAX_SIZE)  // Bank 0 maximum size = 0x4FFF (20.479 bytes)
			{
				DATA_PORT_REG_WR = (PORT_WRIT_MASK | rom_bank_0[temp]);  // Provide ROM data
			}
			else  // assuming NMI vector fetch (above 0x4FFF)
			{
				if(temp == NMI_VEC_LOC_LO)
					DATA_PORT_REG_WR = (PORT_WRIT_MASK | rom_bank_0[0x4FEAU]);  // 1st byte of NMI vector needs to be stored at 0x4FEA (20.458) in ROM
				
				if(temp == NMI_VEC_LOC_HI)
				{
					DATA_PORT_REG_WR = (PORT_WRIT_MASK | rom_bank_0[0x4FEBU]);  // 2nd byte of NMI vector needs to be stored at 0x4FEB (20.459) in ROM
					
					while(!(MISC_PORT_REG_RD & SNES_CART_OFF)){}  // Wait for SNES to finish reading NMI vector
					MISC_PORT_REG_WR = TRANS_SNES_2_CART;  // TRANSCEIVER SNES TO CART
					/*                                                                   */
					/* MCU is "off the hook" now.                                        */
					/* There's a lot of processing going on in the SNES CPU right now    */
					/* and during this time the MCU is able to read ADC inputs.          */
					/*                                                                   */

					rom_bank_0[17413] = (uint8_t)((adc_32[0] / 8) >> 8);  // Freq1 HI
					rom_bank_0[17412] = (uint8_t)((adc_32[0] / 8) & 0xFF);  // Freq1 LO

					rom_bank_0[17424] = (uint8_t)((adc_32[1] / 8) >> 8);  // Freq2 HI
					rom_bank_0[17423] = (uint8_t)((adc_32[1] / 8) & 0xFF);  // Freq2 LO

					rom_bank_0[17435] = (uint8_t)((adc_32[2] / 8) >> 8);  // Freq3 HI
					rom_bank_0[17436] = (uint8_t)((adc_32[2] / 8) & 0xFF);  // Freq3 LO

					rom_bank_0[17446] = (uint8_t)((adc_32[3] / 8) >> 8);  // Freq4 HI
					rom_bank_0[17445] = (uint8_t)((adc_32[3] / 8) & 0xFF);  // Freq4 LO
						
					/*                                                                   */
					/* MCU going back to "normal" ROM simulation mode...                 */
					/*                                                                   */

					while(MISC_PORT_REG_RD & SNES_CART_OFF){}
						MISC_PORT_REG_WR = SNES_IRQ_OFF; // TRANSCEIVER CART TO SNES
				}
			}
		}
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;

    /**Configure the main internal regulator output voltage 
    */
  __HAL_RCC_PWR_CLK_ENABLE();

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = 16;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Activate the Over-Drive mode 
    */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Configure the Systick interrupt time 
    */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    /**Configure the Systick 
    */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  file: The file name as string.
  * @param  line: The line in file as a number.
  * @retval None
  */
void _Error_Handler(char *file, int line)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while(1)
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
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
