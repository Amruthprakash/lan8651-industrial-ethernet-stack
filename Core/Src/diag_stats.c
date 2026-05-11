/* =============================================================================
 * diag_stats.c  -  LAN8650/1 Diagnostic & Statistics Module
 *
 * Register address encoding: addr32 = (MMS << 16) | reg16
 *
 * =============================================================================
 * REGISTERS IN USE AND THEIR PURPOSE
 * =============================================================================
 *
 * MMS1 - MAC Registers
 * --------------------
 * STATS0  (0x0208)  RX error breakdown per frame:
 *                     RXSE  [31:24] - Symbol errors (RXER asserted during RX)
 *                     LFER  [23:16] - Length field mismatch errors
 *                     OFRX  [15:8]  - Oversize frames received (>1518 bytes, no CRC err)
 *                     UFRX  [7:0]   - Undersize frames received (<64 bytes, no CRC err)
 *
 * STATS1  (0x0209)  RX buffer/resource errors:
 *                     RXBOVR [23:16] - Frames dropped due to RX buffer overrun
 *
 * STATS2  (0x020A)  FCS integrity:
 *                     FCSE  [7:0]   - Frame Check Sequence errors (bad CRC, valid length)
 *
 * STATS3  (0x020B)  Type ID match counters for protocol classification:
 *                     TID1MCNT [7:0]   - IPv4 frames matched (MAC_TIDM1 = 0x0800)
 *                     TID2MCNT [15:8]  - ARP  frames matched (MAC_TIDM2 = 0x0806)
 *                     MAC_TIDM1 (MMS1 0x02A): write 0x0800 to match IPv4 EtherType.
 *                     MAC_TIDM2 (MMS1 0x02B): write 0x0806 to match ARP  EtherType.
 *                     Both registers written in DIAG_Init() so the MAC hardware
 *                     increments the counters automatically on every matching frame.
 *
 * STATS5  (0x020D)  Broadcast traffic:
 *                     BFRX  [15:8]  - Broadcast frames received without error
 *
 * STATS6  (0x020E)  TFRX [31:0]   - Total frames RX including errored frames
 *
 * STATS7  (0x020F)  FRX  [31:0]   - Frames RX without error (filtered + copied)
 *
 * STATS11 (0x0213)  TFTX [31:0]   - Total frames TX including errored frames
 *
 * STATS12 (0x0214)  FTX  [31:0]   - Frames TX without error (no underrun/retry)
 *
 * All STATS fields are RC (Read-to-Clear), 8-bit unless noted, saturate at max.
 *
 * MMS2 - PHY PCS Registers
 * ------------------------
 * T1SPCSDIAG2 (0x08F6)  CORTXCNT [15:0] - Corrupted TX count: number of locally
 *                         initiated transmissions that resulted in a corrupted MDI
 *                         signal. Each count = one physical layer collision event.
 *                         RC, 16-bit, saturates at 0xFFFF.
 *                         A cumulative lifetime total is maintained in software
 *                         (g_total_cortxcnt) since this register clears on read.
 *
 * MMS4 - PHY Vendor Specific Registers
 * -------------------------------------
 * STS1     (0x0018)  Sticky PHY event flags (RC - cleared on read):
 *                     SQI     [12] - Signal Quality Indication status changed
 *                     PSTC    [11] - PLCA Status (PST) changed
 *                     TXCOL   [10] - Physical collision detected during TX
 *                     TXJAB   [9]  - TX jabber: PCS held TX >2ms, disabled 16ms
 *                     EMPCYC  [7]  - Empty PLCA cycle (no node transmitted)
 *                     RXINTO  [6]  - Frame received during own TX opportunity
 *                     BCNBFTO [4]  - BEACON received before TX opportunity expired
 *                     PLCASYM [2]  - PLCA symbols detected on the medium
 *
 * STS2     (0x0019)  Supply monitoring:
 *                     UV33 [8] - 3.3V VDDA/VDDAU under-voltage detected
 *
 * CDCTL0   (0x0087)  CDEN [15] - Collision Detect Enable.
 *                     Set 0 when PLCA active (recommended), 1 for CSMA/CD mode.
 *
 * SQICTL   (0x00A0)  Signal Quality Indicator control:
 *                     SQIRST [15] - Reset SQI block (self-clearing)
 *                     SQIEN  [14] - Enable SQI measurement
 *
 * SQISTS0  (0x00A1)  SQI measurement result:
 *                     SQIERR  [7]   - Error during SQI accumulation
 *                     SQIVLD  [6]   - SQI measurement valid and ready to read
 *                     SQIVAL  [5:3] - SQI value 0(worst)..7(best), SNR bands
 *                     SQIERRC [2:0] - Error code when SQIERR=1
 *
 * PLCA_CTRL0 (0xCA01) EN  [15] - PLCA Enable. Must be 0 on all nodes for CSMA/CD.
 *                      RST [14] - PLCA Reset (self-clearing)
 *
 * PLCA_CTRL1 (0xCA02) NCNT [15:8] - Node count (coordinator only)
 *                      ID   [7:0]  - This node's PLCA local ID
 *
 * PLCA_STS   (0xCA03) PST  [15] - PLCA active: node is receiving periodic BEACONs
 *
 * =============================================================================
 */

