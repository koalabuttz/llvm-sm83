; runtime.ll — Minimal SM83 runtime library in LLVM IR
; Implements __mulqi3 (8-bit multiply) needed by the test ROM.

target datalayout = "e-p:16:8-i8:8-i16:8-n8-S8"
target triple = "sm83-unknown-none"

; 8-bit multiply via shift-and-add
define i8 @__mulqi3(i8 %a, i8 %b) noinline {
entry:
  br label %loop

loop:
  %result = phi i8 [0, %entry], [%next_result, %loop_end]
  %multiplicand = phi i8 [%a, %entry], [%next_mc, %loop_end]
  %multiplier = phi i8 [%b, %entry], [%next_mp, %loop_end]
  %counter = phi i8 [8, %entry], [%next_cnt, %loop_end]
  ; Check low bit of multiplier
  %bit = and i8 %multiplier, 1
  %has_bit = icmp ne i8 %bit, 0
  br i1 %has_bit, label %do_add, label %skip_add

do_add:
  %added = add i8 %result, %multiplicand
  br label %loop_end

skip_add:
  br label %loop_end

loop_end:
  %next_result = phi i8 [%added, %do_add], [%result, %skip_add]
  %next_mc = shl i8 %multiplicand, 1
  %next_mp = lshr i8 %multiplier, 1
  %next_cnt = sub i8 %counter, 1
  %done = icmp eq i8 %next_cnt, 0
  br i1 %done, label %exit, label %loop

exit:
  ret i8 %next_result
}
