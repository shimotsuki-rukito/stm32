/**
  ******************************************************************************
  * @file    usbd_msc_hid_core.c
  * @brief   Composite MTP + MSC device class.
  *          Interface 0: MTP (Vendor Specific, driven by MS OS Descriptors)
  *          Interface 1: MSC (Bulk-Only Transport)
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "usbd_msc_hid_core.h"
#include "usbd_msc_core.h"
#include "usbd_desc.h"
#include "usbd_req.h"


/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @{
  */


/** @defgroup USBD_MSC_MTP
  * @brief Composite MSC + MTP module
  * @{
  */

/** @defgroup USBD_MSC_MTP_Private_FunctionPrototypes
  * @{
  */

static uint8_t  USBD_MSC_HID_Init   (void *pdev, uint8_t cfgidx);
static uint8_t  USBD_MSC_HID_DeInit (void *pdev, uint8_t cfgidx);
static uint8_t  USBD_MSC_HID_Setup  (void *pdev, USB_SETUP_REQ *req);
static uint8_t *USBD_MSC_HID_GetCfgDesc(uint8_t speed, uint16_t *length);
static uint8_t  USBD_MSC_HID_DataIn (void *pdev, uint8_t epnum);
static uint8_t  USBD_MSC_HID_DataOut(void *pdev, uint8_t epnum);

/**
  * @}
  */

/** @defgroup USBD_MSC_MTP_Private_Variables
  * @{
  */

USBD_Class_cb_TypeDef  USBD_MSC_HID_cb =
{
    USBD_MSC_HID_Init,
    USBD_MSC_HID_DeInit,
    USBD_MSC_HID_Setup,
    NULL,   /* EP0_TxSent  */
    NULL,   /* EP0_RxReady */
    USBD_MSC_HID_DataIn,
    USBD_MSC_HID_DataOut,
    NULL,   /* SOF         */
    NULL,
    NULL,
    USBD_MSC_HID_GetCfgDesc,
#ifdef USB_OTG_HS_CORE
    USBD_MSC_HID_GetCfgDesc,  /* same descriptor for FS */
#endif
};

#ifdef USB_OTG_HS_INTERNAL_DMA_ENABLED
  #if defined ( __ICCARM__ )
    #pragma data_alignment=4
  #endif
#endif /* USB_OTG_HS_INTERNAL_DMA_ENABLED */

/*
 * Configuration descriptor layout (62 bytes total):
 *
 *  [0]  9  – Configuration descriptor
 *  [9]  9  – Interface 0: MTP (Vendor Specific, 3 endpoints)
 * [18]  7  – Endpoint: MTP Bulk IN  (0x81)
 * [25]  7  – Endpoint: MTP Bulk OUT (0x01)
 * [32]  7  – Endpoint: MTP Interrupt IN (0x83, event notifications)
 * [39]  9  – Interface 1: MSC (Bulk-Only Transport, 2 endpoints)
 * [48]  7  – Endpoint: MSC Bulk IN  (0x82)
 * [55]  7  – Endpoint: MSC Bulk OUT (0x02)
 */
