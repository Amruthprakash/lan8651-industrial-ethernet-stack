# diag_stats.py
# =============================================================
# LAN8650/1  T1S SDK — Diagnostics and statistics.
#
# DiagSnap
# --------
# Dataclass that mirrors the binary layout of DIAG_GetSnapshot()
# in diag_stats.c.  Parses the 64-byte HID snapshot into named
# fields grouped by category (frame counters, error counters,
# PLCA config/status, hardware flags).
#
# DiagPoller
# ----------
# Background polling loop.  Calls a user-supplied callback with
# a fresh DiagSnap every <interval> seconds, and accumulates
# running totals for all RC (Read-to-Clear) counter fields.
#
# Usage
# -----
#   from hid_transport import HIDTransport
#   from diag_stats import DiagSnap, DiagPoller
#
#   hid = HIDTransport(vid=0xCAFE, pid=0x4011)
#
#   # One-shot
#   snap = DiagSnap.parse(hid.get_diag_raw())
#   print(snap)
#
#   # Continuous
#   def on_snap(snap: DiagSnap):
#       print(snap.summary())
#
#   poller = DiagPoller(hid, on_snap, interval=0.5)
#   poller.start()
#   ...
#   poller.stop()
#   print(poller.totals)
# =============================================================

import time
import struct
import threading
import logging
from dataclasses import dataclass, field
from typing import Callable, Dict, Optional

from constants import (
    SNAP_FMT, SNAP_BYTES,
    SNAP_FMT_EXT, SNAP_BYTES_EXT,
)

logger = logging.getLogger(__name__)

# RC (Read-to-Clear) counter field names — accumulated by DiagPoller
RC_KEYS = frozenset({
    "tfrx", "frx", "tftx", "ftx",
    "rxse", "lfer", "ofrx", "ufrx",
    "rxbovr", "fcse", "bfrx",
    "cortxcnt",
})

# ── Helpers ───────────────────────────────────────────────────────────────────

def _ok(val: bool) -> str:
    return "YES" if val else "NO"

def _pass_fail(condition: bool, pass_label="OK", fail_label="FAIL") -> str:
    return pass_label if condition else fail_label


# ══════════════════════════════════════════════════════════════════════════════

