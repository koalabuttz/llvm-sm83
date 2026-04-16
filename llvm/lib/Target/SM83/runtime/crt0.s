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
    ; Compute DE = BC - HL (= _bss_end - _bss_start). Previous version
    ; computed HL - BC, which wrapped to a huge count whenever BSS was
    ; non-empty and clobbered all of RAM. Exposed by c-vblank-test.c.
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
    ; 3b. Copy HRAM image from ROM (_hram_load) to HRAM (_hram_start).
    ;     Used for the OAM DMA trampoline and any user functions/data
    ;     tagged with __attribute__((section(".hram"))).
    ;     HL = _hram_load (source), DE = _hram_start (dest), BC = length.
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
    ;    DI first so a pending interrupt can't re-enter the ISR after main
    ;    has cleaned up; this also guarantees deterministic termination for
    ;    headless test harnesses (sm83sim.py ends when HALT is entered with
    ;    IME=0 or IE=0).
    ; -----------------------------------------------------------------------
    di
.Lhalt_loop:
    halt
    nop
    jr      .Lhalt_loop

    .size _start, . - _start
