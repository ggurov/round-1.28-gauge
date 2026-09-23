#!/usr/bin/env python3
"""
Flash an image in small chunks, all within one esptool session.

This board hangs on any single esptool write larger than about 16 KB:

    16 KB -> OK
    32 KB -> "No more data to read from the serial port"
    64 KB -> same
   128 KB -> cannot even reconnect

It also drops out of the ROM bootloader after each *successful* write, so
issuing one esptool call per chunk does not work either - only the first
chunk lands and the second cannot reconnect.

The fix is to hand esptool every chunk as a separate `write-flash` region in a
single invocation.  esptool then performs many small writes inside one
connection, which never trips the limit and never needs to reconnect.

The chip still has to be sitting in the ROM bootloader to start with:
    hold BOOT, tap RESET, release BOOT
and because the whole thing is one session, that is the only time you touch
the board.

Usage:
    python tools/flash_chunked.py --port COM6 0x20000 firmware/build/round_gauge.bin
    python tools/flash_chunked.py --port COM6 0x8000 part.bin 0x20000 app.bin
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile

CHUNK_KB = 16
FLASH_ARGS = ["--flash-mode", "dio", "--flash-freq", "40m", "--flash-size", "16MB"]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="COM6")
    ap.add_argument("--baud", type=int, default=460800)
    ap.add_argument("--chunk-kb", type=int, default=CHUNK_KB)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("pairs", nargs="+",
                    help="alternating offset and file, e.g. 0x20000 app.bin")
    args = ap.parse_args()

    if len(args.pairs) % 2 != 0:
        print("need alternating offset/file arguments", file=sys.stderr)
        return 2

    chunk = args.chunk_kb * 1024
    tmpdir = tempfile.mkdtemp(prefix="flashchunk")
    regions: list[tuple[int, str]] = []
    total_bytes = 0

    for i in range(0, len(args.pairs), 2):
        offset = int(args.pairs[i], 0)
        path = args.pairs[i + 1]
        if not os.path.exists(path):
            print(f"missing {path}", file=sys.stderr)
            return 2

        size = os.path.getsize(path)
        total_bytes += size
        n = (size + chunk - 1) // chunk
        print(f"{path}: {size} bytes -> {hex(offset)}, {n} chunks")

        with open(path, "rb") as fh:
            for c in range(n):
                data = fh.read(chunk)
                cpath = os.path.join(tmpdir, f"{i:02d}_{c:04d}.bin")
                with open(cpath, "wb") as cf:
                    cf.write(data)
                regions.append((offset + c * chunk, cpath))

    print(f"\ntotal: {total_bytes} bytes in {len(regions)} chunks of {args.chunk_kb} KB")

    cmd = [
        sys.executable, "-m", "esptool", "--chip", "esp32s3",
        "-p", args.port, "-b", str(args.baud),
        "--before", "no-reset", "--after", "no-reset",
        "write-flash", *FLASH_ARGS,
    ]
    for offset, path in regions:
        cmd += [hex(offset), path]

    if args.dry_run:
        print("dry run, command length:", len(" ".join(cmd)))
        return 0

    print(f"\nflashing in one session ({len(cmd)} argv entries) ...\n", flush=True)
    proc = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
    out = (proc.stdout or "") + (proc.stderr or "")

    verified = out.count("Hash of data verified")
    for line in out.splitlines():
        s = line.strip()
        if any(k in s for k in ("A fatal error", "Failed to connect", "Invalid head",
                                "No more data", "stopped responding", "Serial data stream")):
            print("  !", s)
        elif s.startswith("Writing at") or s.startswith("Wrote") or "Hash of data" in s:
            pass  # too noisy

    print(f"\n{verified}/{len(regions)} chunk writes verified")

    ok = proc.returncode == 0 and verified == len(regions)
    if not ok:
        print("FAILED", file=sys.stderr)
    else:
        print("all chunks flashed and verified")

    shutil.rmtree(tmpdir, ignore_errors=True)
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
