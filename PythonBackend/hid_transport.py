# hid_transport.py
# =============================================================
# Low-level USB HID transport layer for the T1S Adapter SDK.
# VID/PID are NOT hardcoded here; always supplied by the caller.
# =============================================================

import hid
import struct

from constants import (
    DEFAULT_VID, DEFAULT_PID,
    CMD_READ_REG, CMD_WRITE_REG, CMD_GET_STATUS,
    CMD_TOGGLE_PLCA, CMD_SET_PLCA_CONFIG, CMD_PLCA_RESET,
    CMD_GET_DIAG, CMD_HARD_RESET,
    PLCA_CTRL0, PLCA_CTRL1, PLCA_STATUS,
)


class HIDTransport:

    def __init__(self, vid: int = DEFAULT_VID, pid: int = DEFAULT_PID):
        """
        Open the HID device.

        Args:
            vid: USB Vendor ID  (default: DEFAULT_VID from constants.py)
            pid: USB Product ID (default: DEFAULT_PID from constants.py)

        Pass explicit vid/pid from the application layer (tester.py or
        T1SAdapter) so this module stays firmware-independent.
        """
        self.vid = vid
        self.pid = pid
        self.dev = hid.device()
        self.dev.open(vid, pid)

    def close(self):
        """Release the HID device handle."""
        try:
            self.dev.close()
        except Exception:
            pass

    def _send(self, pkt: bytearray) -> list:
        self.dev.write([0] + list(pkt))
        return self.dev.read(64, 2000)

    # -------------------------
    # RAW REGISTER ACCESS
    # -------------------------

    def read_reg(self, addr: int) -> int:
        pkt = bytearray(64)
        pkt[0] = CMD_READ_REG
        pkt[1:5] = struct.pack("<I", addr)

        resp = self._send(pkt)
        return struct.unpack("<I", bytes(resp[5:9]))[0]

    def write_reg(self, addr: int, val: int):
        pkt = bytearray(64)
        pkt[0] = CMD_WRITE_REG
        pkt[1:5] = struct.pack("<I", addr)
        pkt[5:9] = struct.pack("<I", val)

        self._send(pkt)

    # -------------------------
    # PLCA CONTROL
    # -------------------------

    def enable_plca(self, enable: bool):
        val = self.read_reg(PLCA_CTRL0)
        val = (val | 0x8000) if enable else (val & ~0x8000)
        self.write_reg(PLCA_CTRL0, val)

    def toggle_plca(self) -> int:
        pkt = bytearray(64)
        pkt[0] = CMD_TOGGLE_PLCA

        resp = self._send(pkt)
        return struct.unpack("<I", bytes(resp[5:9]))[0]

    def set_plca_config(self, node_id: int, node_count: int):
        pkt = bytearray(64)
        pkt[0] = CMD_SET_PLCA_CONFIG
        pkt[1] = node_id & 0xFF
        pkt[2] = node_count & 0xFF

        self._send(pkt)

    def reset_plca(self):
        pkt = bytearray(64)
        pkt[0] = CMD_PLCA_RESET
        self._send(pkt)

    def get_plca_status(self) -> int:
        """Return the PST (beacon active) bit from PLCA_STATUS."""
        val = self.read_reg(PLCA_STATUS)
        return (val >> 15) & 1

    # -------------------------
    # DIAGNOSTICS
    # -------------------------

    def get_diag_raw(self) -> bytes:
        """
        Request the 64-byte diagnostic snapshot from firmware.

        The firmware fills the response buffer using DIAG_GetSnapshot()
        (diag_stats.c).  Pass the returned bytes to DiagSnap.parse().
        """
        pkt = bytearray(64)
        pkt[0] = CMD_GET_DIAG
        resp = self._send(pkt)
        return bytes(resp)

    # -------------------------
    # SYSTEM CONTROL
    # -------------------------

    def hard_reset(self):
        """
        Trigger a full MCU hard reset via CMD_HARD_RESET.

        The device will re-enumerate on USB after ~500 ms.
        Re-open HIDTransport after calling this.
        """
        pkt = bytearray(64)
        pkt[0] = CMD_HARD_RESET
        try:
            self._send(pkt)
        except Exception:
            pass   # device disappears immediately — read timeout is expected