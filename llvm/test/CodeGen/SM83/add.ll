; RUN: llc -march=sm83 < %s | FileCheck %s

; Test i8 addition.

define i8 @add_i8(i8 %a, i8 %b) {
; CHECK-LABEL: add_i8:
; CHECK:       add a, c
; CHECK:       ret
  %result = add i8 %a, %b
  ret i8 %result
}

; Test i16 addition (uses ADD HL, rr).

define i16 @add_i16(i16 %a, i16 %b) {
; CHECK-LABEL: add_i16:
; CHECK:       add hl, de
; CHECK:       ret
  %result = add i16 %a, %b
  ret i16 %result
}
