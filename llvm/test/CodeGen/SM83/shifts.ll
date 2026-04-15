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

; Constant i8 shifts — unrolled, no loop.

define i8 @shl_i8_const3(i8 %a) {
; CHECK-LABEL: shl_i8_const3:
; CHECK:       sla
; CHECK:       sla
; CHECK:       sla
; CHECK-NOT:   dec
; CHECK:       ret
  %result = shl i8 %a, 3
  ret i8 %result
}

define i8 @srl_i8_const2(i8 %a) {
; CHECK-LABEL: srl_i8_const2:
; CHECK:       srl
; CHECK:       srl
; CHECK-NOT:   dec
; CHECK:       ret
  %result = lshr i8 %a, 2
  ret i8 %result
}

define i8 @sra_i8_const4(i8 %a) {
; CHECK-LABEL: sra_i8_const4:
; CHECK:       sra
; CHECK:       sra
; CHECK:       sra
; CHECK:       sra
; CHECK-NOT:   dec
; CHECK:       ret
  %result = ashr i8 %a, 4
  ret i8 %result
}

; Constant i16 shifts — unrolled, no loop.

define i16 @shl_i16_const2(i16 %a) {
; CHECK-LABEL: shl_i16_const2:
; CHECK:       sla
; CHECK:       rl
; CHECK:       sla
; CHECK:       rl
; CHECK-NOT:   dec
; CHECK:       ret
  %result = shl i16 %a, 2
  ret i16 %result
}

define i16 @srl_i16_const1(i16 %a) {
; CHECK-LABEL: srl_i16_const1:
; CHECK:       srl
; CHECK:       rr
; CHECK-NOT:   dec
; CHECK:       ret
  %result = lshr i16 %a, 1
  ret i16 %result
}

define i16 @sra_i16_const3(i16 %a) {
; CHECK-LABEL: sra_i16_const3:
; CHECK:       sra
; CHECK:       rr
; CHECK:       sra
; CHECK:       rr
; CHECK:       sra
; CHECK:       rr
; CHECK-NOT:   dec
; CHECK:       ret
  %result = ashr i16 %a, 3
  ret i16 %result
}

; Shift count exactly 8 on i16 — must NOT be treated as shift-by-0.
; The constant unrolling fast path must fall through to the loop for N >= 8.

define i16 @shl_i16_const8(i16 %a) {
; CHECK-LABEL: shl_i16_const8:
; CHECK:       or a
; CHECK:       sla
; CHECK:       rl
; CHECK:       dec
; CHECK:       ret
  %result = shl i16 %a, 8
  ret i16 %result
}
