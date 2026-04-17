// oam-dma-test.c — exercise the HRAM-resident gb_oam_dma routine.
//
// Fills a 160-byte WRAM shadow buffer with a known pattern, triggers
// OAM DMA via gb_oam_dma_trigger(shadow), and then reads back the
// first five bytes of OAM at $FE00..$FE04. The harness asserts them
// at $C100..$C104.
//
// Proves:
//   (a) crt0 copies .hram content from ROM to $FF80+, making
//       gb_oam_dma callable at runtime.
//   (b) The DMA trigger correctly copies (src_page << 8) to $FE00.
//   (c) The busy-wait in HRAM doesn't crash or run off the end.

#include <gb.h>

// 256-byte-aligned shadow buffer so DMA can use a page-aligned address.
// Declared volatile so the compiler won't fuse adjacent byte stores into
// 16-bit stores (a pre-existing codegen bug where fused i16 stores can
// route the value through HL and clobber it while forming the address).
static volatile unsigned char shadow[256] __attribute__((aligned(256)));

void main(void) {
    volatile unsigned char *out = (volatile unsigned char *)0xC100;

    // Fill the first 5 bytes with a known pattern.
    shadow[0] = 0xA1;
    shadow[1] = 0xB2;
    shadow[2] = 0xC3;
    shadow[3] = 0xD4;
    shadow[4] = 0xE5;

    gb_oam_dma_trigger((const void *)shadow);

    // OAM starts at $FE00.
    volatile unsigned char *oam = (volatile unsigned char *)0xFE00;
    out[0] = oam[0];
    out[1] = oam[1];
    out[2] = oam[2];
    out[3] = oam[3];
    out[4] = oam[4];
}
