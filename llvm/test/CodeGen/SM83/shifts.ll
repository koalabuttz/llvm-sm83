; RUN: llc -march=sm83 < %s | FileCheck %s

; Variable i8 left shift — loop: LD A,amt; OR A; skip if zero; SLA; DEC; JR NZ.

define i8 @shl_i8(i8 %a, i8 %b) {
; CHECK-LABEL: shl_i8:
; CHECK:       or a
; CHECK:       sla
; CHECK:       dec
; CHECK:       ret
  %result = shl i8 %a, %b
  ret i8 %result
}

define i8 @srl_i8(i8 %a, i8 %b) {
; CHECK-LABEL: srl_i8:
; CHECK:       or a
; CHECK:       srl
; CHECK:       dec
; CHECK:       ret
  %result = lshr i8 %a, %b
  ret i8 %result
}

define i8 @sra_i8(i8 %a, i8 %b) {
; CHECK-LABEL: sra_i8:
; CHECK:       or a
; CHECK:       sra
; CHECK:       dec
; CHECK:       ret
  %result = ashr i8 %a, %b
  ret i8 %result
}

; Variable i16 left shift — loop: SLA lo, RL hi (carry propagates lo→hi).

define i16 @shl_i16(i16 %a, i16 %b) {
; CHECK-LABEL: shl_i16:
; CHECK:       or a
; CHECK:       sla
; CHECK:       rl
; CHECK:       dec
; CHECK:       ret
  %result = shl i16 %a, %b
  ret i16 %result
}

; Variable i16 logical right shift — loop: SRL hi, RR lo (carry propagates hi→lo).

define i16 @srl_i16(i16 %a, i16 %b) {
; CHECK-LABEL: srl_i16:
; CHECK:       or a
; CHECK:       srl
; CHECK:       rr
; CHECK:       dec
; CHECK:       ret
  %result = lshr i16 %a, %b
  ret i16 %result
}

; Variable i16 arithmetic right shift — loop: SRA hi, RR lo.

define i16 @sra_i16(i16 %a, i16 %b) {
; CHECK-LABEL: sra_i16:
; CHECK:       or a
; CHECK:       sra
; CHECK:       rr
; CHECK:       dec
; CHECK:       ret
  %result = ashr i16 %a, %b
  ret i16 %result
}
