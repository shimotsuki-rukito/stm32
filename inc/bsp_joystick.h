/**
  ******************************************************************************
  * @file    bsp_joystick.h
  * @author  yidianusb
  * @version V1.0.0
  * @date    2025-xx-xx
  * @brief   adc驱动摇杆头文件.
  ******************************************************************************
  * @attention
  *
  * 实验平台: 亿点冰糖葫STM32F407 OTG开发板
  * 淘    宝: https://yidianusb.taobao.com
  *
  ******************************************************************************
  */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BSP_JOYSTICK_H
#define	__BSP_JOYSTICK_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/

/* JoyStick X */
#define JOYSTICK_X_ADC_GPIO_PORT      GPIOF
#define JOYSTICK_X_ADC_GPIO_PIN       GPIO_Pin_6
#define JOYSTICK_X_ADC_GPIO_CLK       RCC_AHB1Periph_GPIOF
#define JOYSTICK_X_ADC                ADC3
#define JOYSTICK_X_ADC_CLK_CMD        RCC_APB2PeriphClockCmd
#define JOYSTICK_X_ADC_CLK            RCC_APB2Periph_ADC3
#define JOYSTICK_X_ADC_CHANNEL        ADC_Channel_4
#define JOYSTICK_X_ADC_IRQ            ADC_IRQn
#define JOYSTICK_X_ADC_INT_FUNCTION   ADC_IRQHandler

/* JoyStick Y */
#define JOYSTICK_Y_ADC_GPIO_PORT      GPIOF
#define JOYSTICK_Y_ADC_GPIO_PIN       GPIO_Pin_7
#define JOYSTICK_Y_ADC_GPIO_CLK       RCC_AHB1Periph_GPIOF
#define JOYSTICK_Y_ADC                ADC3
#define JOYSTICK_Y_ADC_CLK_CMD        RCC_APB2PeriphClockCmd
#define JOYSTICK_Y_ADC_CLK            RCC_APB2Periph_ADC3
#define JOYSTICK_Y_ADC_CHANNEL        ADC_Channel_5
#define JOYSTICK_Y_ADC_IRQ            ADC_IRQn
#define JOYSTICK_Y_ADC_INT_FUNCTION   ADC_IRQHandler

/* JoyStick SW */
#define JOYSTICK_SW_ADC_GPIO_PORT     GPIOF
#define JOYSTICK_SW_ADC_GPIO_PIN      GPIO_Pin_5
#define JOYSTICK_SW_ADC_GPIO_CLK      RCC_AHB1Periph_GPIOF


/* Exported macro ------------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/ 

void JoyStick_Init(void);

#ifdef __cplusplus
}
#endif


#endif /* __BSP_JOYSTICK_H */



