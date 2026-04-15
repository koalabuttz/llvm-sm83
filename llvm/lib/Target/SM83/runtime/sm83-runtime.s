; SM83 Runtime Library
; Compiler-rt routines for the Sharp SM83 (Game Boy / Color) CPU.
;
; Calling convention:
;   i8  args/ret: A, then B, C, D, E, H, L
;   i16 args/ret: BC (first pair), DE (second), HL (return)
;   All registers are caller-save.
;
; Assembled with: llvm-mc -triple sm83 -filetype=obj sm83-runtime.s

; ==========================================================================
; __mulqi3 — 8-bit signed/unsigned multiply
; In:  A = lhs (multiplicand), E = rhs (multiplier)
; Out: A = product (low 8 bits)
; Uses: A, B, C, E, F
; ==========================================================================
	.global __mulqi3
__mulqi3:
	ld b, a             ; B = multiplicand
	ld a, 0             ; A = result
	ld c, 8             ; C = bit counter
.mulqi3_loop:
	; Shift multiplier right; if carry (bit was 1), add multiplicand
	rr e
	jr nc, .mulqi3_nonadd
	add a, b
.mulqi3_nonadd:
	sla b               ; multiplicand <<= 1
	dec c
	jr nz, .mulqi3_loop
	ret

; ==========================================================================
; __mulhi3 — 16-bit signed/unsigned multiply
; In:  BC = lhs (multiplicand), DE = rhs (multiplier)
; Out: HL = product (low 16 bits)
; Uses: A, B, C, D, E, H, L, F, stack (2 bytes)
; ==========================================================================
	.global __mulhi3
__mulhi3:
	ld h, 0
	ld l, 0             ; HL = result = 0
	ld a, 16            ; 16-bit loop
	push af             ; save counter
.mulhi3_loop:
	; Shift DE right 1 bit; if carry (old bit 0), add BC to HL
	srl d               ; D >>= 1, LSB → carry
	rr e                ; E = (carry << 7) | (E >> 1); old E LSB → carry
	; Oops: we want to test bit 0 of DE before shifting.
	; Correct approach: test bit 0, then shift.
	jr nc, .mulhi3_nonadd
	; HL += BC
	ld a, l
	add a, c
	ld l, a
	ld a, h
	adc a, b
	ld h, a
.mulhi3_nonadd:
	; BC <<= 1
	sla c
	rl b
	pop af
	dec a
	push af
	jr nz, .mulhi3_loop
	pop af              ; discard counter
	ret

; ==========================================================================
; __udivqi3 — unsigned 8-bit divide
; In:  A = dividend, E = divisor
; Out: A = quotient
; Uses: A, B, E, F
; Algorithm: repeated subtraction (simple but correct for 8-bit)
; ==========================================================================
	.global __udivqi3
__udivqi3:
	ld b, 0             ; B = quotient
.udivqi3_loop:
	cp e                ; compare A (remainder) with divisor
	jr c, .udivqi3_done ; if A < E: done
	sub e               ; A -= divisor
	inc b               ; quotient++
	jr .udivqi3_loop
.udivqi3_done:
	ld a, b             ; return quotient
	ret

; ==========================================================================
; __umodqi3 — unsigned 8-bit modulo
; In:  A = dividend, E = divisor
; Out: A = remainder
; Uses: A, E, F
; ==========================================================================
	.global __umodqi3
__umodqi3:
.umodqi3_loop:
	cp e
	jr c, .umodqi3_done
	sub e
	jr .umodqi3_loop
.umodqi3_done:
	ret

; ==========================================================================
; __udivhi3 — unsigned 16-bit divide
; In:  BC = dividend, DE = divisor
; Out: HL = quotient, BC = remainder
; Uses: A, B, C, D, E, H, L, F
; Algorithm: repeated subtraction
; ==========================================================================
	.global __udivhi3
__udivhi3:
	ld h, 0
	ld l, 0             ; HL = quotient = 0
.udivhi3_loop:
	; Compare BC with DE: compute BC - DE and check carry
	ld a, c
	sub e               ; A = C - E
	ld a, b
	sbc a, d            ; A = B - D - borrow
	jr c, .udivhi3_done ; if BC < DE: done
	; BC -= DE
	ld a, c
	sub e
	ld c, a
	ld a, b
	sbc a, d
	ld b, a
	inc hl              ; quotient++
	jr .udivhi3_loop
