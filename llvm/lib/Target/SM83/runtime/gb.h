/*
 * gb.h — Game Boy hardware register definitions for SM83.
 *
 * Usage:
 *   #include "gb.h"
 *   REG_LCDC = LCDC_ON | LCDC_BG_ON;
 */

#ifndef _GB_H
#define _GB_H

/* --- Joypad ------------------------------------------------------------ */
#define REG_P1    (*(volatile unsigned char *)0xFF00)

/* --- Serial ------------------------------------------------------------- */
#define REG_SB    (*(volatile unsigned char *)0xFF01)
#define REG_SC    (*(volatile unsigned char *)0xFF02)

/* --- Timer -------------------------------------------------------------- */
#define REG_DIV   (*(volatile unsigned char *)0xFF04)
#define REG_TIMA  (*(volatile unsigned char *)0xFF05)
#define REG_TMA   (*(volatile unsigned char *)0xFF06)
#define REG_TAC   (*(volatile unsigned char *)0xFF07)

/* --- Interrupt flags ---------------------------------------------------- */
#define REG_IF    (*(volatile unsigned char *)0xFF0F)

/* --- Sound -------------------------------------------------------------- */
#define REG_NR10  (*(volatile unsigned char *)0xFF10)
#define REG_NR11  (*(volatile unsigned char *)0xFF11)
#define REG_NR12  (*(volatile unsigned char *)0xFF12)
#define REG_NR13  (*(volatile unsigned char *)0xFF13)
#define REG_NR14  (*(volatile unsigned char *)0xFF14)
#define REG_NR21  (*(volatile unsigned char *)0xFF16)
#define REG_NR22  (*(volatile unsigned char *)0xFF17)
#define REG_NR23  (*(volatile unsigned char *)0xFF18)
#define REG_NR24  (*(volatile unsigned char *)0xFF19)
#define REG_NR30  (*(volatile unsigned char *)0xFF1A)
#define REG_NR31  (*(volatile unsigned char *)0xFF1B)
#define REG_NR32  (*(volatile unsigned char *)0xFF1C)
#define REG_NR33  (*(volatile unsigned char *)0xFF1D)
#define REG_NR34  (*(volatile unsigned char *)0xFF1E)
#define REG_NR41  (*(volatile unsigned char *)0xFF20)
#define REG_NR42  (*(volatile unsigned char *)0xFF21)
#define REG_NR43  (*(volatile unsigned char *)0xFF22)
#define REG_NR44  (*(volatile unsigned char *)0xFF23)
#define REG_NR50  (*(volatile unsigned char *)0xFF24)
#define REG_NR51  (*(volatile unsigned char *)0xFF25)
#define REG_NR52  (*(volatile unsigned char *)0xFF26)

/* --- Wave RAM ($FF30-$FF3F) --------------------------------------------- */
#define WAVE_RAM  ((volatile unsigned char *)0xFF30)

/* --- CGB speed switch (CGB only) --------------------------------------- */
#define REG_KEY1  (*(volatile unsigned char *)0xFF4D)  /* Prepare speed switch */

/* --- LCD ---------------------------------------------------------------- */
#define REG_LCDC  (*(volatile unsigned char *)0xFF40)
#define REG_STAT  (*(volatile unsigned char *)0xFF41)
#define REG_SCY   (*(volatile unsigned char *)0xFF42)
#define REG_SCX   (*(volatile unsigned char *)0xFF43)
#define REG_LY    (*(volatile unsigned char *)0xFF44)
#define REG_LYC   (*(volatile unsigned char *)0xFF45)
#define REG_DMA   (*(volatile unsigned char *)0xFF46)
#define REG_BGP   (*(volatile unsigned char *)0xFF47)
#define REG_OBP0  (*(volatile unsigned char *)0xFF48)
#define REG_OBP1  (*(volatile unsigned char *)0xFF49)
#define REG_WY    (*(volatile unsigned char *)0xFF4A)
#define REG_WX    (*(volatile unsigned char *)0xFF4B)

