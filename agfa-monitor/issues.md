# AGFA-MON ROM Bug Tracker

Issues discovered after the first full ROM build and hardware test (2026-04-01).

---

## Priority Order for Next Build and latest fix status by Adrian April 2 2026 12:02PM PT

*(Updated 06-APR-2026 with issues discovered during VERA extended API testing)*

1. **#9** - Hex output corrupted - **VERIFIED FIXED**
2. **#2** - Cold boot freeze - **FIXED (Part A - new fix, April 2 2026)**. Root cause was VT_MAGIC=$A55A set during VERA_INIT before JMP thunks were installed. See details below.
3. **#4** - All VERA text output garbled - **VERIFIED FIXED**
4. **#6** - Double LF in BASIC at col 80 - **VERIFIED FIXED**
5. **#3** - Cursor blink via VSYNC IRQ - **Pending (standalone test first)**
6. **#7** - GETLINE buffer overflow - **FIXED (April 2 2026)**
7. **#8** - P/X commands output S-records to VERA - **FIXED (April 2 2026)**
8. **#5** - Backspace not handled on VERA - **VERIFIED FIXED**
9. **#1** - Em-dash in nvram_config.inc - **VERIFIED FIXED**
10. **#15** - Most VERA API calls lack ISR/IPL protection - **OPEN. Umbrella issue covering CLR_ROW, CLRSCR, CLR_EOS, HANDLE_LF, STREAM_START/CHAR, GOTOXY, CURSOR_ON/OFF. All must have ORI/ANDI IPL blocking added. Subsumes #11 and #12.**
11. **#12** - VT_VEC_CLRSCR graphical corruption - **OPEN (see #15)**
12. **#13** - VT_VEC_GOTOXY ignores VT_SCROLL_ROW - **OPEN**
13. **#14** - VT_VEC_SCROLL_DOWN over-scroll breaks display - **OPEN**
14. **#11** - STREAM_START/CHAR not IPL-protected - **OPEN (see #15)**
15. **#10** - VT_VEC_HANDLE_LF clobbers D2 - **OPEN (see #15)**

---

## Build 02-APR-2026 14:48 - All feedback.md bugs fixed + build version feature

Applied all fixes from feedback.md (1:59 PM build) in one pass:

| Bug | Root Cause | Fix |
|-----|-----------|-----|
| #9 Hex 0F/0F corruption | PUTCHAR `MOVE.W VT_MAGIC,D1` clobbered D1 | MOVEM.L save/restore D1 around VT_MAGIC check |
| #4 VERA text garbled | MOVE.W to Dn doesn't zero upper 16 bits on 68020 | MOVEQ #0 before all MOVE.W in VERA_PUTCHAR, VERA_CLR_ROW, VERA_STREAM_START |
| #5 VERA backspace | No $08 handler in VERA_PUTCHAR | Added BS handler: decrement col, write space+attr |
| #6 Double LF at col 80 | Immediate wrap at col 80 + EhBASIC CR/LF = double advance | VT100-style pending wrap (V_FLAGS bit 5) |
| #8 P/X S-record to VERA | EMIT_SRECS D3 saved NV_CONSOLE_OUT but was clobbered by address math | Changed to stack save/restore |
| Buffer overrun (~165 chars) | Same as #9: PUTCHAR clobbered D1 broke GETLINE's remaining counter | Fixed by #9 |
| Feature: build version | N/A | Added "BUILD 02-APR-2026" line to STR_BANNER |

**Files changed:**
- `agfa-mon.m68` - PUTCHAR D1 save/restore, PUTCHAR_SERIAL label, EMIT_SRECS stack save, STR_BANNER build line
- `include/vera_text.inc` - VERA_PUTCHAR rewrite (BS, pending wrap, zero-extension), VERA_CLR_ROW fix, VERA_STREAM_START fix, VERA_CLRSCR pending wrap clear, VT_VF_PEND_WRAP constant

---

## Fixed Issues (detailed)

### Issue #9 - Hex output corrupted: lower nibble always 'F'

**Severity:** Maximum
**Status:** Fixed in agfa-mon.m68

