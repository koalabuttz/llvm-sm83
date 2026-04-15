; RUN: llvm-mc -triple sm83 -filetype=obj %s | llvm-objdump -d - | FileCheck %s

; Test JR → JP relaxation for out-of-range branches.

	.text
	.globl test_relaxation
test_relaxation:
	; Near branch: should stay as JR (2 bytes)
; CHECK: jr
	jr .Lnear
	nop
.Lnear:
	nop

	; Far branch: should relax to JP (3 bytes)
	; Fill 130+ bytes to push the target out of JR range.
; CHECK: jp
	jr .Lfar
	.space 130
.Lfar:
	nop
	ret
