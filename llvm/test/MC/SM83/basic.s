; RUN: llvm-mc -triple sm83 -show-encoding < %s | FileCheck %s
; RUN: llvm-mc -filetype=obj -triple sm83 < %s | llvm-objdump -d - | FileCheck --check-prefix=CHECK-INST %s

; === Misc single-byte ===

nop
; CHECK: nop  ; encoding: [0x00]
; CHECK-INST: nop

halt
; CHECK: halt ; encoding: [0x76]
; CHECK-INST: halt

di
; CHECK: di   ; encoding: [0xf3]
; CHECK-INST: di

ei
; CHECK: ei   ; encoding: [0xfb]
; CHECK-INST: ei

daa
; CHECK: daa  ; encoding: [0x27]
; CHECK-INST: daa

cpl
; CHECK: cpl  ; encoding: [0x2f]
; CHECK-INST: cpl

scf
; CHECK: scf  ; encoding: [0x37]
; CHECK-INST: scf

ccf
; CHECK: ccf  ; encoding: [0x3f]
; CHECK-INST: ccf

; === Rotate A ===

rlca
; CHECK: rlca ; encoding: [0x07]
; CHECK-INST: rlca

rrca
; CHECK: rrca ; encoding: [0x0f]
; CHECK-INST: rrca

rla
; CHECK: rla  ; encoding: [0x17]
; CHECK-INST: rla

rra
; CHECK: rra  ; encoding: [0x1f]
; CHECK-INST: rra

; === 8-bit register loads ===

ld b, c
; CHECK: ld b, c ; encoding: [0x41]
; CHECK-INST: ld b, c

ld a, d
; CHECK: ld a, d ; encoding: [0x7a]
; CHECK-INST: ld a, d

ld h, l
; CHECK: ld h, l ; encoding: [0x65]
; CHECK-INST: ld h, l

; === 8-bit immediate loads ===

ld a, 255
; CHECK: ld a, 255 ; encoding: [0x3e,0xff]
; CHECK-INST: ld a, 255

ld b, 42
; CHECK: ld b, 42  ; encoding: [0x06,0x2a]
; CHECK-INST: ld b, 42

; === 8-bit ALU ===

add a, b
; CHECK: add a, b  ; encoding: [0x80]
; CHECK-INST: add a, b

adc a, c
; CHECK: adc a, c  ; encoding: [0x89]
; CHECK-INST: adc a, c

sub d
; CHECK: sub d      ; encoding: [0x92]
; CHECK-INST: sub d

sbc a, e
; CHECK: sbc a, e  ; encoding: [0x9b]
; CHECK-INST: sbc a, e

and h
; CHECK: and h      ; encoding: [0xa4]
; CHECK-INST: and h

or l
; CHECK: or l       ; encoding: [0xb5]
; CHECK-INST: or l

xor a
; CHECK: xor a      ; encoding: [0xaf]
; CHECK-INST: xor a

cp b
; CHECK: cp b       ; encoding: [0xb8]
; CHECK-INST: cp b

; === ALU with immediate ===

add a, 10
; CHECK: add a, 10  ; encoding: [0xc6,0x0a]
; CHECK-INST: add a, 10

sub 20
; CHECK: sub 20     ; encoding: [0xd6,0x14]
; CHECK-INST: sub 20

; === INC / DEC ===

inc b
; CHECK: inc b      ; encoding: [0x04]
; CHECK-INST: inc b

dec c
; CHECK: dec c      ; encoding: [0x0d]
; CHECK-INST: dec c

inc hl
; CHECK: inc hl     ; encoding: [0x23]
; CHECK-INST: inc hl

dec de
; CHECK: dec de     ; encoding: [0x1b]
; CHECK-INST: dec de

inc sp
; CHECK: inc sp     ; encoding: [0x33]
; CHECK-INST: inc sp

dec sp
; CHECK: dec sp     ; encoding: [0x3b]
; CHECK-INST: dec sp

; === 16-bit loads ===

ld bc, 4660
; CHECK: ld bc, 4660 ; encoding: [0x01,0x34,0x12]
; CHECK-INST: ld bc, 4660

ld de, 22136
; CHECK: ld de, 22136 ; encoding: [0x11,0x78,0x56]
; CHECK-INST: ld de, 22136

