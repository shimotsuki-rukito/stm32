/**
  ******************************************************************************
  * @file    usbd_msc_hid_core.c
  * @brief   Composite MTP + MSC device class.
  *          Interface 0: MTP (Vendor Specific, driven by MS OS Descriptors)
  *          Interface 1: MSC (Bulk-Only Transport)
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "usbd_msc_hid_core.h"
#include "usbd_msc_core.h"
#include "usbd_desc.h"
#include "usbd_req.h"

/* MS OS Extended Properties descriptor defined in usbd_req.c */
extern const uint8_t  USBD_MS_ExtPropertiesDesc[66];
#define USBD_MS_EXT_PROPERTIES_SIZE  66U

/* ---- MTP minimal responder ------------------------------------------------
 *
 * Implements the four operations Windows WUDFWpdMtp calls during enumeration:
 *   0x1001 GetDeviceInfo  – returns Manufacturer="vivo", Model="iQOO 13"
 *   0x1002 OpenSession    – responds OK
 *   0x1003 CloseSession   – responds OK
 *   0x1004 GetStorageIDs  – returns empty array (no MTP objects)
 *
 * After Windows reads GetDeviceInfo it writes:
 *   DEVPKEY_Device_Manufacturer = "vivo"
 *   DEVPKEY_Device_FriendlyName = "iQOO 13"
 * These appear in Device Manager as Device Mfg and Description.
 * --------------------------------------------------------------------------*/

#define MTP_ST_IDLE     0U
#define MTP_ST_DATA_IN  1U  /* data container being transmitted             */
#define MTP_ST_RESP_IN  2U  /* response container being transmitted         */

#define MTP_CTYPE_CMD   1U
#define MTP_CTYPE_DATA  2U
#define MTP_CTYPE_RESP  3U

#define MTP_OP_GET_DEVICE_INFO  0x1001U
#define MTP_OP_OPEN_SESSION     0x1002U
#define MTP_OP_CLOSE_SESSION    0x1003U
#define MTP_OP_GET_STORAGE_IDS  0x1004U

#define MTP_RESP_OK                 0x2001U
#define MTP_RESP_OP_NOT_SUPPORTED   0x2005U

/*
 * DeviceInfo dataset payload (everything after the 12-byte MTP container
 * header).  Fields follow the PIMA 15740 / MTP 1.1 layout.
 */
static const uint8_t mtp_dev_info_payload[] =
{
    /* StandardVersion = 100 */
    0x64, 0x00,
    /* VendorExtensionID = 6 (Microsoft) */
    0x06, 0x00, 0x00, 0x00,
    /* VendorExtensionVersion = 100 */
    0x64, 0x00,
    /* VendorExtensionDesc: empty string (NumChars = 0) */
    0x00,
    /* FunctionalMode = 0 */
    0x00, 0x00,
    /* OperationsSupported: count=4, then each opcode (UINT16 LE) */
    0x04, 0x00,
    0x01, 0x10,  /* 0x1001 GetDeviceInfo  */
    0x02, 0x10,  /* 0x1002 OpenSession    */
    0x03, 0x10,  /* 0x1003 CloseSession   */
    0x04, 0x10,  /* 0x1004 GetStorageIDs  */
    /* EventsSupported: count=0 */
    0x00, 0x00,
    /* DevicePropertiesSupported: count=0 */
    0x00, 0x00,
    /* CaptureFormats: count=0 */
    0x00, 0x00,
    /* PlaybackFormats: count=0 */
    0x00, 0x00,
    /* Manufacturer: "vivo"  (NumChars=5 incl. NUL, UTF-16LE) */
    0x05,
    'v', 0x00, 'i', 0x00, 'v', 0x00, 'o', 0x00, 0x00, 0x00,
    /* Model: "iQOO 13"  (NumChars=8 incl. NUL, UTF-16LE) */
    0x08,
    'i', 0x00, 'Q', 0x00, 'O', 0x00, 'O', 0x00,
    ' ', 0x00, '1', 0x00, '3', 0x00, 0x00, 0x00,
    /* DeviceVersion: "6.06"  (NumChars=5 incl. NUL, UTF-16LE) */
    0x05,
    '6', 0x00, '.', 0x00, '0', 0x00, '6', 0x00, 0x00, 0x00,
    /* SerialNumber: "10AECS0322003GZ"  (NumChars=16 incl. NUL, UTF-16LE) */
    0x10,
    '1', 0x00, '0', 0x00, 'A', 0x00, 'E', 0x00,
    'C', 0x00, 'S', 0x00, '0', 0x00, '3', 0x00,
    '2', 0x00, '2', 0x00, '0', 0x00, '0', 0x00,
    '3', 0x00, 'G', 0x00, 'Z', 0x00, 0x00, 0x00,
};
#define MTP_DEV_INFO_PAYLOAD_SZ  ((uint16_t)sizeof(mtp_dev_info_payload))

