#!/usr/bin/env python3
"""Headless counterpart to display_sim.py: reset the board, read the console
until the run finishes (the next wake-up is scheduled), and save the last frame the SerialDumpDisplay backend
sent as a PNG. No GUI and no button presses, so a script (or an agent that
can't see the Tk window) can drive a full render cycle on its own.

    source firmware/env.sh            # esptool comes from the ESP-IDF env
    .venv/bin/python capture.py /dev/cu.usbmodem3101 [--out frame.png] [--seconds 60] [--no-reset]

--no-reset just listens, for when the board has to be reset by hand (esptool
sometimes can't reach it; tap RST once this is running).

--command "glance press short" sends a line to a development build with
GLANCE_DEV_SERIAL_COMMANDS instead of resetting, then captures the run it
starts (see main.cpp for the commands).

Log lines are echoed (frame chunks aren't). Exits non-zero if no complete,
checksum-valid frame arrived.
"""
import argparse
import base64
import os
import subprocess
import sys
import time

import serial

from display_sim import CHECKSUM_LEN, FRAME_PREFIX, HEADER_LEN, checksum, unpack_frame


def reset_board(port):
    # esptool's reset is the one that reliably restarts the app on this
    # board (see README); flash-id is just a harmless command to hang it on.
    # Must run with the ESP-IDF Python, not this venv's.
    idf_env = os.environ.get("IDF_PYTHON_ENV_PATH")
    if not idf_env:
        sys.exit("IDF_PYTHON_ENV_PATH not set -- source firmware/env.sh first")
    result = subprocess.run(
        [os.path.join(idf_env, "bin", "python"), "-m", "esptool", "--chip", "esp32s3", "-p", port,
         "--after", "hard-reset", "flash-id"],
        capture_output=True, text=True, timeout=30,
    )
    if result.returncode != 0:
        sys.exit(f"esptool reset failed:\n{result.stdout}{result.stderr}")


def decode_frame(b64, width, height):
    packet = base64.b64decode(b64)
    if len(packet) < HEADER_LEN + CHECKSUM_LEN:
        return None
    length = int.from_bytes(packet[1:5], "little")
    payload = packet[HEADER_LEN:HEADER_LEN + length]
    received = int.from_bytes(packet[HEADER_LEN + length:HEADER_LEN + length + CHECKSUM_LEN], "little")
    if len(payload) != length or checksum(payload) != received:
        return None
    return unpack_frame(packet[0], payload, width, height)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("port")
    parser.add_argument("--out", default="latest_frame.png")
    parser.add_argument("--seconds", type=float, default=60, help="give up after this long")
    parser.add_argument("--width", type=int, default=800)
    parser.add_argument("--height", type=int, default=480)
    parser.add_argument("--no-reset", action="store_true", help="don't reset the board, just listen")
    parser.add_argument("--command", help="send this serial command instead of resetting")
    args = parser.parse_args()

    if args.command:
        with serial.Serial(args.port, 115200, timeout=0.2) as port:
            port.write(args.command.encode() + b"\n")
            port.flush()
            time.sleep(0.5)  # let the device read it before the port closes
    elif not args.no_reset:
        reset_board(args.port)
        time.sleep(0.3)  # USB-Serial-JTAG re-enumerates on reset
    ser = None

    buf = bytearray()
    chunks, total = {}, None
    frames = 0
    deadline = time.time() + args.seconds
    done = False
    while time.time() < deadline and not done:
        # (Re)open as needed: a hand reset makes the port vanish and return.
        try:
            if ser is None:
                ser = serial.Serial(args.port, 115200, timeout=0.2)
            buf.extend(ser.read(4096))
        except (serial.SerialException, OSError):
            ser = None
            time.sleep(0.2)
            continue
        while (idx := buf.find(b"\n")) != -1:
            line = bytes(buf[:idx]).rstrip(b"\r")
            del buf[: idx + 1]
            if not line.startswith(FRAME_PREFIX):
                text = line.decode("utf-8", errors="replace")
                print(text)
                # The run is over once the next wake-up is scheduled, before
                # deep sleep or, in development builds, waiting awake (app_main
                # no longer returns). Exiting matters: a lingering reader on
                # the port makes the next esptool run fail to connect.
                done |= any(marker in text for marker in
                            ("Returned from app_main()", "main: next refresh", "main: next retry"))
                continue
            try:
                seq_s, total_s, chunk = line[len(FRAME_PREFIX):].split(b":", 2)
                seq, n = int(seq_s), int(total_s)
            except ValueError:
                continue
            if seq == 0 or n != total:
                chunks, total = {}, n
            chunks[seq] = chunk
            if len(chunks) == total:
                image = decode_frame(b"".join(chunks[i] for i in range(total)), args.width, args.height)
                chunks, total = {}, None
                if image is not None:
                    image.save(args.out)
                    frames += 1
                    print(f"[capture] frame {frames} saved to {args.out}")
    if ser is not None:
        ser.close()
    if frames == 0:
        sys.exit("[capture] no complete frame received")


if __name__ == "__main__":
    main()
