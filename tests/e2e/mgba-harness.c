// SM83 mGBA harness: loads a .gb ROM, runs until PC reaches a given exit
// address (or --max-frames is hit), then prints one line per --check
// ADDR=VAL assertion ("PASS" / "FAIL: ..."). Exit 0 if all pass.
//
// Mirrors the CLI of tests/e2e/run-harness.py so shell runners can swap
// `sim` for `mgba` without changing the --check syntax.

#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba-util/vfs.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CHECKS 64

static void silent_log(struct mLogger *l, int category, enum mLogLevel level,
                       const char *format, va_list args) {
	(void)l; (void)category; (void)level; (void)format; (void)args;
}

static struct mLogger g_silent_logger = { .log = silent_log };

struct check {
	uint32_t addr;
	uint8_t expected;
};

static bool parse_hex_u32(const char *s, uint32_t *out) {
	char *end;
	unsigned long v = strtoul(s, &end, 16);
	if (*s == '\0' || *end != '\0' || v > 0xFFFFFFFFu) return false;
	*out = (uint32_t)v;
	return true;
}

static bool parse_check(const char *spec, struct check *c) {
	const char *eq = strchr(spec, '=');
	if (!eq) return false;
	char addr_buf[16];
	size_t alen = (size_t)(eq - spec);
	if (alen == 0 || alen >= sizeof(addr_buf)) return false;
	memcpy(addr_buf, spec, alen);
	addr_buf[alen] = '\0';
	uint32_t addr, val;
	if (!parse_hex_u32(addr_buf, &addr)) return false;
	if (!parse_hex_u32(eq + 1, &val)) return false;
	if (val > 0xFF) return false;
	c->addr = addr;
	c->expected = (uint8_t)val;
	return true;
}

static void usage(const char *argv0) {
	fprintf(stderr,
		"usage: %s ROM --exit-addr HEX [--check ADDR=VAL ...] [--max-frames N]\n"
		"  ROM              path to .gb file\n"
		"  --exit-addr HEX  PC address that means \"test finished\"\n"
		"  --check ADDR=VAL hex bus address / expected byte (repeatable, up to %d)\n"
		"  --max-frames N   timeout in frames (default 600 = ~10s of emulated time)\n",
		argv0, MAX_CHECKS);
}

int main(int argc, char **argv) {
	const char *rom_path = NULL;
	uint32_t exit_addr = 0;
	bool exit_addr_set = false;
	struct check checks[MAX_CHECKS];
	size_t n_checks = 0;
	unsigned max_frames = 600;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--exit-addr") == 0 && i + 1 < argc) {
			if (!parse_hex_u32(argv[++i], &exit_addr)) {
				fprintf(stderr, "bad --exit-addr: %s\n", argv[i]);
				return 2;
			}
			exit_addr_set = true;
		} else if (strcmp(argv[i], "--check") == 0 && i + 1 < argc) {
			if (n_checks >= MAX_CHECKS) {
				fprintf(stderr, "too many --check (max %d)\n", MAX_CHECKS);
				return 2;
			}
			if (!parse_check(argv[++i], &checks[n_checks])) {
				fprintf(stderr, "bad --check spec: %s\n", argv[i]);
				return 2;
			}
			n_checks++;
		} else if (strcmp(argv[i], "--max-frames") == 0 && i + 1 < argc) {
			max_frames = (unsigned)strtoul(argv[++i], NULL, 10);
		} else if (argv[i][0] == '-') {
			fprintf(stderr, "unknown flag: %s\n", argv[i]);
			usage(argv[0]);
			return 2;
		} else if (!rom_path) {
			rom_path = argv[i];
		} else {
			fprintf(stderr, "extra positional argument: %s\n", argv[i]);
			return 2;
		}
	}

	if (!rom_path || !exit_addr_set) {
		usage(argv[0]);
		return 2;
	}

	mLogSetDefaultLogger(&g_silent_logger);

	struct mCore *core = mCoreFind(rom_path);
	if (!core) {
		fprintf(stderr, "mCoreFind failed for %s\n", rom_path);
		return 2;
	}
	if (!core->init(core)) {
		fprintf(stderr, "core->init failed\n");
		return 2;
	}
	mCoreInitConfig(core, NULL);
	if (!mCoreLoadFile(core, rom_path)) {
		fprintf(stderr, "mCoreLoadFile failed for %s\n", rom_path);
		return 2;
	}
	core->reset(core);

	bool reached_exit = false;
	for (unsigned f = 0; f < max_frames; f++) {
		core->runFrame(core);
		int32_t pc = 0;
		if (!core->readRegister(core, "pc", &pc)) {
			fprintf(stderr, "readRegister(pc) failed\n");
			return 2;
		}
		if ((uint32_t)(pc & 0xFFFF) == (exit_addr & 0xFFFF)) {
			reached_exit = true;
			break;
		}
	}

	if (!reached_exit) {
		fprintf(stderr, "TIMEOUT: exit address 0x%04X not reached after %u frames\n",
			exit_addr, max_frames);
		int32_t pc = 0;
		core->readRegister(core, "pc", &pc);
		fprintf(stderr, "  last PC: 0x%04X\n", pc & 0xFFFF);
		return 1;
	}

	int fails = 0;
	for (size_t i = 0; i < n_checks; i++) {
		uint8_t got = (uint8_t)core->busRead8(core, checks[i].addr);
		if (got == checks[i].expected) {
			printf("PASS: [0x%04X] = 0x%02X\n", checks[i].addr, got);
		} else {
			printf("FAIL: [0x%04X] = 0x%02X (expected 0x%02X)\n",
				checks[i].addr, got, checks[i].expected);
			fails++;
		}
	}

	core->unloadROM(core);
	mCoreConfigDeinit(&core->config);
	core->deinit(core);
	return fails == 0 ? 0 : 1;
}
