/**
  ******************************************************************************
  * @file    stm32fxxx_it.c
  * @author  yidianusb
  * @version V1.0.0
  * @date    2025-xx-xx
  * @brief   Main Interrupt Service Routines.
  *          This file provides all exceptions handler and peripherals interrupt
  *          service routine.
  ******************************************************************************
  * @attention
  *
  * 实验平台: 亿点冰糖葫STM32F407 OTG开发板
  * 淘    宝: https://yidianusb.taobao.com
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------ */
#include "stm32fxxx_it.h"
#include "bsp_joystick.h"
/* Private typedef ----------------------------------------------------------- */
/* Private define ------------------------------------------------------------ */
/* Private macro ------------------------------------------------------------- */
/* Private variables --------------------------------------------------------- */
__IO uint32_t remote_wakeup = 0;
/* Private function prototypes ----------------------------------------------- */
extern USB_OTG_CORE_HANDLE USB_OTG_dev;
extern uint32_t USBD_OTG_ISR_Handler(USB_OTG_CORE_HANDLE * pdev);
#ifdef USB_OTG_HS_DEDICATED_EP1_ENABLED
    extern uint32_t USBD_OTG_EP1IN_ISR_Handler(USB_OTG_CORE_HANDLE * pdev);
    extern uint32_t USBD_OTG_EP1OUT_ISR_Handler(USB_OTG_CORE_HANDLE * pdev);
#endif

/******************************************************************************/
/* Cortex-M Processor Exceptions Handlers */
/******************************************************************************/

/**
* @brief   This function handles NMI exception.
* @param  None
* @retval None
*/
void NMI_Handler(void)
{
}

/**
* @brief  This function handles Hard Fault exception.
* @param  None
* @retval None
*/
void HardFault_Handler(void)
{
    /* Go to infinite loop when Hard Fault exception occurs */
    while (1)
    {
    }
}

/**
* @brief  This function handles Memory Manage exception.
* @param  None
* @retval None
*/
void MemManage_Handler(void)
{
    /* Go to infinite loop when Memory Manage exception occurs */
    while (1)
    {
    }
}

/**
* @brief  This function handles Bus Fault exception.
* @param  None
* @retval None
*/
void BusFault_Handler(void)
{
    /* Go to infinite loop when Bus Fault exception occurs */
    while (1)
    {
    }
}

/**
* @brief  This function handles Usage Fault exception.
* @param  None
* @retval None
*/
void UsageFault_Handler(void)
{
    /* Go to infinite loop when Usage Fault exception occurs */
    while (1)
    {
    }
}

/**
* @brief  This function handles SVCall exception.
* @param  None
* @retval None
*/
void SVC_Handler(void)
{
}

/**
* @brief  This function handles Debug Monitor exception.
* @param  None
* @retval None
*/
void DebugMon_Handler(void)
{
}

/**
* @brief  This function handles PendSVC exception.
* @param  None
* @retval None
*/
void PendSV_Handler(void)
{
}

/**
* @brief  This function handles SysTick Handler.
* @param  None
* @retval None
*/
void SysTick_Handler(void)
{

}

/**
* @brief  This function handles EXTI15_10_IRQ Handler.
* @param  None
* @retval None
*/
#ifdef USE_USB_OTG_FS
void OTG_FS_WKUP_IRQHandler(void)
{
    if (USB_OTG_dev.cfg.low_power)
    {
        /* Reset SLEEPDEEP and SLEEPONEXIT bits */
        SCB->SCR &=
            (uint32_t) ~
            ((uint32_t) (SCB_SCR_SLEEPDEEP_Msk | SCB_SCR_SLEEPONEXIT_Msk));

        /* After wake-up from sleep mode, reconfigure the system clock */
        RCC_HSEConfig(RCC_HSE_ON);

        /* Wait till HSE is ready */
        while (RCC_GetFlagStatus(RCC_FLAG_HSERDY) == RESET)
        {
        }

        /* Enable PLL */
        RCC_PLLCmd(ENABLE);

        /* Wait till PLL is ready */
        while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
        {
        }

        /* Select PLL as system clock source */
        RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

        /* Wait till PLL is used as system clock source */
        while (RCC_GetSYSCLKSource() != 0x08)
        {
        }

        USB_OTG_UngateClock(&USB_OTG_dev);
    }

    EXTI_ClearITPendingBit(EXTI_Line18);
}
#endif

/**
* @brief  This function handles EXTI15_10_IRQ Handler.
* @param  None
* @retval None
*/
#ifdef USE_USB_OTG_HS
void OTG_HS_WKUP_IRQHandler(void)
{
    if (remote_wakeup == 1)
    {
        remote_wakeup = 0;
    }
    else
    {
        if (USB_OTG_dev.cfg.low_power)
        {
            /* Reset SLEEPDEEP and SLEEPONEXIT bits */
            SCB->SCR &=
                (uint32_t) ~
                ((uint32_t) (SCB_SCR_SLEEPDEEP_Msk | SCB_SCR_SLEEPONEXIT_Msk));

            /* After wake-up from sleep mode, reconfigure the system clock */
            RCC_HSEConfig(RCC_HSE_ON);

            /* Wait till HSE is ready */
            while (RCC_GetFlagStatus(RCC_FLAG_HSERDY) == RESET)
            {
            }

            /* Enable PLL */
            RCC_PLLCmd(ENABLE);

            /* Wait till PLL is ready */
            while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
            {
            }

            /* Select PLL as system clock source */
            RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

            /* Wait till PLL is used as system clock source */
            while (RCC_GetSYSCLKSource() != 0x08)
            {
            }

            USB_OTG_UngateClock(&USB_OTG_dev);
        }
    }

    EXTI_ClearITPendingBit(EXTI_Line20);
}
#endif

/**
* @brief  This function handles OTG_HS Handler.
* @param  None
* @retval None
*/
#ifdef USE_USB_OTG_HS
    void OTG_HS_IRQHandler(void)
#else
    void OTG_FS_IRQHandler(void)
#endif
{
    USBD_OTG_ISR_Handler(&USB_OTG_dev);
}

#ifdef USB_OTG_HS_DEDICATED_EP1_ENABLED
/**
* @brief  This function handles EP1_IN Handler.
* @param  None
* @retval None
*/
void OTG_HS_EP1_IN_IRQHandler(void)
{
    USBD_OTG_EP1IN_ISR_Handler(&USB_OTG_dev);
}

/**
* @brief  This function handles EP1_OUT Handler.
* @param  None
* @retval None
*/
void OTG_HS_EP1_OUT_IRQHandler(void)
{
    USBD_OTG_EP1OUT_ISR_Handler(&USB_OTG_dev);
}
#endif

#ifndef USE_STM3210C_EVAL
/**
  * @brief  This function handles SDIO global interrupt request.
  * @param  None
  * @retval None
  */
void SDIO_IRQHandler(void)
{
    /* Process All SDIO Interrupt Sources */
    SD_ProcessIRQSrc();
}
#endif

/******************************************************************************/
/* STM32Fxxx Peripherals Interrupt Handlers */
/* Add here the Interrupt Handler for the used peripheral(s) (PPP), for the */
/* available peripheral interrupt handler's name please refer to the startup */
/* file (startup_stm32fxxx.s).  */
/******************************************************************************/

/**
* @brief  This function handles PPP interrupt request.
* @param  None
* @retval None
*/
/* void PPP_IRQHandler(void) { } */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
