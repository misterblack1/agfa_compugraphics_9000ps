# =============================================================================
# Makefile -- AGFA-MON ROM build for Agfa Compugraphic 9000PS
# =============================================================================
#
# USAGE
#   Run from the agfa-monitor directory:
#     make
#
#   Or from the project root:
#     make -C agfa-monitor
#     (or just: make)
#
# WHAT IT DOES (in order)
#   1. Assembles agfa-mon.m68  ->  agfa-rom.bin  (128KB flat binary)
#   2. Splits agfa-rom.bin     ->  four 32KB EPROM images (HH/HM/LM/LL)
#
# TARGETS
#   all     Build everything (default)
#   chips   Produce the four EPROM images (implies assembly)
#   clean   Delete all generated files
#   help    Print this usage summary
#
# PREREQUISITES
#   - GNU make
#   - vasm assembler (vasmm68k_mot or vasmm68k_mot.exe in assembler/)
#   - Python 3 (for split_rom.py)
#   - ehbasic68k.inc must exist (pre-generated; see note below)
#
# REGENERATING ehbasic68k.inc
#   This file is extracted from the EhBASIC 68K source in the ioboard project.
#   It is checked in and rarely needs to change.  If you must regenerate it:
#     powershell -File ../assembler/build.ps1
#   build.ps1 runs the full pipeline including the extraction step.
#
# ROM INSTALLATION
#   After a successful build, four files are ready in this directory:
#     agfa-rom-HH.bin  ->  D24-D31  (MSB)  program into HH socket
#     agfa-rom-HM.bin  ->  D16-D23         program into HM socket
#     agfa-rom-LM.bin  ->  D8-D15          program into LM socket
#     agfa-rom-LL.bin  ->  D0-D7   (LSB)  program into LL socket
#
#   All four chips are AM27C256 (DIP-28).  Use a TL866-3G or compatible
#   EPROM programmer in AM27C256 mode.  Address range A0-A14 (32K) each.
#
#   Connect Z85C30 Channel A (DE-9 RS-232) at 9600 8N1 for the monitor prompt.
#
# =============================================================================

# --- Tool configuration (from project-wide config.mk) -----------------------

include ../config.mk

# --- Source files -------------------------------------------------------------

MON_SRC := agfa-mon.m68

# EhBASIC 68K core, auto-extracted from the ioboard project.
# Pre-generated and checked in.  Do not hand-edit.
EHBASIC_INC := ehbasic68k.inc

# Additional include files pulled in by agfa-mon.m68.
# Listed here so that Make rebuilds the ROM if any of them change.
INCLUDES := \
    include/nvram_config.inc \
    include/vera_init.inc    \
    include/vera_text.inc    \
    include/vera_cursor.inc  \
    include/via_timer.inc

# Generated at build time -- not checked in
VERSION_INC := include/build_version.inc

# --- Output files -------------------------------------------------------------

ROM_BIN := agfa-rom.bin
ROM_LST := agfa-rom.lst

CHIP_HH := agfa-rom-HH.bin
CHIP_HM := agfa-rom-HM.bin
CHIP_LM := agfa-rom-LM.bin
CHIP_LL := agfa-rom-LL.bin

CHIPS   := $(CHIP_HH) $(CHIP_HM) $(CHIP_LM) $(CHIP_LL)

# =============================================================================
# BUILD RULES
# =============================================================================

.PHONY: all chips clean help

all: chips

# ---- Step 2: Split flat binary into four EPROM images -----------------------
#
# The 9000PS ROM bank uses four AM27C256 chips wired in 32-bit interleave.
# Each chip carries exactly one byte lane of every 32-bit longword:
#
#   HH  D24-D31  bytes[0::4] from the flat binary  (most significant)
#   HM  D16-D23  bytes[1::4]
#   LM  D8-D15   bytes[2::4]
#   LL  D0-D7    bytes[3::4]                        (least significant)
#
# split_rom.py pads the input to exactly 128KB (with 0xFF) if needed, then
# writes the four chip files alongside the input binary.

