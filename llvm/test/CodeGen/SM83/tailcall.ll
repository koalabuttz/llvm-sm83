; RUN: llc -march=sm83 < %s | FileCheck %s

; Tail call optimization via peephole: CALL+RET → JP.

declare void @target_func()

define void @simple_tail_call() {
; CHECK-LABEL: simple_tail_call:
; CHECK: jp target_func
; CHECK-NOT: ret
  call void @target_func()
  ret void
}
