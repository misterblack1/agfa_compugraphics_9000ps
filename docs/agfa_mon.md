# Agfa 9000PS - AGFA-MON Firmware Reference

Machine language monitor for the Agfa Compugraphic 9000PS. Written in 68020 assembly by Adrian Black. Burned into Bank 0 EPROMs (the 4x AM27C256 chips at addresses $00000-$1FFFF in 32-bit HH/HM/LM/LL interleave). Provides a serial command-line interface for memory access, program loading, SCSI operations, and EhBASIC.

**Assemble with:** `vasmm68k_mot -m68020 -Fbin -o agfa-rom.bin agfa-mon.m68`

---

## ROM Binary Layout

| Address Range | Content |
|--------------|---------|
| $000000-$0003FF | Exception vector table (256 x longword) |
| $000400-$00045F | API jump table (JMP entries at fixed addresses; $042A-$045F reserved) |
| $000460-$0017FF | Monitor code: START, CMD_LOOP, all command handlers, SCC HAL, utilities, strings |
| $001800-$0058F1 | EhBASIC 68K v3.54 engine (included via agfa-basic.m68) |
| $006000-$007FFF | Extended commands: BURNIN_TEST, NS_SCSI_SCAN and helpers |

---

## Firmware Location and Boot Sequence

**ROM Location:** AGFA-MON is burned into Bank 0 EPROMs (4x AM27C256, addresses 0x00000-0x1FFFF). The 4 chips (HH, HM, LM, LL lanes) hold the 32KB monitor code.

**Activation:** Power-on reset jumps to PC = 0x00004 (reset vector) which points to AGFA-MON cold boot at 0x00460.

### Cold Boot - Exact Sequence from Source (ORG $000460)

1. `MOVEC D0,CACR` with D0=$0001 - enable 68020 instruction cache
2. `MOVEA.L #$02400000,SP` - establish supervisor stack
3. Clear monitor register-save area: CLR.L loop, 18 longwords at $02004050-$02004097
4. `MOVE.L #$FFFFFFFF,$06100000` - rendering controller reset (required before SCC access; without this the PAL does not assert DTACK and every SCC access bus-errors)
5. `CLR.L $06080000` - FIFO/hardware register clear
6. `CLR.L $060C0000` - FIFO/hardware register clear
7. **BSR INIT_HW** - initialise Z85C30 SCC Channel A:
   - `TST.B $07000020` - PAL sync strobe (required before every SCC init sequence)
   - WR9 = $C0 - hardware reset both channels
   - Wait loop: 10240 ($2800) iterations
   - WR4 = $44 - x16 clock, 1 stop bit, no parity
   - WR11 = $50 - BRG to Rx and Tx clocks
   - WR12 = $0A, WR13 = $00 - 9600 baud time constant
   - WR14 = $01 - enable BRG, PCLK source
   - WR3 = $C1 - 8 bits/char, Rx enable
   - WR5 = $E8 - 8 bits/char, Tx enable, /DTR asserted, RTS=0 (RTS=0 keeps /RTSA HIGH -> U18 NAND output LOW -> RS-232 CTS asserted at terminal)
   - WR1 = $00 - no interrupts
8. Print STR_BANNER to serial
9. **BSR RAM_TEST_BOOT:**
   - Print `System starting: RAM test $02008000-$023EFFFF` (no newline yet)
   - RT_PASS with pattern $AAAAAAAA: write entire range, then verify; print one `.` per 512KB block verified; suppress errors after 8 per pattern
   - RT_PASS with pattern $55555555: same
   - If total errors = 0: print ` RAM OK\r\n`
   - If errors: print `\r\n*** RAM ERRORS DETECTED ***\r\n` (each error line shows: `  FAIL @ $XXXXXXXX  wr $XXXXXXXX  rd $XXXXXXXX  diff $XXXXXXXX  BnkN D0 D3 ...`)
10. Print STR_HELP (full command list)
11. **Fall through to CMD_LOOP** (no jump - the code literally falls off the end of START into CMD_LOOP)

### CMD_LOOP - Prompt Cycle

- Print `\r\nAGFA-MON> `
- BSR GETLINE - reads into LINE_BUF ($02004000, 79 chars max); handles BS/DEL with destructive backspace; echoes chars; returns on CR or LF
- Skip leading spaces
- Read and fold first character to uppercase
- Dispatch on character; blank line loops silently; unknown command prints `?\r\n`

### Warm Re-Entry

JMP to $041E (CMD_LOOP_VEC) - enters at the CMD_LOOP label; does NOT re-init hardware or repeat RAM test.

---

## Exact Output on Power-Up

The RAM test line has no newline after the header - dots print inline. With $02008000-$023EFFFF (14 x 512KB blocks), each of the two write+verify passes (AA, then 55) prints 14 dots, giving 28 dots total on one line:

```
Agfa Compugraphic 9000PS
by Adrian Black / Adrian's Digital Basement 2026

System starting: RAM test $02008000-$023EFFFF............................. RAM OK
Commands:
  B                       : Launch BASIC (CALL 1054 to exit)
  C                       : Boot CP/M
  D [addr] [len]          : Hex/ASCII dump (default 128 bytes)
  E <addr>                : Examine/edit memory (. to quit)
  F <addr> <len> <byte>   : Fill memory
  G [addr]                : Execute
  H or ?                  : Show this help
  L                       : Load S-records from terminal
  M <src> <dst> <len>     : Copy memory
  P <addr> <len>          : Send S-records to terminal
  R                       : Show CPU registers
  S                       : Scan SCSI bus
  T                       : RAM burn-in test
  U                       : Boot UNIX
  V                       : NVRAM config
  X                       : Save BASIC program as S-records
All numbers in hex

AGFA-MON>
```

On a RAM error, each failing longword prints inline before the next dot threshold, then the final line changes:
```
System starting: RAM test $02008000-$023EFFFF
  FAIL @ $02100004  wr $AAAAAAAA  rd $AA00AAAA  diff $0000AAAA  Bnk1 D8 D9 D10 D11 D12 D13 D14 D15
..............
*** RAM ERRORS DETECTED ***
```

---

## Exact Output of S (SCSI Scan) Command

NS_STR_ID = `"\r\nID "`, so each ID line starts on its own line after a CRLF. NS_STR_DONE = `"\r\nScan complete.\r\n"`. Empty bus:

```
AGFA-MON> s

ID 0 empty
ID 1 empty
ID 2 empty
ID 3 empty
ID 4 empty
ID 5 empty
ID 6 empty

Scan complete.

AGFA-MON>
```

Device found at ID 0 (exact field widths - vendor 8 chars, model 16 chars, revision 4 chars, space-padded by drive):
```
ID 0 FOUND: [QUANTUM ][LPS240S         ] REV [J542]
 -> Capacity: 238 MB
 -> Geometry: Cyls [1818] Heads [10]
```

Device found but returned CHECK CONDITION status:
```
ID 0 FOUND:  (CHECK CONDITION)
```

**How it works internally:**
1. `NS_RESET_BUS`: assert RST (ICR=$80), wait 200,000 iterations, deassert, read SCSI_RESET_REG to clear interrupt
2. Loop D7 = 0 to 6: print `\r\nID ` + digit
3. `NS_SELECT_TARGET`: write self(ID7)|target bitmask to ODR, assert DBUS ($01), then SEL ($05); poll BSY up to 1,000,000 iterations; if no BSY -> print ` empty`, return -1; if BSY -> delay 10,000 iterations, return 0
4. If selected: print ` FOUND: `, then issue INQUIRY ($12), READ CAPACITY ($25), MODE SENSE ($1A page 4)
5. State machine `NS_RUN_SM` handles Command / Data In / Status / Message In phases with TCR interlock on every byte
6. `NS_RELEASE_BUS`: clear ICR and TCR

---

## Serial Interface

**Port:** Z85C30 SCC Channel A - control $07000002, data $07000003
**Configuration:** 9600 baud, 8N1 (WR4=$44, WR3=$C1, WR5=$E8)
**Connector:** DE-9 RS-232 port

### PUTCHAR

ORG ~$09xx. Saves D1, D2, A0. Poll RR0 bit 2 (Tx buffer empty) then write D0.b to SCC_A_DATA. No echo, no interrupt.

**VERA cursor integration:** When VERA is active, PUTCHAR calls `VT_VEC_CURSOR_ON` after emitting each character to keep the cursor solid while typing. IPL-1 is blocked (interrupt mask raised) inside PUTCHAR to prevent the VSYNC ISR from corrupting VERA ADDR0 during the character write. D1, D2, and A0 are saved and restored across the full sequence.

### GETCHAR

ORG ~$09xx. Poll RR0 bit 0 (Rx char available) then read SCC_A_DATA into D0.b. **Does NOT strip to 7-bit** - returns full 8-bit byte. Destroys D0 only.

### PRINTSTR

ORG ~$09xx. MOVEM.L D0,-(SP) then loop: MOVE.B (A0)+,D0 / BEQ done / BSR PUTCHAR. Advances A0 past the NUL. Restores D0.

### PRINT_CRLF

Sends $0D then $0A.

### GETLINE

ORG ~$09xx. Reads into buffer at A0, max 79 chars (LINE_BUF_SZ-1). Handles:
- CR or LF -> NUL-terminate, print CRLF, return (A0 unchanged, points to buffer base)
- BS ($08) or DEL ($7F) -> destructive backspace (sends BS SP BS), does not go before buffer start
- Any other char -> store, echo, decrement remaining; silently ignores chars when buffer full
- A0 is restored to the buffer base before return

### FLUSH_TX

Used by exception handlers. Polls RR0 bit 2 (Tx empty), then selects RR1 and polls bit 0 (All Sent) to ensure the TX shift register has fully transmitted before an SCC hardware reset.

---

## VERA Cursor Blink

When VERA is installed, a software cursor blink is maintained by the VSYNC interrupt service routine at IPL-1 (~60 Hz field rate). The cursor toggles every ~500 ms (approximately every 30 VSYNC interrupts).

### Cursor Routines

All cursor routines live inside the `VT_ROM_START`..`VT_ROM_END` block in `vera_text.inc`:

| Routine | Purpose |
|---------|---------|
| `CURSOR_TOGGLE` | Toggle cursor visibility (called by VSYNC ISR via thunk) |
| `CURSOR_FORCE_ON` | Force cursor on unconditionally |
| `CURSOR_FORCE_OFF` | Force cursor off unconditionally |
| `SET_CURSOR_ADDR` | Set the VERA tilemap address where the cursor is drawn |