@dataclass
class DiagSnap:
    """
    Parsed diagnostic snapshot from the LAN8650/1 firmware.

    RC counter fields contain the DELTA since the last poll — they are NOT
    cumulative on their own.  Use DiagPoller.totals for running sums.

    Field map
    ---------
    Frame counters (RC)
        tfrx          total frames RX (good + errored)         MMS1 STATS6
        frx           good frames RX                           MMS1 STATS7
        tftx          total frames TX (good + errored)         MMS1 STATS11
        ftx           good frames TX                           MMS1 STATS12

    Error counters (RC)
        cortxcnt      corrupted TX (collision) this poll       MMS2 T1SPCSDIAG2
        total_cortxcnt lifetime cortxcnt (accumulated)
        rxse          RX symbol errors                         MMS1 STATS0 [31:24]
        lfer          length field errors                      MMS1 STATS0 [23:16]
        ofrx          oversize frames RX                       MMS1 STATS0 [15:8]
        ufrx          undersize frames RX                      MMS1 STATS0 [7:0]
        rxbovr        RX buffer overrun drops                  MMS1 STATS1 [23:16]
        fcse          FCS errors                               MMS1 STATS2 [7:0]
        bfrx          broadcast frames RX                      MMS1 STATS5 [15:8]

    PLCA configuration / status
        plca_en       PLCA enabled                             MMS4 PLCA_CTRL0 [15]
        plca_pst      PLCA beacon active (PST bit)             MMS4 PLCA_STS [15]
        plca_id       this node's PLCA node ID                 MMS4 PLCA_CTRL1 [7:0]
        plca_ncnt     PLCA node count                          MMS4 PLCA_CTRL1 [15:8]

    Hardware / power flags (sticky RC)
        plcasym       PLCA symbols detected on medium          MMS4 STS1 [2]
        uv33          3.3 V under-voltage detected             MMS4 STS2 [8]

    Meta
        status        0 = valid snapshot; non-zero = firmware busy
    """

    # Meta
    status:         int  = 1

    # Frame counters
    tfrx:           int  = 0
    frx:            int  = 0
    tftx:           int  = 0
    ftx:            int  = 0

    # TX collision / corruption
    cortxcnt:       int  = 0
    total_cortxcnt: int  = 0

    # Error counters
    rxse:           int  = 0
    lfer:           int  = 0
    ofrx:           int  = 0
    ufrx:           int  = 0
    rxbovr:         int  = 0
    fcse:           int  = 0
    bfrx:           int  = 0

    # PLCA
    plca_en:        bool = False
    plca_pst:       bool = False
    plca_id:        int  = 0
    plca_ncnt:      int  = 0

    # Hardware flags
    plcasym:        bool = False
    uv33:           bool = False

    # ── access helpers ────────────────────────────────────────────────────────

    def get(self, key: str):
        """Attribute access by string key (returns 0 for unknown keys)."""
        return getattr(self, key, 0)

    @property
    def error_free(self) -> bool:
        """True when all error counters are zero this poll."""
        return not any([
            self.rxse, self.lfer, self.ofrx, self.ufrx,
            self.rxbovr, self.fcse, self.cortxcnt,
        ])

    # ── parsing ───────────────────────────────────────────────────────────────

    @classmethod
    def parse(cls, raw: bytes, prev_col: int = 0) -> "DiagSnap":
        """
        Parse a raw 64-byte HID snapshot into a DiagSnap.

        Parameters
        ----------
        raw      : bytes from HIDTransport.get_diag_raw()
        prev_col : running cortxcnt accumulator (maintained by DiagPoller)

        Returns
        -------
        DiagSnap — status=1 (invalid) if raw is too short or malformed.
        """
        if len(raw) < SNAP_BYTES:
            logger.warning("DiagSnap.parse: raw too short (%d < %d)", len(raw), SNAP_BYTES)
            return cls()

        try:
            (status, tfrx, frx, tftx, ftx, cortxcnt,
             rxse, lfer, ofrx, ufrx, rxbovr, fcse,
             bfrx, plca_en, plca_pst,
             plca_id, plca_ncnt) = struct.unpack_from(SNAP_FMT, raw, 0)
        except struct.error as exc:
            logger.error("DiagSnap.parse: unpack error: %s", exc)
            return cls()

        s = cls()
        s.status         = status
        s.tfrx           = tfrx
        s.frx            = frx
        s.tftx           = tftx
        s.ftx            = ftx
        s.cortxcnt       = cortxcnt
        s.total_cortxcnt = prev_col + cortxcnt
        s.rxse           = rxse
        s.lfer           = lfer
        s.ofrx           = ofrx
        s.ufrx           = ufrx
        s.rxbovr         = rxbovr
        s.fcse           = fcse
        s.bfrx           = bfrx
        s.plca_en        = bool(plca_en)
        s.plca_pst       = bool(plca_pst)
        s.plca_id        = plca_id
        s.plca_ncnt      = plca_ncnt

        # Extended layout: plcasym + uv33 appended at bytes 30-31
        if len(raw) >= SNAP_BYTES_EXT:
            try:
                (*_, plcasym, uv33) = struct.unpack_from(SNAP_FMT_EXT, raw, 0)
                s.plcasym = bool(plcasym)
                s.uv33    = bool(uv33)
            except struct.error:
                pass

        return s

    # ── formatted output ──────────────────────────────────────────────────────

    def summary(self) -> str:
        """Single-line summary suitable for polling callbacks."""
        return (
            f"tfrx={self.tfrx:<5} frx={self.frx:<5} "
            f"tftx={self.tftx:<5} ftx={self.ftx:<5} | "
            f"fcse={self.fcse} cortx={self.cortxcnt} rxse={self.rxse} | "
            f"plca={'ON ' if self.plca_en else 'OFF'} "
            f"pst={'ACT' if self.plca_pst else '---'} "
            f"id={self.plca_id}/{self.plca_ncnt}"
        )

    def report(self) -> str:
        """
        Structured multi-section diagnostic report.
        Suitable for STEP 8 output in tester.py.
        """
        valid = "VALID" if self.status == 0 else f"INVALID (status={self.status})"
        rx_errs = self.rxse + self.lfer + self.ofrx + self.ufrx + self.rxbovr + self.fcse
        tx_errs = self.cortxcnt

        lines = [
            f"  Snapshot status  : {valid}",
            "",
            "  ┌─ Frame Counters ──────────────────────────────────┐",
            f"  │  Total RX    (tfrx)  : {self.tfrx:<10}              │",
            f"  │  Good  RX    (frx)   : {self.frx:<10}              │",
            f"  │  Bcast RX    (bfrx)  : {self.bfrx:<10}              │",
            f"  │  Total TX    (tftx)  : {self.tftx:<10}              │",
            f"  │  Good  TX    (ftx)   : {self.ftx:<10}              │",
            "  └───────────────────────────────────────────────────┘",
            "",
            "  ┌─ Error Counters ──────────────────────────────────┐",
            f"  │  FCS errors  (fcse)  : {self.fcse:<10}              │",
            f"  │  Symbol errs (rxse)  : {self.rxse:<10}              │",
            f"  │  Length errs (lfer)  : {self.lfer:<10}              │",
            f"  │  Oversize RX (ofrx)  : {self.ofrx:<10}              │",
            f"  │  Undersize RX(ufrx)  : {self.ufrx:<10}              │",
            f"  │  RX buf ovr  (rxbovr): {self.rxbovr:<10}              │",
            f"  │  Corrupt TX  (cortx) : {self.cortxcnt:<6} (lifetime {self.total_cortxcnt})  │",
            f"  │  RX err total        : {rx_errs:<10}              │",
            f"  │  TX err total        : {tx_errs:<10}              │",
            f"  │  Overall health      : {_pass_fail(rx_errs == 0 and tx_errs == 0):<10}              │",
            "  └───────────────────────────────────────────────────┘",
            "",
            "  ┌─ PLCA Configuration ──────────────────────────────┐",
            f"  │  Enabled             : {_ok(self.plca_en):<10}              │",
            f"  │  Node ID             : {self.plca_id:<10}              │",
            f"  │  Node Count          : {self.plca_ncnt:<10}              │",
            "  └───────────────────────────────────────────────────┘",
            "",
            "  ┌─ PLCA Status ─────────────────────────────────────┐",
            f"  │  Beacon active (PST) : {_ok(self.plca_pst):<10}              │",
            f"  │  Symbols detected    : {_ok(self.plcasym):<10}              │",
            "  └───────────────────────────────────────────────────┘",
            "",
            "  ┌─ Hardware / Power ────────────────────────────────┐",
            f"  │  3.3V under-voltage  : {_ok(self.uv33):<10}              │",
            "  └───────────────────────────────────────────────────┘",
        ]
        return "\n".join(lines)

    def __str__(self) -> str:
        return self.report()


