; RUN: llc -march=sm83 < %s | FileCheck %s

; Test i16 shift left (SLA lo, RL hi).

define i16 @shl_i16(i16 %a, i16 %b) {
; CHECK-LABEL: shl_i16:
; CHECK:       sla
; CHECK:       rl
; CHECK:       ret
  %r = shl i16 %a, %b
  ret i16 %r
}
