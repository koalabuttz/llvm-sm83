// mbc3-rtc-test.c — exercise the MBC3 real-time-clock runtime API.
//
// Enables SRAM/RTC access, latches current live counters, then reads
// each of the five RTC registers (S / M / H / DL / DH) into WRAM.
// The harness runs with --rtc-start 0:10:30:05 (0 days, 10h, 30m, 5s)
// and asserts that the latched values are ≥ the start time. Real RTC
// counts during the run, so values may be slightly larger.

#include <gb.h>

void main(void) {
    volatile unsigned char *out = (volatile unsigned char *)0xC100;

    MBC3_RAM_RTC_ENABLE();
    MBC3_RTC_LATCH();

    MBC3_RTC_SELECT(MBC3_RTC_S);
    out[0] = MBC3_RTC_READ;

    MBC3_RTC_SELECT(MBC3_RTC_M);
    out[1] = MBC3_RTC_READ;

    MBC3_RTC_SELECT(MBC3_RTC_H);
    out[2] = MBC3_RTC_READ;

    MBC3_RTC_SELECT(MBC3_RTC_DL);
    out[3] = MBC3_RTC_READ;

    MBC3_RTC_SELECT(MBC3_RTC_DH);
    out[4] = MBC3_RTC_READ;

    MBC3_RAM_RTC_DISABLE();
}
