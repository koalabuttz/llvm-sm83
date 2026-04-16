// mbc5-test.c — exercise MBC5 bank switching end-to-end.
//
// MBC5 splits the ROM bank register into two: $2000 (low 8 bits) and
// $3000 (bit 8). This test puts a payload in bank 3 (fits in the low
// 8 bits, so a single write to $2000 suffices), copies it to WRAM, then
// restores bank 1. Linked against sm83-crt0-mbc5.o which writes both
// registers at startup.

#include <gb.h>

__attribute__((section(".romx.bank3"), used))
const volatile unsigned char bank3_payload[4] = { 0xC0, 0xFF, 0xEE, 0x42 };

void main(void) {
    volatile unsigned char *dst = (volatile unsigned char *)0xC100;
    const volatile unsigned char *src = bank3_payload;

    BANK_SWITCH(3);
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
    BANK_SWITCH(1);
}
