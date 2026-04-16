; crt0-mbc5.s — SM83 C runtime startup for Game Boy ROM, MBC5 variant.
;
; Differs from crt0.s only in step 1a: MBC5 has two bank registers
; ($2000-$2FFF low 8 bits, $3000-$3FFF bit 8). Rest is identical.
; Link this object instead of sm83-crt0.o when building MBC5 cartridges.

    .section .text._start,"ax",@progbits
    .global _start
    .type   _start, @function

_start:
    ; -----------------------------------------------------------------------
    ; 1. Disable interrupts and initialise stack pointer.
    ; -----------------------------------------------------------------------
    di
    ld      sp, _stack_top          ; SP = $DFFF (top of WRAM)

    ; -----------------------------------------------------------------------
    ; 1a. Force bank 1 at $4000 for MBC5. MBC5 uses separate low/high bank
    ;     registers: $2000-$2FFF = bank[7:0], $3000-$3FFF = bank[8]. Unlike
    ;     MBC1/MBC3, MBC5 permits bank 0 in the $4000-$7FFF window, so we
    ;     explicitly select bank 1 for a clean post-reset state.
    ; -----------------------------------------------------------------------
    ld      a, 1
    ld      [0x2000], a             ; low 8 bits = 1
    xor     a
    ld      [0x3000], a             ; bit 8 = 0

    ; -----------------------------------------------------------------------
    ; 2. Zero BSS  (HL = _bss_start, DE = _bss_end - _bss_start, A = 0)
    ; -----------------------------------------------------------------------
    ld      hl, _bss_start
    ld      bc, _bss_end
    ld      a, c
    sub     l
    ld      e, a
    ld      a, b
    sbc     a, h
    ld      d, a                    ; DE = _bss_end - _bss_start (may be 0)
    ld      a, d
    or      e
    jr      z, .Lbss_done
    xor     a
.Lbss_loop:
    ld      [hl+], a
    dec     de
    ld      a, d
    or      e
    jr      nz, .Lbss_loop
.Lbss_done:

    ; -----------------------------------------------------------------------
    ; 3. Copy initialised data from ROM (_data_load) to WRAM (_data_start).
    ; -----------------------------------------------------------------------
    ld      hl, _data_load
    ld      de, _data_start
    ld      bc, _data_end
    ld      a, c
    sub     e
    ld      c, a
    ld      a, b
    sbc     a, d
    ld      b, a                    ; BC = length
    ld      a, b
    or      c
    jr      z, .Ldata_done
.Ldata_loop:
    ld      a, [hl+]
    ld      [de], a
    inc     de
    dec     bc
    ld      a, b
    or      c
    jr      nz, .Ldata_loop
.Ldata_done:

    ; -----------------------------------------------------------------------
    ; 3b. Copy HRAM image from ROM to HRAM.
    ; -----------------------------------------------------------------------
    ld      hl, _hram_load
    ld      de, _hram_start
    ld      bc, _hram_end
    ld      a, c
    sub     e
    ld      c, a
    ld      a, b
    sbc     a, d
    ld      b, a                    ; BC = _hram_end - _hram_start
    ld      a, b
    or      c
    jr      z, .Lhram_done
.Lhram_loop:
    ld      a, [hl+]
    ld      [de], a
    inc     de
    dec     bc
    ld      a, b
    or      c
    jr      nz, .Lhram_loop
.Lhram_done:

    ; -----------------------------------------------------------------------
    ; 4. Enable VBlank interrupt and global interrupts, then call main().
    ; -----------------------------------------------------------------------
    ld      a, 0x01                 ; IE_VBLANK
    ldh     [0xFF], a               ; $FFFF = IE register

    call    main

    ; -----------------------------------------------------------------------
    ; 5. Halt forever if main() returns.
    ; -----------------------------------------------------------------------
    di
.Lhalt_loop:
    halt
    nop
    jr      .Lhalt_loop

    .size _start, . - _start