### ISR Thunk

The VSYNC ISR calls the cursor toggle indirectly through the thunk vector `VT_VEC_CURSOR_TOGGLE` (a RAM longword in the VERA CCB area). This avoids a direct ROM-to-ROM call and keeps the ISR linkage relocatable.

### IPL Masking in PUTCHAR

Because the VSYNC ISR and PUTCHAR both access VERA ADDR0, PUTCHAR raises the interrupt mask to block IPL-1 during its VERA write window, then restores the original mask afterward. This prevents ISR-induced ADDR0 corruption during character output.

---

## VIA #1 T1 Fallback Timer

When VERA is **not** installed, `VIA_TIMER_INIT` configures VIA #1 Timer 1 (T1) to generate interrupts at approximately 60 Hz, using autovector IPL-4 ($070). The ISR increments the same `TICK_COUNT` longword at `$02001400` that the VSYNC ISR would otherwise maintain. This provides a consistent time base regardless of whether VERA is present.

The GETTICKS API vector ($0424) reads TICK_COUNT and returns it in D0.l.

---

## VERA Console API (Dispatch Vectors)

When VERA is installed, the text console is driven through a set of routines in RAM at `$02004500+` (copied from ROM at boot). These are reached via JMP thunks at `$02004100+`. Call with `JSR addr` (not indirect).

Each thunk slot is 8 bytes (6-byte JMP abs.L + 2 pad).

### Core Text Routines

| Address | Symbol | Input | Effect |
|---------|--------|-------|--------|
| $02004100 | VT_VEC_PUTCHAR | D0.B = char | Write character to console. Handles CR, LF, BS, printable chars. VT100-style pending wrap at col 80. IPL-1 blocked during VERA access. |
| $02004108 | VT_VEC_PRINTSTR | A0 = string | Print NUL-terminated string. Calls VERA_PUTCHAR per character. |
| $02004110 | VT_VEC_CLRSCR | - | Clear entire viewport, reset cursor to (0,0), reset scroll. |
| $02004118 | VT_VEC_CLR_ROW | D1.W = row | Clear physical tilemap row (0-63). Writes 128 space+attr pairs. |
| $02004120 | VT_VEC_HANDLE_LF | - | Advance row with scroll logic (internal, also callable directly). |

### Streaming Routines (High-Speed Sequential Output)

| Address | Symbol | Input | Effect |
|---------|--------|-------|--------|
| $02004128 | VT_VEC_STREAM_START | - | Set VERA ADDR0 to current (row, col) with stride=1. |
| $02004130 | VT_VEC_STREAM_CHAR | D0.B = char | Write char + current attribute to VERA DATA0 (assumes ADDR0 set). |

### Extended Console API

| Address | Symbol | Input | Effect |
|---------|--------|-------|--------|
| $02004138 | VT_VEC_GOTOXY | D0.W = col (0-79), D1.W = row (0-63) | Move cursor to absolute position. Hides cursor at old position. Clears pending-wrap flag. Row is physical tilemap row. |
| $02004140 | VT_VEC_SET_ATTR | D0.B = attr | Set text attribute. Upper nibble = background palette index (0-15), lower nibble = foreground palette index (0-15). |
| $02004148 | VT_VEC_CURSOR_ON | - | Enable cursor blink, force cursor visible. |
| $02004150 | VT_VEC_CURSOR_OFF | - | Hide cursor, disable blink. |
| $02004158 | VT_VEC_SCROLL_UP | - | Scroll viewport up one line. Updates VT_SCROLL_ROW and L0_VSCROLL hardware. Clears new bottom row. Does not move cursor. |
| $02004160 | VT_VEC_SCROLL_DOWN | - | Scroll viewport down one line. Clears new top row. Does not move cursor. |
| $02004168 | VT_VEC_CLR_EOL | - | Clear from cursor position to end of current line (column 79). Writes spaces + current attribute. Does not move cursor. |
| $02004170 | VT_VEC_CLR_EOS | - | Clear from cursor to end of screen. CLR_EOL on current row, then CLR_ROW for all rows below to bottom of viewport. Does not move cursor. |
| $02004178 | VT_VEC_INSERT_LINE | - | *Stub (RTS).* Insert blank line at cursor row, shift lines below down. |
| $02004180 | VT_VEC_DELETE_LINE | - | *Stub (RTS).* Delete cursor row, shift lines below up. |
| $02004188 | VT_VEC_CURSOR_TOGGLE | - | Toggle cursor visibility (used by VSYNC ISR via thunk). |

### Register Clobber Rules

| Routine | Clobbers | Notes |
|---------|----------|-------|
| PUTCHAR | D0 | D1, D2, A0 preserved by PUTCHAR wrapper in agfa-mon.m68 |
| PRINTSTR | D0 | A0 advanced past NUL |
| GOTOXY | D0, D2, A0 | Via CURSOR_FORCE_OFF |
| SET_ATTR | nothing | |
| CLR_EOL | D0-D2 | IPL-1 blocked internally |
| CLR_EOS | D0-D2 | Calls CLR_EOL + CLR_ROW |
| SCROLL_UP/DOWN | D0-D1 | IPL-1 blocked internally |
| CURSOR_ON/OFF | D0, D2, A0 | |

