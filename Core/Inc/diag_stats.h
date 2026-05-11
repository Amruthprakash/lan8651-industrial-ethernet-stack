/* =============================================================================
 * diag_stats.h  -  LAN8650/1 Diagnostic & Statistics Module
 * =============================================================================
 */

#ifndef DIAG_STATS_H
#define DIAG_STATS_H

#include <stdint.h>
#include <stdbool.h>
#include "tc6.h"
extern bool g_diag_ready;

/** Initialise module. Call once after TC6_Init() succeeds. */
void DIAG_Init(TC6_t *tc6);

/**
 * Enable/disable physical collision detection (CDCTL0.CDEN).
 * Recommended: disable when PLCA is active (DS60001734F §11.5.51).
 */
void DIAG_EnableCollisionDetect(bool enable);

/** Enable PLCA TX-opportunity and BEACON hardware counters (CTRCTRL). */
void DIAG_EnablePLCACounters(void);

/** Enable/disable PLCA engine (PLCA_CTRL0.EN). Apply on ALL nodes. */
void DIAG_SetPLCAEnable(bool enable);

/** Call from TC6_CB_OnRxEthernetPacket on every received frame. */
void DIAG_OnRx(void);

/** Call from tud_network_recv_cb on every TX attempt. */
void DIAG_OnTx(bool success);

/** Call every main-loop iteration. Throttled to DIAG_INTERVAL_MS internally. */
void DIAG_Task(void);

void DIAG_GetSnapshot(uint8_t *buf);

// In diag_stats.h — add this extern
extern bool g_diag_reinit_needed;
#endif /* DIAG_STATS_H */
