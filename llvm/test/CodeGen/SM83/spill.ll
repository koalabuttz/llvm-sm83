; RUN: llc -march=sm83 < %s | FileCheck %s

; Register pressure — force spills to stack.

declare i8 @use_val(i8)

define i8 @pressure_test(i8 %a, i8 %b, i8 %c, i8 %d, i8 %e) {
; CHECK-LABEL: pressure_test:
; CHECK: add sp,
; CHECK: ret
entry:
  %v1 = add i8 %a, %b
  %v2 = add i8 %c, %d
  %v3 = add i8 %e, %a
  %v4 = add i8 %b, %c
  %v5 = add i8 %d, %e
  %v6 = add i8 %v1, %v2
  %v7 = add i8 %v3, %v4
  %v8 = add i8 %v5, %v6
  %r = add i8 %v7, %v8
  ret i8 %r
}

define i8 @live_across_call(i8 %x, i8 %y) {
; CHECK-LABEL: live_across_call:
; CHECK: call use_val
; CHECK: ret
  %a = call i8 @use_val(i8 %x)
  %b = add i8 %a, %y
  %c = call i8 @use_val(i8 %b)
  %d = add i8 %c, %x
  ret i8 %d
}
