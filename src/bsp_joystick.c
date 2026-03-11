/**
  ******************************************************************************
  * @file    bsp_joystick.c
  * @author  yidianusb
  * @version V1.0.0
  * @date    2025-xx-xx
  * @brief   adc驱动摇杆模块
  ******************************************************************************
  * @attention
  *
  * 实验平台: 亿点冰糖葫STM32F407 OTG开发板
  * 淘    宝: https://yidianusb.taobao.com
  *
  ******************************************************************************
  */
  
/* Includes ------------------------------------------------------------------*/  
#include "bsp_joystick.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/ 
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Joystick ADC gpio configuration
  * @param  None
  * @retval None
  */
static void JoyStick_ADC_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* Enable peripheral clocks */
    RCC_AHB1PeriphClockCmd(JOYSTICK_X_ADC_GPIO_CLK | JOYSTICK_Y_ADC_GPIO_CLK, ENABLE);

    /* Configure ADC Channel as analog input */
    /* JoyStick X */
    GPIO_InitStructure.GPIO_Pin = JOYSTICK_X_ADC_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP ;
    GPIO_InitStructure.GPIO_Speed = GPIO_Fast_Speed ;
    GPIO_Init(JOYSTICK_X_ADC_GPIO_PORT, &GPIO_InitStructure);
    
    /* JoyStick Y */
    GPIO_InitStructure.GPIO_Pin = JOYSTICK_Y_ADC_GPIO_PIN;
    GPIO_Init(JOYSTICK_Y_ADC_GPIO_PORT, &GPIO_InitStructure);  

    /* JoyStick SW */
    GPIO_InitStructure.GPIO_Pin = JOYSTICK_SW_ADC_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN ;
    GPIO_Init(JOYSTICK_SW_ADC_GPIO_PORT, &GPIO_InitStructure);   
  
}


/**
  * @brief  Joystick ADC mode configuration
  * @param  None
  * @retval None
  */
static void JoyStick_ADC_Mode_Config(void)
{
    ADC_InitTypeDef ADC_InitStructure;
    ADC_CommonInitTypeDef ADC_CommonInitStructure;

    /* Open ADC clock */
    JOYSTICK_X_ADC_CLK_CMD(JOYSTICK_X_ADC_CLK, ENABLE);
    JOYSTICK_Y_ADC_CLK_CMD(JOYSTICK_Y_ADC_CLK, ENABLE);

    /* -------------------ADC Common 结构体 参数 初始化----------------------- */
    ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div2;                    // 时钟为fpclk x分频
    ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;        // 禁止DMA直接访问模式
    ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_20Cycles;  // 采样时间间隔
    ADC_CommonInit(&ADC_CommonInitStructure);

    /* -------------------ADC Init 结构体 参数 初始化-------------------------- */
    ADC_StructInit(&ADC_InitStructure);
    ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;                          
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfConversion = 2;  //转换通道 2个
    ADC_Init(JOYSTICK_X_ADC, &ADC_InitStructure);
 
    ADC_InjectedSequencerLengthConfig(JOYSTICK_X_ADC, 2);

    ADC_InjectedChannelConfig(JOYSTICK_X_ADC, JOYSTICK_X_ADC_CHANNEL, 1, ADC_SampleTime_480Cycles);
    ADC_InjectedChannelConfig(JOYSTICK_Y_ADC, JOYSTICK_Y_ADC_CHANNEL, 2, ADC_SampleTime_480Cycles);

    /* Enable ADC */
    ADC_Cmd(JOYSTICK_X_ADC, ENABLE);

    ADC_SoftwareStartInjectedConv(JOYSTICK_X_ADC);
}

/**
  * @brief  Joystick initialization
  * @param  None
  * @retval None
  */
void JoyStick_Init(void)
{
    JoyStick_ADC_GPIO_Config();
    JoyStick_ADC_Mode_Config();
}



