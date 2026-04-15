; RUN: llvm-mc -triple sm83 -filetype=obj %s | llvm-objdump -d - | FileCheck %s

; Round-trip test: assemble → disassemble → verify.

; -- Basic instructions --
; CHECK: nop
; CHECK: halt
; CHECK: di
; CHECK: ei
	nop
	halt
	di
	ei

; -- 8-bit loads --
; CHECK: ld b, c
; CHECK: ld a, 42
; CHECK: ld a, [hl]
; CHECK: ld [hl], b
	ld b, c
	ld a, 42
	ld a, [hl]
	ld [hl], b

; -- 16-bit loads --
; CHECK: ld bc, 1000
; CHECK: ld de, 2000
; CHECK: ld hl, 32768
; CHECK: ld sp, 57343
	ld bc, 1000
	ld de, 2000
	ld hl, 32768
	ld sp, 57343

; -- ALU --
; CHECK: add a, b
; CHECK: sub c
; CHECK: and d
; CHECK: or e
; CHECK: xor h
; CHECK: cp l
; CHECK: adc a, a
; CHECK: sbc a, b
	add a, b
	sub c
	and d
	or e
	xor h
	cp l
	adc a, a
	sbc a, b

; -- ALU immediate --
; CHECK: add a, 10
; CHECK: sub 20
; CHECK: and 240
; CHECK: cp 0
	add a, 10
	sub 20
	and 0xf0
	cp 0

; -- Inc/Dec --
; CHECK: inc b
; CHECK: dec c
; CHECK: inc de
; CHECK: dec hl
	inc b
	dec c
	inc de
	dec hl

; -- 16-bit add --
; CHECK: add hl, bc
; CHECK: add hl, de
; CHECK: add hl, sp
	add hl, bc
	add hl, de
	add hl, sp

; -- Stack operations --
; CHECK: push bc
; CHECK: pop de
; CHECK: push af
; CHECK: pop af
	push bc
	pop de
	push af
	pop af

; -- Indirect loads (A only) --
; CHECK: ld a, [bc]
; CHECK: ld a, [de]
; CHECK: ld [bc], a
; CHECK: ld [de], a
; CHECK: ld a, [hl+]
; CHECK: ld a, [hl-]
; CHECK: ld [hl+], a
; CHECK: ld [hl-], a
	ld a, [bc]
	ld a, [de]
	ld [bc], a
	ld [de], a
	ld a, [hl+]
	ld a, [hl-]
	ld [hl+], a
	ld [hl-], a

; -- LDH (high page) --
; CHECK: ldh a, [68]
; CHECK: ldh [64], a
	ldh a, [0x44]
	ldh [0x40], a

; -- Rotate A --
; CHECK: rlca
; CHECK: rrca
; CHECK: rla
; CHECK: rra
	rlca
	rrca
	rla
	rra

; -- Misc --
; CHECK: daa
; CHECK: cpl
; CHECK: scf
; CHECK: ccf
	daa
	cpl
	scf
	ccf

; -- RST --
; CHECK: rst 0
; CHECK: rst 8
; CHECK: rst 56
	rst 0
	rst 8
	rst 56

; -- Return --
; CHECK: ret
; CHECK: reti
	ret
	reti