#include "diag_stats.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
   Register addresses: addr32 = (MMS << 16) | reg16
   --------------------------------------------------------------------------- */

/* MMS1 - MAC */
#define ADDR_STATS0         0x00010208u
#define ADDR_STATS1         0x00010209u
#define ADDR_STATS2         0x0001020Au
#define ADDR_STATS3         0x0001020Bu
#define ADDR_STATS5         0x0001020Du
#define ADDR_STATS6         0x0001020Eu
#define ADDR_STATS7         0x0001020Fu
#define ADDR_STATS11        0x00010213u
#define ADDR_STATS12        0x00010214u

/* MMS2 - PHY PCS */
#define ADDR_T1SPCSDIAG2    0x000208F6u

/* MMS4 - PHY Vendor Specific */
#define ADDR_STS1           0x00040018u
#define ADDR_STS2           0x00040019u
#define ADDR_CTRCTRL        0x00040020u
#define ADDR_CDCTL0         0x00040087u
#define ADDR_SQICTL         0x000400A0u
#define ADDR_SQISTS0        0x000400A1u
#define ADDR_PLCA_CTRL0     0x0004CA01u
#define ADDR_PLCA_CTRL1     0x0004CA02u
#define ADDR_PLCA_STS       0x0004CA03u

/* MAC Type ID match registers (MMS1) - write EtherType values to enable STATS3 counting */
#define ADDR_MAC_TIDM1      0x0001002Au   /* MAC_TIDM1: write 0x0800 -> counts IPv4 in TID1MCNT */
#define ADDR_MAC_TIDM2      0x0001002Bu   /* MAC_TIDM2: write 0x0806 -> counts ARP  in TID2MCNT */

/* ---------------------------------------------------------------------------
   Bit-field helpers
   --------------------------------------------------------------------------- */
#define CTRCTRL_TOCTRE      (1u << 1)
#define CTRCTRL_BCNCTRE     (1u << 0)
#define CDCTL0_CDEN         (1u << 15)
#define PLCA_CTRL0_EN       (1u << 15)
#define PLCA_STS_PST        (1u << 15)
#define STS1_SQI            (1u << 12)
#define STS1_PSTC           (1u << 11)
#define STS1_TXCOL          (1u << 10)
#define STS1_TXJAB          (1u <<  9)
#define STS1_EMPCYC         (1u <<  7)
#define STS1_RXINTO         (1u <<  6)
#define STS1_BCNBFTO        (1u <<  4)
#define STS1_PLCASYM        (1u <<  2)
#define STS2_UV33           (1u <<  8)

/* ---------------------------------------------------------------------------
   Timing
   --------------------------------------------------------------------------- */
#ifndef DIAG_INTERVAL_MS
#define DIAG_INTERVAL_MS    500u
#endif

/* ---------------------------------------------------------------------------
   State machine
   --------------------------------------------------------------------------- */