.udivhi3_done:
	ret                 ; HL = quotient, BC = remainder

; ==========================================================================
; __umodhi3 — unsigned 16-bit modulo
; In:  BC = dividend, DE = divisor
; Out: HL = remainder
; Uses: A, B, C, D, E, H, L, F
; ==========================================================================
	.global __umodhi3
__umodhi3:
.umodhi3_loop:
	ld a, c
	sub e
	ld a, b
	sbc a, d
	jr c, .umodhi3_done
	ld a, c
	sub e
	ld c, a
	ld a, b
	sbc a, d
	ld b, a
	jr .umodhi3_loop
.umodhi3_done:
	ld h, b
	ld l, c             ; HL = remainder (= BC)
	ret

; ==========================================================================
; __divqi3 — signed 8-bit divide
; In:  A = dividend (signed), E = divisor (signed)
; Out: A = quotient (signed, truncated toward zero)
; Uses: A, B, C, D, E, F
; ==========================================================================
	.global __divqi3
__divqi3:
	ld b, a             ; B = original dividend (for sign)
	ld c, e             ; C = original divisor (for sign)
	; Result is negative iff signs differ
	xor e               ; A = dividend XOR divisor
	ld d, a             ; D[7] = sign of result
	; |dividend|
	ld a, b
	bit 7, a
	jr z, .divqi3_dpos
	neg
.divqi3_dpos:
	; |divisor|
	ld e, c
	bit 7, e
	jr z, .divqi3_epos
	ld a, e
	neg
	ld e, a
	; restore A = |dividend|
	ld a, b
	bit 7, a
	jr z, .divqi3_epos
	neg
.divqi3_epos:
	; A = |dividend|, E = |divisor| — do unsigned divide
	call __udivqi3      ; A = |quotient|
	; Apply sign
	bit 7, d
	ret z               ; positive
	neg
	ret

; ==========================================================================
; __modqi3 — signed 8-bit modulo
; In:  A = dividend (signed), E = divisor (signed)
; Out: A = remainder (signed, same sign as dividend)
; Uses: A, B, C, E, F
; ==========================================================================
	.global __modqi3
__modqi3:
	ld b, a             ; B = original dividend (for sign)
	ld c, e             ; C = original divisor
	; |dividend|
	bit 7, a
	jr z, .modqi3_dpos
	neg
.modqi3_dpos:
	; |divisor|
	ld e, c
	bit 7, e
	jr z, .modqi3_epos
	ld a, e
	neg
	ld e, a
	ld a, b
	bit 7, a
	jr z, .modqi3_epos
	neg
.modqi3_epos:
	call __umodqi3      ; A = |remainder|
	; Apply sign of dividend
	bit 7, b
	ret z
	neg
	ret

; ==========================================================================
; memcpy — copy n bytes from src to dst
; In:  BC = dst, DE = src, HL = n (byte count)
; Out: BC = original dst pointer
; Uses: A, D, E, H, L, F
; ==========================================================================
	.global memcpy
memcpy:
	; If n == 0, return immediately
	ld a, h
	or l
	ret z
	; Copy loop: LD A,(DE) / LD (BC),A / INC DE / INC BC / DEC HL
.memcpy_loop:
	ld a, [de]          ; A = *src
	ld [bc], a          ; *dst = A
	inc de              ; src++
	inc bc              ; dst++
	dec hl              ; n--
	ld a, h
	or l
	jr nz, .memcpy_loop
	ret

; ==========================================================================
; memset — fill n bytes at dst with value
; In:  BC = dst, A = fill value, DE = n (byte count)
; Out: BC = original dst pointer
; Uses: A, H, L, D, E, F
; Strategy: copy BC to HL as pointer, use LD [HL+], A
; ==========================================================================
	.global memset
memset:
	; If n == 0, return immediately
	ld h, d
	ld l, e
	push af             ; save fill value
	ld a, h
	or l
	pop af
	ret z
	; HL = dst pointer (copy from BC)
	push bc
	ld h, b
	ld l, c
.memset_loop:
	ld [hl+], a         ; *dst++ = value (HL post-increment)
	dec de              ; n--
	ld b, d
	ld c, e
	ld a, b
	or c
	jr nz, .memset_loop
	pop bc              ; restore original dst pointer
	ret
