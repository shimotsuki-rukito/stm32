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
#include "usb_dcd.h"


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

    /* ---------- Interface 0: MTP (Vendor Specific + MS OS Descriptor) ----------
     * Keep class 0xFF so Windows queries the MS OS String Descriptor (0xEE).
     * Firmware returns CompatibleID="MTP" via vendor request bRequest=0x01,
     * wIndex=0x0004.  Windows then matches VID_2D95&PID_6012&MS_COMP_MTP in
     * wpdmtp.inf, loading "iQOO 13"/"vivo" strings – same path as real phone.
     */
    0x09,                            /* bLength */
    USB_INTERFACE_DESCRIPTOR_TYPE,   /* bDescriptorType */
    MTP_INTERFACE,                   /* bInterfaceNumber (0) */
    0x00,                            /* bAlternateSetting */
    0x03,                            /* bNumEndpoints */
    0xFF,                            /* bInterfaceClass: Vendor Specific */
    0xFF,                            /* bInterfaceSubClass */
    0x00,                            /* bInterfaceProtocol */
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

/* ---------------------------------------------------------------------------
 * MTP responder state machine
 *
 * Windows flow (WUDFWpdMtp):
 *   1. Arms MTP Bulk OUT endpoint → sends OpenSession command container
 *   2. Device sends OK response container (12 bytes, Bulk IN)
 *   3. Host sends GetDeviceInfo command container
 *   4. Device sends DeviceInfo data container (119 bytes, Bulk IN)
 *   5. Device sends OK response container (12 bytes, Bulk IN)
 *   6. Windows reads Manufacturer="vivo", Model="iQOO 13" from the dataset
 *      and writes them to the registry (FriendlyName / DEVPKEY_Device_Manufacturer)
 * ---------------------------------------------------------------------------
 */

typedef enum {
    MTP_STATE_IDLE = 0,
    MTP_STATE_SENDING_DATA,   /* data container in flight */
    MTP_STATE_SENDING_RESP,   /* response container in flight */
} MTP_State_t;

static MTP_State_t mtp_state          = MTP_STATE_IDLE;
static uint32_t    mtp_transaction_id = 0;

/* Receive buffer – large enough for any MTP command container (max 12+20 bytes) */
static uint8_t mtp_rx_buf[MTP_MAX_PACKET];

/*
 * MTP GetDeviceInfo data container (119 bytes).
 *
 * Layout: 12-byte container header + 107-byte DeviceInfo dataset.
 *
 * Bytes [8..11] = TransactionID, filled at runtime before each send.
 *
 * DeviceInfo dataset fields (MTP 1.1 §5.4.1):
 *   StandardVersion           UINT16  = 100
 *   MTPVendorExtensionID      UINT32  = 6 (Microsoft)
 *   MTPVersion                UINT16  = 100
 *   MTPExtensions             String  = "" (empty)
 *   FunctionalMode            UINT16  = 0
 *   OperationsSupported       AUInt16 = {0x1001, 0x1002}
 *   EventsSupported           AUInt16 = {}
 *   DevicePropertiesSupported AUInt16 = {}
 *   CaptureFormats            AUInt16 = {}
 *   PlaybackFormats           AUInt16 = {}
 *   Manufacturer              String  = "vivo"
 *   Model                     String  = "iQOO 13"
 *   DeviceVersion             String  = "2.00"
 *   SerialNumber              String  = "10AECS0322003GZ"
 */
