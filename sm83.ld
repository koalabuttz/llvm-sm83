/* sm83.ld — Game Boy (SM83) linker script.
 *
 * Supports both ROM-only (32 KB, no MBC) and MBC1 cartridges up to 256 KB
 * (16 banks). Bank 0 is always mapped at $0000–$3FFF and holds all code;
 * banks 1..15 are data-only banks, paged into $4000–$7FFF by writing the
 * bank number to $2000 (see runtime/gb.h:BANK_SWITCH).
 *
 * This layout follows the GBDK convention: code always runs from bank 0,
 * so a BANK_SWITCH can never page out the currently-executing function.
 * Code that doesn't fit in 16 KB cannot be bank-switched safely without
 * a cross-bank call trampoline — deferred to Round 6.
 *
 * Memory map:
 *   $0000–$003F  RST vectors (8 × 8 bytes)
 *   $0040–$0067  Interrupt service routine dispatch (JP trampolines)
 *   $0100–$0103  Entry point: NOP; JP $0150  (written by make-gb-rom.py)
 *   $0104–$014F  Nintendo logo + ROM header  (written by make-gb-rom.py)
 *   $0150–$3FFF  All code and read-only data (bank 0, fixed)
 *   $4000–$7FFF  Data bank window (bank 1 default, any bank via BANK_SWITCH)
 *   $C000–$DFFF  WRAM (8 KB)
 *   $FF80–$FFFE  HRAM (127 bytes)
 *
 * Usage:
 *   ld.lld --no-check-sections -T sm83.ld foo.o runtime.o -o foo.elf
 *   python3 make-gb-rom.py foo.elf -o foo.gb                 # 32 KB ROM-only
 *   python3 make-gb-rom.py foo.elf -o foo.gb --mbc1 --rom-banks 16
 *
 * The --no-check-sections flag is required because all banks share VMA
 * $4000-$7FFF (that's the whole point of MBC overlay banking).
 */

OUTPUT_ARCH(sm83)
ENTRY(_start)

MEMORY {
    ROM0 (rx)  : ORIGIN = 0x0000, LENGTH = 0x4000  /* 16 KB bank 0 (fixed) */
    ROMX (rx)  : ORIGIN = 0x4000, LENGTH = 0x4000  /* 16 KB bank window */
    WRAM (rw)  : ORIGIN = 0xC000, LENGTH = 0x2000  /* 8 KB Work RAM */
    HRAM (rw)  : ORIGIN = 0xFF80, LENGTH = 0x007F  /* 127 bytes High RAM */
}

