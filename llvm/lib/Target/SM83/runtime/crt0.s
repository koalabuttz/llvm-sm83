; crt0.s — SM83 C runtime startup for Game Boy ROM
;
; Placed at $0150 (just after the Nintendo header).
; Sets up the hardware, zeroes BSS, copies .data initialisation image from
; ROM to WRAM, then calls main().  On return from main(), loops forever.
;
; Memory layout produced by sm83.ld:
;   _bss_start / _bss_end   — zero-init region in WRAM
;   _data_start / _data_end — destination in WRAM
;   _data_load              — source (ROM copy) of initialised data
;   _stack_top              — top of WRAM stack ($DFFF)

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
    ; 2. Zero BSS  (HL = _bss_start, DE = _bss_end - _bss_start, A = 0)
    ; -----------------------------------------------------------------------
    ld      hl, _bss_start
    ld      bc, _bss_end
    ; Compute length = _bss_end - _bss_start into DE
    ld      d, b
    ld      e, c
    ld      a, l
    sub     e
    ld      e, a
    ld      a, h
    sbc     a, d
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
    ;    HL = _data_load (source), DE = _data_start (dest), BC = length
    ; -----------------------------------------------------------------------
    ld      hl, _data_load
    ld      de, _data_start
    ld      bc, _data_end
    ; Compute length = _data_end - _data_start
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
    ; 4. Enable VBlank interrupt and global interrupts, then call main().
    ; -----------------------------------------------------------------------
    ld      a, 0x01                 ; IE_VBLANK
    ldh     [$FF], a                ; $FFFF = IE register

    call    main

    ; -----------------------------------------------------------------------
    ; 5. Halt forever if main() returns.
    ; -----------------------------------------------------------------------
.Lhalt_loop:
    halt
    nop
    jr      .Lhalt_loop

    .size _start, . - _start