/* RX buffer primed on the MTP OUT endpoint.  Must survive until DataOut CB. */
__ALIGN_BEGIN static uint8_t mtp_rx_buf[MTP_MAX_PACKET]  __ALIGN_END;
/* TX buffer used for data and response containers.                           */
__ALIGN_BEGIN static uint8_t mtp_tx_buf[256]             __ALIGN_END;

static uint8_t  mtp_state  = MTP_ST_IDLE;
static uint32_t mtp_txn_id = 0U;


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

/*---  MSC callbacks (external)  ---*/
extern uint8_t  USBD_MSC_Init    (void *pdev, uint8_t cfgidx);
extern uint8_t  USBD_MSC_DeInit  (void *pdev, uint8_t cfgidx);
extern uint8_t  USBD_MSC_Setup   (void *pdev, USB_SETUP_REQ *req);
extern uint8_t  USBD_MSC_DataIn  (void *pdev, uint8_t epnum);
extern uint8_t  USBD_MSC_DataOut (void *pdev, uint8_t epnum);

/** @defgroup USBD_MSC_MTP_Private_Functions
  * @{
  */

/* ---- MTP helper: write a 12-byte container header into buf[0..11] --------- */
static void mtp_write_header(uint8_t *buf,
                              uint32_t total_len,
                              uint16_t ctype,
                              uint16_t code,
                              uint32_t txn)
{
    buf[0]  = (uint8_t)(total_len);
    buf[1]  = (uint8_t)(total_len >>  8);
    buf[2]  = (uint8_t)(total_len >> 16);
    buf[3]  = (uint8_t)(total_len >> 24);
    buf[4]  = (uint8_t)(ctype);
    buf[5]  = (uint8_t)(ctype >> 8);
    buf[6]  = (uint8_t)(code);
    buf[7]  = (uint8_t)(code >> 8);
    buf[8]  = (uint8_t)(txn);
    buf[9]  = (uint8_t)(txn >>  8);
    buf[10] = (uint8_t)(txn >> 16);
    buf[11] = (uint8_t)(txn >> 24);
}

/* ---- MTP: build and transmit a response-only container ------------------- */
static void mtp_send_response(void *pdev, uint16_t resp_code)
{
    mtp_write_header(mtp_tx_buf, 12U, MTP_CTYPE_RESP, resp_code, mtp_txn_id);
    mtp_state = MTP_ST_RESP_IN;
    DCD_EP_Tx(pdev, MTP_IN_EP, mtp_tx_buf, 12U);
}

