/*
 * c-vblank-test.c — End-to-end VBlank interrupt integration test.
 *
 * Exercises:
 *   1. SM83_ISR(vblank) — auto-placement into .isr.vblank via the
 *      interrupt("vblank") clang attribute (Round 4 Item 1).
 *   2. __attribute__((section(".hram"))) — HRAM placement via crt0's
 *      ROM→HRAM copy pass (Round 4 Item 3).
 *   3. VBlank dispatch through crt0 + linker-script interrupt stubs.
 *
 * Expected result (checked by run-harness.py):
 *   WRAM[$C100] = 3    — VBlank handler fired three times
 *   WRAM[$C101] = 0xA5 — sentinel written after main loop exits
 *   HRAM[$FF80]  < FF  — hram_marker() executed (arbitrary non-FF value)
 */

#include "../../llvm/lib/Target/SM83/runtime/gb.h"

/* Counter incremented by the VBlank ISR. WRAM-resident, so crt0 zero-inits. */
static volatile unsigned char vblank_count;

/* Function that MUST live in HRAM. Writes a sentinel so the test can
 * confirm the ROM→HRAM copy pass actually ran. */
static void GB_HRAM hram_marker(void) {
    *(volatile unsigned char *)0xFF80 = 0x42;
}

/* VBlank handler. Placed automatically at $0040 by the interrupt("vblank")
 * attribute. */
SM83_ISR(vblank) {
    vblank_count++;
}

void main(void) {
    /* hram_marker lives in HRAM (section ".hram"). Calling it verifies:
     *   (a) the linker placed the function address in $FF80-$FFFE
     *   (b) crt0 copied the code from ROM to HRAM at boot
     *   (c) execution from HRAM works (it must, for OAM DMA). */
    hram_marker();

    /* Enable VBlank interrupt and unmask IME. */
    REG_IE = INT_VBLANK;
    __asm__ volatile ("ei");

    /* Wait for three VBlanks via HALT. sm83sim raises IF.vblank every
     * vblank_period steps; each HALT wakes, dispatches to our ISR, and
     * RETI returns here to re-check the loop condition. */
    while (vblank_count < 3) {
        __asm__ volatile ("halt");
    }

    /* Commit results to WRAM for the test harness to inspect. */
    *(volatile unsigned char *)0xC100 = vblank_count;
    *(volatile unsigned char *)0xC101 = 0xA5;

    /* main() returns → crt0 disables interrupts and halts permanently. */
}