### Tilemap Cell Format

Each cell in the VERA tilemap is 2 bytes:
- **Byte 0:** Character index (0-255, Code Page 437)
- **Byte 1:** Attribute - upper nibble = background color (palette 0-15), lower nibble = foreground color (palette 0-15)

VRAM address formula: `(row * 256) + (col * 2)`

The tilemap is 128 columns x 64 rows (16 KB). Only columns 0-79 are visible at 1x scale. The 64-row circular buffer enables zero-blit hardware scrolling via the L0_VSCROLL register.

---

## Command Reference

All numeric arguments are hex (no prefix). Commands are case-insensitive (folded to uppercase in dispatcher). Blank lines loop silently. Unknown command prints `?` + CRLF.

---

### B - Launch EhBASIC

Checks for JMP opcode ($4EF9) at BASIC_RAM+$0100 to detect whether EhBASIC has been cold-started before.
- Not yet cold-started -> jump directly to BASIC_START (cold start at ORG $001800)
- Already initialised -> print `Cold/Warm? `, read one character, echo CRLF:
  - `W` or `w` -> set A3 = $02008000, JMP to BASIC_RAM+$0100 (warm start, program preserved)
  - Anything else -> cold start

Exit BASIC with: `CALL 1054` (decimal) - this is $041E = CMD_LOOP_VEC.

---

### D [addr] [len] - Hex/ASCII Dump

If len omitted or zero: default 128 bytes ($80). Output format, 16 bytes per row:
```
02050000: 4E F9 00 00 04 06 4E F9 00 00 04 0C 4E F9 00 00  N.....N.....N...
```
Address, colon, space, then 16 hex bytes with spaces, then 3-space padding for partial last row, then ASCII (dots for non-printable). Pressing any key during dump aborts immediately (polls RR0 bit 0 at start of each row).

---

### E \<addr\> - Examine/Edit Memory

Shows one byte per line: `XXXXXXXX: YY ` then waits for input.
- Empty line (just Enter) -> keep current byte, advance to addr+1
- Hex digits -> write new byte, advance to addr+1
- `.` or `q` or `Q` -> return to CMD_LOOP

---

### F \<addr\> \<len\> \<byte\> - Fill Memory

Writes the single byte value to all addresses in [addr, addr+len). No output.

---

### G [addr] - Execute

Restores D0-D7 from SAVE_D0 ($02004050) and A1-A6 from SAVE_A0+4 ($02004074). **A0 is NOT restored from the save area** - it holds the call target address. Calls target with JSR.

On return (if user code does RTS):
- Saves D0-D7 to SAVE_D0, A0-A6 to SAVE_A0
- Resets SP to $02400000
- Prints `\r\nRETURNED\r\n`
- Displays full register dump (same as R command)

If no addr given: uses SAVE_PC from the last L command load. If addr given: stores it in SAVE_PC first.

---

### H or ? - Help

Prints STR_HELP (the full command list) and returns to CMD_LOOP.

---

### L - Load S-Records

Prints `Send S-records (S7/S8/S9 to end)...\r\n` then reads from serial:
- Syncs on 'S' character (discards everything before it)
- Handles S1 (2-byte address), S2 (3-byte address), S3 (4-byte address)
- S7 / S8 / S9 -> terminates
- S0 and unknown types -> drain to end of line, skip
- Per record: prints `.` if checksum OK, `!` if checksum fails (but still loads)
- First data record's address is saved to SAVE_PC (enables bare `G`)
- On terminator: drains rest of line, then prints `OK XXXXXXXX\r\n`

---

### M \<src\> \<dst\> \<len\> - Move (Copy) Memory

Copies len bytes from src to dst using a simple byte-by-byte loop with post-increment on both pointers. **Note:** overlapping regions where dst > src will corrupt data (no direction-awareness). No output.

---

### S - Scan SCSI Bus

