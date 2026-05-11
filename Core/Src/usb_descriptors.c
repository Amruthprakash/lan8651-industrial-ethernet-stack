#include "tusb.h"
#include "class/net/net_device.h"
#include <string.h>

/* ----------------------------------------------------------------
   OS Detection
---------------------------------------------------------------- */
typedef enum {
    OS_UNKNOWN     = 0,
    OS_WINDOWS     = 1,
    OS_MACOS_LINUX = 2
} host_os_t;

volatile host_os_t g_detected_os = OS_UNKNOWN;

//--------------------------------------------------------------------
// Endpoints
// Single config — one set of endpoints shared by NCM + HID
//--------------------------------------------------------------------
#define EPNUM_NET_NOTIF    0x81    // IN  EP1  interrupt  (NCM link status)
#define EPNUM_NET_OUT      0x02    // OUT EP2  bulk       (PC → MCU)
#define EPNUM_NET_IN       0x82    // IN  EP2  bulk       (MCU → PC)
#define EPNUM_HID          0x83    // IN  EP3  interrupt  (HID reports)

//--------------------------------------------------------------------
// String Indices
//--------------------------------------------------------------------
enum {
    STRID_LANGID       = 0,
    STRID_MANUFACTURER = 1,
    STRID_PRODUCT      = 2,
    STRID_SERIAL       = 3,
    STRID_NET_ITF      = 4,
    STRID_MAC          = 5,
    STRID_HID_ITF      = 6,
    STRID_COUNT        = 7
};

//--------------------------------------------------------------------
// Device Descriptor
// bcdUSB = 0x0201 — required for NCM
// bNumConfigurations = 1 — single config works on Windows/macOS/Linux
//--------------------------------------------------------------------
static tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0201,              // NCM requires 2.01

    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0xCAFE,
    .idProduct          = 0x4011,              // new PID — different from RNDIS
    .bcdDevice          = 0x0101,

    .iManufacturer      = STRID_MANUFACTURER,
    .iProduct           = STRID_PRODUCT,
    .iSerialNumber      = STRID_SERIAL,

    .bNumConfigurations = 0x01                 // one config for all OSes
};

const uint8_t * tud_descriptor_device_cb(void)
{
    return (const uint8_t *) &desc_device;
}

//--------------------------------------------------------------------
// HID Report Descriptor
//--------------------------------------------------------------------
static uint8_t const desc_hid_report[] =
{
    TUD_HID_REPORT_DESC_GENERIC_INOUT(64)
};

uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void) instance;
    return desc_hid_report;
}

//--------------------------------------------------------------------
// Configuration Descriptor
// Single config: NCM + HID
// Interface layout:
//   0 = NCM control
//   1 = NCM data
//   2 = HID
//
// Windows 10+  → loads UsbNcm driver automatically (Win11) or
//                one-time manual install (Win10)
// macOS        → loads built-in NCM driver automatically
// Linux        → loads cdc_ncm module automatically
//--------------------------------------------------------------------
#define CONFIG_TOTAL_LEN   (TUD_CONFIG_DESC_LEN    \
                          + TUD_CDC_NCM_DESC_LEN    \
                          + TUD_HID_DESC_LEN)

static uint8_t const desc_configuration[] =
{
    // Config header
    TUD_CONFIG_DESCRIPTOR(
        1,                  // bConfigurationValue
        3,                  // bNumInterfaces: NCM ctrl + NCM data + HID
        0,                  // iConfiguration
        CONFIG_TOTAL_LEN,
        0x00,               // bmAttributes
        100),               // bMaxPower 200mA

    // NCM — interfaces 0 (control) + 1 (data)
    TUD_CDC_NCM_DESCRIPTOR(
        0,                  // bFirstInterface = NCM control
        STRID_NET_ITF,      // iInterface
        STRID_MAC,          // iMACAddress — built dynamically
        EPNUM_NET_NOTIF, 64,// notification EP, size 64
        EPNUM_NET_OUT,      // data OUT
        EPNUM_NET_IN,       // data IN
        CFG_TUD_NET_ENDPOINT_SIZE,
        CFG_TUD_NET_MTU),

    // HID — interface 2
    TUD_HID_DESCRIPTOR(
        2,                  // bInterfaceNumber
        STRID_HID_ITF,      // iInterface
        HID_ITF_PROTOCOL_NONE,
        sizeof(desc_hid_report),
        EPNUM_HID,
        CFG_TUD_HID_EP_BUFSIZE,
        10)                 // polling interval ms
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
    (void) index;
    return desc_configuration;
}

//--------------------------------------------------------------------
// BOS Descriptor
// Required for NCM — Windows reads this to find MS OS 2.0 descriptor
// which tells it to load the NCM driver automatically
//--------------------------------------------------------------------

#define BOS_TOTAL_LEN      (TUD_BOS_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN)
#define MS_OS_20_DESC_LEN  0xB2

static uint8_t const desc_bos[] =
{
    TUD_BOS_DESCRIPTOR(BOS_TOTAL_LEN, 1),
    TUD_BOS_MS_OS_20_DESCRIPTOR(MS_OS_20_DESC_LEN, 1)
};

const uint8_t * tud_descriptor_bos_cb(void)
{
    return desc_bos;
}

