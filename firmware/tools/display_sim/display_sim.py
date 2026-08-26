#!/usr/bin/env python3
"""PC-side viewer for the SerialDumpDisplay backend (M2).

Reads the console UART, separates plain log lines from binary frame
packets, and renders each frame as it arrives. Protocol (must match
firmware/components/serial_dump_display/serial_dump_display.hpp):

    [MAGIC "GLNC" 4B] [TYPE 1B] [LENGTH 4B LE] [PAYLOAD] [CHECKSUM 2B LE]

PAYLOAD is a 1bpp framebuffer, packed MSB-first, row-major. Bit 1 == white,
0 == black. CHECKSUM is a 16-bit wraparound sum of the payload bytes.
Anything that isn't part of a packet is assumed to be normal log text.
"""

import argparse
import threading
import tkinter as tk
from tkinter import scrolledtext

import serial
from PIL import Image, ImageTk

MAGIC = b"GLNC"
TYPE_FRAME = 0x01
HEADER_LEN = len(MAGIC) + 1 + 4  # magic + type + length
CHECKSUM_LEN = 2


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
    def __init__(self, root: tk.Tk, port: str, baud: int, width: int, height: int):
        self.width = width
        self.height = height
        self.serial = serial.Serial(port, baud, timeout=0.1)
        self.buffer = bytearray()

        root.title(f"Glance display_sim -- {port}")
        frame = tk.Frame(root)
        frame.pack(fill=tk.BOTH, expand=True)

        self.image_label = tk.Label(frame, relief=tk.SUNKEN)
        self.image_label.pack(side=tk.LEFT, padx=8, pady=8)

        self.log_widget = scrolledtext.ScrolledText(frame, width=60, height=30, state=tk.DISABLED)
        self.log_widget.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=8, pady=8)

        self._tk_image = None  # keep a reference so Tk doesn't garbage-collect it
        self._stop = False
        self._reader_thread = threading.Thread(target=self._read_loop, daemon=True)
        self._reader_thread.start()

    def stop(self):
        self._stop = True
        self.serial.close()

    def _read_loop(self):
        while not self._stop:
            chunk = self.serial.read(4096)
            if chunk:
                self.buffer.extend(chunk)
                self._drain_buffer()

    def _drain_buffer(self):
        while True:
            idx = self.buffer.find(MAGIC)
            if idx == -1:
                # No magic in the buffer at all -- flush everything as log,
                # but keep the last few bytes in case a magic sequence is
                # split across two reads.
                if len(self.buffer) > len(MAGIC):
                    self._emit_log(bytes(self.buffer[: -len(MAGIC)]))
                    del self.buffer[: -len(MAGIC)]
                return

            if idx > 0:
                self._emit_log(bytes(self.buffer[:idx]))
                del self.buffer[:idx]

            if len(self.buffer) < HEADER_LEN:
                return  # wait for more data

            pkt_type = self.buffer[4]
            length = int.from_bytes(self.buffer[5:9], "little")
            total_len = HEADER_LEN + length + CHECKSUM_LEN
            if len(self.buffer) < total_len:
                return  # wait for the rest of this packet

            payload = bytes(self.buffer[HEADER_LEN : HEADER_LEN + length])
            received_checksum = int.from_bytes(
                self.buffer[HEADER_LEN + length : total_len], "little"
            )
            del self.buffer[:total_len]

            if pkt_type != TYPE_FRAME:
                self._emit_log(f"[display_sim] unknown packet type 0x{pkt_type:02x}\n")
                continue
            if checksum(payload) != received_checksum:
                self._emit_log("[display_sim] checksum mismatch, dropping frame\n")
                continue
            if length != self.width * self.height // 8:
                self._emit_log(
                    f"[display_sim] unexpected payload size {length}, "
                    f"expected {self.width * self.height // 8}\n"
                )
                continue
            self._render_frame(payload)

    def _emit_log(self, data):
        text = data.decode("utf-8", errors="replace") if isinstance(data, bytes) else data
        self.log_widget.after(0, self._append_log_text, text)

    def _append_log_text(self, text):
        self.log_widget.configure(state=tk.NORMAL)
        self.log_widget.insert(tk.END, text)
        self.log_widget.see(tk.END)
        self.log_widget.configure(state=tk.DISABLED)

    def _render_frame(self, payload: bytes):
        image = unpack_1bpp(payload, self.width, self.height)
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
    args = parser.parse_args()

    root = tk.Tk()
    app = DisplaySimApp(root, args.port, args.baud, args.width, args.height)
    root.protocol("WM_DELETE_WINDOW", lambda: (app.stop(), root.destroy()))
    root.mainloop()


if __name__ == "__main__":
    main()
