# AGFA-MON API Reference & Callable Routines

This document describes all the callable entry points, commands, and routines available in AGFA-MON firmware. This is a quick reference for code that needs to call ROM routines or understand what functions are available.

---

## API Jump Table (Fixed Addresses)

The ROM exports a jump table at fixed addresses for use by programs (EhBASIC, test programs, etc.). These addresses are **guaranteed to not change** and can be directly CALL'd or JMP'd to.

### PUTCHAR - $0406
```
Input:      D0.B = character to output
Output:     (none)
Clobbers:   None (D1 saved/restored on serial path; D0-D2/A0 saved/restored on VERA path)
Preserves:  All registers
Special:    On VERA path, briefly raises IPL to block VSYNC ISR during VERA access
```

Outputs a single character to serial or VERA based on config.

### GETCHAR - $040C
```
Input:      (none)
Output:     D0.B = character read
Clobbers:   D0
Preserves:  A0-A6, D1-D7, SR, CCR
Blocks:     Until character available on serial
```

Blocking input from serial RS-232 only. PS/2 keyboard support is in development - when complete, GETCHAR will redirect to PS/2 input when configured via NVRAM. For now, always reads from the Z85C30 SCC.

### PRINTSTR - $0412
```
Input:      A0 = pointer to NUL-terminated string
Output:     (none; A0 left pointing past the NUL)
Clobbers:   A0, D0 (saved/restored on VERA path; modified on serial path)
Preserves:  A1-A6, D1-D7, SR, CCR (mostly)
```

Prints a NUL-terminated string to serial or VERA. A0 is left pointing one byte past the NUL terminator on both paths. Always reload A0 before reusing the same string pointer.

### GETLINE - $0418
```
Input:      A0 = pointer to 80-byte input buffer
Output:     (none - no return value; D0 restored to original)
Clobbers:   Nothing (D0, D1, A1 saved/restored internally; A0 never modified)
Preserves:  All registers
Side effects: Fills buffer at A0 with typed line; NUL-terminated
```

Reads a line into the buffer with full editing support:
- Handles backspace ($08) and delete ($7F) with destructive echo
- Returns on CR ($0D) or LF ($0A)
- Auto-appends NUL terminator
- Max 79 characters + NUL
- To get the length after return, scan the buffer for the NUL terminator

**Design note:** GETLINE does not return a character count in D0. D0 is saved on entry and restored on exit - the original caller value is preserved unchanged. This has been verified on hardware. If you need the length, scan the buffer after the call. This is a known API limitation; a future ROM revision could return the count in D0, but that would require a reflash.

### CMD_LOOP - $041E
```
Input:      (none)
Output:     (never returns; enters monitor command loop)
Clobbers:   All registers modified (not an API for return)
```

Returns to the AGFA-MON monitor prompt. Does not return to caller.

### GETTICKS - $0424
```
Input:      (none)
Output:     D0.L = 60Hz tick count
Clobbers:   D0
Preserves:  A0-A6, D1-D7, SR, CCR
```

