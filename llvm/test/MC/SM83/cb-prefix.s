; RUN: llvm-mc -triple sm83 -filetype=obj %s | llvm-objdump -d - | FileCheck %s

; CB-prefixed instructions: rotate, shift, bit, set, res.

; -- Rotate/Shift register B --
; CHECK: rlc b
; CHECK: rrc b
; CHECK: rl b
; CHECK: rr b
; CHECK: sla b
; CHECK: sra b
; CHECK: swap b
; CHECK: srl b
	rlc b
	rrc b
	rl b
	rr b
	sla b
	sra b
	swap b
	srl b

; -- Rotate/Shift register A --
; CHECK: rlc a
; CHECK: rrc a
; CHECK: rl a
; CHECK: rr a
; CHECK: sla a
; CHECK: sra a
; CHECK: swap a
; CHECK: srl a
	rlc a
	rrc a
	rl a
	rr a
	sla a
	sra a
	swap a
	srl a

; -- Rotate/Shift [HL] --
; CHECK: rlc [hl]
; CHECK: rrc [hl]
; CHECK: rl [hl]
; CHECK: rr [hl]
; CHECK: sla [hl]
; CHECK: sra [hl]
; CHECK: swap [hl]
; CHECK: srl [hl]
	rlc [hl]
	rrc [hl]
	rl [hl]
	rr [hl]
	sla [hl]
	sra [hl]
	swap [hl]
	srl [hl]

; -- BIT test --
; CHECK: bit 0, b
; CHECK: bit 3, c
; CHECK: bit 7, a
; CHECK: bit 5, [hl]
	bit 0, b
	bit 3, c
	bit 7, a
	bit 5, [hl]

; -- SET --
; CHECK: set 0, b
; CHECK: set 4, d
; CHECK: set 7, a
; CHECK: set 6, [hl]
	set 0, b
	set 4, d
	set 7, a
	set 6, [hl]

; -- RES --
; CHECK: res 0, b
; CHECK: res 2, e
; CHECK: res 7, a
; CHECK: res 3, [hl]
	res 0, b
	res 2, e
	res 7, a
	res 3, [hl]