See [Exact Output of S (SCSI Scan) Command](#exact-output-of-s-scsi-scan-command) above. Scans IDs 0-6 only. Interrupts are masked during the scan (temporary workaround for ISR/SCSI interaction).

---

### P \<addr\> \<len\> - Print S-Records

Emits memory as Motorola S3 records (32 data bytes per record). Record format: `S3` + byte_count_hex + 4-byte address + data bytes + checksum. Terminated with the fixed string `S70500000000FA\r\n` (S7 record pointing to address $00000000). No S0 header. Output can be captured and reloaded with L.

---

### R - Show CPU Registers

Prints four lines from the save area and live control registers:
```
D: 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
A: 00000000 00000000 00000000 00000000 00000000 00000000 00000000
PC SR: 00000000 0000
VBR CACR ISP MSP: 00000000 00000001 02400000 02400000
```
D0-D7 from SAVE_D0; A0-A6 from SAVE_A0 (7 values, no A7/SP); PC from SAVE_PC, SR from SAVE_SR; VBR/CACR/ISP/MSP read live from CPU control registers with MOVEC.

---

### C - Boot CP/M

Restores D0-D7 from SAVE_D0, A1-A6 from SAVE_A0+4 (same as G - A0 is NOT restored, it holds the target). JSR to $00020000 (Bank 1 entry point). CP/M-68K is installed in Bank 1. If the OS returns: saves registers, resets SP, prints `\r\nRETURNED\r\n`, shows register dump.

---

### U - Boot UNIX

JMP to $40000 (Bank 2 entry point). UNIX image is expected in Bank 2. Does not return to monitor; if the OS returns the result is undefined.

---

### V - NVRAM Config

Enters the NVRAM configuration menu. Allows interactive viewing and modification of persistent settings stored in the Xicor X2804A EEPROM at $07100000. See the NVRAM Configuration section in [AGFA-MON Memory Map](agfa_mon_memory_map.md) for field definitions.

---

### T - RAM Burn-In Test

Prints `\r\nRAM burn-in $02008000-$023EFFFF. Any key to stop.\r\n` then loops indefinitely.

Each pass cycles through 4+1 patterns in this order: $FFFFFFFF (label: `FF`), $AAAAAAAA (`AA`), $55555555 (`55`), $00000000 (`00`), address-as-data (`AD`). End-of-table sentinel: $7FFFFFFF.

Per pattern: write phase (entire range, no keycheck), then verify phase (one dot per 512KB, keycheck after each dot). Error cap: 4 per pattern. Pass output:
```
Pass 1: FF.............. AA.............. 55.............. 00.............. AD.............. ok
Pass 2: FF.............. ...
```
`ok` if no new errors this pass; `err` if any new errors.

On keypress: `\r\nStopped after N passes, N total errors.\r\n`, returns to CMD_LOOP.

---

### X - Save BASIC Program as S-Records

Checks $4EF9 at BASIC_RAM+$0100; if absent prints `BASIC not initialised\r\n`. If present:
- Follows next-line pointer chain from BASIC_RAM+Smeml ($02008100+$012E) to find end of program
- Emits S3 records from BASIC_RAM+LAB_WARM ($02008100) through end-of-program marker + 4 bytes
- Output can be captured, then reloaded with L + warm-started with `W` to restore the program

---

## Monitor Workspace (RAM)

Defined in agfa-mon.m68 EQU constants:

| Address | Size | Name | Content |
|---------|------|------|---------|
| $02004000 | 80 B | LINE_BUF | Input line buffer (GETLINE target, also NS_DATA_BUF for SCSI scan) |
| $02004050 | 32 B | SAVE_D0 | D0-D7 (8 x longword) |
| $02004070 | 28 B | SAVE_A0 | A0-A6 (7 x longword) |
| $02004090 | 4 B | SAVE_PC | Saved PC / G command target |
| $02004094 | 2 B | SAVE_SR | Saved SR |
| $02008000 | 256 KB | BASIC_RAM | EhBASIC 68K RAM pool (program, variables, heap) |
| $02400000 | -- | STACK_TOP | Supervisor stack top (SP initialised here; grows down) |

**Safe user program load range:** $02040000 through $023EFFFF (below the 64 KB guard zone). Recommend $02050000 for most programs (clear of EhBASIC RAM at $02008000-$0204FFFF).

---

## API Jump Table (Fixed Addresses)

At ORG $000400 - fixed JMP entries compatible with the I/O board EhBASIC stub. Reserved range $042A-$045F holds space for future entries.

| ROM Address | EQU Name | Target | Convention |
|-------------|----------|--------|------------|
| $0400 | START_VEC | START | Cold restart (full re-init, re-tests RAM) |
| $0406 | PUT_VEC | PUTCHAR | In: D0.b = char to send. Out: nothing. Blocks until TX ready. |
| $040C | GET_VEC | GETCHAR | In: nothing. Out: D0.b = received char. Blocks. |
| $0412 | PRT_VEC | PRINTSTR | In: A0 = NUL-terminated string. Advances A0. Preserves D0. |
| $0418 | GL_VEC | GETLINE | In: A0 = 80-byte buffer. Out: buffer filled, A0 = buffer base. |
| $041E | EXIT_VEC | CMD_LOOP | Return to monitor prompt. No arguments. |
| $0424 | TICKS_VEC | GETTICKS | In: nothing. Out: D0.l = current TICK_COUNT. Preserves all other registers. |
| $042A-$045F | *(reserved)* | - | Reserved for future jump table entries. |
| $0460 | - | START | Cold boot entry point (reset vector points here). |

### Calling Convention

To call from user code:
```asm
PUTCHAR_VEC  EQU  $0406
EXIT_VEC     EQU  $041E
TICKS_VEC    EQU  $0424

    MOVE.B  #'A',D0
    JSR     PUTCHAR_VEC     ; direct JSR to fixed address

    JSR     TICKS_VEC       ; D0.l = current tick count
    JMP     EXIT_VEC        ; return to monitor (no RTS back to caller)
```

**Important:** `$041E` is CMD_LOOP, not a subroutine - it does not return to the caller. Use JMP, not JSR, when exiting.

---

## SCSI Scanner (S Command)

AM5380 registers used (stride-1 from $05000000):

| Register | Address | Used for |
|----------|---------|---------|
| SCSI_ODR | $05000000 | Write: output data / Select target |
| SCSI_ICR | $05000001 | Write: DBUS/SEL/ACK/RST assertion |
| SCSI_MR  | $05000002 | Write: mode (cleared before select) |
| SCSI_TCR | $05000003 | Write: TCR phase match (interlock) |
| SCSI_STATUS | $05000004 | Read: bus status (BSY bit 6, REQ bit 5) |
| SCSI_RESET_REG | $05000007 | Read: clears interrupt/parity after bus reset |

### NS_RESET_BUS

ICR=$00 (release bus), MR=$00, ICR=$80 (assert RST), wait 200,000 iterations, ICR=$00, read SCSI_RESET_REG.

### NS_SELECT_TARGET (for current D7)

- TCR=$00, MR=$00
- ODR = $80 | (1 << D7) - self (ID7) + target bit
- ICR=$01 (DBUS), ICR=$05 (DBUS+SEL)
- Poll STATUS bit 6 (BSY) up to 1,000,000 iterations
- Timeout -> print ` empty`, ICR=$00, return D0=-1
- BSY seen -> ICR=$00, wait 10,000 iterations, return D0=0

### NS_RUN_SM (State Machine)

Handles Command / Data In / Status / Message In phases. For each REQ:
- Read STATUS, mask bits [4:2], LSR by 2 -> write to TCR (phase interlock)
- Command phase ($08): write CDB byte from A5 to ODR, assert DBUS ($01), then ACK+DBUS ($11); after REQ drops, hold DBUS ($01) - **must not drop DBUS between command bytes**
- Data In phase ($04): read ODR, store to (A4)+, assert ACK ($10)
- Status phase ($0C): read ODR into D5 (SCSI status byte), assert ACK ($10)
- Message In phase ($1C): read and discard ODR, assert ACK ($10)
- Timeout waiting for REQ: 2,000,000 iterations; timeout waiting for REQ-drop: 2,000,000 iterations
- Safety cutoff: aborts after 1,000 total byte exchanges regardless

### CDB Tables (in ROM)

- INQUIRY: `$12 $00 $00 $00 $24 $00` (36-byte response into LINE_BUF)
- READ CAPACITY: `$25 $00 $00 $00 $00 $00 $00 $00 $00 $00` (8-byte response)
- MODE SENSE page 4: `$1A $00 $04 $00 $24 $00` (36-byte response)

### Capacity Calculation

LINE_BUF[0..3] = max LBA (big-endian longword); total_blocks = max_LBA + 1; capacity_MB = total_blocks / 2 / 1024 (assumes 512-byte blocks, integer arithmetic - truncated).

### Geometry

Checks LINE_BUF[4] == $04 (confirms page 4 response); cylinders from LINE_BUF[6..8] (24-bit big-endian); heads from LINE_BUF[9].

---

## Programming for AGFA-MON

This section describes how to write, assemble, load, and execute programs on AGFA-MON.

### Development Workflow

**Goal:** Write a 68020 assembly program, assemble to S-records, load via serial terminal, and execute.

**Steps:**
1. Write assembly source in Motorola syntax (`.m68` file)
2. Assemble with **vasm** to S-record format (`.s19` file)
3. Load via terminal using **L** command (pastes S-records)
4. Execute with **G** command (jumps to program)

### Step-by-Step Guide

#### Step 1: Write Assembly Source

Create a `.m68` file with your program. Use **Motorola vasm syntax** (not AT&T).

**Example: Simple program that prints "Hello" and exits**
```asm
;=====================================================================
; hello.m68 - Print "Hello World" and return to monitor
; Load at: $02050000
; Run with: G 2050000
;=====================================================================

    ORG $02050000

PUTCHAR_VEC     EQU $0406   ; Monitor jump table entry
EXIT_VEC        EQU $041E   ; Return to monitor

START:
    LEA     MSG_HELLO,A0
    BSR     PRINTSTR
    JMP     EXIT_VEC

PRINTSTR:
    ; Print NUL-terminated string at A0
    MOVEM.L D0/A0,-(SP)
.loop:
    MOVE.B  (A0)+,D0
    BEQ.S   .done
    MOVEA.L #PUTCHAR_VEC,A1
    JSR     (A1)
    BRA.S   .loop
.done:
    MOVEM.L (SP)+,D0/A0
    RTS

MSG_HELLO:
    DC.B    "Hello World",13,10,0

    END START
```

**Key points:**
- Use **ORG** to set load address (e.g., `$02050000` - well above monitor workspace)
- Call monitor routines via jump table addresses: `PUTCHAR_VEC=$0406`, `EXIT_VEC=$041E`
- End program with `JMP $041E` to return to monitor (do NOT use RTS)
- Terminate strings with NUL byte (0x00)
- Always wrap D0/A0/A1 if used; monitor may restore them

#### Step 2: Assemble with vasm

**Install vasm:**
Download from http://sun.hasenbraten.de/vasm/. Copy `vasmm68k_mot.exe` to your working directory.

**Assemble to S-records:**
```bash
vasmm68k_mot -m68020 -Fsrec -o hello.s19 hello.m68
```

**Command flags:**
- `-m68020`: 68020 instruction set
- `-Fsrec`: Output Motorola S-record format (required for terminal load)
- `-o hello.s19`: Output filename

**Output:** `hello.s19` with S3 records (32-bit addresses) and S7 terminator.

**Example output:**
```
S00F000068656C6C6F2E6D36382E73313999
S32500020500003340007A47000203C8000202...
S32500020502003340007A4700020400000202...
S7050002050000F6
```

#### Step 3: Load via Terminal

**On your terminal (minicom, Putty, etc.):**

1. Connect to board at 9600 8N1
2. Power-on board or press Return to get AGFA-MON prompt
3. Type **L** and press Enter
4. Paste entire `hello.s19` file contents (all S-records at once)
5. Monitor parses and loads into RAM
6. Monitor prints `OK 02050000` (program start address)
7. Prompt returns

**Example session:**
```
AGFA-MON> L
[paste hello.s19 contents here]
OK 02050000
AGFA-MON>
```

#### Step 4: Execute with G Command

Type **G** followed by program address:

```
AGFA-MON> G 2050000
Hello World
AGFA-MON>
```

Program runs, calls PUTCHAR via jump table, prints "Hello World", and returns to monitor via `JMP $041E`.

### Memory Layout for Programs

Use these address ranges for loading programs:

| Range | Size | Purpose |
|-------|------|---------|
| 0x02000000 - 0x02003FFF | 16 KB | Monitor workspace (LINE_BUF, register saves) - **do not use** |
| 0x02004000 - 0x02007FFF | 16 KB | System variables - **do not use** |
| 0x02008000 - 0x0203FFFF | 256 KB | EhBASIC RAM - **do not overwrite** |
| **0x02040000 - 0x023EFFFF** | **3.8 MB** | **User program space - use this** |
| 0x023F0000 - 0x023FFFFF | 64 KB | Guard zone (prevents stack overflow) - **do not use** |

**Safe load addresses:**
- Small programs (< 1 KB): `$02050000`
- Larger programs: `$02100000` or higher
- EhBASIC programs: stay below `$02008000` (BASIC owns this range)

### Monitor Jump Table API

Call these fixed addresses to use monitor routines:

```asm
PUTCHAR_VEC     EQU $0406   ; Output D0.b to serial
GETCHAR_VEC     EQU $040C   ; Input byte into D0.b (blocking)
PRINTSTR_VEC    EQU $0412   ; Print NUL-terminated string at A0
GETLINE_VEC     EQU $0418   ; Read line into buffer at A0
CMD_LOOP_VEC    EQU $041E   ; Return to monitor (safe exit)
TICKS_VEC       EQU $0424   ; Get tick count into D0.l
```

**Example: Using PRINTSTR**
```asm
LEA     STR_MSG,A0
MOVEA.L #$0412,A1
JSR     (A1)
; returns to next instruction, A0 preserved
```

**Example: Using PUTCHAR in a loop**
```asm
MOVE.B  #'X',D0
MOVEA.L #$0406,A1
JSR     (A1)
; character sent to serial port
```

### Creating S-Records from Existing Programs

Use the **P** command to save a loaded program back as S-records:

**Example:**
```
AGFA-MON> L
[load program at $02050000]
OK 02050000
AGFA-MON> P 2050000 400
S32500020500003340007A47...
S7050002050000F6
[copy output to terminal, save as .s19 file]
```

**P command format:** `P <start_addr> <length_in_bytes>`
- Outputs S3 records (32-bit addresses)
- Includes checksum
- Terminates with S7 (end record)
- Output can be captured and reloaded with L command

### Example Programs

**Program 1: Print "TEST" 10 times**
```asm
    ORG $02050000
PUTCHAR_VEC     EQU $0406
PRINTSTR_VEC    EQU $0412
CMD_LOOP_VEC    EQU $041E

START:
    MOVE.L  #10,D0
.loop:
    LEA     STR_TEST,A0
    MOVEA.L #PRINTSTR_VEC,A1
    JSR     (A1)
    SUBQ.L  #1,D0
    BNE.S   .loop
    JMP     CMD_LOOP_VEC

STR_TEST:
    DC.B    "TEST",13,10,0
    END START
```

**Program 2: Read and echo characters**
```asm
    ORG $02050000
GETCHAR_VEC     EQU $040C
PUTCHAR_VEC     EQU $0406
CMD_LOOP_VEC    EQU $041E

START:
    MOVE.L  #20,D1          ; Read 20 characters
.loop:
    MOVEA.L #GETCHAR_VEC,A0
    JSR     (A0)             ; Input into D0
    MOVEA.L #PUTCHAR_VEC,A0
    JSR     (A0)             ; Echo to output
    SUBQ.L  #1,D1
    BNE.S   .loop
    JMP     CMD_LOOP_VEC
    END START
```

### Common Issues and Solutions

**"Illegal instruction" error after G:**
- Program jumped to wrong address (L command didn't print OK address?)
- Program overlaps monitor workspace or EhBASIC RAM
- **Fix:** Use D command to verify program was loaded correctly at expected address

**Program runs but produces garbage output:**
- Program not calling monitor PUTCHAR correctly
- Verify you're using jump table address `$0406`, not hardcoding PUTCHAR routine
- Monitor may have relocated serial I/O code between versions
- **Fix:** Always use jump table API, never call monitor code directly

**Program hangs after G:**
- Program infinite loop or waiting on non-existent device
- Press Ctrl+C to abort (if implemented)
- Power-cycle board to return to monitor
- **Fix:** Use D command to inspect program at load address; verify loop conditions

**S-record won't load (L command timeout):**
- Line too long (> 80 characters per S-record)
- Corrupt S-record checksum
- Paste too fast for serial buffer (slow paste or add delays between records)
- **Fix:** Regenerate S-records with vasm; verify file with text editor

### vasm Syntax Reference for AGFA-MON Programs

**Labels and ORG:**
```asm
    ORG $02050000           ; Set load address
START:                      ; Label
    NOP
    BRA START
```

**Addressing modes:**
```asm
    MOVE.B  D0,D1           ; Register to register
    MOVE.L  #$12345678,D0   ; Immediate (hex, no prefix needed)
    MOVE.L  (A0),D0         ; Indirect
    LEA     LABEL,A0        ; Load effective address
    MOVEA.L #$0406,A1       ; Load long address
    JSR     (A1)            ; Jump to subroutine
    JMP     (A0)            ; Jump (no return)
```

**No spaces after commas - this is critical:**
```asm
    MOVE.L D0,D1            ; correct
    MOVE.L D0, D1           ; WRONG (vasm treats space as separator)
```

**Character literals with hex:**
```asm
    MOVE.B  #'A',D0         ; Character A
    MOVE.B  #$0D,D0         ; Carriage return (0x0D)
    MOVE.B  #';',D0         ; Semicolon - NO! `;` starts comment
    MOVE.B  #$3B,D0         ; Semicolon (use hex)
```

**Data declarations:**
```asm
    DC.B    'H','e','l','l','o',0     ; Bytes with NUL terminator
    DC.L    $12345678                  ; Longword
    DS.B    100                        ; Reserve 100 bytes (WARNING: creates output!)
```

**Strings:**
```asm
MSG:    DC.B    "Hello",13,10,0        ; String with CRLF + NUL
        DC.B    "World",0
```

---

## Exception Handlers

All vectors ($008-$3FC) share a single unified handler. The handler prints diagnostic information to **serial only** (VERA output is not used, since VERA state may be corrupt at fault time), then waits for a keypress, flushes the SCC TX shift register, and performs a **full cold restart**.

### Unified Handler Sequence

1. `MOVEA.L #$02400000,SP` - reset stack pointer
2. `BSR INIT_HW` - reinitialise SCC
3. Print: `** EXCEPTION **` header
4. Print PC value from exception frame
5. Print vector offset (word from exception frame) with symbolic name lookup (e.g. `BUS ERROR`, `ADDRESS ERROR`, `ILLEGAL INSTR`, etc.)
6. Print SR value from exception frame
7. Print format word from 68020 exception frame
8. Print `Press any key to reboot...`
9. `BSR GETCHAR` - wait for keypress (serial only)
10. `BSR FLUSH_TX` - wait for SCC TX shift register to fully emit
11. `BRA START` - full cold boot restart (hardware re-init + RAM test + help screen)

**Output goes to serial only.** VERA display is not updated during exception handling.

**Implication:** Any exception in user code will print PC, vector, SR, and frame format, pause for acknowledgment, then fully cold-restart the monitor (re-runs RAM test). The register save area ($02004050-$02004094) reflects state at the last CMD_GO/CMD_LOAD, not at the time of the fault.

---

## Building AGFA-MON from Source

```bash
vasmm68k_mot -m68020 -Fbin -Ldebug -o agfa-rom.bin agfa-mon.m68 > agfa-rom.lst
```

The `-Ldebug` flag (or equivalent listing flag) generates `agfa-rom.lst` alongside the binary. The version string is embedded at assembly time via a `DC.B` constant in the source.

Source files required:
- `agfa-mon.m68` - main file; includes agfa-basic.m68 at ORG $001800 via `INCLUDE`
- `agfa-basic.m68` - EhBASIC hardware wrapper + RAM EQU layout; includes ehbasic68k.inc
- `agfa-basic.m68` includes `ehbasic68k.inc` - extracted from I/O board basic ROM by `build.ps1` (lines 193+ of `../ioboard/basic/basic68k-rom.m68`). **Must exist before assembling.**
- `build.ps1` - generates ehbasic68k.inc, runs vasmm68k_mot, generates listing, and splits binary into 4x 32KB EPROM images via `split_rom.py`

**Output:** `agfa-rom.bin` (binary, starts at $000000) and `agfa-rom.lst` (listing file). Binary is split into HH/HM/LM/LL 32KB images for burning to AM27C256 EPROMs.

**Current installation:** AGFA-MON is physically burned into the 4x AM27C256 chips in Bank 0 sockets. CP/M-68K is in Bank 1.

---

## Cross-References

- **[Original Firmware](original_firmware.md)** - Atlas Monitor (the factory ROM firmware that AGFA-MON replaces)
- **[AGFA-MON Memory Map](agfa_mon_memory_map.md)** - detailed RAM layout including VERA CCB, EhBASIC internals, and EEPROM
- **[System Overview](../hardware/system_overview.md)** - CPU board architecture, bus structure, and peripheral addresses
- **[Interrupt Map](../hardware/interrupt_map.md)** - 68020 interrupt levels and vector assignments
- **[SCSI Controller](../scsi_controller.md)** - AM5380 register reference and bus protocol details
- **[Hardware Reference](../hardware_reference.md)** - monolithic source document (this file was extracted from its AGFA-MON section)
