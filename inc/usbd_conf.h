/**
  ******************************************************************************
  * @file    usbd_conf.h
  * @author  yidianusb
  * @version V1.0.0
  * @date    2025-xx-xx
  * @brief   USB Device configuration file
  ******************************************************************************
  * @attention
  *
  * 实验平台: 亿点冰糖葫STM32F407 OTG开发板
  * 淘    宝: https://yidianusb.taobao.com
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_CONF__H__
#define __USBD_CONF__H__

/* Includes ------------------------------------------------------------------*/
#include "usb_conf.h"

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @{
  */
  
/** @defgroup USBD_CONF
  * @brief This file is the device library configuration file
  * @{
  */ 

/** @defgroup USBD_CONF_Exported_Defines
  * @{
  */ 


#define USBD_CFG_MAX_NUM           1
#define USBD_ITF_MAX_NUM           1
#define USB_MAX_STR_DESC_SIZ       255 

#define USBD_SELF_POWERED               

/* Class Layer Parameter */

#define MSC_IN_EP                    0x82
#define MSC_OUT_EP                   0x02

#define HID_IN_EP                    0x81

/*4 Bytes max*/
#define HID_IN_PACKET                4

#ifdef USE_USB_OTG_HS  
#ifdef USE_ULPI_PHY
#define MSC_MAX_PACKET               512
#else
#define MSC_MAX_PACKET               64
#endif
#else  /*USE_USB_OTG_FS*/
#define MSC_MAX_PACKET                64
#endif


#define MSC_MEDIA_PACKET             (32*1024)

/**
  * @}
  */ 


/** @defgroup USB_CONF_Exported_TypesDefinitions
  * @{
  */ 
/**
  * @}
  */ 


/** @defgroup USB_CONF_Exported_Macros
  * @{
  */ 
/**
  * @}
  */ 

/** @defgroup USB_CONF_Exported_Variables
  * @{
  */ 
/**
  * @}
  */ 

/** @defgroup USB_CONF_Exported_FunctionsPrototype
  * @{
  */ 
/**
  * @}
  */ 

#endif //__USBD_CONF__H__

/**
  * @}
  */ 

/**
  * @}
  */ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

