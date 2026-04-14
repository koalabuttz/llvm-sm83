; RUN: llc -march=sm83 < %s | FileCheck %s

; Test function with inter-function call.
declare i8 @callee(i8)

define i8 @caller(i8 %x) {
; CHECK-LABEL: caller:
; CHECK:       call callee
; CHECK:       ret
  %r = call i8 @callee(i8 %x)
  ret i8 %r
}

; Test function with local variables (stack frame).
define i8 @with_locals(i8 %x, i8 %y) {
; CHECK-LABEL: with_locals:
; CHECK:       add sp, -2
; CHECK:       add a,
; CHECK:       add sp, 2
; CHECK:       ret
  %a = alloca i8
  %b = alloca i8
  store i8 %x, ptr %a
  store i8 %y, ptr %b
  %va = load i8, ptr %a
  %vb = load i8, ptr %b
  %r = add i8 %va, %vb
  ret i8 %r
}

; Test i16 local variable (alloca i16).
define i16 @i16_local(i16 %x) {
; CHECK-LABEL: i16_local:
; CHECK:       add sp, -2
; CHECK:       ld [hl],
; CHECK:       inc hl
; CHECK:       ld [hl],
; CHECK:       add sp, 2
; CHECK:       ret
  %p = alloca i16
  store i16 %x, ptr %p
  %v = load i16, ptr %p
  ret i16 %v
}