typedef enum {
    DIAG_IDLE = 0,
    DIAG_WAIT_STATS0,
    DIAG_WAIT_STATS1,
    DIAG_WAIT_STATS2,
    DIAG_WAIT_STATS3,
    DIAG_WAIT_STATS5,
    DIAG_WAIT_STATS6,
    DIAG_WAIT_STATS7,
    DIAG_WAIT_STATS11,
    DIAG_WAIT_STATS12,
    DIAG_WAIT_DIAG2,
    DIAG_WAIT_STS1,
    DIAG_WAIT_STS2,
    DIAG_WAIT_SQISTS0,
    DIAG_WAIT_PLCA_CTRL0,
    DIAG_WAIT_PLCA_CTRL1,
    DIAG_WAIT_PLCA_STS,
    DIAG_PRINT,
} DiagState_t;

/* ---------------------------------------------------------------------------
   Collected data for one diagnostic cycle
   --------------------------------------------------------------------------- */
typedef struct {
    /* STATS0 */
    uint8_t  rxse;
    uint8_t  lfer;
    uint8_t  ofrx;
    uint8_t  ufrx;
    /* STATS1 */
    uint8_t  rxbovr;
    /* STATS2 */
    uint8_t  fcse;
    /* STATS3 - Type ID match counts */
    uint8_t  ipv4_cnt;     /* TID1MCNT - IPv4 (0x0800) */
    uint8_t  arp_cnt;      /* TID2MCNT - ARP  (0x0806) */
    /* STATS5 */
    uint8_t  bfrx;
    /* STATS6/7/11/12 - 32-bit */
    uint32_t tfrx;
    uint32_t frx;
    uint32_t tftx;
    uint32_t ftx;
    /* T1SPCSDIAG2 */
    uint16_t cortxcnt;
    /* STS1 flags */
    bool     sts1_sqi;
    bool     sts1_pstc;
    bool     sts1_txcol;
    bool     sts1_txjab;
    bool     sts1_empcyc;
    bool     sts1_rxinto;
    bool     sts1_bcnbfto;
    bool     sts1_plcasym;
    /* STS2 flags */
    bool     sts2_uv33;
    /* SQI */
    uint8_t  sqi_val;
    bool     sqi_valid;
    bool     sqi_err;
    /* PLCA */
    bool     plca_en;
    uint8_t  plca_id;
    uint8_t  plca_ncnt;
    bool     plca_pst;
} DiagData_t;

/* ---------------------------------------------------------------------------
   Module globals
   --------------------------------------------------------------------------- */
static TC6_t       *g_tc6               = NULL;
static DiagState_t  g_state             = DIAG_IDLE;
static DiagData_t   g_data              = {0};
static uint32_t     g_last_tick         = 0;
bool g_diag_reinit_needed = false;   // add near other globals
/* Software counters - cumulative, never cleared */
static uint32_t     g_sw_rx             = 0;
static uint32_t     g_sw_tx_ok          = 0;
static uint32_t     g_sw_tx_fail        = 0;
static uint32_t     g_total_cortxcnt    = 0;   /* lifetime collision total */
bool g_diag_ready = false;
/* CDCTL0 read-modify-write state */
static bool         g_cdctl_rmw_pending = false;
static bool         g_cdctl_rmw_enable  = false;

extern void UART4_Send(char *msg);

/* ---------------------------------------------------------------------------
   Forward declarations
   --------------------------------------------------------------------------- */
static void DIAG_FireNextRead(void);
static void DIAG_Callback(TC6_t *inst, bool success, uint32_t addr,
                           uint32_t value, void *tag, void *globalTag);
static void DIAG_CDCTL_ReadCB(TC6_t *inst, bool success, uint32_t addr,
                                uint32_t value, void *tag, void *globalTag);
static void DIAG_Print(void);

/* =============================================================================
   PUBLIC API
   ============================================================================= */

