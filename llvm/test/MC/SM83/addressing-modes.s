; RUN: llvm-mc -triple sm83 -show-encoding < %s | FileCheck %s
; RUN: llvm-mc -filetype=obj -triple sm83 < %s | llvm-objdump -d - | FileCheck --check-prefix=CHECK-INST %s

; Tests for all SM83 memory addressing modes.

; === [HL] indirect ===

ld a, [hl]
; CHECK: ld a, [hl]   ; encoding: [0x7e]
; CHECK-INST: ld a, [hl]

ld [hl], a
; CHECK: ld [hl], a   ; encoding: [0x77]
; CHECK-INST: ld [hl], a

ld [hl], 85
; CHECK: ld [hl], 85  ; encoding: [0x36,0x55]
; CHECK-INST: ld [hl], 85

add a, [hl]
; CHECK: add a, [hl]  ; encoding: [0x86]
; CHECK-INST: add a, [hl]

sub [hl]
; CHECK: sub [hl]     ; encoding: [0x96]
; CHECK-INST: sub [hl]

; === [BC] / [DE] indirect ===

ld a, [bc]
; CHECK: ld a, [bc]   ; encoding: [0x0a]
; CHECK-INST: ld a, [bc]

ld a, [de]
; CHECK: ld a, [de]   ; encoding: [0x1a]
; CHECK-INST: ld a, [de]

ld [bc], a
; CHECK: ld [bc], a   ; encoding: [0x02]
; CHECK-INST: ld [bc], a

ld [de], a
; CHECK: ld [de], a   ; encoding: [0x12]
; CHECK-INST: ld [de], a

; === [HL+] / [HL-] post-increment/decrement ===

ld a, [hl+]
; CHECK: ld a, [hl+]  ; encoding: [0x2a]
; CHECK-INST: ld a, [hl+]

ld a, [hl-]
; CHECK: ld a, [hl-]  ; encoding: [0x3a]
; CHECK-INST: ld a, [hl-]

ld [hl+], a
; CHECK: ld [hl+], a  ; encoding: [0x22]
; CHECK-INST: ld [hl+], a

ld [hl-], a
; CHECK: ld [hl-], a  ; encoding: [0x32]
; CHECK-INST: ld [hl-], a

; === Direct memory [addr] ===

ld a, [49152]
; CHECK: ld a, [49152] ; encoding: [0xfa,0x00,0xc0]
; CHECK-INST: ld a, [49152]

ld [49152], a
; CHECK: ld [49152], a ; encoding: [0xea,0x00,0xc0]
; CHECK-INST: ld [49152], a

ld [256], sp
; CHECK: ld [256], sp  ; encoding: [0x08,0x00,0x01]
; CHECK-INST: ld [256], sp

; === LDH high-page addressing ===

ldh a, [68]
; CHECK: ldh a, [68]   ; encoding: [0xf0,0x44]
; CHECK-INST: ldh a, [68]

ldh [128], a
; CHECK: ldh [128], a  ; encoding: [0xe0,0x80]
; CHECK-INST: ldh [128], a

; === LDH [C] — high-page via C register ===

ldh a, [c]
; CHECK: ldh a, [c]    ; encoding: [0xf2]
; CHECK-INST: ldh a, [c]

ldh [c], a
; CHECK: ldh [c], a    ; encoding: [0xe2]
; CHECK-INST: ldh [c], a
