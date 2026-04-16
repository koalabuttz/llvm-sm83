// cgb-test.c — exercise CGB MMIO defines + --cgb-only header byte.
//
// This ROM is built with --cgb-only so make-gb-rom.py stamps $0143 = 0xC0.
// At runtime we use VRAM_BANK / WRAM_BANK to touch the CGB-exclusive VBK
// and SVBK registers. sm83sim.py doesn't model VRAM banks, so we only
// confirm the writes don't crash and that we can halt cleanly.
//
// Harness asserts $C100 = 0xA5 (sentinel proving main ran to completion).

#include <gb.h>

void main(void) {
    // Touch CGB-only registers. Without --cgb-only these do nothing on
    // DMG, but sm83sim treats them as plain $FFxx writes.
    VRAM_BANK(1);
    VRAM_BANK(0);
    WRAM_BANK(2);
    WRAM_BANK(1);

    *(volatile unsigned char *)0xC100 = 0xA5;
}
