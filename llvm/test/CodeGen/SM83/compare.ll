; RUN: llc -march=sm83 < %s | FileCheck %s

; Test unsigned comparison with branch.

define i8 @compare_ult(i8 %a, i8 %b) {
; CHECK-LABEL: compare_ult:
; CHECK:       cp
; CHECK:       ret
entry:
  %cmp = icmp ult i8 %a, %b
  br i1 %cmp, label %then, label %else
then:
  ret i8 1
else:
  ret i8 0
}
