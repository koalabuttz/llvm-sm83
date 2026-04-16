; RUN: llc -march=sm83 < %s | FileCheck %s

; Store i8 constant-address path:
;   $FF00-$FFFF → LDH [n],A   (existing)
;   $0000-$FEFF → LD [nn],A   (LDnn_A, Round 8 item 2)

; ---------- low-range constant stores go through LDnn_A, not HL ----------
define void @store_bank_sel(i8 %v) {
; CHECK-LABEL: store_bank_sel:
; CHECK: ld [8192], a
; CHECK-NOT: ld hl,
  store volatile i8 %v, ptr inttoptr (i16 8192 to ptr), align 1
  ret void
}

define void @store_mbc5_high(i8 %v) {
; CHECK-LABEL: store_mbc5_high:
; CHECK: ld [12288], a
; CHECK-NOT: ld hl,
  store volatile i8 %v, ptr inttoptr (i16 12288 to ptr), align 1
  ret void
}

define void @store_latch(i8 %v) {
; CHECK-LABEL: store_latch:
; CHECK: ld [24576], a
; CHECK-NOT: ld hl,
  store volatile i8 %v, ptr inttoptr (i16 24576 to ptr), align 1
  ret void
}

define void @store_sram(i8 %v) {
; Address 40960 ($A000) prints as signed i16 (-24576) but encodes as 0xA000.
; CHECK-LABEL: store_sram:
; CHECK: ld [-24576], a
; CHECK-NOT: ld hl,
  store volatile i8 %v, ptr inttoptr (i16 40960 to ptr), align 1
  ret void
}

; ---------- multiple back-to-back stores: each one goes direct ----------
define void @rtc_latch() {
; CHECK-LABEL: rtc_latch:
; CHECK: ld [24576], a
; CHECK: ld [24576], a
; CHECK-NOT: ld hl,
  store volatile i8 0, ptr inttoptr (i16 24576 to ptr), align 1
  store volatile i8 1, ptr inttoptr (i16 24576 to ptr), align 1
  ret void
}

; ---------- $FF00-$FFFF still goes through LDH, not LDnn_A ----------
define void @store_ffxx(i8 %v) {
; CHECK-LABEL: store_ffxx:
; CHECK: ldh [70], a
  store volatile i8 %v, ptr inttoptr (i16 65350 to ptr), align 1
  ret void
}