# ══════════════════════════════════════════════════════════════════════════════

class DiagPoller:
    """
    Background diagnostic polling loop.

    Polls HIDTransport.get_diag_raw() every <interval> seconds, parses the
    result into a DiagSnap, accumulates RC counter totals, and calls the
    user-supplied callback.

    Parameters
    ----------
    hid       : HIDTransport instance
    callback  : callable(DiagSnap) — called in the daemon thread each cycle
    interval  : polling interval in seconds (default 0.5)

    Attributes
    ----------
    totals    : dict[str, int] — running sums of all RC counter fields
    last_snap : most recent DiagSnap (or None before first poll)
    """

    def __init__(self,
                 hid,
                 callback: Callable[["DiagSnap"], None],
                 interval: float = 0.5):
        self._hid      = hid
        self._callback = callback
        self._interval = interval
        self._running  = False
        self._thread: Optional[threading.Thread] = None
        self._col      = 0   # lifetime cortxcnt accumulator

        self.totals: Dict[str, int] = {k: 0 for k in RC_KEYS}
        self.last_snap: Optional[DiagSnap] = None

    def start(self):
        """Start background polling.  Safe to call multiple times."""
        if self._running:
            return
        self._running = True
        self._thread  = threading.Thread(target=self._loop, daemon=True,
                                         name="diag-poller")
        self._thread.start()
        logger.info("DiagPoller started  interval=%.1fs", self._interval)

    def stop(self):
        """Signal the polling loop to stop.  Returns immediately."""
        self._running = False
        logger.info("DiagPoller stopped")

    def reset_totals(self):
        """Zero all accumulated RC totals without restarting hardware."""
        for k in self.totals:
            self.totals[k] = 0
        self._col = 0

    def poll_once(self) -> DiagSnap:
        """
        Single blocking poll — accumulates totals, does NOT call callback.
        """
        raw  = self._hid.get_diag_raw()
        snap = DiagSnap.parse(raw, self._col)
        if snap.status == 0:
            self._col = snap.total_cortxcnt
            self._accumulate(snap)
            self.last_snap = snap
        return snap

    def totals_report(self) -> str:
        """Formatted string of accumulated RC totals."""
        lines = ["  Accumulated RC totals:"]
        for k, v in sorted(self.totals.items()):
            lines.append(f"    {k:<12} : {v}")
        return "\n".join(lines)

    # ── internal ──────────────────────────────────────────────────────────────

    def _loop(self):
        while self._running:
            t0 = time.monotonic()
            try:
                snap = self.poll_once()
                if snap.status == 0:
                    try:
                        self._callback(snap)
                    except Exception as exc:
                        logger.exception("DiagPoller callback error: %s", exc)
            except Exception as exc:
                logger.error("DiagPoller poll error: %s", exc)

            elapsed = time.monotonic() - t0
            sleep   = self._interval - elapsed
            if sleep > 0:
                time.sleep(sleep)

    def _accumulate(self, snap: DiagSnap):
        for key in self.totals:
            val = snap.get(key)
            if isinstance(val, (int, float)):
                self.totals[key] += val