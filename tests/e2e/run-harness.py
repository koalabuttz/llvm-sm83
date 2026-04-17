#!/usr/bin/env python3
"""Run a .gb ROM in the SM83 simulator and check memory assertions.

Usage:
    python3 run-harness.py test.gb --check ADDR=VAL [--check ADDR=VAL ...]
    python3 run-harness.py test.gb --check 8000=AA --check FF40=81

ADDR and VAL are hex. Exit 0 if all checks pass, 1 otherwise.
"""

import argparse
import sys
from sm83sim import SM83Sim


def main():
    parser = argparse.ArgumentParser(description="SM83 ROM test harness")
    parser.add_argument("rom", help="Path to .gb ROM file")
    parser.add_argument("--check", action="append", default=[],
                        help="Memory assertion: ADDR=VAL (hex)")
    parser.add_argument("--max-steps", type=int, default=500000,
                        help="Maximum CPU steps (default: 500000)")
    parser.add_argument("--rtc-start", default=None,
                        help="Initial MBC3 RTC time HH:MM:SS or D:HH:MM:SS "
                             "(MBC3 carts only; default: all zero)")
    parser.add_argument("--sram-load", metavar="PATH", default=None,
                        help="Preload cartridge SRAM from a .sav file "
                             "before running (battery-save emulation).")
    parser.add_argument("--sram-save", metavar="PATH", default=None,
                        help="Dump cartridge SRAM to a .sav file after the "
                             "ROM halts (battery-save emulation).")
    args = parser.parse_args()

    with open(args.rom, 'rb') as f:
        rom_data = f.read()

    sim = SM83Sim(rom_data)

    if args.sram_load:
        with open(args.sram_load, 'rb') as f:
            saved = f.read()
        if len(saved) != len(sim.cart_ram):
            print(f"WARNING: --sram-load size ({len(saved)} B) != "
                  f"cart SRAM size ({len(sim.cart_ram)} B); loading "
                  f"min of the two", file=sys.stderr)
        n = min(len(saved), len(sim.cart_ram))
        sim.cart_ram[:n] = saved[:n]

    if args.rtc_start:
        parts = args.rtc_start.split(':')
        if len(parts) == 3:
            d, (h, m, s) = 0, (int(x) for x in parts)
        elif len(parts) == 4:
            d, h, m, s = (int(x) for x in parts)
        else:
            print(f"ERROR: --rtc-start must be HH:MM:SS or D:HH:MM:SS",
                  file=sys.stderr)
            sys.exit(2)
        sim.rtc_live_s = s & 0xFF
        sim.rtc_live_m = m & 0xFF
        sim.rtc_live_h = h & 0xFF
        sim.rtc_live_dl = d & 0xFF
        sim.rtc_live_dh = (d >> 8) & 0x01

    halted = sim.run(max_steps=args.max_steps)

    if not halted:
        print(f"FAIL: ROM did not halt within {args.max_steps} steps "
              f"(PC=${sim.pc:04X})", file=sys.stderr)
        sys.exit(1)

    print(f"ROM halted after {sim.steps} steps at PC=${sim.pc:04X}")

    failures = 0
    for check in args.check:
        addr_str, val_str = check.split('=')
        addr = int(addr_str, 16)
        expected = int(val_str, 16)
        actual = sim.mem[addr]
        if actual == expected:
            print(f"  PASS: [${addr:04X}] = ${actual:02X}")
        else:
            print(f"  FAIL: [${addr:04X}] = ${actual:02X} (expected ${expected:02X})")
            failures += 1

    if args.sram_save:
        with open(args.sram_save, 'wb') as f:
            f.write(bytes(sim.cart_ram))

    if failures:
        print(f"\n{failures} check(s) failed")
        sys.exit(1)
    elif args.check:
        print(f"\nAll {len(args.check)} check(s) passed")


if __name__ == '__main__':
    main()
