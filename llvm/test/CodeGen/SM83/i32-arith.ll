; RUN: llc -march=sm83 < %s | FileCheck %s

; i32 arithmetic — tests multi-register decomposition and libcalls.

define i32 @add32(i32 %a, i32 %b) {
; CHECK-LABEL: add32:
; CHECK: add hl, de
; CHECK: ret
  %r = add i32 %a, %b
  ret i32 %r
}

define i32 @sub32(i32 %a, i32 %b) {
; CHECK-LABEL: sub32:
; CHECK: sub
; CHECK: sbc
; CHECK: ret
  %r = sub i32 %a, %b
  ret i32 %r
}

define i32 @mul32(i32 %a, i32 %b) {
; CHECK-LABEL: mul32:
; CHECK: call __mulsi3
; CHECK: ret
  %r = mul i32 %a, %b
  ret i32 %r
}

define i32 @udiv32(i32 %a, i32 %b) {
; CHECK-LABEL: udiv32:
; CHECK: call __udivsi3
; CHECK: ret
  %r = udiv i32 %a, %b
  ret i32 %r
}

define i32 @urem32(i32 %a, i32 %b) {
; CHECK-LABEL: urem32:
; CHECK: call __umodsi3
; CHECK: ret
  %r = urem i32 %a, %b
  ret i32 %r
}

define i32 @and32(i32 %a, i32 %b) {
; CHECK-LABEL: and32:
; CHECK: and
; CHECK: ret
  %r = and i32 %a, %b
  ret i32 %r
}

define i32 @or32(i32 %a, i32 %b) {
; CHECK-LABEL: or32:
; CHECK: or
; CHECK: ret
  %r = or i32 %a, %b
  ret i32 %r
}

define i32 @shl32(i32 %a) {
; CHECK-LABEL: shl32:
; CHECK: sla
; CHECK: rl
; CHECK: ret
  %r = shl i32 %a, 1
  ret i32 %r
}

define i1 @icmp_eq32(i32 %a, i32 %b) {
; CHECK-LABEL: icmp_eq32:
; CHECK: xor
; CHECK: ret
  %r = icmp eq i32 %a, %b
  ret i1 %r
}

define i1 @icmp_ult32(i32 %a, i32 %b) {
; CHECK-LABEL: icmp_ult32:
; CHECK: sbc
; CHECK: ret
  %r = icmp ult i32 %a, %b
  ret i1 %r
}