__ALIGN_BEGIN static uint8_t USBD_MSC_HID_CfgDesc[USB_MSC_MTP_CONFIG_DESC_SIZ] __ALIGN_END =
{
    /* ---------- Configuration Descriptor ---------- */
    0x09,                            /* bLength */
    USB_CONFIGURATION_DESCRIPTOR_TYPE, /* bDescriptorType */
    LOBYTE(USB_MSC_MTP_CONFIG_DESC_SIZ), /* wTotalLength lo */
    HIBYTE(USB_MSC_MTP_CONFIG_DESC_SIZ), /* wTotalLength hi */
    0x02,                            /* bNumInterfaces */
    0x01,                            /* bConfigurationValue */
    0x00,                            /* iConfiguration */
    0xC0,                            /* bmAttributes: bus powered */
    0x32,                            /* MaxPower 100 mA */

    /* ---------- Interface 0: MTP (Still Image class) ----------
     * bInterfaceClass=0x06/0x01/0x01 causes Windows to auto-load
     * wpdmtp.inf (WUDFWpdMtp service), matching real iQOO 13 behaviour.
     */
    0x09,                            /* bLength */
    USB_INTERFACE_DESCRIPTOR_TYPE,   /* bDescriptorType */
    MTP_INTERFACE,                   /* bInterfaceNumber (0) */
    0x00,                            /* bAlternateSetting */
    0x03,                            /* bNumEndpoints */
    0x06,                            /* bInterfaceClass: Still Image (MTP/PTP) */
    0x01,                            /* bInterfaceSubClass */
    0x01,                            /* bInterfaceProtocol */
    0x00,                            /* iInterface */

    /* MTP Bulk IN endpoint */
    0x07,                            /* bLength */
    USB_ENDPOINT_DESCRIPTOR_TYPE,    /* bDescriptorType */
    MTP_IN_EP,                       /* bEndpointAddress: 0x81 IN */
    0x02,                            /* bmAttributes: Bulk */
    LOBYTE(MTP_MAX_PACKET),          /* wMaxPacketSize lo */
    HIBYTE(MTP_MAX_PACKET),          /* wMaxPacketSize hi */
    0x00,                            /* bInterval */

    /* MTP Bulk OUT endpoint */
    0x07,                            /* bLength */
    USB_ENDPOINT_DESCRIPTOR_TYPE,    /* bDescriptorType */
    MTP_OUT_EP,                      /* bEndpointAddress: 0x01 OUT */
    0x02,                            /* bmAttributes: Bulk */
    LOBYTE(MTP_MAX_PACKET),          /* wMaxPacketSize lo */
    HIBYTE(MTP_MAX_PACKET),          /* wMaxPacketSize hi */
    0x00,                            /* bInterval */

    /* MTP Interrupt IN endpoint (event notifications) */
    0x07,                            /* bLength */
    USB_ENDPOINT_DESCRIPTOR_TYPE,    /* bDescriptorType */
    MTP_CMD_EP,                      /* bEndpointAddress: 0x83 IN */
    0x03,                            /* bmAttributes: Interrupt */
    LOBYTE(MTP_CMD_PACKET_SIZE),     /* wMaxPacketSize lo */
    HIBYTE(MTP_CMD_PACKET_SIZE),     /* wMaxPacketSize hi */
    0x0A,                            /* bInterval: 10 ms */

    /* ---------- Interface 1: MSC (Bulk-Only Transport) ---------- */
    0x09,                            /* bLength */
    USB_INTERFACE_DESCRIPTOR_TYPE,   /* bDescriptorType */
    MSC_INTERFACE,                   /* bInterfaceNumber (1) */
    0x00,                            /* bAlternateSetting */
    0x02,                            /* bNumEndpoints */
    0x08,                            /* bInterfaceClass: Mass Storage */
    0x06,                            /* bInterfaceSubClass: SCSI Transparent */
    0x50,                            /* bInterfaceProtocol: Bulk-Only */
    0x05,                            /* iInterface */

    /* MSC Bulk IN endpoint */
    0x07,                            /* bLength */
    USB_ENDPOINT_DESCRIPTOR_TYPE,    /* bDescriptorType */
    MSC_IN_EP,                       /* bEndpointAddress: 0x82 IN */
    0x02,                            /* bmAttributes: Bulk */
    LOBYTE(MSC_MAX_PACKET),          /* wMaxPacketSize lo */
    HIBYTE(MSC_MAX_PACKET),          /* wMaxPacketSize hi */
    0x00,                            /* bInterval */

    /* MSC Bulk OUT endpoint */
    0x07,                            /* bLength */
    USB_ENDPOINT_DESCRIPTOR_TYPE,    /* bDescriptorType */
    MSC_OUT_EP,                      /* bEndpointAddress: 0x02 OUT */
    0x02,                            /* bmAttributes: Bulk */
    LOBYTE(MSC_MAX_PACKET),          /* wMaxPacketSize lo */
    HIBYTE(MSC_MAX_PACKET),          /* wMaxPacketSize hi */
    0x00,                            /* bInterval */
};

/**
  * @}
  */

