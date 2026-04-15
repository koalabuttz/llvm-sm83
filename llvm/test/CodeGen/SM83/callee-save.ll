; RUN: llc -march=sm83 < %s | FileCheck %s

; Callee-saved register DE — verify save/restore around calls.

declare i8 @external_func(i8)

define i8 @uses_de_across_call(i8 %x) {
; CHECK-LABEL: uses_de_across_call:
; CHECK: call external_func
; CHECK: ret
  %a = add i8 %x, 1
  %b = call i8 @external_func(i8 %a)
  %c = add i8 %b, %x
  ret i8 %c
}
