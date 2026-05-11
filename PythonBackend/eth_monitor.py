# eth_monitor.py
# =============================================================
# Standalone continuous Ethernet frame monitor for the T1S Adapter.
# Streams every incoming frame to the console until Ctrl+C,
# then prints a session summary.
#
# Uses EthTransport's background sniffer directly — no adapter needed.
# Run:  python eth_monitor.py
# =============================================================

import time
import signal
import sys
from datetime import datetime

from scapy.all import Ether, Raw, ARP, IP, ICMP, get_if_list
from eth_transport import EthTransport

# =========================
# USER CONFIG
# =========================
IFACE         = r"\Device\NPF_{968225B7-677F-4CFE-9128-D60E4C97EF8C}"
MAC           = "02:12:34:56:78:9b"   # host-side MAC (own frames are filtered out)
POLL_INTERVAL = 0.2                   # how often to drain the sniffer buffer (seconds)
BPF_FILTER    = None                  # optional BPF filter string e.g. "ether proto 0x88B5"
# =========================

_DIVIDER     = "─" * 60
_DIVIDER_FAT = "═" * 60
_frame_count = 0
_start_time  = None


def _header():
    print(_DIVIDER_FAT)
    print("  T1S ETHERNET FRAME MONITOR")
    print(f"  IFACE  : {IFACE}")
    print(f"  OWN MAC: {MAC}  (own frames filtered)")
    if BPF_FILTER:
        print(f"  FILTER : {BPF_FILTER}")
    print("  Press Ctrl+C to stop")
    print(_DIVIDER_FAT)
    print()


def _decode_frame(raw_pkt) -> str:
    """Return a formatted multi-line description of one Ethernet frame."""
    global _frame_count
    _frame_count += 1

    ts  = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    out = []

    try:
        pkt = Ether(raw_pkt) if isinstance(raw_pkt, (bytes, bytearray)) else raw_pkt
        eth_type = pkt.type

        out.append(f"  [{ts}]  Frame #{_frame_count}")
        out.append(f"  {'SRC':<6}: {pkt.src}")
        out.append(f"  {'DST':<6}: {pkt.dst}")
        out.append(f"  {'TYPE':<6}: {hex(eth_type)}", )

        # Layer-aware decode
        if eth_type == 0x0806 and ARP in pkt:
            arp = pkt[ARP]
            op  = "REQUEST" if arp.op == 1 else "REPLY"
            out.append(f"  {'ARP':<6}: {op}  {arp.psrc} ({arp.hwsrc}) → {arp.pdst}")

        elif eth_type == 0x0800 and IP in pkt:
            ip = pkt[IP]
            out.append(f"  {'IP':<6}: {ip.src} → {ip.dst}  proto={ip.proto}")

            if ICMP in pkt:
                icmp = pkt[ICMP]
                kind = "ECHO REQUEST" if icmp.type == 8 else \
                       "ECHO REPLY"   if icmp.type == 0 else f"type={icmp.type}"
                out.append(f"  {'ICMP':<6}: {kind}  id={icmp.id}  seq={icmp.seq}")

        elif eth_type == 0x88B5:
            out.append(f"  {'PROTO':<6}: T1S vendor / test frame (0x88B5)")
            if Raw in pkt:
                payload = bytes(pkt[Raw].load).rstrip(b'\x00')
                out.append(f"  {'DATA':<6}: {payload}")

        else:
            if Raw in pkt:
                payload = bytes(pkt[Raw].load)
                preview = payload[:24].hex()
                tail    = "…" if len(payload) > 24 else ""
                out.append(f"  {'DATA':<6}: {preview}{tail}  ({len(payload)} bytes)")

        out.append(f"  {'LEN':<6}: {len(raw_pkt)} bytes")

    except Exception as exc:
        out.append(f"  [parse error: {exc}]")

    return "\n".join(out)


def _session_summary():
    elapsed = time.time() - _start_time if _start_time else 0
    fps     = _frame_count / elapsed if elapsed > 0 else 0

    print()
    print(_DIVIDER_FAT)
    print("  MONITOR STOPPED")
    print(_DIVIDER_FAT)
    print(f"  Frames received : {_frame_count}")
    print(f"  Session time    : {elapsed:.1f} s")
    print(f"  Average rate    : {fps:.2f} frames/s")
    print()


def main():
    global _start_time

    _header()

    print("  Available interfaces:")
    EthTransport.show_interfaces()

    print("  Opening interface...")
    try:
        eth = EthTransport(iface=IFACE, src_mac=MAC)
    except Exception as exc:
        print(f"\n  ✖ Could not open interface: {exc}")
        print("  Check IFACE string and Npcap/libpcap installation.\n")
        sys.exit(1)

    print("  ✔ Interface open\n")
    print(f"  Streaming frames (Ctrl+C to stop)...\n")
    print(_DIVIDER)

    eth.start_sniffer(filter_exp=BPF_FILTER)
    _start_time = time.time()

    def _sigint(sig, frame):
        eth.stop_sniffer()
        _session_summary()
        sys.exit(0)

    signal.signal(signal.SIGINT, _sigint)

    try:
        while True:
            frames = eth.get_frames()

            for pkt in frames:
                # get_frames() returns raw Scapy packet objects from the buffer
                try:
                    raw = bytes(pkt)
                except Exception:
                    raw = pkt

                print(_decode_frame(raw))
                print(_DIVIDER)

            time.sleep(POLL_INTERVAL)

    except KeyboardInterrupt:
        _sigint(None, None)


if __name__ == "__main__":
    main()