chips: $(CHIPS)

$(CHIP_HH): $(ROM_BIN)
	@echo ""
	@echo "[2/2] Splitting $(ROM_BIN) into EPROM images..."
	$(SPLIT_ROM) $(ROM_BIN)
	@echo ""
	@echo "EPROM images ready:"
	@echo "  $(CHIP_HH)  ->  D24-D31  (MSB)  -- program into HH socket"
	@echo "  $(CHIP_HM)  ->  D16-D23          -- program into HM socket"
	@echo "  $(CHIP_LM)  ->  D8-D15           -- program into LM socket"
	@echo "  $(CHIP_LL)  ->  D0-D7    (LSB)  -- program into LL socket"
	@echo ""
	@echo "Programmer: TL866-3G, AM27C256 mode, A0-A14 (32K each chip)"
	@echo "Serial:     Z85C30 Ch A, DE-9 RS-232, 9600 8N1"

$(CHIP_HM) $(CHIP_LM) $(CHIP_LL): $(CHIP_HH)

# ---- Step 1: Assemble agfa-mon.m68 -> agfa-rom.bin -------------------------
#
# CRITICAL: vasm MUST run from the project root (one level up from here).
#
# Reason: agfa-mon.m68 uses INCBIN directives with paths relative to the
# project root (e.g. "agfa-monitor/vera/font.bin").  vasm resolves INCBIN
# relative to its working directory -- not the source file location.

# ---- Step 0: Generate build version include ----------------------------------
#
# Always regenerated so the build string reflects the current git state.
# The .PHONY ensures Make doesn't skip it because the file already exists.

.PHONY: force-version
$(VERSION_INC): force-version
	@echo "; auto-generated by Makefile -- do not edit" > $(VERSION_INC)
	@echo "STR_BUILD_VERSION:" >> $(VERSION_INC)
	@echo "        DC.B    \"$(BUILD_VERSION_STRING)\",\$$0D,\$$0A,0" >> $(VERSION_INC)
	@echo "        ALIGN   2" >> $(VERSION_INC)

$(ROM_BIN): $(MON_SRC) $(EHBASIC_INC) $(INCLUDES) $(VERSION_INC)
	@echo ""
	@echo "[1/2] Assembling $(MON_SRC)..."
	@echo "      Version: $(BUILD_VERSION_STRING)"
	@echo "      (running vasm from project root -- required for INCBIN paths)"
	cd .. && $(VASM) $(VASM_BIN) \
	    -L agfa-monitor/$(ROM_LST) \
	    -o agfa-monitor/$(ROM_BIN) \
	    agfa-monitor/$(MON_SRC)
	@ls -lh $(ROM_BIN)
	@echo "Assembly complete."

# ---- ehbasic68k.inc: pre-generated, should exist in the repo ----------------

$(EHBASIC_INC):
	@echo ""
	@echo "ERROR: $(EHBASIC_INC) is missing."
	@echo ""
	@echo "This file is normally checked in.  To regenerate it, run:"
	@echo "  powershell -File ../assembler/build.ps1"
	@echo ""
	@exit 1

# =============================================================================
# UTILITY TARGETS
# =============================================================================

clean:
	@echo "Removing generated files..."
	rm -f $(ROM_BIN) $(ROM_LST) $(CHIPS) $(VERSION_INC)
	@echo "Done.  (ehbasic68k.inc kept -- it is checked-in source)"

help:
	@echo ""
	@echo "AGFA-MON ROM build"
	@echo ""
	@echo "  make          Build everything (assemble + split)"
	@echo "  make chips    Same as 'make all'"
	@echo "  make clean    Remove agfa-rom.bin and the four chip files"
	@echo "  make help     This message"
	@echo ""
	@echo "Assembler: $(VASM)"
	@echo "Python:    $(PYTHON)"
	@echo ""