/* --- CGB graphics (CGB only) ------------------------------------------- */
#define REG_VBK   (*(volatile unsigned char *)0xFF4F)  /* VRAM bank select */
#define REG_HDMA1 (*(volatile unsigned char *)0xFF51)  /* HDMA source hi */
#define REG_HDMA2 (*(volatile unsigned char *)0xFF52)  /* HDMA source lo */
#define REG_HDMA3 (*(volatile unsigned char *)0xFF53)  /* HDMA dest hi */
#define REG_HDMA4 (*(volatile unsigned char *)0xFF54)  /* HDMA dest lo */
#define REG_HDMA5 (*(volatile unsigned char *)0xFF55)  /* HDMA length/mode/start */
#define REG_RP    (*(volatile unsigned char *)0xFF56)  /* Infrared port */
#define REG_BCPS  (*(volatile unsigned char *)0xFF68)  /* BG palette index */
#define REG_BCPD  (*(volatile unsigned char *)0xFF69)  /* BG palette data */
#define REG_OCPS  (*(volatile unsigned char *)0xFF6A)  /* OBJ palette index */
#define REG_OCPD  (*(volatile unsigned char *)0xFF6B)  /* OBJ palette data */
#define REG_SVBK  (*(volatile unsigned char *)0xFF70)  /* WRAM bank select */

/* --- Interrupt enable --------------------------------------------------- */
#define REG_IE    (*(volatile unsigned char *)0xFFFF)

/* --- LCDC bit masks ----------------------------------------------------- */
#define LCDC_ON        0x80  /* LCD display enable */
#define LCDC_WIN_MAP   0x40  /* Window tile map: 0=9800, 1=9C00 */
#define LCDC_WIN_ON    0x20  /* Window enable */
#define LCDC_TILE      0x10  /* BG/Win tile data: 0=8800, 1=8000 */
#define LCDC_BG_MAP    0x08  /* BG tile map: 0=9800, 1=9C00 */
#define LCDC_OBJ_SIZE  0x04  /* OBJ size: 0=8x8, 1=8x16 */
#define LCDC_OBJ_ON    0x02  /* OBJ enable */
#define LCDC_BG_ON     0x01  /* BG/Window enable */

/* --- STAT bit masks ----------------------------------------------------- */
#define STAT_LYC_INT   0x40  /* LYC=LY interrupt enable */
#define STAT_OAM_INT   0x20  /* Mode 2 (OAM search) interrupt enable */
#define STAT_VBLANK_INT 0x10 /* Mode 1 (VBlank) interrupt enable */
#define STAT_HBLANK_INT 0x08 /* Mode 0 (HBlank) interrupt enable */
#define STAT_LYC_FLAG  0x04  /* LYC=LY coincidence flag (read-only) */
#define STAT_MODE_MASK 0x03  /* Mode flag (read-only) */

/* --- Interrupt bit masks (for REG_IF / REG_IE) -------------------------- */
#define INT_VBLANK  0x01
#define INT_LCD     0x02
#define INT_TIMER   0x04
#define INT_SERIAL  0x08
#define INT_JOYPAD  0x10

/* --- Joypad bit masks --------------------------------------------------- */
#define P1_SELECT_BTN  0x20  /* Select button keys (active low) */
#define P1_SELECT_DIR  0x10  /* Select direction keys (active low) */
#define P1_DOWN_START  0x08
#define P1_UP_SELECT   0x04
#define P1_LEFT_B      0x02
#define P1_RIGHT_A     0x01

/* --- Timer control (TAC) ------------------------------------------------ */
#define TAC_ENABLE  0x04
#define TAC_4096    0x00  /* 4096 Hz */
#define TAC_262144  0x01  /* 262144 Hz */
#define TAC_65536   0x02  /* 65536 Hz */
#define TAC_16384   0x03  /* 16384 Hz */

/* --- Memory region base addresses --------------------------------------- */
#define VRAM   ((volatile unsigned char *)0x8000)
#define SRAM   ((volatile unsigned char *)0xA000)
#define WRAM   ((volatile unsigned char *)0xC000)
#define OAM    ((volatile unsigned char *)0xFE00)
#define HRAM   ((volatile unsigned char *)0xFF80)

/* --- Tile map base addresses -------------------------------------------- */
#define BG_MAP0  ((volatile unsigned char *)0x9800)
#define BG_MAP1  ((volatile unsigned char *)0x9C00)

/* --- OAM entry structure ------------------------------------------------ */
typedef struct {
    unsigned char y;
    unsigned char x;
    unsigned char tile;
    unsigned char attr;
} oam_entry_t;

#define OAM_ENTRIES ((volatile oam_entry_t *)0xFE00)

/* --- OAM attribute bits ------------------------------------------------- */
#define OAM_PRIORITY  0x80  /* BG/Window over OBJ (0=No, 1=BG priority) */
#define OAM_FLIP_Y    0x40  /* Vertical flip */
#define OAM_FLIP_X    0x20  /* Horizontal flip */
#define OAM_PALETTE   0x10  /* OBJ palette: 0=OBP0, 1=OBP1 */

