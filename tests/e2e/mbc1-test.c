// mbc1-test.c — exercise MBC1 bank switching end-to-end.
//
// A 4-byte payload is placed in ROM bank 2 via the .romx.bank2 section.
// At runtime we write 2 to $2000 (BANK_SWITCH) to page bank 2 in at
// $4000-$7FFF, copy the payload to WRAM, then restore bank 1.
//
// The array is `volatile` and accessed through a pointer cast the
// compiler can't see through: without these, `-O1` const-folds the
// known hex values directly into main() and the .romx.bank2 section
// gets dropped — defeating the whole point of the test.
//
// Harness asserts $C100-$C103 == DE AD BE EF and the object file
// still contains a .romx.bank2 section.

#include "../../llvm/lib/Target/SM83/runtime/gb.h"

__attribute__((section(".romx.bank2"), used))
const volatile unsigned char bank2_payload[4] = { 0xDE, 0xAD, 0xBE, 0xEF };

void main(void) {
    volatile unsigned char *dst = (volatile unsigned char *)0xC100;
    const volatile unsigned char *src = bank2_payload;

    BANK_SWITCH(2);
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
    BANK_SWITCH(1);
}