Returns the current 60Hz tick counter (increments ~60 times/second via VIA #1 T1 timer).

---

### Quick Register Preservation Summary

| Routine | D0 after call | A0 after call | All others | Notes |
|---------|--------------|---------------|------------|-------|
| PUTCHAR | Preserved | Preserved | Preserved | All registers safe |
| GETCHAR | **= char read** | Preserved | Preserved | Only D0 changes |
| PRINTSTR | Preserved | **Past NUL** | Preserved | A0 walks the string on all paths |
| GETLINE | Preserved | Preserved | Preserved | No return value; buffer filled |
| CMD_LOOP | - | - | - | Never returns |
| GETTICKS | **= tick count** | Preserved | Preserved | Only D0 changes |

---

### General Notes

- All routines respect the NVRAM config setting for console I/O (serial vs VERA)
- PUTCHAR on the VERA path raises IPL temporarily (ORI.W #$0100,SR) to prevent VSYNC ISR from accessing VERA ADDR0 simultaneously - this is a brief critical section, not a blocking wait
- GETCHAR currently always reads from serial; PS/2 input is in development and will be selectable via NVRAM config when complete
- GETLINE does not return a value in D0 - check the buffer contents after the call
- GETTICKS counter rolls over after ~2.5 years (32-bit counter @ 60Hz)

---

## Monitor Commands (User-Facing)

Type these single characters at the `AGFA-MON>` prompt:

| Command | Format | Purpose |
|---------|--------|---------|
| `B` | `B` | Launch EhBASIC interpreter (CALL 1054 to exit) |
| `C` | `C` | Boot CP/M |
| `D` | `D [addr] [len]` | Dump memory (hex + ASCII, default 128 bytes) |
| `E` | `E <addr>` | Examine/edit memory interactively (type `.` to quit) |
| `F` | `F <addr> <len> <byte>` | Fill memory range with byte value |
| `G` | `G [addr]` | Execute program (default: last loaded address $02050000) |
| `H` or `?` | `H` or `?` | Show command help |
| `L` | `L` | Load S-records from terminal |
| `M` | `M <src> <dst> <len>` | Copy memory (move) |
| `P` | `P <addr> <len>` | Send S-records to terminal |
| `R` | `R` | Show CPU registers |
| `S` | `S` | Scan SCSI bus |
| `T` | `T` | RAM burn-in test |
| `U` | `U` | Boot UNIX |
| `V` | `V` | NVRAM configuration |
| `X` | `X` | Save BASIC program as S-records |

**All numbers in hexadecimal**

---

## PUTCHAR Special Character Handling

When outputting via VERA, PUTCHAR handles these control characters:

| Char | Hex | Behavior |
|------|-----|----------|
| BS | $08 | Move cursor left one column |
| LF | $0A | Line feed (scroll if at bottom) |
| CR | $0D | Carriage return (column 0, hide cursor) |
| FF | $0C | Clear screen, cursor to (0,0) |

All other bytes are output as characters.

**Example - 500ms delay:**
```asm
GETTICKS_VEC    EQU     $0424

        JSR     GETTICKS_VEC        ; D0.L = current ticks
        ADD.L   #30,D0              ; target = now + 30 ticks (~500ms @ 60Hz)
        MOVE.L  D0,D1               ; save target in D1 (GETTICKS clobbers D0)
.wait:  JSR     GETTICKS_VEC        ; D0.L = current ticks
        CMP.L   D1,D0               ; current >= target?
        BLT.S   .wait               ; no, keep waiting
```

---

## VERA Graphics & Text Functions

VERA support is optional and detected at cold-boot. If VERA is present, text I/O automatically redirects to the video display.

### When VERA is Active

- `PUTCHAR` outputs to VERA text screen instead of serial
- `PRINTSTR` outputs to VERA text screen
- `GETCHAR` may input from PS/2 keyboard (if enabled in config)
- Character output to VERA is **non-blocking** and queued per-frame

### VERA Configuration

Use the `V` command at the AGFA-MON prompt to:
- Select console output (serial or VERA)
- Select console input (serial or PS/2)
- Set display resolution and mode
- View current NVRAM settings

### VERA Memory Layout

```
$04000000-$04000FFF   VERA registers and VRAM
$04000020             IEN (interrupt enable)
$04000021             ISR (interrupt status)
$04002000-$0400FFFF   Video RAM (64KB)
```

### VERA Extended API - Jump Vectors

All VERA-specific calls go through RAM jump thunks at fixed addresses. These are only valid after VERA has been initialized (check `VT_MAGIC == $A55A` first). Call with `JSR VT_VEC_xxx` - no parentheses.

**All entries in this table reflect hardware test results as of 06-APR-2026.**
Status and bug information is current as of that date. Future ROM rebuilds may fix bugs
and change behavior or calling conventions - re-verify after any ROM update.

**Legend:** OK = verified on hardware | OK/BUGGY = verified but buggy - do not use until fixed | INTERNAL = internal use only | STUB = stub - not implemented

**Text output:**

| Address | Symbol | Registers | Status | Notes |
|---------|--------|-----------|--------|-------|
| `$02004100` | `VT_VEC_PUTCHAR` | In: D0.B=char. Preserves all (MOVEM save/restore D0-D2/A0) | OK 06-APR-2026 | CR/LF/BS handled; IPL-blocked during VERA access |
| `$02004108` | `VT_VEC_PRINTSTR` | In: A0=string ptr. A0 walks past NUL. Preserves D0-D7, A1-A6 | OK 06-APR-2026 | Loops VERA_PUTCHAR; no independent save/restore |
| `$02004110` | `VT_VEC_CLRSCR` | No args. Preserves all (MOVEM save/restore D0-D1) | OK/BUGGY 06-APR-2026 - Issue #12 | Clears all 64 rows; resets scroll+cursor to (0,0). Graphical corruption when ISR is active. **Do not use until fixed.** |
| `$02004118` | `VT_VEC_CLR_ROW` | In: D1.W=row (0-63). Preserves all (MOVEM save/restore D0-D2) | OK 06-APR-2026 | Clears all 128 cols; D1 restored to input row on exit |
| `$02004120` | `VT_VEC_HANDLE_LF` | No args. **Clobbers D0, D1, D2** (no save/restore) | OK 06-APR-2026 | Advance row+scroll if needed. D2 clobbered via CURSOR_FORCE_OFF->SET_CURSOR_ADDR |
| `$02004128` | `VT_VEC_STREAM_START` | No args. **Clobbers D0, D1** | OK 06-APR-2026 | Sets VERA ADDR0 to cursor pos, stride=1. Must be followed by STREAM_CHAR calls |
| `$02004130` | `VT_VEC_STREAM_CHAR` | In: D0.B=char. **Clobbers D0** | OK 06-APR-2026 | Writes char+attr to VERA DATA0. VERA auto-increments ADDR0. Does NOT update VT_CUR_COL |
| `$02004168` | `VT_VEC_CLR_EOL` | No args. Preserves all (MOVEM save/restore D0-D2) | OK 06-APR-2026 | Clears cursor col through col 79 on current row. Does not move cursor |
| `$02004170` | `VT_VEC_CLR_EOS` | No args. Preserves all (MOVEM save/restore D0-D2) | OK 06-APR-2026 | CLR_EOL on cursor row, then CLR_ROW on all rows below viewport bottom. Does not move cursor |

**Cursor and position:**

| Address | Symbol | Registers | Status | Notes |
|---------|--------|-----------|--------|-------|
| `$02004138` | `VT_VEC_GOTOXY` | In: D0.W=col, D1.W=row. **Clobbers D0, D2, A0** | OK/BUGGY 06-APR-2026 - Issue #13 | Takes physical tilemap row, not logical viewport row. Ignores VT_SCROLL_ROW. **Do not use until fixed.** |
| `$02004140` | `VT_VEC_SET_ATTR` | In: D0.B=attr. Preserves all | OK 06-APR-2026 | Stores to VT_TEXT_ATTR in CCB. hi nibble=bg, lo nibble=fg |
| `$02004148` | `VT_VEC_CURSOR_ON` | No args. **Clobbers D0, D2, A0** | OK 06-APR-2026 (console verified) | Sets CURSOR_EN=1, forces cursor visible, resets blink timer |
| `$02004150` | `VT_VEC_CURSOR_OFF` | No args. **Clobbers D0, D2, A0** | OK 06-APR-2026 (console verified) | Hides cursor, sets CURSOR_EN=0 (ISR will not re-show) |
| `$02004188` | `VT_VEC_CURSOR_TOGGLE` | No args. **Clobbers D0, D2, A0** | INTERNAL use only | Called by VSYNC ISR to blink cursor. Do not call directly from user code - the ISR owns the cursor blink state. |

**Scrolling:**

| Address | Symbol | Registers | Status | Notes |
|---------|--------|-----------|--------|-------|
| `$02004158` | `VT_VEC_SCROLL_UP` | No args. Preserves all (MOVEM save/restore D0-D1; IPL-blocked) | OK 06-APR-2026 (console verified) | Advance SCROLL_ROW +1, update L0_VSCROLL, clear new bottom row |
| `$02004160` | `VT_VEC_SCROLL_DOWN` | No args. Preserves all (MOVEM save/restore D0-D1; IPL-blocked) | OK/BUGGY 06-APR-2026 - Issue #14 | VT100-style reverse scroll: viewport back one line, new top row blanked. No guard against over-scrolling past VT_SCROLL_ROW=0 - wraps viewport, display breaks until reset. **Do not use until fixed.** |

**Unimplemented stubs - return immediately (RTS only):**

| Address | Symbol | Status |
|---------|--------|--------|
| `$02004178` | `VT_VEC_INSERT_LINE` | STUB planned - not implemented |
| `$02004180` | `VT_VEC_DELETE_LINE` | STUB planned - not implemented |

**Initialization (called by ROM at boot - not for general use):**

| Routine | Purpose |
|---------|---------|
| `VERA_DETECT` | Probe for VERA hardware, return D0.B = 1 if found |
| `VERA_INIT` | Full VERA hardware initialization |
| `VERA_IRQ_INIT` | Set up VSYNC IRQ for cursor blink and GETTICKS |
| `VERA_IRQ_SHUTDOWN` | Disable VSYNC IRQ |
| `VT_INIT_RAM` | Copy VERA text routines from ROM to RAM ($02004500+) |
| `VT_INIT_VECTORS` | Install JMP thunks into VT_VEC_* addresses |

### VERA Extended API - Known Bugs

> The following VERA extended API calls have confirmed bugs discovered during hardware testing.
> **Do not use any of these calls in new code until they are fixed in a ROM rebuild.**
>
> Fixes to these routines may change their register usage, calling convention, or behavior.
> Any code written against the current broken behavior will likely break again after the fix.
> Write your code against the corrected specification once a fixed ROM is available.

**VT_VEC_CLRSCR - Issue #12 - Graphical corruption when VSYNC ISR is active**
`VERA_CLR_ROW` (called 64 times by CLRSCR) has no IPL protection. The VSYNC ISR fires mid-loop and corrupts VERA ADDR0, leaving stale text and black `!` characters on the top row. Variable, non-deterministic count. Affects all programs after `VERA_IRQ_INIT` has been called.
**Do not use until fixed.** Workaround if absolutely necessary:
```asm
ORI.W   #$0700,SR
JSR     VT_VEC_CLRSCR
ANDI.W  #$F8FF,SR
```

**VT_VEC_GOTOXY - Issue #13 - Does not account for hardware scroll offset**
`VERA_GOTOXY` stores the row argument directly into `VT_CUR_ROW` without adding `VT_SCROLL_ROW`. After any scrolling has occurred, row 0 is not the top of the visible viewport - it is a physical tilemap row that may be completely off-screen.
**Do not use until fixed.** Only safe immediately after `VT_VEC_CLRSCR` (which resets `VT_SCROLL_ROW=0`), but that is a fragile dependency. Wait for the ROM fix.

**VT_VEC_SCROLL_DOWN - Issue #14 - Over-scrolling past VT_SCROLL_ROW=0 breaks the display**
No guard against being called more times than the display has scrolled forward. `VT_SCROLL_ROW` wraps to 63, the viewport shifts to blank VRAM, the cursor goes off-screen, and all subsequent output is invisible. Requires a reset to recover.
**Do not use until fixed.**

---

### VERA Extended API - Important Usage Notes

**STREAM_START + STREAM_CHAR - caller must manage cursor**
`STREAM_CHAR` writes directly to VERA VRAM and does not update `VT_CUR_COL`. After streaming, the CCB cursor position is stale. To advance to the next line cleanly, use `VT_VEC_PUTCHAR` with CR ($0D) + LF ($0A) - PUTCHAR saves/restores D0-D2/A0 internally so it is safe to call after streaming. Do NOT call `VT_VEC_HANDLE_LF` directly after streaming if you have a live D2 value - HANDLE_LF clobbers D2 via CURSOR_FORCE_OFF.

**HANDLE_LF clobbers D2**
Despite appearing to only use D0 and D1, `VT_VEC_HANDLE_LF` internally calls `CURSOR_FORCE_OFF` → `SET_CURSOR_ADDR`, which clobbers D2. Do not rely on D2 surviving a HANDLE_LF call. Verified on hardware 06-APR-2026.

**VERA must be initialized before any VT_VEC_* call**
Check `VT_MAGIC ($02000306) == $A55A` AND `CONFIG_BUF+NV_CONSOLE_OUT` bit 0 is set before calling any VT_VEC_* routine. Calling these when VERA is not active will access uninitialized RAM jump thunks.

---

## CPU Register Saving

The monitor maintains a register save area that programs can inspect:

| Address | Size | Content | Symbols |
|---------|------|---------|---------|
| `$02004050` | 32 bytes | D0-D7 data registers | `SAVE_D0` |
| `$02004070` | 28 bytes | A0-A6 address registers | `SAVE_A0` |
| `$02004090` | 4 bytes | PC (program counter) | `SAVE_PC` |
| `$02004094` | 2 bytes | SR (status register) | `SAVE_SR` |

**Note:** A7 (SSP) is at `$02400000` (stack top).

---

## Memory Map

```
$000000-$001FFF   ROM Bank 0 (AGFA-MON firmware, 128KB)
  $000000-$0003FF   Exception vector table (256 vectors)
  $000400-$00045F   API jump table
  $000460+          START code

$02000000-$023FFFFF   RAM (4MB)
  $02001000-$020013FF   VBR (vector table copy, 1KB) - RESERVED
  $02004000-$020040FF   Monitor workspace
    $02004000-$0200407F   LINE_BUF, SAVE_* areas
    $02004096-$020040BF   CONFIG_BUF (NVRAM working copy, 40 bytes)
    $02004100-$020041FF   VERA dispatch JMP thunks (128 bytes)
    $02004500+            VERA text routines (copied from ROM at cold-boot)
  $02008000-$023FFFFF   EhBASIC workspace + user program RAM

$04000000-$0400FFFF   VERA graphics controller (optional)

$05000000-$0500FFFF   SCSI controller (AM5380)

$07000000-$07000FFF   Z85C30 SCC & miscellaneous I/O
```

---

## Hardware Initialization

The following subsystems are initialized at cold-boot:

1. **Z85C30 SCC Channel A** - Serial interface (9600 8N1)
2. **VIA #1 Timer** - 60Hz tick for GETTICKS
3. **VERA (if present)** - Video controller initialization
4. **VIA #2 (if VERA)** - IRQ routing for VERA VSYNC

---

## Interrupt Handling

### IPL Levels

```
IPL 0   Disabled
IPL 1   VERA VSYNC IRQ (via VIA #2 autovector)
IPL 2-6 Reserved
IPL 7   NMI (not used)
```

**PUTCHAR blocks during IPL > 0** to prevent VERA ADDR0 contention with the VSYNC ISR. This ensures safe character output while VERA is being accessed by the interrupt handler.

---

## Useful Addresses for Assembly

### I/O Ports
```asm
SCC_A_CTRL  EQU $07000002   ; SCC Channel A control
SCC_A_DATA  EQU $07000003   ; SCC Channel A data
VERA_BASE   EQU $04000000   ; VERA graphics controller
```

### Entry Points
```asm
PUTCHAR_VEC EQU $0406
GETCHAR_VEC EQU $040C
PRINTSTR_VEC EQU $0412
GETLINE_VEC EQU $0418
CMD_LOOP_VEC EQU $041E
GETTICKS_VEC EQU $0424
```

### RAM Variables
```asm
LINE_BUF    EQU $02004000   ; 80-byte line buffer
SAVE_D0     EQU $02004050   ; D0-D7 save area
SAVE_A0     EQU $02004070   ; A0-A6 save area
SAVE_PC     EQU $02004090   ; PC save
SAVE_SR     EQU $02004094   ; SR save
CONFIG_BUF  EQU $02004096   ; NVRAM config (40 bytes)
STACK_TOP   EQU $02400000   ; supervisor stack base
```

---

## Example: Calling PUTCHAR from Assembly

```asm
        ORG     $02050000       ; standard test program location

        ; Print 'A' to serial/VERA
        MOVEQ   #65,D0          ; ASCII 'A'
        JSR     $0406           ; PUTCHAR_VEC

        ; Print newline
        MOVEQ   #10,D0          ; LF
        JSR     $0406

        HALT
```

---

## Example: Calling PRINTSTR from Assembly

```asm
        ORG     $02050000

        LEA     MESSAGE,A0
        JSR     $0412           ; PRINTSTR_VEC

        HALT

MESSAGE:
        DC.B    "Hello AGFA-MON!",13,10,0
```

register.
- **Canary testing** (loading a known value and checking it survived) is the best way to catch clobbers early. See `examples/hello_world.m68` for a working example.

---

## Command Examples

### Load and Run a Test Program

```
AGFA-MON> L                          ; enter load mode
(paste S-record content)
OK 02050000                          ; confirmation

AGFA-MON> G 02050000                 ; run the program
```

### Dump Memory

```
AGFA-MON> D 2004000 100              ; dump 256 bytes from $2004000
```

### Fill Memory

```
AGFA-MON> F 2008000 1000 55          ; fill $2008000-$2008FFF with $55
```

### Enter Examine/Edit Mode

```
AGFA-MON> E 2004000                  ; start editing at $2004000
02004000: 12345678 ?                 ; shows current value, ? = change
```

---

## See Also

- `agfa_mon.md` - Complete firmware reference (boot sequence, ROM layout, exceptions)
- `agfa_mon_memory_map.md` - Detailed memory layout
- `vera_programmers_guide_digested.md` - VERA graphics controller reference
