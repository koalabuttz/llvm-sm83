; RUN: llc -march=sm83 < %s | FileCheck %s

; Test unsigned less-than.
define i8 @cmp_ult(i8 %a, i8 %b) {
; CHECK-LABEL: cmp_ult:
; CHECK:       cp
; CHECK:       ret
entry:
  %cmp = icmp ult i8 %a, %b
  br i1 %cmp, label %then, label %else
then: ret i8 1
else: ret i8 0
}

; Test unsigned greater-than (swaps operands).
define i8 @cmp_ugt(i8 %a, i8 %b) {
; CHECK-LABEL: cmp_ugt:
; CHECK:       cp
; CHECK:       ret
entry:
  %cmp = icmp ugt i8 %a, %b
  br i1 %cmp, label %then, label %else
then: ret i8 1
else: ret i8 0
}

; Test signed less-than (uses XOR 0x80 sign flip).
define i8 @cmp_slt(i8 %a, i8 %b) {
; CHECK-LABEL: cmp_slt:
; CHECK:       xor
; CHECK:       cp
; CHECK:       ret
entry:
  %cmp = icmp slt i8 %a, %b
  br i1 %cmp, label %then, label %else
then: ret i8 1
else: ret i8 0
}

; Test signed greater-than.
define i8 @cmp_sgt(i8 %a, i8 %b) {
; CHECK-LABEL: cmp_sgt:
; CHECK:       xor
; CHECK:       cp
; CHECK:       ret
entry:
  %cmp = icmp sgt i8 %a, %b
  br i1 %cmp, label %then, label %else
then: ret i8 1
else: ret i8 0
}

; Test 16-bit unsigned less-than.
define i8 @cmp_i16_ult(i16 %a, i16 %b) {
; CHECK-LABEL: cmp_i16_ult:
; CHECK:       cp
; CHECK:       sbc a,
; CHECK:       ret
  %cmp = icmp ult i16 %a, %b
  %r = zext i1 %cmp to i8
  ret i8 %r
}

; Test 16-bit equality.
define i8 @cmp_i16_eq(i16 %a, i16 %b) {
; CHECK-LABEL: cmp_i16_eq:
; CHECK:       xor
; CHECK:       or
; CHECK:       ret
  %cmp = icmp eq i16 %a, %b
  %r = zext i1 %cmp to i8
  ret i8 %r
}
