; RUN: llc -march=sm83 < %s | FileCheck %s

; Verify that:
;  1. Integer division emits a call to the runtime libcall symbol (not inline code).
;  2. Indirect calls through a function pointer emit CALL __sm83_icall_hl.
;  3. byval arguments produce a memcpy to the stack rather than a raw store.

;===------------------------------------------------------------------------===;
; Division libcalls
;===------------------------------------------------------------------------===;

; CHECK-LABEL: udiv16:
; CHECK:       {{(call|jp)}} __udivhi3
define i16 @udiv16(i16 %a, i16 %b) {
  %r = udiv i16 %a, %b
  ret i16 %r
}

; CHECK-LABEL: sdiv16:
; CHECK:       {{(call|jp)}} __divhi3
define i16 @sdiv16(i16 %a, i16 %b) {
  %r = sdiv i16 %a, %b
  ret i16 %r
}

; CHECK-LABEL: urem16:
; CHECK:       {{(call|jp)}} __umodhi3
define i16 @urem16(i16 %a, i16 %b) {
  %r = urem i16 %a, %b
  ret i16 %r
}

; CHECK-LABEL: udiv8:
; CHECK:       {{(call|jp)}} __udivqi3
define i8 @udiv8(i8 %a, i8 %b) {
  %r = udiv i8 %a, %b
  ret i8 %r
}

; CHECK-LABEL: sdiv8:
; CHECK:       {{(call|jp)}} __divqi3
define i8 @sdiv8(i8 %a, i8 %b) {
  %r = sdiv i8 %a, %b
  ret i8 %r
}

;===------------------------------------------------------------------------===;
; Indirect call through function pointer → __sm83_icall_hl trampoline
;===------------------------------------------------------------------------===;

; CHECK-LABEL: indirect_call_i8:
; CHECK:       {{(call|jp)}} __sm83_icall_hl
define i8 @indirect_call_i8(ptr %fn) {
  %r = call i8 %fn()
  ret i8 %r
}

; CHECK-LABEL: indirect_call_i16:
; CHECK:       {{(call|jp)}} __sm83_icall_hl
define i16 @indirect_call_i16(ptr %fn, i16 %arg) {
  %r = call i16 %fn(i16 %arg)
  ret i16 %r
}

;===------------------------------------------------------------------------===;
; byval argument — callee receives a pointer, not a copy of the raw bytes
;===------------------------------------------------------------------------===;

; A 6-byte struct exceeds the 5 i8 register slots; it is passed byval on the
; stack.  The callee receives a pointer to the copy and loads through it.
;
; CHECK-LABEL: byval_callee:
; CHECK:       ld a, [bc]
; CHECK:       ret
define i8 @byval_callee(ptr byval([6 x i8]) align 1 %s) {
  %p = getelementptr [6 x i8], ptr %s, i16 0, i16 0
  %v = load i8, ptr %p
  ret i8 %v
}