// MS OS 2.0 descriptor — tells Windows to load UsbNcm driver
// and associates device with a DeviceInterfaceGUID
static uint8_t const desc_ms_os_20[] =
{
    // Set header
    U16_TO_U8S_LE(0x000A),
    U16_TO_U8S_LE(MS_OS_20_SET_HEADER_DESCRIPTOR),
    U32_TO_U8S_LE(0x06030000),        // Windows 8.1+
    U16_TO_U8S_LE(MS_OS_20_DESC_LEN),

    // Configuration subset header
    U16_TO_U8S_LE(0x0008),
    U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_CONFIGURATION),
    0, 0,
    U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A),

    // Function subset header — applies to interface 0 (NCM control)
    U16_TO_U8S_LE(0x0008),
    U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_FUNCTION),
    0,                                // bFirstInterface = 0
    0,
    U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A - 0x08),

    // Compatible ID — tells Windows this is a WINNCM device
    U16_TO_U8S_LE(0x0014),
    U16_TO_U8S_LE(MS_OS_20_FEATURE_COMPATBLE_ID),
    'W','I','N','N','C','M', 0x00, 0x00,   // compatibleID
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // subCompatibleID

    // Registry property — DeviceInterfaceGUID for driver matching
    U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A - 0x08 - 0x08 - 0x14),
    U16_TO_U8S_LE(MS_OS_20_FEATURE_REG_PROPERTY),
    U16_TO_U8S_LE(0x0007),
    U16_TO_U8S_LE(0x002A),
    'D',0x00,'e',0x00,'v',0x00,'i',0x00,'c',0x00,'e',0x00,
    'I',0x00,'n',0x00,'t',0x00,'e',0x00,'r',0x00,'f',0x00,
    'a',0x00,'c',0x00,'e',0x00,'G',0x00,'U',0x00,'I',0x00,
    'D',0x00,'s',0x00, 0x00, 0x00,
    U16_TO_U8S_LE(0x0050),
    '{',0x00,'1',0x00,'2',0x00,'3',0x00,'4',0x00,'5',0x00,
    '6',0x00,'7',0x00,'8',0x00,'-',0x00,'0',0x00,'D',0x00,
    '0',0x00,'8',0x00,'-',0x00,'4',0x00,'3',0x00,'F',0x00,
    'D',0x00,'-',0x00,'8',0x00,'B',0x00,'3',0x00,'E',0x00,
    '-',0x00,'1',0x00,'2',0x00,'7',0x00,'C',0x00,'A',0x00,
    '8',0x00,'A',0x00,'F',0x00,'F',0x00,'F',0x00,'9',0x00,
    'D',0x00,'}',0x00, 0x00, 0x00, 0x00, 0x00
};

// Vendor control request handler — serves MS OS 2.0 descriptor to Windows
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                 tusb_control_request_t const *request)
{
    if (stage != CONTROL_STAGE_SETUP) return true;

    if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_VENDOR &&
        request->bRequest == 1 &&
        request->wIndex == 7)
    {
        uint16_t total_len;
        memcpy(&total_len, desc_ms_os_20 + 8, 2);
        return tud_control_xfer(rhport, request,
                                (void *)(uintptr_t) desc_ms_os_20,
                                total_len);
    }

    return false;
}

//--------------------------------------------------------------------
// String Descriptors
//--------------------------------------------------------------------
static const char * string_desc_arr[STRID_COUNT] =
{
    [STRID_LANGID]       = (const char[]) { 0x09, 0x04 },
    [STRID_MANUFACTURER] = "STM32",
    [STRID_PRODUCT]      = "USB-10BASE-T1S Bridge (NCM)",
    [STRID_SERIAL]       = "10BASE001",
    [STRID_NET_ITF]      = "NCM Network Interface",
    [STRID_MAC]          = NULL,   // built dynamically
    [STRID_HID_ITF]      = "HID Control Interface",
};

static uint16_t _desc_str[33];

uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void) langid;
    unsigned int chr_count = 0;

    // Windows probes 0xEE for MS OS 1.0 — use for detection
    if (index == 0xEE)
    {
        g_detected_os = OS_WINDOWS;
        return NULL;
    }

    switch (index)
    {
    case STRID_LANGID:
        memcpy(&_desc_str[1], string_desc_arr[STRID_LANGID], 2);
        chr_count = 1;
        break;

    case STRID_MAC:
        // Build MAC string from tud_network_mac_address[]
        // {0x02,0x12,0x34,0x56,0x78,0x9B} → "02123456789B"
        for (unsigned i = 0; i < sizeof(tud_network_mac_address); i++)
        {
            _desc_str[1 + chr_count++] =
                "0123456789ABCDEF"[(tud_network_mac_address[i] >> 4) & 0xF];
            _desc_str[1 + chr_count++] =
                "0123456789ABCDEF"[(tud_network_mac_address[i]     ) & 0xF];
        }
        break;

    default:
        if (index >= STRID_COUNT || string_desc_arr[index] == NULL)
            return NULL;

        const char *str = string_desc_arr[index];
        chr_count = strlen(str);
        if (chr_count > 32) chr_count = 32;

        for (size_t i = 0; i < chr_count; i++)
            _desc_str[1 + i] = (uint16_t) str[i];
        break;
    }

    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return _desc_str;
}
