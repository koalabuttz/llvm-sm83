; RUN: llc -march=sm83 < %s | FileCheck %s

; Test i8 shift (by 1 bit — the expand pass does single shifts).

define i8 @shl_i8(i8 %a, i8 %b) {
; CHECK-LABEL: shl_i8:
; CHECK:       sla
; CHECK:       ret
  %result = shl i8 %a, %b
  ret i8 %result
}
