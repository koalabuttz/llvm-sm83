; RUN: llc -march=sm83 < %s | FileCheck %s

; Test i8 subtraction.

define i8 @sub_i8(i8 %a, i8 %b) {
; CHECK-LABEL: sub_i8:
; CHECK:       sub c
; CHECK:       ret
  %result = sub i8 %a, %b
  ret i8 %result
}
