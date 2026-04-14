; RUN: llc -march=sm83 < %s | FileCheck %s

; Test select (ternary) operation.

define i8 @select_i8(i1 %c, i8 %a, i8 %b) {
; CHECK-LABEL: select_i8:
; CHECK:       ret
  %r = select i1 %c, i8 %a, i8 %b
  ret i8 %r
}
