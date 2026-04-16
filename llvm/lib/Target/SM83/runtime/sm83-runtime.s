; SM83 Runtime Library — Hand-written assembly routines
; Provides: indirect call trampoline, memcpy, memset, memmove.
;
; Arithmetic routines (mul, div, mod) are in runtime.ll (compiled by llc).
;
; Assembled with: llvm-mc -triple sm83 -filetype=obj sm83-runtime.s

; ==========================================================================
; __sm83_icall_hl — indirect call trampoline
; SM83 has no indirect CALL instruction.  The compiler lowers a call through
; a function pointer by copying the callee address into HL, then emitting
;   call __sm83_icall_hl
; This function jumps to HL, transferring control to the real callee.
; The callee returns directly to the original call site (via the return
; address already on the stack from the CALL instruction).
; ==========================================================================
	.global __sm83_icall_hl
__sm83_icall_hl:
	jp hl

; ==========================================================================
; memcpy — copy n bytes from src to dst
; SM83 CC: BC = dst (arg0), [SP+2] = src (arg1), [SP+4] = n (arg2)
; Out: HL = dst (return value, i16)
; Uses: A, B, C, D, E, H, L, F
; ==========================================================================
	.global memcpy
memcpy:
	push bc             ; save dst for return value
	; Load src into DE from stack (SP+4 after push = original SP+2)
	ld hl, sp + 4
	ld e, [hl]
	inc hl
	ld d, [hl]
	; Load n into HL from stack (SP+6 after push = original SP+4)
	ld hl, sp + 6
	ld a, [hl]
	inc hl
	ld h, [hl]
	ld l, a             ; HL = n
	; If n == 0, return
	ld a, h
	or l
	jr z, .memcpy_done
	; Copy loop: LD A,[DE] / LD [BC],A / INC DE / INC BC / DEC HL
.memcpy_loop:
	ld a, [de]          ; A = *src
	ld [bc], a          ; *dst = A
	inc de              ; src++
	inc bc              ; dst++
	dec hl              ; n--
	ld a, h
	or l
	jr nz, .memcpy_loop
.memcpy_done:
	pop hl              ; HL = original dst (return value)
	ret

; ==========================================================================
; memset — fill n bytes at dst with value
; SM83 CC: BC = dst (arg0), [SP+2] = val (arg1, i16 but only low byte used),
;          [SP+4] = n (arg2)
; Out: HL = dst (return value, i16)
; Uses: A, B, C, D, E, H, L, F
; ==========================================================================
	.global memset
memset:
	push bc             ; save dst for return value
	; Load fill value from stack (SP+4 after push = original SP+2)
	ld hl, sp + 4
	ld a, [hl]          ; A = fill value (low byte of int)
	; Load n into DE from stack (SP+6 after push = original SP+4)
	ld hl, sp + 6
	ld e, [hl]
	inc hl
	ld d, [hl]          ; DE = n
	; If n == 0, return
	ld a, d
	or e
	jr z, .memset_done
	; Reload fill value (A was consumed by zero-check above)
	ld hl, sp + 4
	ld a, [hl]
	; Use HL as pointer (copy dst from BC)
	ld h, b
	ld l, c
	ld b, a             ; B = fill value
.memset_loop:
	ld a, b             ; reload fill value
	ld [hl+], a         ; *dst++ = value
	dec de              ; n--
	ld a, d
	or e
	jr nz, .memset_loop
.memset_done:
	pop hl              ; HL = original dst (return value)
	ret

; ==========================================================================
; memmove — copy n bytes, handles overlapping regions
; SM83 CC: BC = dst (arg0), [SP+2] = src (arg1), [SP+4] = n (arg2)
; Out: HL = dst (return value, i16)
; Uses: A, B, C, D, E, H, L, F
; Strategy: if dst <= src, forward copy (same as memcpy).
;           if dst > src, backward copy from end.
; ==========================================================================
	.global memmove
memmove:
	push bc             ; save dst for return value
	; Load src into DE
	ld hl, sp + 4
	ld e, [hl]
	inc hl
	ld d, [hl]
	; Load n into HL
	ld hl, sp + 6
	ld a, [hl]
	inc hl
	ld h, [hl]
	ld l, a             ; HL = n
	; If n == 0, return
	ld a, h
	or l
	jr z, .memmove_done
	; Compare dst (BC) vs src (DE): if dst <= src, forward copy
	ld a, c
	sub e
	ld a, b
	sbc a, d            ; carry if BC < DE
	jr c, .memmove_fwd
	; Check BC == DE
	ld a, c
	xor e
	ld l, a
	ld a, b
	xor d
	or l
	jr z, .memmove_done ; dst == src, nothing to do
	; Backward copy: start from dst+n-1, src+n-1
	; Reload n from stack
	ld hl, sp + 6
	ld a, [hl]
	inc hl
	ld h, [hl]
	ld l, a             ; HL = n
	; DE = src + n - 1
	push hl             ; save n
	add hl, de          ; HL = src + n
	dec hl              ; HL = src + n - 1
	ld d, h
	ld e, l             ; DE = src + n - 1
	; BC = dst + n - 1
	pop hl              ; HL = n
	push hl             ; keep n on stack
	add hl, bc          ; HL = dst + n
	dec hl              ; HL = dst + n - 1
	ld b, h
	ld c, l             ; BC = dst + n - 1
	pop hl              ; HL = n
.memmove_bwd:
	ld a, [de]          ; A = *src_end
	ld [bc], a          ; *dst_end = A
	dec de              ; src_end--
	dec bc              ; dst_end--
	dec hl              ; n--
	ld a, h
	or l
	jr nz, .memmove_bwd
	jr .memmove_done
.memmove_fwd:
	; Forward copy (same as memcpy loop)
	; Reload n
	ld hl, sp + 6
	ld a, [hl]
	inc hl
	ld h, [hl]
	ld l, a             ; HL = n
.memmove_fwd_loop:
	ld a, [de]
	ld [bc], a
	inc de
	inc bc
	dec hl
	ld a, h
	or l
	jr nz, .memmove_fwd_loop
.memmove_done:
	pop hl              ; HL = original dst
	ret
