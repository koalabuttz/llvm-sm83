; RUN: llc -march=sm83 < %s | FileCheck %s

; Test that array indexing loops generate correct code.
; The loop counter increment must use INC (not ADD through accumulator),
; and the sum accumulation must not be corrupted by the counter increment.

define i8 @sum_array(ptr %arr, i8 %len) {
; CHECK-LABEL: sum_array:
; CHECK:       .LBB0_2:
; CHECK:       inc {{[bcdehl]}}
; CHECK:       jr c, .LBB0_2
entry:
  %cmp0 = icmp eq i8 %len, 0
  br i1 %cmp0, label %exit, label %loop

loop:
  %i = phi i8 [0, %entry], [%inc, %loop]
  %sum = phi i8 [0, %entry], [%newsum, %loop]
  %idx = zext i8 %i to i16
  %ptr = getelementptr i8, ptr %arr, i16 %idx
  %val = load i8, ptr %ptr
  %newsum = add i8 %sum, %val
  %inc = add i8 %i, 1
  %cmp = icmp ult i8 %inc, %len
  br i1 %cmp, label %loop, label %exit

exit:
  %result = phi i8 [0, %entry], [%newsum, %loop]
  ret i8 %result
}

; Test simple countdown loop uses DEC.

define i8 @countdown(i8 %n) {
; CHECK-LABEL: countdown:
; CHECK:       .LBB1_1:
; CHECK:       dec {{[abcdehl]}}
; CHECK:       jr nz, .LBB1_1
entry:
  br label %loop

loop:
  %i = phi i8 [%n, %entry], [%dec, %loop]
  %dec = add i8 %i, -1
  %cmp = icmp ne i8 %dec, 0
  br i1 %cmp, label %loop, label %exit

exit:
  ret i8 %dec
}