void DIAG_Init(TC6_t *tc6)
{
    g_tc6      = tc6;
    g_state    = DIAG_IDLE;
    g_last_tick = 0;
    memset(&g_data, 0, sizeof(g_data));

    /*
     * Configure MAC Type ID match registers to enable protocol frame counting
     * in STATS3.  Write exactly the EtherType values as specified:
     *
     *   MAC_TIDM1 (MMS1 0x02A) = 0x0800  (IPv4)
     *     -> hardware increments TID1MCNT (STATS3 bits [7:0]) on every IPv4 frame.
     *
     *   MAC_TIDM2 (MMS1 0x02B) = 0x0806  (ARP)
     *     -> hardware increments TID2MCNT (STATS3 bits [15:8]) on every ARP frame.
     *
     * Both counters are 8-bit RC fields that saturate at 0xFF and clear on read.
     *
     * NOTE: Call DIAG_Init() after the 200ms TC6Regs_Init flush in main.c so
     * these writes are not overwritten by TC6Regs_Init's own MAC setup.
     */
    TC6_WriteRegister(tc6, ADDR_MAC_TIDM1, 0x88b5u, true, NULL, NULL);
    TC6_WriteRegister(tc6, ADDR_MAC_TIDM2, 0x0806u, true, NULL, NULL);
}

void DIAG_EnableCollisionDetect(bool enable)
{
    if (g_tc6 == NULL) return;
    g_cdctl_rmw_pending = true;
    g_cdctl_rmw_enable  = enable;
    TC6_ReadRegister(g_tc6, ADDR_CDCTL0, true, DIAG_CDCTL_ReadCB, NULL);
}

void DIAG_EnablePLCACounters(void)
{
    if (g_tc6 == NULL) return;
    TC6_WriteRegister(g_tc6, ADDR_CTRCTRL,
                      CTRCTRL_TOCTRE | CTRCTRL_BCNCTRE, true, NULL, NULL);
}

void DIAG_SetPLCAEnable(bool enable)
{
    if (g_tc6 == NULL) return;
    TC6_WriteRegister(g_tc6, ADDR_PLCA_CTRL0,
                      enable ? PLCA_CTRL0_EN : 0u, true, NULL, NULL);
}

void DIAG_OnRx(void)          { g_sw_rx++; }
void DIAG_OnTx(bool success)  { if (success) g_sw_tx_ok++; else g_sw_tx_fail++; }

void DIAG_Task(void)
{
    if (g_tc6 == NULL) return;

    switch (g_state)
    {
        case DIAG_IDLE:
            if ((HAL_GetTick() - g_last_tick) >= DIAG_INTERVAL_MS)
            {
                g_last_tick = HAL_GetTick();
                memset(&g_data, 0, sizeof(g_data));
                g_state = DIAG_WAIT_STATS0;
                DIAG_FireNextRead();
            }
            break;

        case DIAG_PRINT:
            DIAG_Print();
            g_state = DIAG_IDLE;
            break;

        default:
            break;
    }
}

/* =============================================================================
   INTERNAL - fire next read
   ============================================================================= */
static void DIAG_FireNextRead(void)
{
    uint32_t addr = 0;

    switch (g_state)
    {
        case DIAG_WAIT_STATS0:     addr = ADDR_STATS0;      break;
        case DIAG_WAIT_STATS1:     addr = ADDR_STATS1;      break;
        case DIAG_WAIT_STATS2:     addr = ADDR_STATS2;      break;
        case DIAG_WAIT_STATS3:     addr = ADDR_STATS3;      break;
        case DIAG_WAIT_STATS5:     addr = ADDR_STATS5;      break;
        case DIAG_WAIT_STATS6:     addr = ADDR_STATS6;      break;
        case DIAG_WAIT_STATS7:     addr = ADDR_STATS7;      break;
        case DIAG_WAIT_STATS11:    addr = ADDR_STATS11;     break;
        case DIAG_WAIT_STATS12:    addr = ADDR_STATS12;     break;
        case DIAG_WAIT_DIAG2:      addr = ADDR_T1SPCSDIAG2; break;
        case DIAG_WAIT_STS1:       addr = ADDR_STS1;        break;
        case DIAG_WAIT_STS2:       addr = ADDR_STS2;        break;
        case DIAG_WAIT_SQISTS0:    addr = ADDR_SQISTS0;     break;
        case DIAG_WAIT_PLCA_CTRL0: addr = ADDR_PLCA_CTRL0;  break;
        case DIAG_WAIT_PLCA_CTRL1: addr = ADDR_PLCA_CTRL1;  break;
        case DIAG_WAIT_PLCA_STS:   addr = ADDR_PLCA_STS;    break;
        default:
            g_state = DIAG_IDLE;
            return;
    }

    TC6_ReadRegister(g_tc6, addr, true, DIAG_Callback, NULL);
}

