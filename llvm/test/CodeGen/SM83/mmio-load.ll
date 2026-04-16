; RUN: llc -march=sm83 < %s | FileCheck %s

; Load i8 constant-address path:
;   $FF00-$FFFF → LDH A,[n]   (existing)
;   $0000-$FEFF → LD A,[nn]   (LDA_nn, Round 8 item 2)

define i8 @load_sram() {
; CHECK-LABEL: load_sram:
; CHECK: ld a, [-24576]
; CHECK-NOT: ld hl,
  %v = load volatile i8, ptr inttoptr (i16 40960 to ptr), align 1
  ret i8 %v
}

define i8 @load_rom() {
; CHECK-LABEL: load_rom:
; CHECK: ld a, [16384]
; CHECK-NOT: ld hl,
  %v = load volatile i8, ptr inttoptr (i16 16384 to ptr), align 1
  ret i8 %v
}

define i8 @load_ffxx() {
; CHECK-LABEL: load_ffxx:
; CHECK: ldh a, [70]
  %v = load volatile i8, ptr inttoptr (i16 65350 to ptr), align 1
  ret i8 %v
}
