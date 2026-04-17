/*
 * state-machine.c — codegen benchmark program for SM83.
 *
 * Computes a deterministic 16-bit checksum over a 128-byte in-memory
 * buffer using a small state machine: each byte advances an LFSR-style
 * mixer, and the final checksum is written to address 0xC100.
 *
 * Structure chosen to stress typical SM83 codegen:
 *   - Array indexed by i8 (tests (HL) addressing + inc/dec).
 *   - Mix of 8-bit and 16-bit ops (tests i16 lowering: shift, xor, add).
 *   - One inner loop (tests branch back-edge + countdown).
 *   - Value read/written at a fixed MMIO-style address (tests ldh vs ld nn).
 *
 * Kept free of any gb.h / MMIO / volatile construct beyond the final
 * store, so the same file compiles under both LLVM-SM83 and GBDK (sdcc).
 * The Makefile drives the comparison.
 */

#include <stdint.h>

static uint8_t buf[128];

static uint16_t mix(uint16_t s, uint8_t b) {
    s ^= (uint16_t)b;
    /* 16-bit rotate-left-3 via shifts */
    s = (uint16_t)((s << 3) | (s >> 13));
    s += 0x8F3B;
    return s;
}

void run(void) {
    /* Fill the buffer with a pattern using pure 8-bit arithmetic. */
    for (uint8_t i = 0; i < 128; i++) {
        buf[i] = (uint8_t)(i * 37u + 11u);
    }

    uint16_t state = 0x1234;
    for (uint8_t i = 0; i < 128; i++) {
        state = mix(state, buf[i]);
    }

    /* Store the final checksum at a fixed WRAM address so a test harness
     * (sm83sim's --check) can read it back without the user code needing
     * any target-specific headers. */
    *(volatile uint8_t *)0xC100 = (uint8_t)(state & 0xFF);
    *(volatile uint8_t *)0xC101 = (uint8_t)(state >> 8);
}

void main(void) {
    run();
}
