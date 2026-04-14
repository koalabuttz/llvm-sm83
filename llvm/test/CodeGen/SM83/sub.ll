; RUN: llc -march=sm83 < %s | FileCheck %s

; Test i8 subtraction.

define i8 @sub_i8(i8 %a, i8 %b) {
; CHECK-LABEL: sub_i8:
; CHECK:       sub c
; CHECK:       ret
  %result = sub i8 %a, %b
  ret i8 %result
}

; Test i16 subtraction (lo byte SUB, hi byte SBC).

define i16 @sub_i16(i16 %a, i16 %b) {
; CHECK-LABEL: sub_i16:
; CHECK:       sub
; CHECK:       sbc
; CHECK:       ret
  %result = sub i16 %a, %b
  ret i16 %result
}
