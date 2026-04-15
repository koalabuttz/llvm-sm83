/* sm83.ld — Game Boy (SM83) linker script for ROM-only (no MBC) cartridge
 *
 * Memory map:
 *   $0000–$003F  RST vectors (8 × 8 bytes)
 *   $0040–$0067  Interrupt service routine stubs (VBlank/LCD/Timer/Serial/Joypad)
 *   $0100–$0103  Entry point: NOP; JP $0150  (written by make-gb-rom.py)
 *   $0104–$014F  Nintendo logo + ROM header  (written by make-gb-rom.py)
 *   $0150–$7FFF  ROM bank 0 — main code + read-only data
 *   $C000–$DFFF  WRAM (8 KB)
 *   $FF80–$FFFE  HRAM (127 bytes)
 *
 * Usage:
 *   ld.lld -T sm83.ld foo.o runtime.o -o foo.elf
 *   python3 make-gb-rom.py foo.elf -o foo.gb
 */

OUTPUT_ARCH(sm83)
ENTRY(_start)

MEMORY {
    ROM  (rx)  : ORIGIN = 0x0000, LENGTH = 0x8000  /* 32 KB ROM */
    WRAM (rw)  : ORIGIN = 0xC000, LENGTH = 0x2000  /* 8 KB Work RAM */
    HRAM (rw)  : ORIGIN = 0xFF80, LENGTH = 0x007F  /* 127 bytes High RAM */
}

SECTIONS {
    /*
     * RST vectors at $0000–$003F.
     * Each RST n occupies 8 bytes starting at n*8.
     * Default: each slot falls through via RETI (0xD9).
     */
    .vectors 0x0000 : {
        KEEP(*(.vectors))
        . = 0x0040;
    } > ROM

    /*
     * Interrupt service routine stubs.
     * Hardware jumps to these fixed addresses on interrupt.
     * Each slot is 8 bytes; user code should place an ISR body or RETI stub.
     */
    .isr.vblank 0x0040 : { KEEP(*(.isr.vblank)) . = 0x0048; } > ROM
    .isr.lcd    0x0048 : { KEEP(*(.isr.lcd))    . = 0x0050; } > ROM
    .isr.timer  0x0050 : { KEEP(*(.isr.timer))  . = 0x0058; } > ROM
    .isr.serial 0x0058 : { KEEP(*(.isr.serial)) . = 0x0060; } > ROM
    .isr.joypad 0x0060 : { KEEP(*(.isr.joypad)) . = 0x0068; } > ROM

    /*
     * Entry point at $0100–$0103.
     * make-gb-rom.py overwrites this with: NOP (00), JP (C3), lo($0150), hi($01).
     * The 72 bytes from $0104–$014F (Nintendo logo + header) are also written
     * by make-gb-rom.py and must not be emitted by the linker.
     * We advance past the header so that .text starts at $0150.
     */
    .header 0x0100 (NOLOAD) : {
        . = 0x0150;
    } > ROM

    /*
     * Main program code and read-only data starting at $0150.
     */
    .text 0x0150 : {
        KEEP(*(.text._start))
        KEEP(*(.text.icall_hl))   /* jp hl trampoline — __sm83_icall_hl */
        *(.text*)
        *(.rodata*)
        *(.rodata)
        . = ALIGN(2);
        _rom_end = .;
    } > ROM
    ASSERT(_rom_end <= 0x8000, "ROM bank 0 overflow: code+data exceeds 32 KB")

    /*
     * Initialised data: stored in ROM, copied to WRAM at startup by _start.
     * _data_load is the ROM address of the initialisation image.
     */
    .data : {
        _data_start = .;
        *(.data*)
        . = ALIGN(2);
        _data_end = .;
    } > WRAM AT > ROM
    _data_load = LOADADDR(.data);

    /*
     * Zero-initialised BSS in WRAM — no ROM space consumed.
     * _start must zero this region before calling main.
     */
    .bss (NOLOAD) : {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(2);
        _bss_end = .;
    } > WRAM

    /*
     * Stack: grows downward from the top of WRAM.
     * ld.lld does not place a section here; _start loads SP = $DFFF.
     */
    PROVIDE(_stack_top = ORIGIN(WRAM) + LENGTH(WRAM) - 1);

    /*
     * HRAM — small scratch area above the stack, useful for DMA wait loops.
     */
    .hram (NOLOAD) : {
        *(.hram*)
    } > HRAM

    /* Discard debug and exception-handling sections. */
    /DISCARD/ : {
        *(.debug*)
        *(.comment*)
        *(.eh_frame*)
        *(.note*)
    }
}