static uint8_t mtp_dev_info_container[119] =
{
    /* ---- Container header (12 bytes) ---- */
    0x77, 0x00, 0x00, 0x00,         /* Length = 119 = 0x77 */
    0x02, 0x00,                      /* ContainerType = 2 (Data) */
    0x01, 0x10,                      /* Code = 0x1001 (GetDeviceInfo) */
    0x00, 0x00, 0x00, 0x00,         /* TransactionID – filled at runtime */

    /* ---- DeviceInfo dataset (107 bytes) ---- */
    0x64, 0x00,                      /* StandardVersion = 100 */
    0x06, 0x00, 0x00, 0x00,         /* MTPVendorExtensionID = 6 */
    0x64, 0x00,                      /* MTPVersion = 100 */
    0x00,                            /* MTPExtensions: NumChars=0 (empty) */
    0x00, 0x00,                      /* FunctionalMode = 0 */
    /* OperationsSupported: count=2, {0x1001, 0x1002} */
    0x02, 0x00, 0x00, 0x00,
    0x01, 0x10,
    0x02, 0x10,
    0x00, 0x00, 0x00, 0x00,         /* EventsSupported: count=0 */
    0x00, 0x00, 0x00, 0x00,         /* DevicePropertiesSupported: count=0 */
    0x00, 0x00, 0x00, 0x00,         /* CaptureFormats: count=0 */
    0x00, 0x00, 0x00, 0x00,         /* PlaybackFormats: count=0 */
    /* Manufacturer: "vivo" – NumChars=5 (4 chars + null), UTF-16LE */
    0x05,
    'v', 0x00, 'i', 0x00, 'v', 0x00, 'o', 0x00, 0x00, 0x00,
    /* Model: "iQOO 13" – NumChars=8 (7 chars + null), UTF-16LE */
    0x08,
    'i', 0x00, 'Q', 0x00, 'O', 0x00, 'O', 0x00,
    ' ', 0x00, '1', 0x00, '3', 0x00, 0x00, 0x00,
    /* DeviceVersion: "2.00" – NumChars=5, UTF-16LE */
    0x05,
    '2', 0x00, '.', 0x00, '0', 0x00, '0', 0x00, 0x00, 0x00,
    /* SerialNumber: "10AECS0322003GZ" – NumChars=16, UTF-16LE */
    0x10,
    '1', 0x00, '0', 0x00, 'A', 0x00, 'E', 0x00,
    'C', 0x00, 'S', 0x00, '0', 0x00, '3', 0x00,
    '2', 0x00, '2', 0x00, '0', 0x00, '0', 0x00,
    '3', 0x00, 'G', 0x00, 'Z', 0x00, 0x00, 0x00,
};

/*
 * MTP response container (12 bytes, no parameters).
 * ResponseCode and TransactionID are filled at runtime.
 */
static uint8_t mtp_response_buf[12] =
{
    0x0C, 0x00, 0x00, 0x00,         /* Length = 12 */
    0x03, 0x00,                      /* ContainerType = 3 (Response) */
    0x01, 0x20,                      /* ResponseCode = 0x2001 (OK) */
    0x00, 0x00, 0x00, 0x00,         /* TransactionID – filled at runtime */
};

/**
  * @brief  Queue an MTP response container on the Bulk IN endpoint.
  * @param  pdev:      USB device handle
  * @param  tid:       TransactionID to echo back
  * @param  resp_code: MTP response code (e.g. 0x2001 = OK)
  */
static void MTP_SendResponse(void *pdev, uint32_t tid, uint16_t resp_code)
{
    mtp_response_buf[6]  = (uint8_t)(resp_code);
    mtp_response_buf[7]  = (uint8_t)(resp_code >> 8);
    mtp_response_buf[8]  = (uint8_t)(tid);
    mtp_response_buf[9]  = (uint8_t)(tid >> 8);
    mtp_response_buf[10] = (uint8_t)(tid >> 16);
    mtp_response_buf[11] = (uint8_t)(tid >> 24);
    mtp_state = MTP_STATE_SENDING_RESP;
    DCD_EP_Tx(pdev, MTP_IN_EP, mtp_response_buf, sizeof(mtp_response_buf));
}

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

    /* Arm the MTP Bulk OUT endpoint to receive the first command container.
     * Without this call the endpoint stays in NAK and Windows never gets
     * a response to its GetDeviceInfo request. */
    mtp_state = MTP_STATE_IDLE;
    DCD_EP_PrepareRx(pdev, MTP_OUT_EP, mtp_rx_buf, MTP_MAX_PACKET);

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

    mtp_state = MTP_STATE_IDLE;

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
  *         Route IN-transfer-complete callbacks to the correct handler.
  *
  *  epnum=2 → MSC IN done  (forward to MSC handler)
  *  epnum=1 → MTP IN done  (advance MTP state machine)
  */
