; RUN: llc -march=sm83 < %s | FileCheck %s

; Test conditional branch (icmp eq).

define i8 @branch_eq(i8 %a, i8 %b) {
; CHECK-LABEL: branch_eq:
; CHECK:       cp c
; CHECK:       jr
; CHECK:       ld a, 1
; CHECK:       ret
; CHECK:       ld a, 0
; CHECK:       ret
entry:
  %cmp = icmp eq i8 %a, %b
  br i1 %cmp, label %then, label %else
then:
  ret i8 1
else:
  ret i8 0
}
