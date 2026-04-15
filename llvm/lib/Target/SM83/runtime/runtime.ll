; runtime.ll — SM83 integer arithmetic runtime library
;
; Compiled to sm83-runtime.o by build-llvm-sm83.sh:
;   llc -march=sm83 -filetype=obj runtime.ll -o sm83-runtime.o
;
; Since no SM83 asm parser exists, all functions are expressed in LLVM IR.
;
; Provides the GCC soft-integer libcall symbols that LLVM emits for targets
; without native multiply/divide/modulo:
;
;   __mulqi3                — unsigned  8-bit multiply
;   __mulhi3                — unsigned 16-bit multiply
;   __udivhi3 / __umodhi3  — unsigned 16-bit divide / modulo
;   __divhi3  / __modhi3   — signed   16-bit divide / modulo
;   __udivqi3 / __umodqi3  — unsigned  8-bit divide / modulo
;   __divqi3  / __modqi3   — signed    8-bit divide / modulo
;
; Algorithm: binary long division — O(N) for N-bit integers (N = 16 or 8).
;
; IMPORTANT: all shifts inside the loop are by the CONSTANT 1, so they
; compile to 1–2 shift-by-one instructions on SM83 (no inner shift loop).
; Bit extraction uses `icmp slt x, 0` (tests the sign bit) which is
; a single comparison.  Total cost: 16 iterations × ~10 SM83 instructions
; = ~160 bytes for 16-bit divide, ~80 bytes for 8-bit.  No worst-case blowup.
;
; SM83 calling convention (handled transparently by LLVM's CCState):
;   i8  args:  A, C, B         (first → A; DE is callee-saved)
;   i16 args:  BC              (first → BC; DE is callee-saved)
;   i8  return: A, C, B
;   i16 return: HL, BC

;===------------------------------------------------------------------------===;
; 8-bit unsigned multiply (shift-and-add)
;===------------------------------------------------------------------------===;
;
; Standard binary multiply: test each bit of %b, add %a (shifted) if set.
; 8 iterations × ~6 SM83 instructions = ~48 bytes.  All shifts are by
; constant 1 — no inner shift loop.

define i8 @__mulqi3(i8 %a, i8 %b) {
entry:
  br label %loop

loop:
  %i      = phi i8 [ 0,    %entry ], [ %i_next,  %loop ]
  %result = phi i8 [ 0,    %entry ], [ %r_next,  %loop ]
  %a_cur  = phi i8 [ %a,   %entry ], [ %a_shift, %loop ]
  %b_cur  = phi i8 [ %b,   %entry ], [ %b_shift, %loop ]

  %bit    = and i8 %b_cur, 1
  %do_add = icmp ne i8 %bit, 0
  %added  = add i8 %result, %a_cur
  %r_next = select i1 %do_add, i8 %added, i8 %result

  %a_shift = shl i8 %a_cur, 1
  %b_shift = lshr i8 %b_cur, 1
  %i_next  = add i8 %i, 1
  %done    = icmp eq i8 %i_next, 8
  br i1 %done, label %exit, label %loop

exit:
  ret i8 %r_next
}

;===------------------------------------------------------------------------===;
; 16-bit unsigned multiply (shift-and-add)
;===------------------------------------------------------------------------===;
;
; Same algorithm as __mulqi3, but 16 iterations for 16-bit operands.
; Used by __umodhi3 and __modhi3 (remainder = dividend - quotient * divisor).

define i16 @__mulhi3(i16 %a, i16 %b) {
entry:
  br label %loop

loop:
  %i      = phi i16 [ 0,    %entry ], [ %i_next,  %loop ]
  %result = phi i16 [ 0,    %entry ], [ %r_next,  %loop ]
  %a_cur  = phi i16 [ %a,   %entry ], [ %a_shift, %loop ]
  %b_cur  = phi i16 [ %b,   %entry ], [ %b_shift, %loop ]

  %bit    = and i16 %b_cur, 1
  %do_add = icmp ne i16 %bit, 0
  %added  = add i16 %result, %a_cur
  %r_next = select i1 %do_add, i16 %added, i16 %result

  %a_shift = shl i16 %a_cur, 1
  %b_shift = lshr i16 %b_cur, 1
  %i_next  = add i16 %i, 1
  %done    = icmp eq i16 %i_next, 16
  br i1 %done, label %exit, label %loop

exit:
  ret i16 %r_next
}

;===------------------------------------------------------------------------===;
; 16-bit unsigned divide
;===------------------------------------------------------------------------===;
;
; Walk the dividend MSB-first: each iteration shifts %work left by 1, testing
; whether the outgoing MSB (via `icmp slt %work, 0`) should flow into the
; running remainder.  All shifts are constant-1 — no inner shift loop.

define i16 @__udivhi3(i16 %dividend, i16 %divisor) {
entry:
  br label %loop

loop:
  %i         = phi i16 [ 0,         %entry ], [ %i_next,    %loop ]
  %q         = phi i16 [ 0,         %entry ], [ %q_next,    %loop ]
  %r         = phi i16 [ 0,         %entry ], [ %r_next,    %loop ]
  %work      = phi i16 [ %dividend, %entry ], [ %work_next, %loop ]

  ; Extract the MSB of %work (bit 15) without a variable shift:
  ; treat as signed — negative means bit 15 is set.
  %msb       = icmp slt i16 %work, 0
  %msb1      = zext i1 %msb to i16

  ; Shift remainder left by 1 and bring in this bit.
  %r1        = shl  i16 %r, 1
  %r2        = or   i16 %r1, %msb1

  ; Conditionally subtract divisor.
  %cmp       = icmp uge i16 %r2, %divisor
  %r_sub     = sub  i16 %r2, %divisor
  %r_next    = select i1 %cmp, i16 %r_sub, i16 %r2

  ; Shift quotient left by 1 and record this bit.
  %q1        = shl  i16 %q, 1
  %qbit      = zext i1 %cmp to i16
  %q_next    = or   i16 %q1, %qbit

  ; Advance to next bit of dividend.
  %work_next = shl  i16 %work, 1

  ; Iterate 16 times (bits 15 → 0).
  %i_next    = add  i16 %i, 1
  %done      = icmp eq i16 %i_next, 16
  br i1 %done, label %exit, label %loop

exit:
  ret i16 %q_next
}

;===------------------------------------------------------------------------===;
; 16-bit unsigned modulo
;===------------------------------------------------------------------------===;

define i16 @__umodhi3(i16 %dividend, i16 %divisor) {
  %q  = call i16 @__udivhi3(i16 %dividend, i16 %divisor)
  %qd = mul  i16 %q, %divisor
  %r  = sub  i16 %dividend, %qd
  ret i16 %r
}

;===------------------------------------------------------------------------===;
; 16-bit signed divide  (truncates toward zero — C semantics)
;===------------------------------------------------------------------------===;

define i16 @__divhi3(i16 %dividend, i16 %divisor) {
  ; Result is negative iff the signs differ (bit 15 of XOR).
  %xored     = xor  i16 %dividend, %divisor
  %neg_res   = icmp slt i16 %xored, 0

  ; abs(dividend)
  %dneg      = sub  i16 0, %dividend
  %d_isneg   = icmp slt i16 %dividend, 0
  %adividend = select i1 %d_isneg, i16 %dneg, i16 %dividend

  ; abs(divisor)
  %rneg      = sub  i16 0, %divisor
  %r_isneg   = icmp slt i16 %divisor, 0
  %adivisor  = select i1 %r_isneg, i16 %rneg, i16 %divisor

  %q         = call i16 @__udivhi3(i16 %adividend, i16 %adivisor)

  ; Negate quotient if result is negative.
  %qneg      = sub  i16 0, %q
  %result    = select i1 %neg_res, i16 %qneg, i16 %q
  ret i16 %result
}

;===------------------------------------------------------------------------===;
; 16-bit signed modulo  (remainder has same sign as dividend — C semantics)
;===------------------------------------------------------------------------===;

define i16 @__modhi3(i16 %dividend, i16 %divisor) {
  %d_isneg   = icmp slt i16 %dividend, 0

  %dneg      = sub  i16 0, %dividend
  %adividend = select i1 %d_isneg, i16 %dneg, i16 %dividend

  %r_isneg   = icmp slt i16 %divisor, 0
  %rneg      = sub  i16 0, %divisor
  %adivisor  = select i1 %r_isneg, i16 %rneg, i16 %divisor

  %r         = call i16 @__umodhi3(i16 %adividend, i16 %adivisor)

  ; Remainder takes sign of dividend.
  %rneg2     = sub  i16 0, %r
  %result    = select i1 %d_isneg, i16 %rneg2, i16 %r
  ret i16 %result
}

;===------------------------------------------------------------------------===;
; 8-bit unsigned divide
;===------------------------------------------------------------------------===;

define i8 @__udivqi3(i8 %dividend, i8 %divisor) {
entry:
  br label %loop

loop:
  %i         = phi i8 [ 0,         %entry ], [ %i_next,    %loop ]
  %q         = phi i8 [ 0,         %entry ], [ %q_next,    %loop ]
  %r         = phi i8 [ 0,         %entry ], [ %r_next,    %loop ]
  %work      = phi i8 [ %dividend, %entry ], [ %work_next, %loop ]

  %msb       = icmp slt i8 %work, 0
  %msb1      = zext i1 %msb to i8

  %r1        = shl  i8 %r, 1
  %r2        = or   i8 %r1, %msb1

  %cmp       = icmp uge i8 %r2, %divisor
  %r_sub     = sub  i8 %r2, %divisor
  %r_next    = select i1 %cmp, i8 %r_sub, i8 %r2

  %q1        = shl  i8 %q, 1
  %qbit      = zext i1 %cmp to i8
  %q_next    = or   i8 %q1, %qbit

  %work_next = shl  i8 %work, 1

  %i_next    = add  i8 %i, 1
  %done      = icmp eq i8 %i_next, 8
  br i1 %done, label %exit, label %loop

exit:
  ret i8 %q_next
}

;===------------------------------------------------------------------------===;
; 8-bit unsigned modulo
;===------------------------------------------------------------------------===;

define i8 @__umodqi3(i8 %dividend, i8 %divisor) {
  %q  = call i8 @__udivqi3(i8 %dividend, i8 %divisor)
  %qd = mul  i8 %q, %divisor
  %r  = sub  i8 %dividend, %qd
  ret i8 %r
}

;===------------------------------------------------------------------------===;
; 8-bit signed divide
;===------------------------------------------------------------------------===;

define i8 @__divqi3(i8 %dividend, i8 %divisor) {
  %xored     = xor  i8 %dividend, %divisor
  %neg_res   = icmp slt i8 %xored, 0

  %dneg      = sub  i8 0, %dividend
  %d_isneg   = icmp slt i8 %dividend, 0
  %adividend = select i1 %d_isneg, i8 %dneg, i8 %dividend

  %rneg      = sub  i8 0, %divisor
  %r_isneg   = icmp slt i8 %divisor, 0
  %adivisor  = select i1 %r_isneg, i8 %rneg, i8 %divisor

  %q         = call i8 @__udivqi3(i8 %adividend, i8 %adivisor)

  %qneg      = sub  i8 0, %q
  %result    = select i1 %neg_res, i8 %qneg, i8 %q
  ret i8 %result
}

;===------------------------------------------------------------------------===;
; 8-bit signed modulo
;===------------------------------------------------------------------------===;

define i8 @__modqi3(i8 %dividend, i8 %divisor) {
  %d_isneg   = icmp slt i8 %dividend, 0

  %dneg      = sub  i8 0, %dividend
  %adividend = select i1 %d_isneg, i8 %dneg, i8 %dividend

  %r_isneg   = icmp slt i8 %divisor, 0
  %rneg      = sub  i8 0, %divisor
  %adivisor  = select i1 %r_isneg, i8 %rneg, i8 %divisor

  %r         = call i8 @__umodqi3(i8 %adividend, i8 %adivisor)

  %rneg2     = sub  i8 0, %r
  %result    = select i1 %d_isneg, i8 %rneg2, i8 %r
  ret i8 %result
}
