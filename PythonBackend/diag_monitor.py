# diag_runner.py
# =============================================================
# Standalone continuous diagnostic monitor for the T1S Adapter.
# Streams a live DiagSnap every <POLL_INTERVAL> seconds until
# Ctrl+C is pressed, then prints accumulated RC totals.
#
# Uses HIDTransport + DiagPoller directly — no adapter needed.
# Run:  python diag_runner.py
# =============================================================

import time
import signal
import sys
from datetime import datetime

from hid_transport import HIDTransport
from diag_stats import DiagSnap, DiagPoller
from constants import DEFAULT_VID, DEFAULT_PID

# =========================
# USER CONFIG
# =========================
VID           = DEFAULT_VID   # 0xCAFE
PID           = DEFAULT_PID   # 0x4011
POLL_INTERVAL = 0.5           # seconds between each snapshot
# =========================

_DIVIDER     = "─" * 60
_DIVIDER_FAT = "═" * 60
_poll_count  = 0


def _header():
    print(_DIVIDER_FAT)
    print("  T1S DIAGNOSTIC MONITOR")
    print(f"  VID=0x{VID:04X}  PID=0x{PID:04X}  interval={POLL_INTERVAL}s")
    print(f"  Press Ctrl+C to stop")
    print(_DIVIDER_FAT)
    print()


def _on_snap(snap: DiagSnap):
    global _poll_count
    _poll_count += 1

    ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]

    print(f"{_DIVIDER}")
    print(f"  Poll #{_poll_count:<4}  {ts}")
    print(_DIVIDER)
    print(snap.report())
    print()


def _on_exit(poller: DiagPoller):
    print()
    print(_DIVIDER_FAT)
    print("  MONITOR STOPPED")
    print(_DIVIDER_FAT)
    print()
    print(f"  Total polls     : {_poll_count}")
    print()
    print(poller.totals_report())
    print()

    last = poller.last_snap
    if last:
        health = "✔  No errors detected" if last.error_free else "⚠  Errors detected — review report above"
        print(f"  Final health    : {health}")

    print()


def main():
    _header()

    print("  Connecting to HID device...")
    try:
        hid = HIDTransport(vid=VID, pid=PID)
    except Exception as exc:
        print(f"\n  ✖ Could not open HID device: {exc}")
        print("  Check VID/PID and that the device is connected.\n")
        sys.exit(1)

    print("  ✔ Device opened\n")

    # One-shot pre-check before starting the poller
    print("  Running initial snapshot...\n")
    raw  = hid.get_diag_raw()
    snap = DiagSnap.parse(raw)

    if snap.status != 0:
        print(f"  ⚠ Initial snapshot not valid (status={snap.status})")
        print("  Firmware may still be initialising — continuing anyway.\n")
    else:
        print(snap.report())
        print()

    # Start background poller
    poller = DiagPoller(hid, _on_snap, interval=POLL_INTERVAL)

    def _sigint(sig, frame):
        poller.stop()
        _on_exit(poller)
        hid.close()
        sys.exit(0)

    signal.signal(signal.SIGINT, _sigint)

    print(f"  Streaming diagnostics (Ctrl+C to stop)...\n")
    poller.start()

    # Keep main thread alive — poller runs in a daemon thread
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        _sigint(None, None)


if __name__ == "__main__":
    main()