/*---  MSC callbacks (external)  ---*/
extern uint8_t  USBD_MSC_Init    (void *pdev, uint8_t cfgidx);
extern uint8_t  USBD_MSC_DeInit  (void *pdev, uint8_t cfgidx);
extern uint8_t  USBD_MSC_Setup   (void *pdev, USB_SETUP_REQ *req);
extern uint8_t  USBD_MSC_DataIn  (void *pdev, uint8_t epnum);
extern uint8_t  USBD_MSC_DataOut (void *pdev, uint8_t epnum);

/** @defgroup USBD_MSC_MTP_Private_Functions
  * @{
  */

/**
  * @brief  USBD_MSC_HID_Init
  *         Open all endpoints and initialise the MSC class.
  *         The MTP endpoints are opened here; MTP protocol handling
  *         is performed by the host (Windows WUDFWpdMtp driver).
  */
static uint8_t USBD_MSC_HID_Init(void *pdev, uint8_t cfgidx)
{
    /* Open MTP bulk IN */
    DCD_EP_Open(pdev, MTP_IN_EP,  MTP_MAX_PACKET,       EP_TYPE_BULK);
    /* Open MTP bulk OUT */
    DCD_EP_Open(pdev, MTP_OUT_EP, MTP_MAX_PACKET,       EP_TYPE_BULK);
    /* Open MTP interrupt IN (event channel) */
    DCD_EP_Open(pdev, MTP_CMD_EP, MTP_CMD_PACKET_SIZE,  EP_TYPE_INTR);

    /* Initialise MSC */
    USBD_MSC_Init(pdev, cfgidx);

    return USBD_OK;
}

/**
  * @brief  USBD_MSC_HID_DeInit
  *         Close all endpoints.
  */
static uint8_t USBD_MSC_HID_DeInit(void *pdev, uint8_t cfgidx)
{
    DCD_EP_Close(pdev, MTP_IN_EP);
    DCD_EP_Close(pdev, MTP_OUT_EP);
    DCD_EP_Close(pdev, MTP_CMD_EP);

    USBD_MSC_DeInit(pdev, cfgidx);

    return USBD_OK;
}

/**
  * @brief  USBD_MSC_HID_Setup
  *         Route class-specific setup requests to the correct handler.
  */
static uint8_t USBD_MSC_HID_Setup(void *pdev, USB_SETUP_REQ *req)
{
    switch (req->bmRequest & USB_REQ_RECIPIENT_MASK)
    {
    case USB_REQ_RECIPIENT_INTERFACE:
        if (req->wIndex == MTP_INTERFACE)
        {
            /* MTP has no mandatory class-specific control requests;
             * stall anything unexpected. */
            USBD_CtlError(pdev, req);
        }
        else
        {
            return USBD_MSC_Setup(pdev, req);
        }
        break;

    case USB_REQ_RECIPIENT_ENDPOINT:
        if (req->wIndex == MSC_IN_EP || req->wIndex == MSC_OUT_EP)
        {
            return USBD_MSC_Setup(pdev, req);
        }
        break;

    default:
        break;
    }
    return USBD_OK;
}

/**
  * @brief  USBD_MSC_HID_GetCfgDesc
  */
static uint8_t *USBD_MSC_HID_GetCfgDesc(uint8_t speed, uint16_t *length)
{
    *length = sizeof(USBD_MSC_HID_CfgDesc);
    return USBD_MSC_HID_CfgDesc;
}

/**
  * @brief  USBD_MSC_HID_DataIn
  *         Route IN data to the correct handler.
  */
static uint8_t USBD_MSC_HID_DataIn(void *pdev, uint8_t epnum)
{
    if (epnum == (MSC_IN_EP & ~0x80U))
    {
        return USBD_MSC_DataIn(pdev, epnum);
    }
    /* MTP IN data: nothing to do in this stub implementation */
    return USBD_OK;
}

/**
  * @brief  USBD_MSC_HID_DataOut
  *         Route OUT data to the correct handler.
  */
static uint8_t USBD_MSC_HID_DataOut(void *pdev, uint8_t epnum)
{
    if (epnum == (MSC_OUT_EP & ~0x80U))
    {
        return USBD_MSC_DataOut(pdev, epnum);
    }
    /* MTP OUT data: nothing to do in this stub implementation */
    return USBD_OK;
}

/**
  * @}
  */


/**
  * @}
  */


/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
