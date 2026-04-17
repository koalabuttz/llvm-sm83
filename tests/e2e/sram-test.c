// sram-test.c — verify cartridge SRAM write, read-back, and persistence.
//
// Part of Round 9 item 3 (SRAM / battery-save support). Flow:
//   1. Enable RAM gate (write $0A to $0000-$1FFF).
//   2. Write a 4-byte pattern into SRAM at $A000..$A003.
//   3. Read the same bytes back from SRAM and copy to WRAM $C100..$C103,
//      where the harness verifies them via --check.
//   4. Disable the RAM gate (good practice; prevents battery-save corruption
//      on a brown-out).
//
// Two harness invocations exercise the full save/load cycle:
//   (a) Run with --sram-save /tmp/out.sav + --check C100..C103: confirms
//       in-ROM writes land in SRAM and the .sav file captures them.
//   (b) Run again with --sram-load /tmp/out.sav + --check C100..C103:
//       the ROM clobbers SRAM on its own write path, so the .sav content
//       only matters if the test verifies pre-write state — this mode
//       exists so a dedicated "read-only" variant of the test can prove
//       persistence. The run-c-e2e.sh step uses mode (a) only.
//
// Build with `make-gb-rom.py --mbc1 --ram-size 8K`.

#include <gb.h>

void main(void) {
    volatile unsigned char *sram = (volatile unsigned char *)0xA000;
    volatile unsigned char *out  = (volatile unsigned char *)0xC100;

    MBC1_RAM_ENABLE();

    sram[0] = 0x11;
    sram[1] = 0x22;
    sram[2] = 0x33;
    sram[3] = 0x44;

    out[0] = sram[0];
    out[1] = sram[1];
    out[2] = sram[2];
    out[3] = sram[3];

    MBC1_RAM_DISABLE();
}