**Root Cause:** `PUTCHAR` did `MOVE.W VT_MAGIC,D1` unconditionally, clobbering D1.
`PRINT_HEX2` saved the byte in D1 and read it back after `HEX_NYBBLE → PUTCHAR`. After
the clobber, D1 held $A55A (VT_MAGIC), so the lower nibble was always garbage ('F').
`PRINT_HEX8` rotates through D1 with `ROL.L #4,D1`; after the first nibble PUTCHAR
destroyed D1, making all subsequent nibbles wrong.

**Fix applied:** `PUTCHAR` now saves/restores D1 with `MOVEM.L D1,-(SP)` / `MOVEM.L (SP)+,D1`
around the VT_MAGIC check. Also added `PUTCHAR_SERIAL` global label at the raw SCC
write path (falls through from PUTCHAR's serial path), enabling direct serial writes
without the redirect logic.

---

### Issue #2 - Cold boot: freeze after "Loaded from EEPROM." / "AGFA-MON Ready."

**Severity:** Maximum
**Status:** Fixed in agfa-mon.m68 (April 2 2026 - second attempt)

**Root Cause:** `VERA_INIT` sets `VT_MAGIC = $A55A` in its first step (CCB initialization).
Later, `VERA_INIT` step 9 prints a serial banner via `PUTCHAR`. At this point
`NV_CONSOLE_OUT=1` (from LOAD_CONFIG) AND `VT_MAGIC=$A55A`, so PUTCHAR redirects to
`JSR VT_VEC_PUTCHAR` ($02004100). But `VT_INIT_VECTORS` hasn't run yet - the JMP thunks
at $02004100 are uninitialized RAM (cold boot = crash). On fast power cycle, stale RAM
from the previous session held valid thunks, so it appeared to work.

**First attempt (FAILED):** Removed the magic write from `vera_init.inc` and added it after
`VT_INIT_VECTORS` in `agfa-mon.m68`. This caused a severe regression - garbled serial banner,
infinite reboot loop. Root cause unclear but likely related to binary layout shift from
modifying `vera_init.inc`. **Reverted.**

**Second attempt (CURRENT FIX):** Leave `vera_init.inc` completely untouched. Instead,
temporarily clear `NV_CONSOLE_OUT` around the VERA init sequence in `agfa-mon.m68`:

```asm
MOVE.B  CONFIG_BUF+NV_CONSOLE_OUT,D3   ; save VERA redirect flag
CLR.B   CONFIG_BUF+NV_CONSOLE_OUT      ; force serial during VERA init

BSR     VERA_INIT                       ; sets VT_MAGIC, but PUTCHAR stays serial
BSR     VT_INIT_RAM
BSR     VT_INIT_VECTORS                 ; JMP thunks now ready

MOVE.B  D3,CONFIG_BUF+NV_CONSOLE_OUT   ; restore VERA redirect
```

PUTCHAR checks `NV_CONSOLE_OUT` **before** `VT_MAGIC`, so even though VERA_INIT sets
VT_MAGIC=$A55A, PUTCHAR stays on serial because the redirect flag is forced to 0.
After thunks are populated, the flag is restored.

**Files changed:**
- `agfa-mon.m68` - save/clear/restore NV_CONSOLE_OUT around VERA init block

**Files NOT changed:**
- `include/vera_init.inc` - left untouched to avoid binary layout shift

---

### Issue #4 - All VERA text output garbled (D0/D2 upper word garbage)

**Severity:** High
**Status:** Fixed in vera_text.inc

**Root Cause:** `MOVE.W` to a data register on the 68020 does NOT zero the upper 16 bits.
`VERA_PUTCHAR` used `MOVE.W VT_CUR_ROW,D2` then `LSL.L #8,D2`, shifting garbage upper
bits into significant address bits. Same for the column in D0. Every character was written
to a wrong VRAM address.

**Fix applied:** All word loads before longword arithmetic now use `MOVEQ #0,Dn / MOVE.W`:
- `VERA_PUTCHAR`: `MOVEQ #0,D2 / MOVE.W VT_CUR_ROW,D2` and `MOVEQ #0,D0 / MOVE.W VT_CUR_COL,D0`; `LSL.W #1,D0` replaced by `ADD.L D0,D0`
- `VERA_CLR_ROW`: `MOVEQ #0,D0 / MOVE.W D1,D0` before `LSL.L #8,D0`
- `VERA_STREAM_START`: `MOVEQ #0,D1 / MOVE.W VT_CUR_ROW,D1` and `MOVEQ #0,D0 / MOVE.W VT_CUR_COL,D0`; `LSL.W #1,D0` replaced by `ADD.L D0,D0`

---

### Issue #6 - Double line feed in BASIC at column 80

**Severity:** High
**Status:** Fixed in vera_text.inc

**Root Cause:** `VERA_PUTCHAR` auto-wrapped at col 80 by calling `VERA_HANDLE_LF`
immediately. EhBASIC independently sent CR+LF at the same boundary, causing a second
`VERA_HANDLE_LF` call - double advance.

**Fix applied:** VT100-style pending wrap flag using bit 6 of `V_FLAGS`:
- When col reaches 80: set flag, do NOT call `VERA_HANDLE_LF` yet
- Next printable char: flush wrap (call `VERA_HANDLE_LF`), clear flag, then write char
- Next `$0A`: if flag set → clear flag, skip the advance (absorb EhBASIC's explicit LF)
- Next `$0D`: clear flag, col already 0 (no-op)

---

### Issue #8 - P, L, X commands bypass VERA redirect

**Severity:** Medium
**Status:** Fixed in agfa-mon.m68

**Fix applied:**
- Added `PUTCHAR_SERIAL` global label at the raw SCC write path (falls through from PUTCHAR's serial path). No D1/D0 clobber; safe to call directly.
- Added `PRINTSTR_SERIAL` routine using `PUTCHAR_SERIAL`.
- `CMD_LOAD`: changed initial `BSR PRINTSTR` → `BSR PRINTSTR_SERIAL` (the prompt message).
- `CMD_XBASIC`: changed `BSR PRINTSTR` for `STR_BASIC_NOINIT` → `BSR PRINTSTR_SERIAL`.
- Loading dots (`'.'` / `'!'` in CMD_LOAD) remain on VERA as approved - they use the
  normal `PUTCHAR` path and mirror correctly.

---

### Issue #5 - Backspace not handled in VERA_PUTCHAR

**Severity:** Medium
**Status:** Fixed in vera_text.inc

**Fix applied:** Added `$08` handler in `VERA_PUTCHAR` (before the printable char path):
- If `VT_CUR_COL == 0`: do nothing
- Otherwise: decrement `VT_CUR_COL`, compute VRAM address with correct zero-extension, write space + attribute at the new position

---

### Issue #1 - Em-dash character in nvram_config.inc

**Severity:** Low
**Status:** Fixed in nvram_config.inc

**Fix applied:** Replaced em-dash (-) with `--` in the `NVC_STR_CONSOLE_OPTS` `DC.B` string at the bitmask prompt line. (Em-dashes in comments are harmless and left as-is.)

---

## Issue #3 - No cursor blink (VERA VSYNC IRQ not implemented)

**Severity:** Medium (cosmetic but important for usability)
**Status:** In progress. Standalone test program to be written before ROM integration.

### Plan

1. Write standalone test `vera_cursor_irq_test.m68`:
   - Copy 1KB ROM vector table ($000000-$0003FF) to RAM (e.g., $02001000)
   - Relocate VBR to RAM: `MOVEC A0,VBR` (VBR = 0 at boot; ROM is not writable)
   - Install `VERA_VSYNC_IRQ` handler at `VBR_RAM + $064` (level-1 autovector)
   - Enable VERA VSYNC interrupt: `BSET #0,$04000026` (IEN bit 0)
   - Lower IPL: `ANDI #$F8FF,SR`
   - Run for 600 frames (~10 sec at 60Hz); cursor blinks every 30 frames (~0.5s)
   - Exit: raise IPL, disable VERA interrupt, `MOVEQ #0,D0 / MOVEC D0,VBR` to restore VBR
2. Validate on hardware
3. Integrate into ROM: `START` relocates VBR to RAM at boot; cursor handler runs permanently

### Key Hardware Facts

- VERA VSYNC interrupt: `IEN` at `$04000026` (VERA_BASE+$06), bit 0
- ISR at `$04000027`, write `$01` to clear VSYNC flag
- 68020 level-1 autovector address: `VBR + $064`
- VBR is 0 at boot (ROM) - must be moved to RAM before any vector can be installed
- VERA IRQ routed via VIA #2 → IPL level 1 (confirmed by user, via2-irq-test)

### Cursor Blink Logic

- Blink timer: decrement each VSYNC; toggle at zero, reset to 30
- Toggle: write `~VT_TEXT_ATTR` to attribute byte at `(VT_CUR_ROW * 256) + (VT_CUR_COL * 2) + 1` in VRAM
- Must use correct zero-extension (MOVEQ #0,Dn / MOVE.W) - same fix as #4 applied here too

---

## Issue #7 - GETLINE buffer overflow

**Severity:** Medium
**Status:** Fixed in nvram_config.inc (April 2 2026). See detailed section below.

**Note:** The monitor's main GETLINE (agfa-mon.m68) was correctly limited to 79 chars.
The overflow was in NVC_GETLINE (nvram_config.inc) - DBRA D7=79 stored 80 chars + NUL = 81 bytes
into an 80-byte buffer, Fix: D7=78.

---

## Previously Fixed Issues (for reference)

| Issue | Description | Status |
|-------|-------------|--------|
| STREAM_NYBBLE fall-through to data | STREAM_CHAR placed before STREAM_NYBBLE; fall-through executed VT_HEX_TABLE bytes as code | Fixed in vera_text.inc |
| JSR (VT_VEC_xxx) crash | Vector slots held raw addresses; JSR executed pointer data as code | Fixed: switched to JMP thunk design |
| vera_text_test re-initialized VERA | Test called VERA_DETECT/VERA_INIT it shouldn't | Fixed: test only calls VT_INIT_RAM + VT_INIT_VECTORS |
| INCBIN path conflict | `../vera/font.bin` failed from project root | Fixed: `agfa-monitor/vera/font.bin` relative to project root |
| build.ps1 variable ordering | $RomBin used $MonDir before it was defined | Fixed: moved variable definitions to top |
| Splash cursor position | Splash wrote to row 0; text test printed on top of banner | Fixed: splash starts at row 5, cursor placed below splash |

---

### Issue #7 - NVC_GETLINE buffer overflow (nvram_config.inc)

**Severity:** Medium
**Status:** Fixed in nvram_config.inc (April 2 2026)

**Root Cause:** `NVC_GETLINE` used `MOVEQ #79,D7` with `DBRA D7,.read`. DBRA iterates
from 79 down to 0 inclusive = **80 iterations**. Each stores one byte at `(A0)+`. After
80 chars, `A0` points to `LINE_BUF + 80 = $02004050 = SAVE_D0`. The `.done` handler then
writes a NUL byte at `(A0)`, corrupting the first byte of the monitor's register save area.

In the NVRAM config menu, this could corrupt monitor state, potentially causing unexpected
behavior when returning to the main command loop.

**Fix applied:** Changed `MOVEQ #79,D7` to `MOVEQ #78,D7` in `NVC_GETLINE`. Now stores
max 79 chars (DBRA 78→0), leaving room for the NUL terminator within the 80-byte buffer.

---

### Issue #8 - P/X commands output S-records to VERA instead of serial

**Severity:** Medium
**Status:** Fixed in agfa-mon.m68 (April 2 2026 - second fix, same approach)

**Root Cause:** `EMIT_SRECS` (used by both P and X commands) called `PUTCHAR`,
`PRINT_HEX2`, `PRINT_CRLF`, and `PRINTSTR` - all of which redirect to VERA when
`NV_CONSOLE_OUT` bit 0 is set. S-record output needs to go to serial only for terminal
capture (e.g., saving to file).

The previous fix only changed the L command's prompt and X command's "not initialized"
message to `PRINTSTR_SERIAL`, but the actual S-record data path was unaffected.

**Fix applied (April 2 2026):** `EMIT_SRECS` now saves `NV_CONSOLE_OUT` to D3 at entry,
clears it for the duration of S-record output (forcing all PUTCHAR/PRINTSTR calls to serial),
and restores it from D3 at the `.term` label before returning. Same save/clear/restore
pattern used for Issue #2. This applies to both P (put/dump) and X (XBASIC dump) commands.

---

## Issue #10 - VT_VEC_HANDLE_LF clobbers D2

**Severity:** Medium (silent register corruption)
**Status:** Open - needs fix in vera_text.inc
**Discovered:** 06-APR-2026 during VERA extended API testing

**Root Cause:** `VERA_HANDLE_LF` is documented as clobbering only D0 and D1, but it also
clobbers D2. The call chain is:

```
VERA_HANDLE_LF
  → BSR CURSOR_FORCE_OFF
      → TST.B VT_CURSOR_STATE
      → (if cursor shown) BSR SET_CURSOR_ADDR
          → MOVEQ #0,D2          ← clobbers D2
          → MOVE.W VT_CUR_ROW,D2
          ...
```

If the cursor is currently visible when `VT_VEC_HANDLE_LF` is called, D2 is silently
overwritten with cursor address calculation data. If the cursor is hidden (VT_CURSOR_STATE=0),
`CURSOR_FORCE_OFF` returns immediately and D2 is safe - making this a state-dependent,
intermittent bug.

**Impact:** Any caller that holds a value in D2 and calls `VT_VEC_HANDLE_LF` directly may
see silent data corruption. Callers that go through `VT_VEC_PUTCHAR` are safe - PUTCHAR
saves/restores D0-D2/A0 via MOVEM, wrapping the internal HANDLE_LF call.

**Workaround (current):** Do not call `VT_VEC_HANDLE_LF` directly if D2 holds a live value.
Instead, use `VT_VEC_PUTCHAR` with LF ($0A) - PUTCHAR's MOVEM save/restore protects D2.

**Fix (proposed):** Add `MOVEM.L D0-D2,-(SP)` / `MOVEM.L (SP)+,D0-D2` save/restore to
`VERA_HANDLE_LF` in `vera_text.inc`, making it consistent with other VERA routines that
touch VERA registers.

---

## Issue #15 - Most VERA API calls lack ISR protection - VSYNC can corrupt VERA ADDR0 at any time

**Severity:** High (affects most VERA API calls; causes intermittent, non-deterministic corruption)
**Status:** Open - systematic fix needed across vera_text.inc
**Discovered:** 06-APR-2026 during VERA extended API testing

**Background:** The VSYNC ISR fires at ~60Hz and calls `CURSOR_TOGGLE` → `SET_CURSOR_ADDR`,
which writes three bytes to VERA ADDR0 registers and one byte to VERA DATA0. If this ISR fires
while any VERA API call is in the middle of its own VERA register access sequence, VERA ADDR0
is corrupted mid-operation. The subsequent DATA0 write by the original routine lands at the
wrong VRAM address.

**Affected calls - confirmed or likely vulnerable:**

| Call | Risk | Reason |
|------|------|--------|
| `VT_VEC_CLR_ROW` | **High** | 256-write DBRA loop with no IPL - ISR can fire many times during the loop (Issue #12) |
| `VT_VEC_CLRSCR` | **High** | Calls CLR_ROW 64 times with no outer IPL (Issue #12) |
| `VT_VEC_CLR_EOS` | **High** | Calls CLR_ROW in a loop with no outer IPL |
| `VT_VEC_HANDLE_LF` | **Medium** | No IPL; calls CURSOR_FORCE_OFF (3-write ADDR0 setup) and may call CLR_ROW on scroll |
| `VT_VEC_STREAM_START` | **Medium** | Sets ADDR0 with no IPL - ISR can fire before first STREAM_CHAR (Issue #11) |
| `VT_VEC_STREAM_CHAR` | **Medium** | Each call writes to DATA0 with no IPL (Issue #11) |
| `VT_VEC_GOTOXY` | **Low** | Calls CURSOR_FORCE_OFF → SET_CURSOR_ADDR (3-write ADDR0 setup) with no IPL |
| `VT_VEC_CURSOR_ON` | **Low** | Calls CURSOR_FORCE_ON → SET_CURSOR_ADDR with no IPL |
| `VT_VEC_CURSOR_OFF` | **Low** | Calls CURSOR_FORCE_OFF → SET_CURSOR_ADDR with no IPL |

**Already protected (no action needed):**

| Call | Protection |
|------|-----------|
| `VT_VEC_PUTCHAR` | `ORI.W #$0100,SR` at entry, `ANDI.W #$F8FF,SR` before return |
| `VT_VEC_CLR_EOL` | Same |
| `VT_VEC_SCROLL_UP` | Same - also covers the inner CLR_ROW call |
| `VT_VEC_SCROLL_DOWN` | Same - also covers the inner CLR_ROW call |

**Root cause of the inconsistency:** IPL protection was added to some routines but not
others during development. There is no consistent policy across the VERA text API.

**Fix (proposed):** Add `ORI.W #$0700,SR` / `ANDI.W #$F8FF,SR` at the entry/exit of
every VERA API routine that writes to VERA hardware registers and does not already have it.
Using `#$0700` (block all interrupts, not just VSYNC) is safest - the SCC receive ISR can
also corrupt VERA state if it triggers a PUTCHAR mid-operation.

The recommended approach per routine:
- **Loop-based routines** (`CLR_ROW`, `CLR_EOS`): add IPL block inside the loop body so
  each space+attr pair is an atomic write. Blocking for the entire loop (~256 writes) is
  also acceptable given each row takes only ~10µs at 16MHz.
- **Single-write routines** (`GOTOXY`, `CURSOR_ON/OFF`, `HANDLE_LF`): add IPL block
  around the full VERA register access sequence (ADDR0 setup + DATA0 write).
- **Streaming** (`STREAM_START`, `STREAM_CHAR`): add a paired `STREAM_END` routine that
  lowers IPL, so the caller raises IPL once for the entire streaming session.

**Note:** Issues #11 and #12 are specific instances of this broader problem.

---

## Issue #12 - VT_VEC_CLRSCR causes graphical anomalies (black `!` and garbage text at top of screen)

**Severity:** High (visible corruption, reproducible)
**Status:** Open - needs fix in vera_text.inc
**Discovered:** 06-APR-2026 during VERA extended API testing

**Symptom:** After calling `VT_VEC_CLRSCR`, the top row of the screen shows black `!` characters
(CP437 $21) spanning a variable portion of row 0 - sometimes the full 80 columns, sometimes
only 10-23, sometimes none. Random chunks of stale text may also appear elsewhere on screen.
The count and position varies between runs (non-deterministic).

Confirmed by two independent test programs: one using `VT_VEC_CLRSCR` + `VT_VEC_GOTOXY` +
`VT_VEC_PUTCHAR`, and another using ONLY `VT_VEC_CLRSCR`. Both exhibit identical symptoms,
proving the bug is in `VERA_CLRSCR` / `VERA_CLR_ROW` itself.

**Root Cause:** `VERA_CLR_ROW` performs 128 space+attribute writes to VERA via a DBRA loop
with no IPL protection. The VSYNC ISR fires at ~60Hz and calls `CURSOR_TOGGLE` →
`SET_CURSOR_ADDR`, which overwrites VERA ADDR0 mid-loop. When the ISR returns, the DBRA loop
continues writing from the corrupted ADDR0 (reset back to the cursor's attribute cell near
address 0), causing the remaining write iterations to land in the wrong VRAM cells. Cells
beyond the loop's new endpoint are never cleared and retain stale content from the previous
screen state. The stale content at those addresses (monitor output, prior test output, etc.)
becomes visible after the partial clear.

**Impact:** `VT_VEC_CLRSCR` is unreliable whenever the VSYNC ISR is active (i.e., after
`VERA_IRQ_INIT` has been called). Do not rely on a clean screen after calling it.

**Workaround (current):** Bracket `VT_VEC_CLRSCR` with full interrupt disable:
```asm
ORI.W   #$0700,SR       ; block ALL interrupts (VSYNC + SCC)
JSR     VT_VEC_CLRSCR
ANDI.W  #$F8FF,SR       ; re-enable interrupts
```

**Fix (proposed):** Add `ORI.W #$0700,SR` / `ANDI.W #$F8FF,SR` IPL blocking inside
`VERA_CLR_ROW` in `vera_text.inc`, wrapping the DBRA write loop. Since `VERA_CLRSCR`
calls `VERA_CLR_ROW` 64 times, the fix should be in `VERA_CLR_ROW` (protects the inner
loop) rather than in `VERA_CLRSCR` (would block ISR for the entire 64-row clear, ~1ms).

---

## Issue #14 - VT_VEC_SCROLL_DOWN has no guard against over-scrolling past VT_SCROLL_ROW=0

**Severity:** Critical (permanently breaks display state; requires reset to recover)
**Status:** Open - needs fix in vera_text.inc. API should not be used until fixed.
**Discovered:** 06-APR-2026 during VERA extended API testing

**Behavior (intentional - VT100-style):** When `VT_VEC_SCROLL_DOWN` is called, the
viewport moves back one line and the newly revealed top row is blanked. This mirrors
VT100 reverse-scroll behavior - a new blank line appears at the top. This is by design.

**Bug:** There is no guard against calling `VT_VEC_SCROLL_DOWN` more times than the
display has scrolled forward. `VT_SCROLL_ROW` is decremented unconditionally and wraps
mod 64. If the caller over-scrolls past `VT_SCROLL_ROW=0`, the value wraps to 63 and
the viewport shifts to show physical rows 63, 0, 1, 2... - a region of mostly blank or
uninitialized VRAM far from where any content was written.

`VT_CUR_ROW` is not adjusted by `SCROLL_DOWN`, so after over-scrolling the cursor is
at a physical row that is no longer within the visible viewport. All subsequent text
output (including the monitor prompt) goes to off-screen VRAM. The display appears dead
- no visible text, no cursor - until the machine is reset or power-cycled.

**There is no recovery without a reset.** The display state is held entirely in RAM
(`VT_SCROLL_ROW`, `VT_CUR_ROW`); a reset re-runs the boot sequence and restores them.

**Fix (proposed):** Add a guard in `VERA_SCROLL_DOWN`: if `VT_SCROLL_ROW == 0`, return
immediately without modifying any state. This prevents wrap-around at the cost of
silently ignoring calls beyond the scroll origin.

**Do not use `VT_VEC_SCROLL_DOWN` until this is fixed.** Callers have no safe way to
know how many times it can be called - `VT_SCROLL_ROW` is readable directly from the
CCB at `$02000304`, but there is no documented API to query the safe scroll-back limit.

---

## Issue #13 - VT_VEC_GOTOXY ignores hardware scroll offset (logical vs physical row)

**Severity:** High (silent positioning bug in scrolled display)
**Status:** Not fixed.
**Discovered:** 06-APR-2026 during VERA extended API testing

**Root Cause:** `VERA_GOTOXY` stored `D1` directly into `VT_CUR_ROW` without adding
`VT_SCROLL_ROW`. VERA uses a circular 64-row tilemap; the visible viewport starts at
physical row `VT_SCROLL_ROW`, not row 0. Passing a logical row `N` (0 = top of viewport)
to GOTOXY caused it to write to physical row `N`, which is `VT_SCROLL_ROW` rows *above*
the top of the visible area once any scrolling has occurred.

The issue was masked in testing because the test programs called `VT_VEC_CLRSCR` first,
which resets `VT_SCROLL_ROW = 0`, making physical == logical by coincidence. Any use of
GOTOXY after normal monitor activity (which scrolls as text is printed) would place the
cursor at the wrong position.

---

## Issue #11 - VT_VEC_STREAM_START / VT_VEC_STREAM_CHAR not IPL-protected

**Severity:** Low (intermittent, race condition)
**Status:** Open - needs fix in vera_text.inc
**Discovered:** 06-APR-2026 during VERA extended API testing

**Root Cause:** `VERA_STREAM_START` and `STREAM_CHAR` write directly to VERA registers
without blocking the VSYNC ISR (no `ORI.W #$0100,SR`). If the VSYNC ISR fires between
`STREAM_START` and a `STREAM_CHAR` call, or between two `STREAM_CHAR` calls, the ISR's
`SET_CURSOR_ADDR` will overwrite VERA ADDR0, causing subsequent `STREAM_CHAR` writes to
land at the cursor's attribute cell instead of the streaming destination.

This is a low-probability race (streaming ~12 chars takes ~24µs vs 16,700µs per frame),
but it is a real hazard for longer streaming operations or on faster hardware.

**Workaround (current):** Callers can bracket streaming with their own IPL block:
```asm
ORI.W   #$0100,SR           ; block VSYNC ISR
JSR     VT_VEC_STREAM_START
; ... STREAM_CHAR calls ...
ANDI.W  #$F8FF,SR           ; re-enable VSYNC ISR
```

**Fix (proposed):** Add IPL blocking inside `VERA_STREAM_START` (raise on entry, lower on
exit - or provide a paired `VERA_STREAM_END` that lowers IPL). Alternatively, document that
callers must wrap streaming with IPL blocking, consistent with the pattern used in
`VERA_SCROLL_UP`, `VERA_CLR_EOL`, and `VERA_CLR_EOS`.