ld hl, 39612
; CHECK: ld hl, 39612 ; encoding: [0x21,0xbc,0x9a]
; CHECK-INST: ld hl, 39612

ld sp, hl
; CHECK: ld sp, hl   ; encoding: [0xf9]
; CHECK-INST: ld sp, hl

; === 16-bit ALU ===

add hl, bc
; CHECK: add hl, bc  ; encoding: [0x09]
; CHECK-INST: add hl, bc

add hl, de
; CHECK: add hl, de  ; encoding: [0x19]
; CHECK-INST: add hl, de

add hl, hl
; CHECK: add hl, hl  ; encoding: [0x29]
; CHECK-INST: add hl, hl

add sp, -1
; CHECK: add sp, -1  ; encoding: [0xe8,0xff]
; CHECK-INST: add sp, -1

add sp, 3
; CHECK: add sp, 3   ; encoding: [0xe8,0x03]
; CHECK-INST: add sp, 3

; === PUSH / POP ===

push bc
; CHECK: push bc     ; encoding: [0xc5]
; CHECK-INST: push bc

push af
; CHECK: push af     ; encoding: [0xf5]
; CHECK-INST: push af

pop de
; CHECK: pop de      ; encoding: [0xd1]
; CHECK-INST: pop de

pop hl
; CHECK: pop hl      ; encoding: [0xe1]
; CHECK-INST: pop hl

; === CB-prefix shift/rotate ===

rlc b
; CHECK: rlc b       ; encoding: [0xcb,0x00]
; CHECK-INST: rlc b

rrc c
; CHECK: rrc c       ; encoding: [0xcb,0x09]
; CHECK-INST: rrc c

rl d
; CHECK: rl d        ; encoding: [0xcb,0x12]
; CHECK-INST: rl d

rr e
; CHECK: rr e        ; encoding: [0xcb,0x1b]
; CHECK-INST: rr e

sla h
; CHECK: sla h       ; encoding: [0xcb,0x24]
; CHECK-INST: sla h

sra l
; CHECK: sra l       ; encoding: [0xcb,0x2d]
; CHECK-INST: sra l

srl a
; CHECK: srl a       ; encoding: [0xcb,0x3f]
; CHECK-INST: srl a

swap b
; CHECK: swap b      ; encoding: [0xcb,0x30]
; CHECK-INST: swap b

; === CB-prefix bit ops ===

bit 3, a
; CHECK: bit 3, a    ; encoding: [0xcb,0x5f]
; CHECK-INST: bit 3, a

bit 0, [hl]
; CHECK: bit 0, [hl] ; encoding: [0xcb,0x46]
; CHECK-INST: bit 0, [hl]

set 7, c
; CHECK: set 7, c    ; encoding: [0xcb,0xf9]
; CHECK-INST: set 7, c

set 1, [hl]
; CHECK: set 1, [hl] ; encoding: [0xcb,0xce]
; CHECK-INST: set 1, [hl]

res 5, d
; CHECK: res 5, d    ; encoding: [0xcb,0xaa]
; CHECK-INST: res 5, d

res 2, [hl]
; CHECK: res 2, [hl] ; encoding: [0xcb,0x96]
; CHECK-INST: res 2, [hl]

; === Control flow ===

ret
; CHECK: ret          ; encoding: [0xc9]
; CHECK-INST: ret

reti
; CHECK: reti         ; encoding: [0xd9]
; CHECK-INST: reti

rst 0
; CHECK: rst 0        ; encoding: [0xc7]
; CHECK-INST: rst 0

rst 56
; CHECK: rst 56       ; encoding: [0xff]
; CHECK-INST: rst 56

; === LD HL, SP + offset ===

ld hl, sp + 5
; CHECK: ld hl, sp + 5  ; encoding: [0xf8,0x05]
; CHECK-INST: ld hl, sp + 5

ld hl, sp + -2
; CHECK: ld hl, sp + -2 ; encoding: [0xf8,0xfe]
; CHECK-INST: ld hl, sp + -2

; === INC/DEC (HL) ===

inc [hl]
; CHECK: inc [hl]    ; encoding: [0x34]
; CHECK-INST: inc [hl]

dec [hl]
; CHECK: dec [hl]    ; encoding: [0x35]
; CHECK-INST: dec [hl]