/* --- Section placement helpers ------------------------------------------ */
/*
 * Place a function or data in HRAM ($FF80-$FFFE).
 *
 * HRAM is the only region accessible during OAM DMA ($FF46 write), so the
 * OAM DMA trampoline MUST use GB_HRAM. The linker stores the contents in
 * ROM and the crt0 copies them to HRAM at startup.
 *
 * Example:
 *   static void GB_HRAM oam_dma_wait(void) {
 *       REG_DMA = 0xC1;
 *       for (unsigned char i = 0; i < 40; i++) __asm__("nop");
 *   }
 */
#define GB_HRAM __attribute__((section(".hram"), noinline))

/*
 * Place a data object in HRAM as uninitialised storage.
 * (Currently routes through the same loaded .hram section; acceptable for
 * small scratch variables. Extend the linker script with .hram.bss if a
 * zero-init distinction is required.)
 */
#define GB_HRAM_DATA __attribute__((section(".hram")))

/*
 * Declare an interrupt service routine pinned to a hardware vector.
 *
 * `vec` must be one of: vblank, lcd, timer, serial, joypad.
 * The compiler emits the full register-preserving prologue, uses RETI, and
 * places the function in .isr.<vec> which the linker script maps to
 * address $40/$48/$50/$58/$60 respectively.
 *
 * Example:
 *   SM83_ISR(vblank) { vblank_count++; }
 */
#define SM83_ISR(vec) \
    __attribute__((interrupt(#vec))) void vec##_isr(void)

/* --- MBC1 bank switching ------------------------------------------------ */
/*
 * BANK_SWITCH(n) — map ROM bank `n` into the $4000-$7FFF window.
 *
 * MBC1 decodes writes to $2000-$3FFF as the lower 5 bits of the ROM bank
 * number. Writing 0 is silently remapped by hardware to bank 1, so bank 0
 * cannot be selected through this register (and doesn't need to be —
 * bank 0 is permanently mapped at $0000-$3FFF).
 *
 * Banks ≥ 32 also need the upper 2 bits written to $4000-$5FFF (mode 0),
 * which Round 5 does NOT support — callers using --rom-banks > 16 must
 * write those bits themselves.
 *
 * Example:
 *   extern const unsigned char sprite_tiles[]
 *       __attribute__((section(".romx.bank2")));
 *   BANK_SWITCH(2);
 *   blit(sprite_tiles);          // accessed through $4000
 *   BANK_SWITCH(1);              // restore default
 */
#define BANK_SWITCH(n) do { \
    *(volatile unsigned char *)0x2000 = (unsigned char)(n); \
} while (0)

/*
 * MBC1 SRAM enable/disable. Writing $0A to $0000-$1FFF enables the
 * A000-$BFFF SRAM region for read/write; any other value (canonically $00)
 * disables it. Always disable SRAM before power-down to prevent battery-
 * backed saves from getting corrupted.
 */
#define MBC1_RAM_ENABLE()  do { \
    *(volatile unsigned char *)0x0000 = 0x0A; \
} while (0)

#define MBC1_RAM_DISABLE() do { \
    *(volatile unsigned char *)0x0000 = 0x00; \
} while (0)

/* --- CGB-specific helpers ---------------------------------------------- */
/*
 * Select VRAM bank 0 or 1 for access through $8000-$9FFF.
 * Bank 0: BG + OBJ tile data and tile maps.
 * Bank 1: extra tile data (CGB) and per-tile BG attribute map.
 */
#define VRAM_BANK(n)  do { REG_VBK  = (unsigned char)(n); } while (0)

/*
 * Select WRAM bank at $D000-$DFFF. Banks 1-7 are available on CGB;
 * value 0 is silently treated as bank 1 by hardware.
 * $C000-$CFFF always maps bank 0.
 */
#define WRAM_BANK(n)  do { REG_SVBK = (unsigned char)(n); } while (0)

/*
 * Request CGB double-speed (8 MHz) CPU mode.
 *
 * The CGB procedure is:
 *   1. Disable interrupts and joypad line (write $30 to P1).
 *   2. Write $01 to KEY1 to arm the switch.
 *   3. Execute STOP; hardware latches the new speed on resume.
 *
 * This helper skips step 1 — caller is responsible for DI + P1 prep,
 * because wrapping them here would interfere with user interrupt setup.
 * Tests: check (REG_KEY1 & 0x80) != 0 to confirm double-speed after.
 */
#define CGB_DOUBLE_SPEED() do { \
    REG_KEY1 = 0x01; \
    __asm__ volatile("stop" ::: "memory"); \
} while (0)

#endif /* _GB_H */
