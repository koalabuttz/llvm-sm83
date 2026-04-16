; RUN: llc -march=sm83 < %s | FileCheck %s

; Test interrupt handler support.
; Functions with the "interrupt" attribute should:
; 1. Push all registers (AF, BC, DE, HL) in the prologue
; 2. Pop all registers in the epilogue (reverse order)
; 3. Return with RETI instead of RET

define void @vblank_handler() #0 {
; CHECK-LABEL: vblank_handler:
; CHECK:       push af
; CHECK-NEXT:  push bc
; CHECK-NEXT:  push de
; CHECK-NEXT:  push hl
; CHECK:       pop hl
; CHECK-NEXT:  pop de
; CHECK-NEXT:  pop bc
; CHECK-NEXT:  pop af
; CHECK-NEXT:  reti
  ret void
}

; Normal functions should not have the interrupt prologue/epilogue.
define void @normal_function() {
; CHECK-LABEL: normal_function:
; CHECK-NOT:   push af
; CHECK:       ret
; CHECK-NOT:   reti
  ret void
}

attributes #0 = { "interrupt" }