SECTIONS {
    /* RST vectors at $0000–$003F. */
    .vectors 0x0000 : {
        KEEP(*(.vectors))
        . = 0x0040;
    } > ROM0

    /*
     * Interrupt vector dispatch table at $0040-$0067.
     *
     * Each 8-byte slot holds a 3-byte `JP <handler>` trampoline. Handler
     * bodies live in .text (KEEP'd below). Undefined handlers default to
     * the RETI stub via the PROVIDE lines further down.
     */
    .isr_dispatch 0x0040 : {
        BYTE(0xC3); SHORT(vblank_isr);
        BYTE(0); BYTE(0); BYTE(0); BYTE(0); BYTE(0);
        BYTE(0xC3); SHORT(lcd_isr);
        BYTE(0); BYTE(0); BYTE(0); BYTE(0); BYTE(0);
        BYTE(0xC3); SHORT(timer_isr);
        BYTE(0); BYTE(0); BYTE(0); BYTE(0); BYTE(0);
        BYTE(0xC3); SHORT(serial_isr);
        BYTE(0); BYTE(0); BYTE(0); BYTE(0); BYTE(0);
        BYTE(0xC3); SHORT(joypad_isr);
        BYTE(0); BYTE(0); BYTE(0); BYTE(0); BYTE(0);
    } > ROM0

    /* Header placeholder; make-gb-rom.py writes $0100-$014F post-link. */
    .header 0x0100 (NOLOAD) : {
        . = 0x0150;
    } > ROM0

    /*
     * All program code and read-only data lives in bank 0.
     *
     * A function stays at the same ROM address regardless of which bank
     * is currently paged in at $4000 — so BANK_SWITCH(N) paging bank N
     * into the window cannot orphan the currently-executing code.
     *
     * If a program doesn't fit in bank 0's 16 KB, the user must either
     * move rarely-called code into a high bank via .romx.bankN AND never
     * BANK_SWITCH away from it while it's running (Round 5 scope), or
     * use a far-call trampoline (Round 6).
     */
    .text 0x0150 : {
        KEEP(*(.text._start))
        KEEP(*(.text.icall_hl))    /* jp hl trampoline — __sm83_icall_hl */
        KEEP(*(.isr.vblank))
        KEEP(*(.isr.lcd))
        KEEP(*(.isr.timer))
        KEEP(*(.isr.serial))
        KEEP(*(.isr.joypad))
        *(.text*)
        *(.rodata*)
        *(.rodata)
        . = ALIGN(2);
        _default_isr_stub = .;
        BYTE(0xD9);                /* RETI — one-byte default ISR body */
        . = ALIGN(2);
        _rom0_end = .;
    } > ROM0
    ASSERT(_rom0_end <= 0x4000,
           "ROM0 overflow: code + rodata exceeds 16 KB bank 0")

    PROVIDE(vblank_isr = _default_isr_stub);
    PROVIDE(lcd_isr    = _default_isr_stub);
    PROVIDE(timer_isr  = _default_isr_stub);
    PROVIDE(serial_isr = _default_isr_stub);
    PROVIDE(joypad_isr = _default_isr_stub);

    /*
     * Banks 1..15: opt-in data banks via __attribute__((section(".romx.bankN"))).
     *
     * All banks share VMA $4000-$7FFF because MBC1 overlays them into the
     * same address window. They have distinct LMAs at N * $4000 in the
     * ROM file so make-gb-rom.py lays them out at the right cartridge
     * offsets. Requires ld.lld --no-check-sections.
     *
     * Users must BANK_SWITCH(N) to page in bank N before reading its
     * contents through $4000-$7FFF. Bank 1 is the hardware default at
     * reset, but crt0 explicitly writes 1 to $2000 for determinism.
     */
    .romx.bank1  0x4000 : AT(0x04000) { *(.romx.bank1*)  . = ALIGN(2); } > ROMX
    .romx.bank2  0x4000 : AT(0x08000) { *(.romx.bank2*)  . = ALIGN(2); } > ROMX
    .romx.bank3  0x4000 : AT(0x0C000) { *(.romx.bank3*)  . = ALIGN(2); } > ROMX
    .romx.bank4  0x4000 : AT(0x10000) { *(.romx.bank4*)  . = ALIGN(2); } > ROMX
    .romx.bank5  0x4000 : AT(0x14000) { *(.romx.bank5*)  . = ALIGN(2); } > ROMX
    .romx.bank6  0x4000 : AT(0x18000) { *(.romx.bank6*)  . = ALIGN(2); } > ROMX
    .romx.bank7  0x4000 : AT(0x1C000) { *(.romx.bank7*)  . = ALIGN(2); } > ROMX
    .romx.bank8  0x4000 : AT(0x20000) { *(.romx.bank8*)  . = ALIGN(2); } > ROMX
    .romx.bank9  0x4000 : AT(0x24000) { *(.romx.bank9*)  . = ALIGN(2); } > ROMX
    .romx.bank10 0x4000 : AT(0x28000) { *(.romx.bank10*) . = ALIGN(2); } > ROMX
    .romx.bank11 0x4000 : AT(0x2C000) { *(.romx.bank11*) . = ALIGN(2); } > ROMX
    .romx.bank12 0x4000 : AT(0x30000) { *(.romx.bank12*) . = ALIGN(2); } > ROMX
    .romx.bank13 0x4000 : AT(0x34000) { *(.romx.bank13*) . = ALIGN(2); } > ROMX
    .romx.bank14 0x4000 : AT(0x38000) { *(.romx.bank14*) . = ALIGN(2); } > ROMX
    .romx.bank15 0x4000 : AT(0x3C000) { *(.romx.bank15*) . = ALIGN(2); } > ROMX

    /*
     * Initialised data: VMA in WRAM, LMA appended to bank 0 in the ROM
     * file so crt0 can copy it at boot.
     */
    .data : AT(_rom0_end) {
        _data_start = .;
        *(.data*)
        . = ALIGN(2);
        _data_end = .;
    } > WRAM
    _data_load = LOADADDR(.data);

    /* .bss: LMA = VMA so ld doesn't reserve an overlapping ROM range. */
    .bss (NOLOAD) : AT(ADDR(.bss)) {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(2);
        _bss_end = .;
    } > WRAM

    PROVIDE(_stack_top = ORIGIN(WRAM) + LENGTH(WRAM) - 1);

    /*
     * HRAM — 127 bytes of fast memory at $FF80-$FFFE.
     * Code here executes during OAM DMA ($FF46 write). Contents stored in
     * ROM after .data and copied to HRAM by crt0.
     */
    .hram 0xFF80 : AT(LOADADDR(.data) + SIZEOF(.data)) {
        _hram_start = .;
        *(.hram*)
        . = ALIGN(2);
        _hram_end = .;
    } > HRAM
    _hram_load = LOADADDR(.hram);
    ASSERT(_hram_end <= 0xFFFF, "HRAM overflow: exceeds 127 bytes at $FF80")
    ASSERT(LOADADDR(.hram) + SIZEOF(.hram) <= 0x4000,
           "Bank 0 overflow: code + data + hram exceeds 16 KB")

    /* Discard debug and exception-handling sections. */
    /DISCARD/ : {
        *(.debug*)
        *(.comment*)
        *(.eh_frame*)
        *(.note*)
    }
}
