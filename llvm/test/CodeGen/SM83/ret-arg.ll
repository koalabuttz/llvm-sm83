; RUN: llc -march=sm83 < %s | FileCheck %s

; Test returning arguments (identity functions).

; First i8 arg comes in A, return in A — no copy needed.
define i8 @identity_i8(i8 %x) {
; CHECK-LABEL: identity_i8:
; CHECK:       ret
  ret i8 %x
}

; Second i8 arg comes in C, return through A.
define i8 @second_i8(i8 %a, i8 %b) {
; CHECK-LABEL: second_i8:
; CHECK:       ld a, c
; CHECK-NEXT:  ret
  ret i8 %b
}

; Void return — just ret.
define void @void_func() {
; CHECK-LABEL: void_func:
; CHECK:       ret
  ret void
}
