#!/usr/bin/env python3
"""
Identify the Waveshare ESP32-S3-LCD-1.28 on this machine.

The board cannot be auto-reset into the ROM bootloader (its CH343P DTR/RTS
lines are not wired to EN/GPIO0), so this script does not try: it scans the
serial ports, connects with --before no-reset, and reports whatever answers.

The board must already be in download mode first:
  * from the running firmware: type `bootloader` at the gauge> prompt
  * from cold:                 hold BOOT, tap RESET, release BOOT

Run:  python tools/probe.py
"""

from __future__ import annotations

import subprocess
import sys

try:
    from serial.tools import list_ports
except ImportError:
    print("pyserial is missing.  python -m pip install -r tools/requirements.txt",
          file=sys.stderr)
    raise SystemExit(2)

# WCH bridges: (vid, pid, label)
KNOWN_BRIDGES = {
    (0x1A86, 0x55D3): "CH343",
    (0x1A86, 0x7523): "CH340",
    (0x1A86, 0x55D4): "CH343P",
    (0x10C4, 0xEA60): "CP2102",
    (0x0403, 0x6001): "FT232R",
}

VID_TOKENS = {"VID_1A86", "VID_303A", "VID_10C4", "VID_0403"}


def candidate_ports() -> list[tuple[str, str]]:
    found: list[tuple[str, str]] = []
    for p in list_ports.comports():
        label = KNOWN_BRIDGES.get((p.vid or 0, p.pid or 0))
        if label is None:
            hwid = (p.hwid or "").upper()
            if not any(tok in hwid for tok in VID_TOKENS):
                continue
            label = "usb-serial"
        found.append((p.device, f"{label}  {p.description or ''}".strip()))
    return found


def probe(port: str) -> bool:
    print(f"\n--- probing {port} ---")
    cmd = [sys.executable, "-m", "esptool", "--port", port,
           "--before", "no-reset", "--after", "no-reset", "flash-id"]
    try:
        rc = subprocess.call(cmd)
    except KeyboardInterrupt:
        return False
    return rc == 0


def main() -> int:
    ports = candidate_ports()
    if not ports:
        print("No USB serial adapters found. Is the board plugged in?", file=sys.stderr)
        return 1

    print("USB serial adapters:")
    for dev, desc in ports:
        print(f"  {dev:<8} {desc}")

    print("\nIf nothing responds below, put the board into download mode first:")
    print("  from the app :  type `bootloader` at the gauge> prompt")
    print("  from cold    :  hold BOOT, tap RESET, release BOOT")

    for dev, _ in ports:
        if probe(dev):
            print(f"\nOK: {dev} is in the ROM bootloader and ready to flash.")
            print(f"    tools\\idf.bat -p {dev} flash monitor")
            return 0

    print("\nNo ESP32 responded. The board is probably not in download mode.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
