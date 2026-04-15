; RUN: llvm-mc -triple sm83 -show-encoding < %s | FileCheck %s
; RUN: llvm-mc -filetype=obj -triple sm83 < %s | llvm-objdump -d - | FileCheck --check-prefix=CHECK-INST %s

; Tests for condition code parsing and the "c" ambiguity.
; "c" is the carry condition code in jp/jr/call/ret, and register C elsewhere.

; === Conditional JP ===

jp nz, 512
; CHECK: jp nz, 512   ; encoding: [0xc2,0x00,0x02]
; CHECK-INST: jp nz, 512

jp z, 768
; CHECK: jp z, 768    ; encoding: [0xca,0x00,0x03]
; CHECK-INST: jp z, 768

jp nc, 1024
; CHECK: jp nc, 1024  ; encoding: [0xd2,0x00,0x04]
; CHECK-INST: jp nc, 1024

jp c, 1280
; CHECK: jp c, 1280   ; encoding: [0xda,0x00,0x05]
; CHECK-INST: jp c, 1280

; === Unconditional JP ===

jp 336
; CHECK: jp 336       ; encoding: [0xc3,0x50,0x01]
; CHECK-INST: jp 336

jp hl
; CHECK: jp hl        ; encoding: [0xe9]
; CHECK-INST: jp hl

; === Conditional JR ===

jr nz, 32
; CHECK: jr nz, 32    ; encoding: [0x20,0x20]
; CHECK-INST: jr nz,

jr z, 48
; CHECK: jr z, 48     ; encoding: [0x28,0x30]
; CHECK-INST: jr z,

jr nc, 64
; CHECK: jr nc, 64    ; encoding: [0x30,0x40]
; CHECK-INST: jr nc,

jr c, 16
; CHECK: jr c, 16     ; encoding: [0x38,0x10]
; CHECK-INST: jr c,

; === Unconditional JR ===

jr 16
; CHECK: jr 16        ; encoding: [0x18,0x10]
; CHECK-INST: jr {{[0-9]+}}

; === Conditional CALL ===

call nz, 8192
; CHECK: call nz, 8192 ; encoding: [0xc4,0x00,0x20]
; CHECK-INST: call nz, 8192

call z, 12288
; CHECK: call z, 12288 ; encoding: [0xcc,0x00,0x30]
; CHECK-INST: call z, 12288

call nc, 16384
; CHECK: call nc, 16384 ; encoding: [0xd4,0x00,0x40]
; CHECK-INST: call nc, 16384

call c, 4096
; CHECK: call c, 4096  ; encoding: [0xdc,0x00,0x10]
; CHECK-INST: call c, 4096

; === Unconditional CALL ===

call 4096
; CHECK: call 4096     ; encoding: [0xcd,0x00,0x10]
; CHECK-INST: call 4096

; === Conditional RET ===

ret nz
; CHECK: ret nz        ; encoding: [0xc0]
; CHECK-INST: ret nz

ret z
; CHECK: ret z         ; encoding: [0xc8]
; CHECK-INST: ret z

ret nc
; CHECK: ret nc        ; encoding: [0xd0]
; CHECK-INST: ret nc

ret c
; CHECK: ret c         ; encoding: [0xd8]
; CHECK-INST: ret c

; === "c" as register (NOT condition code) ===

ld a, c
; CHECK: ld a, c       ; encoding: [0x79]
; CHECK-INST: ld a, c

ld c, 42
; CHECK: ld c, 42      ; encoding: [0x0e,0x2a]
; CHECK-INST: ld c, 42

add a, c
; CHECK: add a, c      ; encoding: [0x81]
; CHECK-INST: add a, c

; === "c" inside brackets (high-page addressing, not condition code) ===

ldh a, [c]
; CHECK: ldh a, [c]    ; encoding: [0xf2]
; CHECK-INST: ldh a, [c]

ldh [c], a
; CHECK: ldh [c], a    ; encoding: [0xe2]
; CHECK-INST: ldh [c], a
