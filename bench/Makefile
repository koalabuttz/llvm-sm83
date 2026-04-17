# SM83 codegen benchmark vs GBDK / SDCC.
#
# Usage:
#   make                      # build both, print size table
#   make clean                # remove build/ artifacts
#
# Overrides:
#   GBDK=/path/to/gbdk        # required; no default — set via env
#   LLVM_BUILD=/path          # default: ../build-sm83-$(uname -m)
#                             # (the repo-relative build dir created by
#                             # build-llvm-sm83.sh)

BENCH_DIR  := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
LLVM_BUILD ?= $(abspath $(BENCH_DIR)/../build-sm83-$(shell uname -m))
GBDK       ?=

CLANG      := $(LLVM_BUILD)/bin/clang
LLVM_SIZE  := $(LLVM_BUILD)/bin/llvm-size
SDCC       := $(GBDK)/bin/sdcc
LCC        := $(GBDK)/bin/lcc

# Each bench name maps to its source path. c-test.c is reused from the
# e2e suite to keep a single source-of-truth; bench-only programs
# (state-machine.c) live in this directory.
BENCHES := state-machine c-test
SRC_state-machine := state-machine.c
SRC_c-test        := ../tests/e2e/c-test.c

LLVM_OBJS := $(addprefix build/llvm-,$(addsuffix .o,$(BENCHES)))
GBDK_OBJS := $(addprefix build/gbdk-,$(addsuffix .rel,$(BENCHES)))

all: bench
.PHONY: all bench clean check-deps

# Gate the build rules on toolchain presence. `make clean` doesn't need
# either toolchain, so we only error when a build rule actually fires.
check-deps:
	@if [ -z "$(GBDK)" ]; then \
	    echo "ERROR: GBDK is not set. Download gbdk-2020 from"; \
	    echo "  https://github.com/gbdk-2020/gbdk-2020/releases/latest"; \
	    echo "  (pick gbdk-linux-arm64.tar.gz on ARM64, gbdk-linux64 on x86)"; \
	    echo "  tar xzf gbdk-linux-*.tar.gz into e.g. /tmp/gbdk"; \
	    echo "Then rerun with:  GBDK=/tmp/gbdk make"; \
	    exit 1; \
	fi
	@if [ ! -x "$(GBDK)/bin/lcc" ]; then \
	    echo "ERROR: $(GBDK)/bin/lcc is missing or not executable."; \
	    echo "Verify GBDK points at an extracted gbdk-2020 release."; \
	    exit 1; \
	fi
	@if [ ! -x "$(CLANG)" ]; then \
	    echo "ERROR: $(CLANG) not found. Run /data/build-llvm-sm83.sh first,"; \
	    echo "or override LLVM_BUILD=/path/to/build-sm83-ARCH."; \
	    exit 1; \
	fi

build: check-deps
	@mkdir -p build

# --- LLVM-SM83 --------------------------------------------------------------
build/llvm-%.o: | build
	$(CLANG) --target=sm83-unknown-none -ffreestanding -Os -c $(SRC_$*) -o $@

# --- GBDK / SDCC ------------------------------------------------------------
# SDCC's lcc looks for sources in its own cwd unless given an absolute path.
# We resolve SRC_$* relative to the Makefile's dir, then hand the absolute
# path to lcc so `cd build` doesn't break the include search.
build/gbdk-%.rel: | build
	cd build && $(LCC) -Wa-l -Wf--max-allocs-per-node50000 \
	    -c -o gbdk-$*.rel $(abspath $(SRC_$*))

# --- Summary table ----------------------------------------------------------
bench: $(LLVM_OBJS) $(GBDK_OBJS)
	@echo ""
	@echo "=== SM83 codegen size comparison (.text / _CODE bytes) ==="
	@printf "%-24s %12s %12s %10s\n" "Program" "LLVM-SM83" "GBDK/SDCC" "Delta"
	@printf "%-24s %12s %12s %10s\n" "----" "----" "----" "----"
	@for b in $(BENCHES); do \
	    llvm_sz=$$($(LLVM_SIZE) -A build/llvm-$$b.o 2>/dev/null \
	               | awk '/^\.text/ {print $$2; found=1} END {if(!found)print 0}'); \
	    gbdk_sz=$$(awk '/^A _CODE/ { for(i=1;i<=NF;i++) if($$i~/size/){ split($$(i+1),a,","); print strtonum("0x"a[1]); found=1} } END {if(!found)print 0}' build/gbdk-$$b.rel); \
	    delta=$$(( gbdk_sz - llvm_sz )); \
	    printf "%-24s %12s %12s %+10s\n" "$$b.c" "$$llvm_sz" "$$gbdk_sz" "$$delta"; \
	done

clean:
	rm -rf build
