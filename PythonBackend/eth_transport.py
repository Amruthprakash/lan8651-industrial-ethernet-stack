# eth_transport.py

from scapy.all import sendp, sniff, Ether, get_if_list, get_if_addr, get_if_hwaddr
import threading
import logging

logger = logging.getLogger(__name__)


class EthTransport:
    def __init__(self, iface: str, src_mac: str):
        if not iface:
            raise ValueError("Interface must be provided")

        if iface not in get_if_list():
            raise ValueError(f"Interface '{iface}' not found")

        if not src_mac:
            raise ValueError("Source MAC must be provided")

        self.iface = iface
        self.src_mac = src_mac.lower()

        self._rx_buffer = []
        self._running = False
        self._thread = None

    def show_interfaces():
        print("\nAvailable interfaces:\n")
        for i in get_if_list():
            try:
                print(i)
                print("  IP :", get_if_addr(i))
                print("  MAC:", get_if_hwaddr(i))
                print()
            except:
                pass
    # -------------------------
    # TX
    # -------------------------
    def send(self, frame: bytes):
        if not isinstance(frame, (bytes, bytearray)):
            raise TypeError("Frame must be bytes")

        sendp(frame, iface=self.iface, verbose=False)

    # -------------------------
    # RX (single frame)
    # -------------------------
    def recv(self, timeout=1, filter_exp=None):
        packets = sniff(
            iface=self.iface,
            timeout=timeout,
            count=1,
            filter=filter_exp
        )

        if not packets:
            return None

        pkt = packets[0]

        if Ether in pkt:
            if pkt[Ether].src.lower() == self.src_mac:
                return None

        return bytes(pkt)

    # -------------------------
    # RX (background)
    # -------------------------
    def start_sniffer(self, filter_exp=None):
        if self._running:
            logger.warning("Sniffer already running")
            return

        self._running = True

        def _cb(pkt):
            if Ether in pkt:
                if pkt[Ether].src.lower() == self.src_mac:
                    return
                self._rx_buffer.append(pkt)

        def _sniff():
            sniff(
                iface=self.iface,
                prn=_cb,
                store=False,
                stop_filter=lambda x: not self._running,
                filter=filter_exp
            )

        self._thread = threading.Thread(target=_sniff, daemon=True)
        self._thread.start()

    def stop_sniffer(self):
        self._running = False

    def get_frames(self):
        frames = self._rx_buffer[:]
        self._rx_buffer.clear()
        return frames