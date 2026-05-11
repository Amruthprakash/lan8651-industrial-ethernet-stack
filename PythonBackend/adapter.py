# adapter.py
# =============================================================
# Mid-level T1S Adapter — bridges HIDTransport (register/PLCA)
# and EthTransport (raw Ethernet TX/RX), and exposes diagnostic
# access via DiagPoller / DiagSnap.
# =============================================================

import time
import logging
from typing import Callable, Optional
from scapy.all import Ether, Raw, ARP, IP, ICMP

from constants import DEFAULT_VID, DEFAULT_PID
from hid_transport import HIDTransport
from eth_transport import EthTransport
from diag_stats import DiagSnap, DiagPoller

logger = logging.getLogger(__name__)


class T1SAdapter:

    def __init__(self,
                 iface: str,
                 mac: str,
                 vid: int = DEFAULT_VID,
                 pid: int = DEFAULT_PID):
        """
        Initialise the T1S Adapter.

        Args:
            iface : Host network interface name (PCAP/NPF device string).
            mac   : Source MAC address string for EthTransport.
            vid   : USB Vendor ID  (default from constants.py).
            pid   : USB Product ID (default from constants.py).

        VID/PID flow: tester.py -> T1SAdapter(vid, pid) -> HIDTransport(vid, pid)
        """
        if not iface or not mac:
            raise ValueError("Interface and MAC are required")

        self.hid = HIDTransport(vid=vid, pid=pid)
        self.eth = EthTransport(iface, src_mac=mac)

        self._poller: Optional[DiagPoller] = None

    # =========================================================
    # LOW LEVEL
    # =========================================================

    def send_frame(self, frame: bytes):
        """Send a raw Ethernet frame."""
        self.eth.send(frame)

    def sniff(self, timeout: float = 5, filter_exp: str = None) -> list:
        """Blocking receive — collects all frames until timeout."""
        frames = []
        t0 = time.time()

        while time.time() - t0 < timeout:
            pkt = self.eth.recv(timeout=0.5, filter_exp=filter_exp)
            if pkt:
                frames.append(pkt)

        return frames

    # =========================================================
    # HIGH LEVEL
    # =========================================================

    def send_data(self, payload: bytes,
                  dst_mac: str,
                  eth_type: int = 0x88B5):
        """Build and send an Ethernet frame from raw payload bytes."""
        frame = Ether(
            dst=dst_mac,
            src=self.eth.src_mac,
            type=eth_type
        ) / Raw(load=payload.ljust(46, b'\x00'))

        self.eth.send(bytes(frame))

    def receive(self, timeout: float = 5, filter_exp: str = None) -> list:
        """Receive and decode frames into dicts."""
        raw_frames = self.sniff(timeout, filter_exp)
        decoded = []

        for f in raw_frames:
            try:
                pkt = Ether(f)
                decoded.append({
                    "src":     pkt.src,
                    "dst":     pkt.dst,
                    "type":    pkt.type,
                    "payload": bytes(pkt.payload),
                })
            except Exception:
                continue

        return decoded

    # =========================================================
    # NETWORK UTILITIES
    # =========================================================

    def ping(self, dst_ip: str, timeout: float = 2) -> bool:
        """Send an ICMP echo request and wait for any ICMP reply."""
        pkt = Ether(
            src=self.eth.src_mac,
            dst="ff:ff:ff:ff:ff:ff"
        ) / IP(dst=dst_ip) / ICMP()

        self.send_frame(bytes(pkt))
        responses = self.sniff(timeout, filter_exp="icmp")
        return len(responses) > 0

    def scan_nodes(self, timeout: float = 3) -> list:
        """Basic node discovery via ARP broadcast over 192.168.0.0/24."""
        pkt = Ether(
            dst="ff:ff:ff:ff:ff:ff",
            src=self.eth.src_mac
        ) / ARP(pdst="192.168.0.0/24")

        self.send_frame(bytes(pkt))
        responses = self.sniff(timeout, filter_exp="arp")

        nodes = set()
        for r in responses:
            try:
                p = Ether(r)
                if ARP in p:
                    nodes.add((p[ARP].psrc, p[ARP].hwsrc))
            except Exception:
                continue

        return list(nodes)

    # =========================================================
    # BACKGROUND SNIFF
    # =========================================================

    def start_sniffing(self, filter_exp: str = None):
        self.eth.start_sniffer(filter_exp)

    def stop_sniffing(self):
        self.eth.stop_sniffer()

    def get_sniffed_frames(self) -> list:
        frames = self.eth.get_frames()
        decoded = []

        for pkt in frames:
            try:
                decoded.append({
                    "src":     pkt.src,
                    "dst":     pkt.dst,
                    "type":    pkt.type,
                    "payload": bytes(pkt.payload),
                })
            except Exception:
                continue

        return decoded

    # =========================================================
    # DIAGNOSTICS
    # =========================================================

    def get_diag_snap(self) -> DiagSnap:
        """
        Single blocking diagnostic poll — returns a DiagSnap.

        No background thread, no accumulated totals.
        Use start_diag_polling() for continuous monitoring.
        """
        raw = self.hid.get_diag_raw()
        return DiagSnap.parse(raw)

    def start_diag_polling(self,
                           callback: Callable[[DiagSnap], None],
                           interval: float = 0.5) -> DiagPoller:
        """
        Start background diagnostic polling.

        Creates and starts a DiagPoller, stores a reference so it can be
        stopped via stop_diag_polling().  Replaces any previously running
        poller for this adapter instance.

        Args:
            callback : callable(DiagSnap) — called each poll cycle.
            interval : polling interval in seconds (default 0.5).

        Returns:
            The running DiagPoller instance (for direct totals access).
        """
        if self._poller is not None:
            self._poller.stop()

        self._poller = DiagPoller(self.hid, callback, interval=interval)
        self._poller.start()
        return self._poller

    def stop_diag_polling(self):
        """Stop the background diagnostic poller if running."""
        if self._poller is not None:
            self._poller.stop()
            self._poller = None

    def get_diag_totals(self) -> dict:
        """
        Return accumulated RC counter totals from the active poller.
        Returns an empty dict if no poller has been started.
        """
        if self._poller is None:
            return {}
        return dict(self._poller.totals)