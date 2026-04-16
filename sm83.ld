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
     * Interrupt vector dispatch table at $0040-$0067.
     *
     * Each 8-byte slot holds a 3-byte `JP <handler>` trampoline, NOP-padded
     * to 8 bytes so the next vector lands at the correct fixed address.
     * The handler bodies themselves live in .text (see the KEEP(*(.isr.*))
     * lines below). Vectors whose handler is not defined resolve to
     * _default_isr_stub (a single RETI byte) via the PROVIDE lines further
     * down.
     *
     * Opcode reference: 0xC3 = JP nn (3 bytes, little-endian target).
     */
    .isr_dispatch 0x0040 : {
        BYTE(0xC3); SHORT(vblank_isr);
        BYTE(0); BYTE(0); BYTE(0); BYTE(0); BYTE(0);   /* pad to 8 bytes */
        BYTE(0xC3); SHORT(lcd_isr);
        BYTE(0); BYTE(0); BYTE(0); BYTE(0); BYTE(0);
        BYTE(0xC3); SHORT(timer_isr);
        BYTE(0); BYTE(0); BYTE(0); BYTE(0); BYTE(0);
        BYTE(0xC3); SHORT(serial_isr);
        BYTE(0); BYTE(0); BYTE(0); BYTE(0); BYTE(0);
        BYTE(0xC3); SHORT(joypad_isr);
        BYTE(0); BYTE(0); BYTE(0); BYTE(0); BYTE(0);
    } > ROM

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
        /* ISR handler bodies. The dispatch table at $0040-$0067 JPs here.
         * Each KEEP'd because interrupt handlers have no static call site
         * (hardware-invoked through the dispatch table). */
        KEEP(*(.isr.vblank))
        KEEP(*(.isr.lcd))
        KEEP(*(.isr.timer))
        KEEP(*(.isr.serial))
        KEEP(*(.isr.joypad))
        *(.text*)
        *(.rodata*)
        *(.rodata)
        . = ALIGN(2);
        /* Default handler for vectors with no user-defined ISR. */
        _default_isr_stub = .;
        BYTE(0xD9);   /* RETI */
        . = ALIGN(2);
        _rom_end = .;
    } > ROM
    ASSERT(_rom_end <= 0x8000, "ROM bank 0 overflow: code+data exceeds 32 KB")

    /*
     * Default any undefined ISR vectors to the RETI stub. PROVIDE creates
     * these symbols only if they were not already defined (i.e. the user
     * did not write __attribute__((interrupt("<name>")))).
     */
    PROVIDE(vblank_isr = _default_isr_stub);
    PROVIDE(lcd_isr    = _default_isr_stub);
    PROVIDE(timer_isr  = _default_isr_stub);
    PROVIDE(serial_isr = _default_isr_stub);
    PROVIDE(joypad_isr = _default_isr_stub);

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
     * HRAM — 127 bytes of fast memory at $FF80-$FFFE.
     *
     * Code in HRAM executes normally even while OAM DMA is transferring
     * ($FF46 write), so the OAM DMA trampoline MUST live here; during the
     * transfer the CPU cannot fetch from ROM or WRAM.
     *
     * Contents are stored in ROM and copied to HRAM by _start at boot
     * (analogous to .data → WRAM). Mark uninitialised HRAM with a distinct
     * section if needed (currently unused; add .hram.bss as NOLOAD if
     * required in the future).
     */
    .hram 0xFF80 : {
        _hram_start = .;
        *(.hram*)
        . = ALIGN(2);
        _hram_end = .;
    } > HRAM AT > ROM
    _hram_load = LOADADDR(.hram);
    ASSERT(_hram_end <= 0xFFFF, "HRAM overflow: exceeds 127 bytes at $FF80")

    /* Discard debug and exception-handling sections. */
    /DISCARD/ : {
        *(.debug*)
        *(.comment*)
        *(.eh_frame*)
        *(.note*)
    }
}
