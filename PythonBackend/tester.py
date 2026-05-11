# tester.py
# =============================================================
# T1S Adapter test script.
# All hardware-specific identity (VID/PID, interface, MACs,
# node config) is declared in the USER CONFIG block below.
# Nothing below that block should need editing for a new device.
# =============================================================

import time
from scapy.all import Ether, Raw

from adapter import T1SAdapter
from eth_transport import EthTransport
from constants import PLCA_CTRL0, PLCA_CTRL1, PLCA_STATUS

EthTransport.show_interfaces()

# =========================
# USER CONFIG
# Change these values to match your setup.
# =========================

# USB HID device identity of the STM32 bridge firmware
VID = 0xCAFE
PID = 0x4011

# Host network interface (copy from show_interfaces() output above)
IFACE = r"\Device\NPF_{968225B7-677F-4CFE-9128-D60E4C97EF8C}"

# MAC address assigned to this host-side port
MAC = "02:00:00:00:00:03"

# PLCA topology config
NODE_ID    = 0
NODE_COUNT = 2

# Destination MAC for the test frame
DST_MAC = "02:00:00:00:00:01"


# =========================
# INIT
# =========================
print("\n==============================")
print(" T1S ADAPTER TEST START")
print("==============================\n")

dev = T1SAdapter(iface=IFACE, mac=MAC, vid=VID, pid=PID)


# =========================
# STEP 0: INITIAL READ
# =========================
ctrl1 = dev.hid.read_reg(PLCA_CTRL1)

read_node_id  = ctrl1 & 0xFF
read_node_cnt = (ctrl1 >> 8) & 0xFF

print("-----------INITIAL READ-----------\n")
print(f"PLCA_CTRL1 = {hex(ctrl1)}")
print(f"Node ID    = {read_node_id}")
print(f"Node Count = {read_node_cnt}\n")


# =========================
# STEP 1: Disable PLCA
# =========================
print("[STEP 1] Disabling PLCA...")
dev.hid.enable_plca(False)

ctrl = dev.hid.read_reg(PLCA_CTRL0)
print(f"PLCA_CTRL0 = {hex(ctrl)}")

if (ctrl & 0x8000) == 0:
    print("✔ PLCA Disabled\n")
else:
    print("✖ Failed to disable PLCA\n")


# =========================
# STEP 2: Set Node Config
# =========================
print("[STEP 2] Setting PLCA Node Config...")

dev.hid.set_plca_config(NODE_ID, NODE_COUNT)

ctrl1 = dev.hid.read_reg(PLCA_CTRL1)

read_node_id  = ctrl1 & 0xFF
read_node_cnt = (ctrl1 >> 8) & 0xFF

print(f"PLCA_CTRL1 = {hex(ctrl1)}")
print(f"Node ID    = {read_node_id}")
print(f"Node Count = {read_node_cnt}")

if read_node_id == NODE_ID and read_node_cnt == NODE_COUNT:
    print("✔ Node configuration correct\n")
else:
    print("✖ Node configuration mismatch\n")


# =========================
# STEP 3: Reset PLCA
# =========================
print("[STEP 3] Resetting PLCA...")
dev.hid.reset_plca()

time.sleep(0.01)
print("✔ Reset issued\n")


# =========================
# STEP 4: Enable PLCA
# =========================
print("[STEP 4] Enabling PLCA...")
dev.hid.enable_plca(True)

ctrl = dev.hid.read_reg(PLCA_CTRL0)
print(f"PLCA_CTRL0 = {hex(ctrl)}")

if (ctrl & 0x8000):
    print("✔ PLCA Enabled\n")
else:
    print("✖ Failed to enable PLCA\n")


# =========================
# STEP 5: Check PLCA Status
# =========================
print("[STEP 5] Checking PLCA status...")

time.sleep(0.1)

status = dev.hid.read_reg(PLCA_STATUS)
pst = (status >> 15) & 1

print(f"PLCA_STATUS = {hex(status)}")
print(f"PST (Beacon Active) = {pst}")

if pst:
    print("✔ PLCA is ACTIVE (Beacon detected)\n")
else:
    print("⚠ PLCA NOT active (check wiring / other nodes)\n")


# =========================
# STEP 6: Send Test Frame
# =========================
print("[STEP 6] Sending test Ethernet frame...")

payload = b"T1S_TEST_FRAME"

frame = Ether(
    dst=DST_MAC,
    src=MAC,
    type=0x88B5
) / Raw(load=payload.ljust(46, b'\x00'))

dev.send_frame(bytes(frame))

print("✔ Frame sent\n")


# =========================
# STEP 7: Sniff Response
# =========================
print("[STEP 7] Listening for incoming frames...")

frames = dev.sniff(timeout=3)

if not frames:
    print("⚠ No frames received\n")
else:
    print(f"✔ Received {len(frames)} frame(s)\n")

    for i, f in enumerate(frames):
        try:
            pkt = Ether(f)
            print(f"--- Frame {i+1} ---")
            print("SRC :", pkt.src)
            print("DST :", pkt.dst)
            print("TYPE:", hex(pkt.type))

            if Raw in pkt:
                print("DATA:", pkt[Raw].load)

            print()
        except Exception:
            pass


# =========================
# STEP 8: Diagnostics
# =========================
print("[STEP 8] Reading diagnostic snapshot...\n")

snap = dev.get_diag_snap()

if snap.status != 0:
    print(f"⚠ Snapshot not valid (status={snap.status}) — firmware may still be initialising\n")
else:
    print(snap.report())
    print()

    # ── 8a: continuous poll for 3 seconds, print summary each cycle ──────────
    print("[STEP 8a] Background diagnostic poll for 3 s (0.5 s interval)...\n")

    def _on_diag(s):
        print(f"  [POLL] {s.summary()}")

    poller = dev.start_diag_polling(_on_diag, interval=0.5)
    time.sleep(3)
    dev.stop_diag_polling()

    print()

    # ── 8b: accumulated RC totals ─────────────────────────────────────────────
    print("[STEP 8b] Accumulated RC totals after poll period:\n")
    print(poller.totals_report())
    print()

    # ── 8c: health summary ────────────────────────────────────────────────────
    last = poller.last_snap
    if last:
        health = "✔ No errors" if last.error_free else "⚠ Errors detected — see report above"
        print(f"  Final health check : {health}")
    print()


# =========================
# DONE
# =========================
print("==============================")
print(" TEST COMPLETE")
print("==============================\n")

dev.hid.close()