/* =============================================================================
   INTERNAL - decode callback
   ============================================================================= */
static void DIAG_Callback(TC6_t *inst, bool success, uint32_t addr,
                            uint32_t value, void *tag, void *globalTag)
{
    (void)inst; (void)tag; (void)globalTag;
    /* On failure value=0, safe defaults for all fields */
    (void)success;

    switch (addr)
    {
        case ADDR_STATS0:
            g_data.rxse  = (uint8_t)((value >> 24) & 0xFF);
            g_data.lfer  = (uint8_t)((value >> 16) & 0xFF);
            g_data.ofrx  = (uint8_t)((value >>  8) & 0xFF);
            g_data.ufrx  = (uint8_t)( value        & 0xFF);
            g_state = DIAG_WAIT_STATS1;
            break;

        case ADDR_STATS1:
            /* Only RXBOVR [23:16] requested from this register */
            g_data.rxbovr = (uint8_t)((value >> 16) & 0xFF);
            g_state = DIAG_WAIT_STATS2;
            break;

        case ADDR_STATS2:
            g_data.fcse = (uint8_t)(value & 0xFF);
            g_state = DIAG_WAIT_STATS3;
            break;

        case ADDR_STATS3:
            /*
             * TID2MCNT [15:8]  -> ARP  (TIDM2 = 0x0806)
             * TID1MCNT  [7:0]  -> IPv4 (TIDM1 = 0x0800)
             */
            g_data.arp_cnt  = (uint8_t)((value >>  8) & 0xFF);
            g_data.ipv4_cnt = (uint8_t)( value        & 0xFF);
            g_state = DIAG_WAIT_STATS5;
            break;
        case ADDR_PLCA_CTRL1:
            g_data.plca_ncnt = (uint8_t)((value >> 8) & 0xFF);
            g_data.plca_id   = (uint8_t)( value       & 0xFF);

            /* 0xFFFF means PHY lost register state — trigger re-init */
            if ((value & 0xFFFF) == 0xFFFF)
            {
                g_diag_reinit_needed = true;
            }

            g_state = DIAG_WAIT_PLCA_STS;
            break;

        case ADDR_STATS5:
            /* Only BFRX [15:8] requested */
            g_data.bfrx = (uint8_t)((value >> 8) & 0xFF);
            g_state = DIAG_WAIT_STATS6;
            break;

        case ADDR_STATS6:
            g_data.tfrx = value;
            g_state = DIAG_WAIT_STATS7;
            break;

        case ADDR_STATS7:
            g_data.frx = value;
            g_state = DIAG_WAIT_STATS11;
            break;

        case ADDR_STATS11:
            g_data.tftx = value;
            g_state = DIAG_WAIT_STATS12;
            break;

        case ADDR_STATS12:
            g_data.ftx = value;
            g_state = DIAG_WAIT_DIAG2;
            break;

        case ADDR_T1SPCSDIAG2:
            g_data.cortxcnt = (uint16_t)(value & 0xFFFF);
            /* Accumulate into lifetime total - this register clears on read */
            g_total_cortxcnt += g_data.cortxcnt;
            g_state = DIAG_WAIT_STS1;
            break;

        case ADDR_STS1:
            g_data.sts1_sqi     = (value & STS1_SQI)     ? true : false;
            g_data.sts1_pstc    = (value & STS1_PSTC)    ? true : false;
            g_data.sts1_txcol   = (value & STS1_TXCOL)   ? true : false;
            g_data.sts1_txjab   = (value & STS1_TXJAB)   ? true : false;
            g_data.sts1_empcyc  = (value & STS1_EMPCYC)  ? true : false;
            g_data.sts1_rxinto  = (value & STS1_RXINTO)  ? true : false;
            g_data.sts1_bcnbfto = (value & STS1_BCNBFTO) ? true : false;
            g_data.sts1_plcasym = (value & STS1_PLCASYM) ? true : false;
            g_state = DIAG_WAIT_STS2;
            break;

        case ADDR_STS2:
            g_data.sts2_uv33 = (value & STS2_UV33) ? true : false;
            g_state = DIAG_WAIT_SQISTS0;
            break;

        case ADDR_SQISTS0:
            g_data.sqi_err   = (value & (1u << 7)) ? true : false;
            g_data.sqi_valid = (value & (1u << 6)) ? true : false;
            g_data.sqi_val   = (uint8_t)((value >> 3) & 0x07);
            g_state = DIAG_WAIT_PLCA_CTRL0;
            break;

        case ADDR_PLCA_CTRL0:
            g_data.plca_en = (value & PLCA_CTRL0_EN) ? true : false;
            g_state = DIAG_WAIT_PLCA_CTRL1;
            break;

        case ADDR_PLCA_STS:
            g_data.plca_pst = (value & PLCA_STS_PST) ? true : false;
            g_state = DIAG_PRINT;

            g_diag_ready = true;   // ✅ ADD THIS

            return;

        default:
            g_state = DIAG_IDLE;
            return;
    }

    DIAG_FireNextRead();
}

