; RUN: llc -march=sm83 < %s | FileCheck %s
;
; Cross-block dead-load DCE: LD r,imm / LD r,r whose destination is not
; live at any use site gets deleted. The canonical repro is a countdown
; loop where register allocation sometimes emits a stray `ld c, 0` in
; the entry block even though C is never read — the intra-block peephole
; can't see across to the successor blocks, but the cross-block pass
; recomputes live-ins and catches it.

;===------------------------------------------------------------------------===;
; Countdown loop — classic stray `ld c, 0` regression
;===------------------------------------------------------------------------===;

; CHECK-LABEL: countdown:
; CHECK-NOT: ld c, 0
; CHECK: ld b, a
; CHECK: dec b
; CHECK: jr nz,
; CHECK: ld a, b
; CHECK: ret
define i8 @countdown(i8 %n) {
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

;===------------------------------------------------------------------------===;
; Dead load in a straight-line function: `ld c, 5` then never referenced.
;===------------------------------------------------------------------------===;

; CHECK-LABEL: straight_line:
; CHECK-NOT: ld c,
; CHECK: ld a, b
; CHECK: ret
define i8 @straight_line(i8 %x, i8 %y) {
  ; The `and` and `or` below use only %x, so %y (which gets allocated to B
  ; by the i8-arg CC) is the live value. Any spare load into C from the
  ; allocator's speculative prep should be eliminated.
  %r = and i8 %x, %y
  ret i8 %r
}

;===------------------------------------------------------------------------===;
; Live use of an ABI-delivered register must NOT be broken.
;===------------------------------------------------------------------------===;

; Third i8 argument lands in C per the SM83 CC. It's read by `add a, c`
; so the pass must not incorrectly flag it as dead. Verifies the safety
; net: if recomputeLiveIns misbehaves, the `add a, c` read would be broken.
;
; CHECK-LABEL: live_use:
; CHECK: add a, c
; CHECK: ret
define i8 @live_use(i8 %x, i8 %y, i8 %z) {
  %a = add i8 %x, %z
  %r = add i8 %a, %y
  ret i8 %r
}
