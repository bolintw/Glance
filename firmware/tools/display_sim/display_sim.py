#!/usr/bin/env python3
"""PC-side viewer for the SerialDumpDisplay backend (M2).

Reads the console UART, separates plain log lines from frame chunk lines,
and renders each frame as it arrives once all its chunks are in. Protocol
(must match firmware/components/serial_dump_display/serial_dump_display.hpp):
a frame is split across multiple plain-text lines of the form

    GLNC:<seq>:<total>:<base64 chunk>

Concatenating the base64 chunks for seq 0..total-1 in order and decoding
the result gives [TYPE 1B] [LENGTH 4B LE] [PAYLOAD] [CHECKSUM 2B LE].
PAYLOAD is a 1bpp framebuffer, packed MSB-first, row-major. Bit 1 == white,
0 == black. CHECKSUM is a 16-bit wraparound sum of the payload bytes.

Earlier versions of this tool tried a raw binary protocol written directly
to the console UART, and later one giant base64 line per frame. Both ran
into platform issues (stdout's automatic \\n -> \\r\\n translation
corrupting raw payload bytes equal to 0x0A, a fixed-size TX ring buffer
rejecting single writes bigger than itself, and single ~64KB console lines
being unreliable even over the otherwise-solid plain-text log path).
Chunking into small lines sidesteps all of that.

Every rendered frame is also written to disk (--dump-path, default
latest_frame.png next to this script). The GUI is for a human watching
live; the dump file is for an agent -- it has no way to see the Tk
window, but can just read the image file after each flush().
"""

import argparse
import base64
import binascii
import threading
import time
import tkinter as tk
from pathlib import Path
from tkinter import scrolledtext

import serial
from PIL import Image, ImageTk
from serial.tools import list_ports

FRAME_PREFIX = b"GLNC:"
TYPE_FRAME = 0x01
HEADER_LEN = 1 + 4  # type + length
CHECKSUM_LEN = 2
DEFAULT_DUMP_PATH = Path(__file__).parent / "latest_frame.png"


def checksum(payload: bytes) -> int:
    return sum(payload) & 0xFFFF


