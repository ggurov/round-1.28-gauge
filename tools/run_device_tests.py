#!/usr/bin/env python3
"""
Collect the on-target Unity results over the serial console.

The ESP32-S3-LCD-1.28 cannot be reset by the host (no DTR/RTS to EN/GPIO0), so
after flashing the test app the chip may need a physical RESET press before it
runs.  This script waits patiently and says so rather than failing immediately.

It looks for the marker test_main.c prints last:

    TESTS_COMPLETE total=<n> failures=<n> ignored=<n>

Run:  python tools/run_device_tests.py --port COM6
"""

from __future__ import annotations

import argparse
import re
import sys
import time

try:
    import serial
except ImportError:
    print("pyserial is missing.  python -m pip install -r tools/requirements.txt",
          file=sys.stderr)
    raise SystemExit(2)

MARKER = re.compile(r"TESTS_COMPLETE total=(\d+) failures=(\d+) ignored=(\d+)")
HINT_AFTER = 12.0      # seconds before we tell the user to press RESET
GIVE_UP_AFTER = 150.0  # seconds before we call it a failure


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="COM6")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--timeout", type=float, default=GIVE_UP_AFTER)
    args = ap.parse_args()

    print(f"listening on {args.port} @ {args.baud} for the test summary ...")
    try:
        port = serial.Serial(args.port, args.baud, timeout=0.2)
    except Exception as exc:  # noqa: BLE001
        print(f"could not open {args.port}: {exc}", file=sys.stderr)
        return 2

    start = time.time()
    hinted = False
    saw_any = False
    buf = bytearray()
    result = None

    try:
        port.dtr = False
        port.rts = False
    except Exception:  # noqa: BLE001
        pass

    try:
        while True:
            elapsed = time.time() - start
            if elapsed > args.timeout:
                break

            chunk = port.read(4096)
            if chunk:
                saw_any = True
                buf += chunk
                sys.stdout.write(chunk.decode("utf-8", errors="replace"))
                sys.stdout.flush()
                text = buf.decode("utf-8", errors="replace")
                m = MARKER.search(text)
                if m:
                    result = (int(m.group(1)), int(m.group(2)), int(m.group(3)))
                    break
            else:
                if not hinted and elapsed > HINT_AFTER:
                    hinted = True
                    print("\n--- no output yet -------------------------------------")
                    print("If the board is sitting in the ROM bootloader after")
                    print("flashing, press RESET once so the test app starts.")
                    print("-------------------------------------------------------\n")
                    sys.stdout.flush()
                time.sleep(0.05)
    except KeyboardInterrupt:
        print("\ninterrupted", file=sys.stderr)
    finally:
        port.close()

    print()
    if result is None:
        print("no TESTS_COMPLETE marker seen", file=sys.stderr)
        if not saw_any:
            print("(no serial output at all - is the port right, and did the app start?)",
                  file=sys.stderr)
        return 1

    total, failures, ignored = result
    print(f"device tests: {total} run, {total - failures} passed, {failures} failed, "
          f"{ignored} ignored")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
