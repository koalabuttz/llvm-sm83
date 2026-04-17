#!/usr/bin/env python3
"""Run a .gb ROM in mGBA (via libmgba) and check memory assertions.

CLI-compatible with run-harness.py (same --check ADDR=VAL protocol) so
shell runners can swap the sim backend for mGBA without touching the
assertion syntax.

Usage:
    python3 run-mgba-harness.py test.gb --check ADDR=VAL [--check ADDR=VAL ...]

Requires:
    * A companion test.elf in the same directory as test.gb (used to
      locate the `__sm83_exit` symbol — the address the harness waits
      for before reading memory).
    * The `mgba-harness` binary built alongside the rest of the
      toolchain. Default lookup is $BUILD/bin/mgba-harness where $BUILD
      is /data/llvm-sm83/build-sm83-$(uname -m); override with
      $SM83_MGBA_HARNESS.

Unsupported vs run-harness.py: --max-steps (mGBA counts frames instead,
see --max-frames), --rtc-start (RTC latch via mGBA not wired up yet —
the few tests that need it should pin --backend=sim).
"""

import argparse
import os
import platform
import re
import subprocess
import sys


def find_exit_addr(elf_path: str) -> int:
    """Return address of the __sm83_exit symbol in *elf_path*."""
    build = os.environ.get(
        "SM83_BUILD",
        f"/data/llvm-sm83/build-sm83-{platform.machine()}",
    )
    objdump = os.path.join(build, "bin", "llvm-objdump")
    if not os.path.exists(objdump):
        objdump = "llvm-objdump"
    out = subprocess.check_output([objdump, "--syms", elf_path], text=True)
    for line in out.splitlines():
        # Format: "ADDR g [F] .text SIZE NAME" — the F flag is present
        # only for function-type symbols; __sm83_exit is a label.
        m = re.match(r"([0-9a-fA-F]+)\s.*\s__sm83_exit\s*$", line)
        if m:
            return int(m.group(1), 16)
    raise RuntimeError(f"{elf_path}: __sm83_exit symbol not found")


def find_harness() -> str:
    env = os.environ.get("SM83_MGBA_HARNESS")
    if env:
        return env
    build = os.environ.get(
        "SM83_BUILD",
        f"/data/llvm-sm83/build-sm83-{platform.machine()}",
    )
    path = os.path.join(build, "bin", "mgba-harness")
    if os.path.exists(path):
        return path
    raise RuntimeError(
        f"mgba-harness binary not found at {path}; "
        "set SM83_MGBA_HARNESS or build via build-llvm-sm83.sh"
    )


def main() -> int:
    p = argparse.ArgumentParser(description="mGBA-backed SM83 ROM harness")
    p.add_argument("rom", help="Path to .gb ROM file")
    p.add_argument("--check", action="append", default=[],
                   help="Memory assertion: ADDR=VAL (hex)")
    p.add_argument("--max-frames", type=int, default=600,
                   help="Timeout in frames (default: 600)")
    p.add_argument("--exit-addr",
                   help="Override the PC exit address (hex). Default: "
                        "read __sm83_exit from <rom>.elf")
    # Quietly accept run-harness.py flags so shell runners don't need to
    # branch on --backend before/after the flag list.
    p.add_argument("--max-steps", type=int, default=None,
                   help=argparse.SUPPRESS)
    p.add_argument("--rtc-start", default=None, help=argparse.SUPPRESS)
    args = p.parse_args()

    if args.rtc_start is not None:
        print("run-mgba-harness.py: --rtc-start not supported "
              "(pin test to --backend=sim)", file=sys.stderr)
        return 2

    if args.exit_addr:
        exit_addr = int(args.exit_addr, 16)
    else:
        elf = os.path.splitext(args.rom)[0] + ".elf"
        if not os.path.exists(elf):
            print(f"run-mgba-harness.py: need {elf} to locate __sm83_exit "
                  "(or pass --exit-addr)", file=sys.stderr)
            return 2
        exit_addr = find_exit_addr(elf)

    cmd = [
        find_harness(), args.rom,
        "--exit-addr", f"0x{exit_addr:04x}",
        "--max-frames", str(args.max_frames),
    ]
    for c in args.check:
        cmd += ["--check", c]

    return subprocess.call(cmd)


if __name__ == "__main__":
    sys.exit(main())