/* =============================================================================
   INTERNAL - CDCTL0 read-modify-write callback
   ============================================================================= */
static void DIAG_CDCTL_ReadCB(TC6_t *inst, bool success, uint32_t addr,
                                uint32_t value, void *tag, void *globalTag)
{
    (void)addr; (void)tag; (void)globalTag;
    if (!success || !g_cdctl_rmw_pending) return;
    g_cdctl_rmw_pending = false;

    if (g_cdctl_rmw_enable)
        value |=  CDCTL0_CDEN;
    else
        value &= ~CDCTL0_CDEN;

    TC6_WriteRegister(inst, ADDR_CDCTL0, value, true, NULL, NULL);
}

/* =============================================================================
   INTERNAL - print report
   ============================================================================= */
#define FLAG(x)  ((x) ? "Y" : "-")

static void DIAG_Print(void)
{
	char buf[512];


    snprintf(buf, sizeof(buf),
        "|  SW RX forwarded        : %-14lu|\r\n"
        "|  SW TX ok               : %-14lu|\r\n"
        "|  SW TX fail             : %-14lu|\r\n",
        (unsigned long)g_sw_rx,
        (unsigned long)g_sw_tx_ok,
        (unsigned long)g_sw_tx_fail);

    snprintf(buf, sizeof(buf),
        "|  STATS6  (TFRX)         : %-14lu|\r\n"
        "|  STATS7  (FRX)          : %-14lu|\r\n"
        "|  STATS0  (RXSE)         : %-14u|\r\n"
        "|  STATS0  (LFER)         : %-14u|\r\n"
        "|  STATS0  (OFRX)         : %-14u|\r\n"
        "|  STATS0  (UFRX)         : %-14u|\r\n"
        "|  STATS1  (RXBOVR)       : %-14u|\r\n"
        "|  STATS2  (FCSE)         : %-14u|\r\n"
        "|  STATS5  (BFRX)         : %-14u|\r\n",
        (unsigned long)g_data.tfrx,
        (unsigned long)g_data.frx,
        g_data.rxse,
        g_data.lfer,
        g_data.ofrx,
        g_data.ufrx,
        g_data.rxbovr,
        g_data.fcse,
        g_data.bfrx);


    /* ---- Protocol classification (STATS3) ----------------------------- */
//    snprintf(buf, sizeof(buf),
//        "|  STATS3  (TID1-IPv4)    : %-14u|\r\n"
//        "|  STATS3  (TID2-ARP)     : %-14u|\r\n",
//        g_data.ipv4_cnt,
//        g_data.arp_cnt);
//    UART4_Send(buf);

    /* ---- MAC TX (MMS1) ------------------------------------------------ */

    snprintf(buf, sizeof(buf),
        "|  STATS11 (TFTX)         : %-14lu|\r\n"
        "|  STATS12 (FTX)          : %-14lu|\r\n",
        (unsigned long)g_data.tftx,
        (unsigned long)g_data.ftx);

    snprintf(buf, sizeof(buf),
        "|  T1SPCSDIAG2 (CORTXCNT) : %-14u|\r\n"
        "|  Total CORTXCNT (life)  : %-14lu|\r\n",
        g_data.cortxcnt,
        (unsigned long)g_total_cortxcnt);

    snprintf(buf, sizeof(buf),
        "|  STS1 (TXCOL)           : %-14s|\r\n"
        "|  STS1 (TXJAB)           : %-14s|\r\n"
        "|  STS1 (EMPCYC)          : %-14s|\r\n"
        "|  STS1 (RXINTO)          : %-14s|\r\n"
        "|  STS1 (PLCASYM)         : %-14s|\r\n"
        "|  STS2 (UV33)            : %-14s|\r\n",
        FLAG(g_data.sts1_txcol),
        FLAG(g_data.sts1_txjab),
        FLAG(g_data.sts1_empcyc),
        FLAG(g_data.sts1_rxinto),
        FLAG(g_data.sts1_plcasym),
        FLAG(g_data.sts2_uv33));


    /* ---- SQI (MMS4 SQISTS0) ------------------------------------------- */
//    UART4_Send("+------------------------------------------+\r\n");
//    UART4_Send("| [SQI - MMS4 SQISTS0]                     |\r\n");
//    if (g_data.sqi_err)
//    {
//        UART4_Send("|  SQISTS0 (SQIERR)       : ERR-toggle SQIEN|\r\n");
//    }
//    else if (g_data.sqi_valid)
//    {
//        snprintf(buf, sizeof(buf),
//            "|  SQISTS0 (SQIVAL)       : %-14u|\r\n", g_data.sqi_val);
//        UART4_Send(buf);
//    }
//    else
//    {
//        UART4_Send("|  SQISTS0 (SQIVLD)       : pending       |\r\n");
//    }

    /* ---- PLCA (MMS4) -------------------------------------------------- */

    snprintf(buf, sizeof(buf),
        "|  PLCA_CTRL0 (EN)        : %-14s|\r\n"
        "|  PLCA_STS   (PST)       : %-14s|\r\n"
        "|  PLCA_CTRL1 (ID)        : %-14u|\r\n"
        "|  PLCA_CTRL1 (NCNT)      : %-14u|\r\n",
        g_data.plca_en  ? "ENABLED"  : "DISABLED",
        g_data.plca_pst ? "ACTIVE"   : "INACTIVE",
        g_data.plca_id,
        g_data.plca_ncnt);

}

void DIAG_GetSnapshot(uint8_t *buf)
{
    memset(buf, 0, 64);

    // ---- Pack data (match Python later)
    uint32_t offset = 1;

    memcpy(&buf[offset], &g_data.tfrx, 4); offset += 4;
    memcpy(&buf[offset], &g_data.frx, 4); offset += 4;
    memcpy(&buf[offset], &g_data.tftx, 4); offset += 4;
    memcpy(&buf[offset], &g_data.ftx, 4); offset += 4;

    memcpy(&buf[offset], &g_data.cortxcnt, 2); offset += 2;

    buf[offset++] = g_data.rxse;
    buf[offset++] = g_data.lfer;
    buf[offset++] = g_data.ofrx;
    buf[offset++] = g_data.ufrx;
    buf[offset++] = g_data.rxbovr;
    buf[offset++] = g_data.fcse;
    buf[offset++] = g_data.bfrx;   // add this line

    buf[offset++] = g_data.plca_en;
    buf[offset++] = g_data.plca_pst;
    buf[offset++] = g_data.plca_id;
    buf[offset++] = g_data.plca_ncnt;
}
