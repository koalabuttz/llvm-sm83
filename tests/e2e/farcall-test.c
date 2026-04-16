// farcall-test.c — exercise compiler-emitted inline far calls end-to-end.
//
// Two callees live in switchable ROM banks: one in .romx.bank2 (within
// MBC1's 5-bit range) and one in .romx.bank30 (past the old 15-bank
// linker-script cap; requires Round 7's 127-bank expansion and MBC3
// addressing). main() calls each in turn, stores the results to WRAM,
// and halts. The harness asserts the expected results at $C100/$C101.
//
// The compiler emits, at each call site, an inline
//   ld a, N ; ld [hl], a  (or ld [$2000], a)
//   call callee
//   ld a, 1 ; ld [hl], a  (restore default bank)
// sequence. Without this, the CALL would land in whatever bank happens
// to be mapped at $4000 (usually bank 1 by crt0), producing wrong code.

#include <gb.h>

__attribute__((section(".romx.bank2")))
unsigned char add_fifty(unsigned char x) {
    return x + 50;
}

__attribute__((section(".romx.bank30")))
unsigned char add_hundred(unsigned char x) {
    return x + 100;
}

void main(void) {
    volatile unsigned char *out = (volatile unsigned char *)0xC100;
    // Input 5 + 50 = 55 (0x37)
    out[0] = add_fifty(5);
    // Input 5 + 100 = 105 (0x69)
    out[1] = add_hundred(5);
}
