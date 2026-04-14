; RUN: llc -march=sm83 < %s | FileCheck %s

; Test i8 addition.

define i8 @add_i8(i8 %a, i8 %b) {
; CHECK-LABEL: add_i8:
; CHECK:       add a, c
; CHECK:       ret
  %result = add i8 %a, %b
  ret i8 %result
}
