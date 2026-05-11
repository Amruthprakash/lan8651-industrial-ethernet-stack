# constants.py
# =============================================================
# LAN8650/1  T1S SDK — shared constants, register addresses,
# HID command bytes, and diagnostic snapshot format strings.
#
# All SDK modules import from here.  Never hardcode these values
# in transport, adapter, or tester files.
# =============================================================

import struct

# ── Default USB device identity ───────────────────────────────────────────────
# Used as parameter defaults in HIDTransport.__init__ and T1SAdapter.__init__.
# Override by passing explicit vid=/pid= from tester.py or your application.
# MAC address and network interface are test-rig-specific — declare them in
# tester.py only, never here.
DEFAULT_VID = 0xCAFE
DEFAULT_PID = 0x4011

# ── HID command bytes  (must match hid.c firmware) ────────────────────────────
CMD_READ_REG        = 0x01   # read a 32-bit register
CMD_WRITE_REG       = 0x02   # write a 32-bit register
CMD_GET_STATUS      = 0x03   # get device status byte
CMD_TOGGLE_PLCA     = 0x05   # toggle PLCA enable bit
CMD_SET_PLCA_CONFIG = 0x06   # set node-id + node-count in one command
CMD_PLCA_RESET      = 0x08   # assert PLCA RST (self-clearing)
CMD_GET_DIAG        = 0x10   # request 64-byte diagnostic snapshot
CMD_HARD_RESET      = 0x11   # full MCU hard reset

# ── Register addresses  (addr32 = (MMS << 16) | reg16) ───────────────────────

# MMS1 — MAC / frame statistics
NETWORK_CONFIG  = 0x00010001   # NCFGR — promiscuous mode, speed, etc.
PROMISC_BIT     = (1 << 4)     # NCFGR bit 4: enable promiscuous RX

# MMS4 — PHY vendor-specific (PLCA)
PLCA_CTRL0      = 0x0004CA01   # PLCA Control 0 — EN [15], RST [14]
PLCA_CTRL1      = 0x0004CA02   # PLCA Control 1 — NCNT [15:8], ID [7:0]
PLCA_STATUS     = 0x0004CA03   # PLCA Status    — PST beacon active [15]

# Legacy aliases (kept for backward compatibility with older tester scripts)
PLCA_CTRL       = PLCA_CTRL0
PLCA_CTRL1_ADDR = PLCA_CTRL1   # name-clash guard — prefer PLCA_CTRL1 in new code

# ── Diagnostic snapshot binary layout ─────────────────────────────────────────
# Matches DIAG_GetSnapshot() in diag_stats.c.
# Packed at offset 0 of the 64-byte HID response payload.
#
# Legacy format — 30 bytes:
#   B  : status       (0 = valid)
#   4I : tfrx, frx, tftx, ftx
#   H  : cortxcnt
#   9B : rxse, lfer, ofrx, ufrx, rxbovr, fcse, bfrx, plca_en, plca_pst
#   2B : plca_id, plca_ncnt
SNAP_FMT   = "<BIIIIHBBBBBBBBBBB"    # 30 bytes
SNAP_BYTES = struct.calcsize(SNAP_FMT)

# Extended format — 32 bytes (appends plcasym, uv33):
SNAP_FMT_EXT   = "<BIIIIHBBBBBBBBBBBBB"  # 32 bytes
SNAP_BYTES_EXT = struct.calcsize(SNAP_FMT_EXT)