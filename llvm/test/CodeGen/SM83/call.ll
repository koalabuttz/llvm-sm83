; RUN: llc -march=sm83 < %s | FileCheck %s

; Test function call with argument and return value.

declare i8 @external_func(i8)

define i8 @call_test() {
; CHECK-LABEL: call_test:
; CHECK:       ld a, 10
; CHECK-NEXT:  jp external_func
  %r = call i8 @external_func(i8 10)
  ret i8 %r
}