def unpack_1bpp(payload: bytes, width: int, height: int) -> Image.Image:
    image = Image.new("1", (width, height))
    pixels = image.load()
    bytes_per_row = width // 8
    for y in range(height):
        row_offset = y * bytes_per_row
        for x in range(width):
            byte = payload[row_offset + x // 8]
            bit = (byte >> (7 - (x % 8))) & 1
            pixels[x, y] = 255 if bit else 0
    return image


class DisplaySimApp:
    def __init__(self, root: tk.Tk, port: str, baud: int, width: int, height: int, dump_path: Path):
        self.width = width
        self.height = height
        self.baud = baud
        self.port = port
        self.dump_path = dump_path
        # ESP32-S3's native USB-Serial-JTAG peripheral IS the same chip
        # being reset, so any reset (physical RST or software) drops and
        # re-enumerates the USB connection -- unlike a board with a
        # separate USB-UART bridge chip, where the bridge stays connected
        # across a target reset. Track vid/pid so we can find the port
        # again even if macOS assigns it a new /dev path after
        # re-enumerating.
        self._vid_pid = self._lookup_vid_pid(port)
        self.serial = serial.Serial(port, baud, timeout=0.1)
        self.buffer = bytearray()

        root.title(f"Glance display_sim -- {port}")
        root.configure(bg="white")

        controls = tk.Frame(root, bg="white")
        controls.pack(fill=tk.X, padx=8, pady=(8, 0))
        tk.Button(controls, text="Try software reset", command=self.try_software_reset).pack(
            side=tk.LEFT
        )
        tk.Label(
            controls,
            text="app_main() runs once -- press the board's physical RST after this "
            "window opens to get a frame. Software reset is unreliable on this "
            "board/macOS combo (see README) -- try it, but don't rely on it.",
            wraplength=500,
            justify=tk.LEFT,
            bg="white",
            fg="black",
        ).pack(side=tk.LEFT, padx=8)

        frame = tk.Frame(root, bg="white")
        frame.pack(fill=tk.BOTH, expand=True)

        # Explicit colors everywhere below: on macOS in dark mode, Tk's
        # default widget colors can end up light-text-on-light-background
        # (or similarly low-contrast) since the classic Tk widgets don't
        # reliably follow the system theme -- this was the actual reason
        # nothing appeared to render even though data was arriving and
        # being inserted correctly (visible only as the scrollbar shrinking
        # with no visible text).
        self.image_label = tk.Label(frame, relief=tk.SUNKEN, bg="white")
        self.image_label.pack(side=tk.LEFT, padx=8, pady=8)

        self.log_widget = scrolledtext.ScrolledText(
            frame, width=60, height=30, state=tk.DISABLED, bg="white", fg="black", insertbackground="black"
        )
        self.log_widget.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=8, pady=8)

        self._tk_image = None  # keep a reference so Tk doesn't garbage-collect it
        self._chunks = {}  # seq -> base64 chunk bytes, for the frame currently being assembled
        self._chunks_total = None
        self._stop = False
        self._reader_thread = threading.Thread(target=self._read_loop, daemon=True)
        self._reader_thread.start()

    def try_software_reset(self):
        """Best-effort only -- do not rely on this. Mirrors idf-monitor's
        plain hard-reset pulse (RTS only, matches its Reset.hard()), which
        is correct for restarting the app rather than entering the ROM
        bootloader. On this board, tested over ESP32-S3's native
        USB-Serial-JTAG peripheral via macOS's CDC-ACM driver, this
        sometimes does nothing at all, and a different (bootloader-entry)
        DTR+RTS sequence was found to reliably strand the chip in "waiting
        for download" instead of restarting the app -- so that one is
        deliberately NOT used here. If this button doesn't produce a new
        frame, press the board's physical RST button instead; that always
        works."""
        try:
            self.serial.rts = True  # EN=LOW, chip in reset
            time.sleep(0.1)
            self.serial.rts = False  # EN=HIGH, chip out of reset
        except (serial.SerialException, OSError):
            pass  # the read loop's _reconnect() will notice and recover

    def stop(self):
        self._stop = True
        self.serial.close()

    def _lookup_vid_pid(self, port):
        for p in list_ports.comports():
            if p.device == port:
                return (p.vid, p.pid)
        return None

    def _find_port_to_reconnect(self):
        candidates = list(list_ports.comports())
        # Prefer the exact same path if it came back.
        for p in candidates:
            if p.device == self.port:
                return p.device
        # Otherwise, macOS may have assigned a new /dev path on
        # re-enumeration -- fall back to matching vid/pid.
        if self._vid_pid:
            for p in candidates:
                if (p.vid, p.pid) == self._vid_pid:
                    return p.device
        return None

    def _reconnect(self):
        self._emit_log("[display_sim] connection lost, waiting for the board to come back...\n")
        while not self._stop:
            candidate = self._find_port_to_reconnect()
            if candidate:
                try:
                    self.serial = serial.Serial(candidate, self.baud, timeout=0.1)
                    self.port = candidate
                    self._emit_log(f"[display_sim] reconnected on {candidate}\n")
                    return
                except (serial.SerialException, OSError):
                    pass
            time.sleep(0.3)

    def _read_loop(self):
        while not self._stop:
            try:
                chunk = self.serial.read(4096)
            except (serial.SerialException, OSError):
                self._reconnect()
                continue
            if chunk:
                self.buffer.extend(chunk)
                self._drain_buffer()

    def _drain_buffer(self):
        while True:
            idx = self.buffer.find(b"\n")
            if idx == -1:
                return  # wait for the rest of the line
            line = bytes(self.buffer[:idx]).rstrip(b"\r")
            del self.buffer[: idx + 1]
            self._handle_line(line)

    def _handle_line(self, line: bytes):
        if not line.startswith(FRAME_PREFIX):
            self._emit_log(line.decode("utf-8", errors="replace") + "\n")
            return

        rest = line[len(FRAME_PREFIX) :]
        try:
            seq_str, total_str, chunk = rest.split(b":", 2)
            seq, total = int(seq_str), int(total_str)
        except ValueError:
            self._emit_log(f"[display_sim] malformed frame chunk line: {rest[:40]!r}\n")
            return

        if seq == 0:
            self._chunks = {}
            self._chunks_total = total
        if total != self._chunks_total:
            # A new frame started before the previous one finished (e.g. a
            # chunk got dropped) -- restart tracking from this chunk.
            self._chunks = {}
            self._chunks_total = total
        self._chunks[seq] = chunk

        if len(self._chunks) != self._chunks_total:
            return  # still waiting on more chunks
        b64 = b"".join(self._chunks[i] for i in range(self._chunks_total))
        self._chunks = {}
        self._chunks_total = None

        try:
            packet = base64.b64decode(b64)
        except binascii.Error as e:
            self._emit_log(f"[display_sim] bad base64 after reassembling frame: {e}\n")
            return

        if len(packet) < HEADER_LEN + CHECKSUM_LEN:
            self._emit_log("[display_sim] frame packet too short\n")
            return

        pkt_type = packet[0]
        length = int.from_bytes(packet[1:5], "little")
        expected_total = HEADER_LEN + length + CHECKSUM_LEN
        if len(packet) != expected_total:
            self._emit_log(
                f"[display_sim] frame length mismatch: header says {length}, "
                f"decoded packet is {len(packet)} bytes\n"
            )
            return

        payload = packet[HEADER_LEN : HEADER_LEN + length]
        received_checksum = int.from_bytes(packet[HEADER_LEN + length : expected_total], "little")

        if pkt_type != TYPE_FRAME:
            self._emit_log(f"[display_sim] unknown packet type 0x{pkt_type:02x}\n")
            return
        if checksum(payload) != received_checksum:
            self._emit_log("[display_sim] checksum mismatch, dropping frame\n")
            return
        if length != self.width * self.height // 8:
            self._emit_log(
                f"[display_sim] unexpected payload size {length}, "
                f"expected {self.width * self.height // 8}\n"
            )
            return
        self._render_frame(payload)

    def _emit_log(self, text: str):
        self.log_widget.after(0, self._append_log_text, text)

    def _append_log_text(self, text):
        self.log_widget.configure(state=tk.NORMAL)
        self.log_widget.insert(tk.END, text)
        self.log_widget.see(tk.END)
        self.log_widget.configure(state=tk.DISABLED)

    def _render_frame(self, payload: bytes):
        image = unpack_1bpp(payload, self.width, self.height)
        image.save(self.dump_path)
        self._emit_log(f"[display_sim] frame written to {self.dump_path}\n")
        self.image_label.after(0, self._show_image, image)

    def _show_image(self, image: Image.Image):
        self._tk_image = ImageTk.PhotoImage(image)
        self.image_label.configure(image=self._tk_image)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="Serial port, e.g. /dev/cu.usbmodemXXXX")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--width", type=int, default=800)
    parser.add_argument("--height", type=int, default=480)
    parser.add_argument(
        "--dump-path",
        type=Path,
        default=DEFAULT_DUMP_PATH,
        help="Where to write each rendered frame as a PNG, overwritten on every flush() "
        "(default: %(default)s). Lets an agent without eyes on the GUI inspect the "
        "current frame by just reading this file.",
    )
    args = parser.parse_args()

    root = tk.Tk()
    app = DisplaySimApp(root, args.port, args.baud, args.width, args.height, args.dump_path)
    root.protocol("WM_DELETE_WINDOW", lambda: (app.stop(), root.destroy()))
    root.mainloop()


if __name__ == "__main__":
    main()
