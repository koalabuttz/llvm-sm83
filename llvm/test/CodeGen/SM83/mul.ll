; RUN: llc -march=sm83 < %s | FileCheck %s

; Test i8 multiply (lowered to __mulqi3 libcall).

define i8 @mul_i8(i8 %a, i8 %b) {
; CHECK-LABEL: mul_i8:
; CHECK:       call __mulqi3
; CHECK:       ret
  %r = mul i8 %a, %b
  ret i8 %r
}
