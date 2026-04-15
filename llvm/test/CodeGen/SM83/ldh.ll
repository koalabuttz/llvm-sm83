; RUN: llc -march=sm83 -verify-machineinstrs < %s | FileCheck %s

; Test LDH (high-page) addressing for $FF00-range constants.
; Loads/stores to addresses $FF00-$FFFF should use LDH instructions
; (2 bytes) instead of the normal LD HL,nn + LD r,(HL) sequence (3 bytes).

; Load from $FF40 (LCD Control register)
define i8 @load_ff40() {
; CHECK-LABEL: load_ff40:
; CHECK:       ldh a, [64]
; CHECK:       ret
  %ptr = inttoptr i16 65344 to ptr  ; 0xFF40
  %val = load i8, ptr %ptr
  ret i8 %val
}

; Store to $FF40
define void @store_ff40(i8 %val) {
; CHECK-LABEL: store_ff40:
; CHECK:       ldh [64], a
; CHECK-NEXT:  ret
  %ptr = inttoptr i16 65344 to ptr  ; 0xFF40
  store i8 %val, ptr %ptr
  ret void
}

; Load from $FF00 (Joypad register) — lowest valid LDH address
define i8 @load_ff00() {
; CHECK-LABEL: load_ff00:
; CHECK:       ldh a, [0]
; CHECK:       ret
  %ptr = inttoptr i16 65280 to ptr  ; 0xFF00
  %val = load i8, ptr %ptr
  ret i8 %val
}

; Store to $FFFF (Interrupt Enable register) — highest valid LDH address
define void @store_ffff(i8 %val) {
; CHECK-LABEL: store_ffff:
; CHECK:       ldh [-1], a
; CHECK-NEXT:  ret
  %ptr = inttoptr i16 65535 to ptr  ; 0xFFFF
  store i8 %val, ptr %ptr
  ret void
}

; Load from $FF44 (LY - LCD Y coordinate)
define i8 @load_ly() {
; CHECK-LABEL: load_ly:
; CHECK:       ldh a, [68]
; CHECK:       ret
  %ptr = inttoptr i16 65348 to ptr  ; 0xFF44
  %val = load i8, ptr %ptr
  ret i8 %val
}