static uint8_t USBD_MSC_HID_DataIn(void *pdev, uint8_t epnum)
{
    if (epnum == (MSC_IN_EP & ~0x80U))
    {
        return USBD_MSC_DataIn(pdev, epnum);
    }

    /* MTP Bulk IN transfer complete (epnum == 1) */
    if (epnum == (MTP_IN_EP & ~0x80U))
    {
        if (mtp_state == MTP_STATE_SENDING_DATA)
        {
            /* Data container fully sent; now send the response container. */
            MTP_SendResponse(pdev, mtp_transaction_id, 0x2001 /* OK */);
        }
        else if (mtp_state == MTP_STATE_SENDING_RESP)
        {
            /* Response container sent; transaction complete.
             * Re-arm the OUT endpoint so we can receive the next command. */
            mtp_state = MTP_STATE_IDLE;
            DCD_EP_PrepareRx(pdev, MTP_OUT_EP, mtp_rx_buf, MTP_MAX_PACKET);
        }
    }
    return USBD_OK;
}

/**
  * @brief  USBD_MSC_HID_DataOut
  *         Route OUT-data-received callbacks to the correct handler.
  *
  *  epnum=2 → MSC OUT data  (forward to MSC handler)
  *  epnum=1 → MTP command container received (parse and respond)
  */
static uint8_t USBD_MSC_HID_DataOut(void *pdev, uint8_t epnum)
{
    if (epnum == (MSC_OUT_EP & ~0x80U))
    {
        return USBD_MSC_DataOut(pdev, epnum);
    }

    /* MTP Bulk OUT data received (epnum == 1) */
    if (epnum == (MTP_OUT_EP & ~0x80U))
    {
        /* Ignore if we are busy sending a previous transaction. */
        if (mtp_state != MTP_STATE_IDLE)
        {
            return USBD_OK;
        }

        /* Parse MTP command container header:
         *   Bytes [4-5] ContainerType (must be 1 = Command)
         *   Bytes [6-7] OperationCode
         *   Bytes [8-11] TransactionID                           */
        uint16_t container_type = (uint16_t)(mtp_rx_buf[4] | ((uint16_t)mtp_rx_buf[5] << 8));
        uint16_t opcode         = (uint16_t)(mtp_rx_buf[6] | ((uint16_t)mtp_rx_buf[7] << 8));
        uint32_t tid            = (uint32_t)( mtp_rx_buf[8]
                                            | ((uint32_t)mtp_rx_buf[9]  << 8)
                                            | ((uint32_t)mtp_rx_buf[10] << 16)
                                            | ((uint32_t)mtp_rx_buf[11] << 24));

        if (container_type != 0x0001)
        {
            /* Not a command container; silently re-arm and wait. */
            DCD_EP_PrepareRx(pdev, MTP_OUT_EP, mtp_rx_buf, MTP_MAX_PACKET);
            return USBD_OK;
        }

        mtp_transaction_id = tid;

        switch (opcode)
        {
        case 0x1001: /* GetDeviceInfo – respond with data + OK */
            /* Patch TransactionID into the pre-built data container. */
            mtp_dev_info_container[8]  = (uint8_t)(tid);
            mtp_dev_info_container[9]  = (uint8_t)(tid >> 8);
            mtp_dev_info_container[10] = (uint8_t)(tid >> 16);
            mtp_dev_info_container[11] = (uint8_t)(tid >> 24);
            mtp_state = MTP_STATE_SENDING_DATA;
            DCD_EP_Tx(pdev, MTP_IN_EP,
                      mtp_dev_info_container,
                      sizeof(mtp_dev_info_container));
            break;

        case 0x1002: /* OpenSession – respond with OK only */
            MTP_SendResponse(pdev, tid, 0x2001 /* OK */);
            break;

        default:
            /* Unsupported operation: respond with 0x2005 (Operation Not Supported) */
            MTP_SendResponse(pdev, tid, 0x2005);
            break;
        }
    }
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
