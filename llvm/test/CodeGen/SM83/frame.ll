; RUN: llc -march=sm83 < %s | FileCheck %s

; Test function with inter-function call.
; The caller passes an argument and uses the return value.

declare i8 @callee(i8)

define i8 @caller(i8 %x) {
; CHECK-LABEL: caller:
; CHECK:       call callee
; CHECK:       ret
  %r = call i8 @callee(i8 %x)
  ret i8 %r
}
