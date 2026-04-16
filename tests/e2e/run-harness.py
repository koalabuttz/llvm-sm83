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
    args = parser.parse_args()

    with open(args.rom, 'rb') as f:
        rom_data = f.read()

    sim = SM83Sim(rom_data)
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

    if failures:
        print(f"\n{failures} check(s) failed")
        sys.exit(1)
    elif args.check:
        print(f"\nAll {len(args.check)} check(s) passed")


if __name__ == '__main__':
    main()
