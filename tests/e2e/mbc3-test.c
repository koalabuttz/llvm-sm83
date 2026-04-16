// mbc3-test.c — exercise MBC3 bank switching end-to-end.
//
// Same shape as mbc1-test.c but uses ROM bank 5 (bank index ≥ 5 proves the
// sim's 7-bit MBC3 mask isn't silently truncating to MBC1's 5 bits when it
// shouldn't — MBC1 caps bank reg at 5 bits but still accepts 5, so this
// case doesn't disprove the widening; a separate high-bank test would).
//
// Harness asserts $C100-$C103 == DE AD BE EF and the .romx.bank5 section
// is present in the object file. Linked with the standard crt0.o (MBC3
// shares the $2000 bank register with MBC1, so no crt0 variant needed).

#include "../../llvm/lib/Target/SM83/runtime/gb.h"

__attribute__((section(".romx.bank5"), used))
const volatile unsigned char bank5_payload[4] = { 0xDE, 0xAD, 0xBE, 0xEF };

void main(void) {
    volatile unsigned char *dst = (volatile unsigned char *)0xC100;
    const volatile unsigned char *src = bank5_payload;

    BANK_SWITCH(5);
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
    BANK_SWITCH(1);
}