/* ---- MTP: called from DataOut when a packet arrives on MTP_OUT_EP -------- */
static void mtp_handle_out(void *pdev)
{
    uint16_t ctype;
    uint16_t opcode;

    /* Decode container header */
    ctype  = (uint16_t)mtp_rx_buf[4] | ((uint16_t)mtp_rx_buf[5] << 8);
    opcode = (uint16_t)mtp_rx_buf[6] | ((uint16_t)mtp_rx_buf[7] << 8);
    mtp_txn_id = (uint32_t)mtp_rx_buf[8]
               | ((uint32_t)mtp_rx_buf[9]  <<  8)
               | ((uint32_t)mtp_rx_buf[10] << 16)
               | ((uint32_t)mtp_rx_buf[11] << 24);

    if (ctype != MTP_CTYPE_CMD)
    {
        /* Not a command container – ignore and re-arm */
        DCD_EP_PrepareRx(pdev, MTP_OUT_EP, mtp_rx_buf, MTP_MAX_PACKET);
        return;
    }

    switch (opcode)
    {
    case MTP_OP_GET_DEVICE_INFO:
    {
        /* Data phase: DeviceInfo container */
        uint32_t data_len = 12U + MTP_DEV_INFO_PAYLOAD_SZ;
        mtp_write_header(mtp_tx_buf, data_len,
                         MTP_CTYPE_DATA, MTP_OP_GET_DEVICE_INFO, mtp_txn_id);
        memcpy(&mtp_tx_buf[12], mtp_dev_info_payload, MTP_DEV_INFO_PAYLOAD_SZ);
        mtp_state = MTP_ST_DATA_IN;
        DCD_EP_Tx(pdev, MTP_IN_EP, mtp_tx_buf, (uint32_t)data_len);
        break;
    }

    case MTP_OP_OPEN_SESSION:
    case MTP_OP_CLOSE_SESSION:
        /* No data phase – respond immediately */
        mtp_send_response(pdev, MTP_RESP_OK);
        break;

    case MTP_OP_GET_STORAGE_IDS:
    {
        /* Data phase: empty storage-ID array (count = 0, UINT32) */
        uint32_t data_len = 12U + 4U;
        mtp_write_header(mtp_tx_buf, data_len,
                         MTP_CTYPE_DATA, MTP_OP_GET_STORAGE_IDS, mtp_txn_id);
        mtp_tx_buf[12] = 0U; mtp_tx_buf[13] = 0U;
        mtp_tx_buf[14] = 0U; mtp_tx_buf[15] = 0U;
        mtp_state = MTP_ST_DATA_IN;
        DCD_EP_Tx(pdev, MTP_IN_EP, mtp_tx_buf, data_len);
        break;
    }

    default:
        mtp_send_response(pdev, MTP_RESP_OP_NOT_SUPPORTED);
        break;
    }
}

/* ---- MTP: called from DataIn when MTP IN endpoint finishes a transfer ---- */
static void mtp_handle_in(void *pdev)
{
    if (mtp_state == MTP_ST_DATA_IN)
    {
        /* Data container sent; follow up with OK response */
        mtp_send_response(pdev, MTP_RESP_OK);
    }
    else if (mtp_state == MTP_ST_RESP_IN)
    {
        /* Transaction complete; re-arm OUT endpoint for next command */
        mtp_state = MTP_ST_IDLE;
        DCD_EP_PrepareRx(pdev, MTP_OUT_EP, mtp_rx_buf, MTP_MAX_PACKET);
    }
}

/**
  * @brief  USBD_MSC_HID_Init
  *         Open all endpoints and initialise the MSC class.
  */
static uint8_t USBD_MSC_HID_Init(void *pdev, uint8_t cfgidx)
{
    /* Open MTP bulk IN */
    DCD_EP_Open(pdev, MTP_IN_EP,  MTP_MAX_PACKET,       EP_TYPE_BULK);
    /* Open MTP bulk OUT */
    DCD_EP_Open(pdev, MTP_OUT_EP, MTP_MAX_PACKET,       EP_TYPE_BULK);
    /* Open MTP interrupt IN (event channel) */
    DCD_EP_Open(pdev, MTP_CMD_EP, MTP_CMD_PACKET_SIZE,  EP_TYPE_INTR);

    /* Prime MTP OUT endpoint for first command */
    mtp_state = MTP_ST_IDLE;
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

    USBD_MSC_DeInit(pdev, cfgidx);

    return USBD_OK;
}

/**
  * @brief  USBD_MSC_HID_Setup
  *         Route class-specific setup requests to the correct handler.
  */
static uint8_t USBD_MSC_HID_Setup(void *pdev, USB_SETUP_REQ *req)
{
    /* MS OS Extended Properties (interface-level vendor request).
     * Windows sends: bmRequestType=0xC1, bRequest=vendor_code(0x01),
     *                wValue=wInterfaceNumber, wIndex=0x0005.
     * Respond with the FriendlyName property so Windows stores
     * "iQOO 13" as the device's friendly name in the registry. */
    if (((req->bmRequest & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_VENDOR) &&
        (req->bRequest == 0x01) &&
        (req->wIndex   == 0x0005) &&
        (req->wValue   == MTP_INTERFACE))
    {
        uint16_t len = (req->wLength < USBD_MS_EXT_PROPERTIES_SIZE)
                       ? req->wLength : USBD_MS_EXT_PROPERTIES_SIZE;
        USBD_CtlSendData(pdev, (uint8_t *)USBD_MS_ExtPropertiesDesc, len);
        return USBD_OK;
    }

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
    if (epnum == (MTP_IN_EP & ~0x80U))
    {
        mtp_handle_in(pdev);
    }
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
    if (epnum == (MTP_OUT_EP & ~0x80U))
    {
        mtp_handle_out(pdev);
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
