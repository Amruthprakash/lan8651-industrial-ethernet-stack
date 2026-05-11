#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// MCU / OS Configuration
//--------------------------------------------------------------------+
#define CFG_TUSB_MCU              OPT_MCU_STM32H5
#define CFG_TUSB_OS               OPT_OS_NONE

#define CFG_TUSB_RHPORT0_MODE     (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)

#define CFG_TUSB_DEBUG            0

//--------------------------------------------------------------------+
// DEVICE CONFIGURATION
//--------------------------------------------------------------------+
#define CFG_TUD_ENABLED           1
#define CFG_TUD_MAX_SPEED         OPT_MODE_FULL_SPEED
#define CFG_TUD_ENDPOINT0_SIZE    64

//--------------------------------------------------------------------+
// CLASS CONFIGURATION
//--------------------------------------------------------------------+
#define CFG_TUD_CDC               0
#define CFG_TUD_MSC               0
#define CFG_TUD_HID               1
#define CFG_TUD_MIDI              0


// ← Switch from RNDIS/ECM to NCM
#define CFG_TUD_ECM_RNDIS         0   // was 1
#define CFG_TUD_NCM               1   // was 0

#define CFG_TUD_HID_EP_BUFSIZE    64  // fixed typo: was "64a"

//--------------------------------------------------------------------+
// NCM specific — must be larger than MTU
//--------------------------------------------------------------------+
#define CFG_TUD_NCM_IN_NTB_MAX_SIZE    2048
#define CFG_TUD_NCM_OUT_NTB_MAX_SIZE   2048

// Number of NTB blocks — 1 is fine for a bridge
#define CFG_TUD_NCM_OUT_NTB_N     1
#define CFG_TUD_NCM_IN_NTB_N      1


#define CFG_TUD_VENDOR            1    // needed for tud_vendor_control_xfer_cb
#define CFG_TUD_VENDOR_RX_BUFSIZE 64
#define CFG_TUD_VENDOR_TX_BUFSIZE 64
//--------------------------------------------------------------------+
// NETWORK BUFFERS
//--------------------------------------------------------------------+
#ifndef CFG_TUD_NET_MTU
#define CFG_TUD_NET_MTU           1514
#endif

#define CFG_TUD_NET_RX_BUFSIZE    2048
#define CFG_TUD_NET_TX_BUFSIZE    2048

#ifdef __cplusplus
}
#endif

#endif
