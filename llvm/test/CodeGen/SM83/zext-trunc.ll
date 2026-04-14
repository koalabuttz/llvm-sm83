; RUN: llc -march=sm83 < %s | FileCheck %s

; Test truncation from i16 to i8.

define i8 @trunc_i16_to_i8(i16 %x) {
; CHECK-LABEL: trunc_i16_to_i8:
; CHECK:       ld a, c
; CHECK-NEXT:  ret
  %result = trunc i16 %x to i8
  ret i8 %result
}

; Test zero extension from i8 to i16.

define i16 @zext_i8_to_i16(i8 %x) {
; CHECK-LABEL: zext_i8_to_i16:
; CHECK:       ld l, a
; CHECK-NEXT:  ld h, 0
; CHECK-NEXT:  ret
  %result = zext i8 %x to i16
  ret i16 %result
}
