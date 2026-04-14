; RUN: llc -march=sm83 < %s | FileCheck %s

; Test returning i8 constants.

define i8 @ret_42() {
; CHECK-LABEL: ret_42:
; CHECK:       ld a, 42
; CHECK-NEXT:  ret
  ret i8 42
}

define i8 @ret_0() {
; CHECK-LABEL: ret_0:
; CHECK:       ld a, 0
; CHECK-NEXT:  ret
  ret i8 0
}

define i8 @ret_neg1() {
; CHECK-LABEL: ret_neg1:
; CHECK:       ld a, -1
; CHECK-NEXT:  ret
  ret i8 -1
}

; Test returning i16 constants.

define i16 @ret_i16_1000() {
; CHECK-LABEL: ret_i16_1000:
; CHECK:       ld hl, 1000
; CHECK-NEXT:  ret
  ret i16 1000
}

define i16 @ret_i16_0() {
; CHECK-LABEL: ret_i16_0:
; CHECK:       ld hl, 0
; CHECK-NEXT:  ret
  ret i16 0
}
