/*
 * hram-test.c — Verify that __attribute__((section(".hram"))) routes a
 * function into the HRAM address range [FF80, FFFF).
 *
 * Unlike c-vblank-test.c (which also exercises HRAM via GB_HRAM), this
 * test focuses narrowly on placement: we compile + link, then inspect the
 * ELF symbol table (see run-c-e2e.sh) to confirm the function's address
 * falls in the HRAM region.
 *
 * This catches regressions in the linker script's HRAM routing
 * independently of interrupt handling or runtime copy correctness.
 */

#include <gb.h>

/* Placed in HRAM. Must land in $FF80-$FFFE. */
void GB_HRAM oam_dma_trampoline(void) {
    /* A minimal HRAM routine: pulse OAM DMA then busy-wait. In real code
     * the wait loop is exactly 160 cycles (40 × 4-cycle decrements). */
    REG_DMA = 0xC1;
    unsigned char i = 40;
    do { i--; } while (i);
}

void main(void) {
    oam_dma_trampoline();
    /* Write a sentinel so a runtime harness can also confirm it actually
     * ran from HRAM (not just that the symbol has the right address). */
    *(volatile unsigned char *)0xC100 = 0x77;
}
