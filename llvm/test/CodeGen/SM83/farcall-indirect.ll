; RUN: llc -march=sm83 < %s | FileCheck %s
;
; Round 8 item 1: indirect calls through a function pointer load from
; memory. The pre-Round-8 LowerCall path did CopyToReg(HL, fn_ptr) but
; failed to declare HL as a live-in register for the CALL node, so
; regalloc could let HL go dead before the call was emitted. Fix adds
; HL to the CALL's Ops, keeping the function pointer live across any
; intervening argument-setup code that also uses HL.

; Indirect call through a function pointer read from memory. The HL
; register must carry the pointer into __sm83_icall_hl.

define i8 @indirect_from_mem(ptr %slot, i8 %arg) {
; CHECK-LABEL: indirect_from_mem:
; CHECK: ld l, {{[abcdehl]}}
; CHECK: ld h, {{[abcdehl]}}
; CHECK: call __sm83_icall_hl
  %fp = load ptr, ptr %slot, align 1
  %r = call i8 %fp(i8 %arg)
  ret i8 %r
}
