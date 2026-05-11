#include "tusb.h"
#include <string.h>
#include "tc6.h"
#include "diag_stats.h"
extern TC6_t *tc6;

#define CMD_READ_REG     0x01
#define CMD_WRITE_REG    0x02
#define CMD_GET_STATUS   0x03
#define CMD_SEND_FRAME   0x04
#define CMD_TOGGLE_PLCA  0x05
#define CMD_SET_PLCA_CONFIG  0x06
#define CMD_PLCA_RESET 0x08
#define CMD_GET_DIAG  0x10
#define CMD_HARD_RESET   0x11
#define REG_PLCA_CTRL1   0x0004CA02
#define REG_PLCA_CTRL0   0x0004CA01
static uint8_t tx_report[64];
static uint8_t hid_tx_buf[64];
static uint32_t pending_reg_addr;
static uint8_t pending_cmd = 0;
bool g_hid_reset_requested = false;   // picked up by main loop
/* --------------------------------------------------------- */
/* Callback to read registers */
/* --------------------------------------------------------- */

void TC6_RegReadDone(
        TC6_t *inst,
        bool success,
        uint32_t addr,
        uint32_t value,
        void *tag,
        void *globalTag)
{
    memset(hid_tx_buf,0,sizeof(hid_tx_buf));
    uint32_t new_val = 0;
    if(pending_cmd == CMD_TOGGLE_PLCA)
    {
        if(value & 0x8000)
            new_val = value & ~0x8000;   // disable PLCA
        else
            new_val = value | 0x8000;    // enable PLCA

        TC6_WriteRegister(
                tc6,
                addr,
                new_val,
                true,
                NULL,
                NULL);

        memcpy(&hid_tx_buf[1], &addr, 4);
        memcpy(&hid_tx_buf[5], &new_val, 4);



        pending_cmd = 0;
    }
    else
    {
        memcpy(&hid_tx_buf[1], &addr, 4);
        memcpy(&hid_tx_buf[5], &value, 4);

    }

    hid_tx_buf[0] = success ? 0 : 1;

    tud_hid_report(0, hid_tx_buf, sizeof(hid_tx_buf));
}

/* --------------------------------------------------------- */
/* HID Report Descriptor */
/* --------------------------------------------------------- */

/* --------------------------------------------------------- */
/* Host requests data (rarely used) */
/* --------------------------------------------------------- */

uint16_t tud_hid_get_report_cb(uint8_t instance,
                               uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t *buffer,
                               uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;

    memcpy(buffer, tx_report, reqlen);
    return reqlen;
}

/* --------------------------------------------------------- */
/* Host sends command */
/* --------------------------------------------------------- */

void tud_hid_set_report_cb(uint8_t instance,
                           uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer,
                           uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;

    memset(tx_report,0,sizeof(tx_report));

    uint8_t cmd = buffer[0];



    uint32_t addr =
        buffer[1] |
        (buffer[2]<<8) |
        (buffer[3]<<16) |
        (buffer[4]<<24);

    uint32_t value =
        buffer[5] |
        (buffer[6]<<8) |
        (buffer[7]<<16) |
        (buffer[8]<<24);

    bool send_now = true;

    switch(cmd)
    {

    case CMD_READ_REG:
    {

        pending_reg_addr = addr;

        TC6_ReadRegister(
                tc6,
                addr,
                true,
                TC6_RegReadDone,
                NULL);


    send_now = false;   // IMPORTANT
    }
    break;

    case CMD_WRITE_REG:
    {

        TC6_WriteRegister(
                tc6,
                addr,
                value,
                true,
                NULL,
                NULL);

        hid_tx_buf[0] = 0;

        //tud_hid_report(0, hid_tx_buf, sizeof(hid_tx_buf));
    }
    break;
    case CMD_HARD_RESET:
    {
        g_hid_reset_requested = true;
        tx_report[0] = 0;   // ACK immediately, reset happens in main loop
    }
    break;
    case CMD_GET_STATUS:
    {
        uint8_t txc,rxc;
        bool sync;

        TC6_GetState(tc6,&txc,&rxc,&sync);

        tx_report[0] = 0;
        tx_report[1] = sync;
        tx_report[2] = txc;
        tx_report[3] = rxc;
    }
        break;
    case CMD_GET_DIAG:
    {
        uint8_t report[64] = {0};

        extern bool g_diag_ready;   // declare

        if(g_diag_ready)
        {
            DIAG_GetSnapshot(report);
            report[0] = 0;  // success

            g_diag_ready = false;  // consume snapshot
        }
        else
        {
            report[0] = 1;  // not ready
        }

        tud_hid_report(0, report, 64);
    }
       break;

    case CMD_SEND_FRAME:

        TC6_SendRawEthernetPacket(tc6,
                                  &buffer[1],
                                  bufsize-1,
                                  0,
                                  NULL,
                                  NULL);

        tx_report[0] = 0;

        break;

     case CMD_TOGGLE_PLCA:
      {
          pending_cmd = CMD_TOGGLE_PLCA;

          TC6_ReadRegister(
                  tc6,
                  REG_PLCA_CTRL0,
                  true,
                  TC6_RegReadDone,
                  NULL);

          send_now = false;
      }
      break;

     case CMD_SET_PLCA_CONFIG:
     {
         uint8_t node_id   = buffer[1];
         uint8_t node_cnt  = buffer[2];

         uint32_t val = ((uint32_t)node_cnt << 8) | node_id;

         TC6_WriteRegister(
                 tc6,
                 REG_PLCA_CTRL1,
                 val,
                 true,
                 NULL,
                 NULL);

         tx_report[0] = 0;
     }
     break;

     case CMD_PLCA_RESET:
     {
         uint32_t val = 0x4000; // bit14 = RST

         TC6_WriteRegister(
                 tc6,
                 REG_PLCA_CTRL0,
                 val,
                 true,
                 NULL,
                 NULL);

         tx_report[0] = 0;
     }
     break;

    default:
        tx_report[0] = 1;
        break;
    }

    if(send_now)
    {
        tud_hid_report(0, tx_report, sizeof(tx_report));
    }
}
