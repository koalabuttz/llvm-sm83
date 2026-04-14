; RUN: llc -march=sm83 < %s | FileCheck %s

; Test i8 bitwise operations.

define i8 @and_i8(i8 %a, i8 %b) {
; CHECK-LABEL: and_i8:
; CHECK:       and c
; CHECK:       ret
  %result = and i8 %a, %b
  ret i8 %result
}

define i8 @or_i8(i8 %a, i8 %b) {
; CHECK-LABEL: or_i8:
; CHECK:       or c
; CHECK:       ret
  %result = or i8 %a, %b
  ret i8 %result
}

define i8 @xor_i8(i8 %a, i8 %b) {
; CHECK-LABEL: xor_i8:
; CHECK:       xor c
; CHECK:       ret
  %result = xor i8 %a, %b
  ret i8 %result
}

; Test i16 bitwise operations (lo/hi byte decomposition).

define i16 @and_i16(i16 %a, i16 %b) {
; CHECK-LABEL: and_i16:
; CHECK:       and
; CHECK:       and
; CHECK:       ret
  %result = and i16 %a, %b
  ret i16 %result
}

define i16 @or_i16(i16 %a, i16 %b) {
; CHECK-LABEL: or_i16:
; CHECK:       or
; CHECK:       or
; CHECK:       ret
  %result = or i16 %a, %b
  ret i16 %result
}

define i16 @xor_i16(i16 %a, i16 %b) {
; CHECK-LABEL: xor_i16:
; CHECK:       xor
; CHECK:       xor
; CHECK:       ret
  %result = xor i16 %a, %b
  ret i16 %result
}
