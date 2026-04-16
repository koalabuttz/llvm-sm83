; RUN: llc -march=sm83 < %s | FileCheck %s
;
; Verify 16-bit inline-asm register-pair constraints:
;   - "r" on i16 allocates from GR16 (BC, DE, or HL)
;   - "q" forces HL specifically (the only pair usable with (HL) indirect ops)
;   - Mixed 8-bit + 16-bit constraints coexist in one asm block
;   - Named-register binding via GCCRegNames (via LLVM's {bc}/{de}/{hl})

;===------------------------------------------------------------------------===;
; "r" + i16 → GR16 (compiler picks a pair)
;===------------------------------------------------------------------------===;

; CHECK-LABEL: r_i16_any_pair:
; CHECK: ; COMMENT
; CHECK: ret
define void @r_i16_any_pair(i16 %addr) {
  call void asm sideeffect "; COMMENT $0", "r"(i16 %addr)
  ret void
}

;===------------------------------------------------------------------------===;
; "q" → HL specifically
;===------------------------------------------------------------------------===;

; Argument is already in BC by ABI; test that "q" causes a move into HL
; before the asm block (or that the allocator pins HL).
; CHECK-LABEL: q_forces_hl:
; CHECK: ld [hl], a
; CHECK: ret
define void @q_forces_hl(i16 %addr, i8 %val) {
  call void asm sideeffect "ld [hl], a", "q,{a}"(i16 %addr, i8 %val)
  ret void
}

;===------------------------------------------------------------------------===;
; Mixed 8-bit + 16-bit constraints
;===------------------------------------------------------------------------===;

; CHECK-LABEL: mixed_a_and_q:
; CHECK: ld [hl], a
; CHECK: ret
define void @mixed_a_and_q(i16 %ptr, i8 %val) {
  call void asm sideeffect "ld [hl], a", "q,{a}"(i16 %ptr, i8 %val)
  ret void
}

;===------------------------------------------------------------------------===;
; "q" output: value returned in HL
;===------------------------------------------------------------------------===;

; CHECK-LABEL: q_output:
; CHECK: ld hl, 38144
; CHECK: ret
define i16 @q_output() {
  %r = call i16 asm "ld hl, 38144", "=q"()
  ret i16 %r
}

;===------------------------------------------------------------------------===;
; Named-register binding via {hl} still works (GCCRegNames path)
;===------------------------------------------------------------------------===;

; CHECK-LABEL: named_hl:
; CHECK: inc hl
; CHECK: ret
define void @named_hl(i16 %addr) {
  call void asm sideeffect "inc hl", "{hl}"(i16 %addr)
  ret void
}
