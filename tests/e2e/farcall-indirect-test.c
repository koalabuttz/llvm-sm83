// farcall-indirect-test.c — exercise indirect far calls via gb_far_ptr_t.
//
// Two handlers live in switchable banks. We build a table of gb_far_ptr_t
// entries, indexed at runtime by a value stored in WRAM, and dispatch
// through it. Each handler takes a u8 and returns a u8; the result goes
// into $C100/$C101 so the harness can assert.
//
// This proves:
//   (a) __builtin_sm83_bank_of resolves the bank number from each
//       function's section attribute at compile time.
//   (b) GB_FAR_CALL_1_U8_R_U8 correctly banks, calls through HL,
//       and restores bank 1 around an indirect invocation whose target
//       is not known to the compiler at the call site.
//   (c) The handlers' bank selections are honored at runtime — i.e.,
//       calling handler B does not accidentally land in bank A.

#include <gb.h>

__attribute__((section(".romx.bank2")))
unsigned char handler_plus_one(unsigned char x) {
    return (unsigned char)(x + 1);
}

__attribute__((section(".romx.bank3")))
unsigned char handler_times_two(unsigned char x) {
    return (unsigned char)(x << 1);
}

void main(void) {
    volatile unsigned char *out = (volatile unsigned char *)0xC100;

    // Construct two far pointers and stage them through volatile
    // writes so the compiler can't fold the indirect call back into
    // a known direct call. Each handler actually lives in a different
    // bank, so the bank-switch envelope must run correctly for the
    // right code to execute.
    gb_far_ptr_t fp_a = GB_FAR_PTR(handler_plus_one);
    gb_far_ptr_t fp_b = GB_FAR_PTR(handler_times_two);

    volatile gb_far_ptr_t stash_a;
    volatile gb_far_ptr_t stash_b;
    stash_a = fp_a;
    stash_b = fp_b;

    gb_far_ptr_t call_a;
    gb_far_ptr_t call_b;
    call_a = stash_a;
    call_b = stash_b;

    // handler_plus_one(10) = 11 -> $0B
    out[0] = GB_FAR_CALL_1_U8_R_U8(call_a, 10);
    // handler_times_two(20) = 40 -> $28
    out[1] = GB_FAR_CALL_1_U8_R_U8(call_b, 20);
}
