; RUN: llvm-mc -triple sm83 -show-encoding < %s | FileCheck %s
; RUN: llvm-mc -filetype=obj -triple sm83 < %s | llvm-objdump -d - | FileCheck --check-prefix=CHECK-INST %s

; Tests for SP-specific instructions that use dedicated opcodes
; (SP is not in the GR16 register class).

; === LD SP, imm16 ===

ld sp, 65534
; CHECK: ld sp, 65534 ; encoding: [0x31,0xfe,0xff]
; CHECK-INST: ld sp, 65534

ld sp, 0
; CHECK: ld sp, 0     ; encoding: [0x31,0x00,0x00]
; CHECK-INST: ld sp, 0

; === ADD HL, SP ===

add hl, sp
; CHECK: add hl, sp   ; encoding: [0x39]
; CHECK-INST: add hl, sp

; === Other SP instructions (already existed) ===

ld sp, hl
; CHECK: ld sp, hl    ; encoding: [0xf9]
; CHECK-INST: ld sp, hl

inc sp
; CHECK: inc sp       ; encoding: [0x33]
; CHECK-INST: inc sp

dec sp
; CHECK: dec sp       ; encoding: [0x3b]
; CHECK-INST: dec sp

add sp, 127
; CHECK: add sp, 127  ; encoding: [0xe8,0x7f]
; CHECK-INST: add sp, 127

add sp, -128
; CHECK: add sp, -128 ; encoding: [0xe8,0x80]
; CHECK-INST: add sp, -128
