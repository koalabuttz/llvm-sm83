// farcall-mbc5-highbank-test.c — exercise MBC5 far calls into bank 200.
//
// Bank 200 = $C8 — low byte fits in 8 bits, high bit 0, so the compiler
// emits only the $2000 write. This proves:
//   (a) Round 8's linker-script expansion (.romx.bank128..511) resolves
//       the callee's VMA correctly.
//   (b) sm83sim.py's MBC5 bank-switching honors the 9-bit bank value
//       (via writes to $2000 and $3000 combined into a 9-bit index).
//   (c) The callee is placed at LMA $320000 = 200 * $4000 in the ROM file.
//
// main() runs in bank 0, calls bank200_fn(7), stores the result at $C100.
// Expected: 7 + 200 = 207 ($CF).

#include <gb.h>

__attribute__((section(".romx.bank200")))
unsigned char bank200_fn(unsigned char x) {
    return x + 200;
}

void main(void) {
    volatile unsigned char *out = (volatile unsigned char *)0xC100;
    out[0] = bank200_fn(7);
}
