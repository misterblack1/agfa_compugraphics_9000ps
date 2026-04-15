# AGFA-MON RAM Memory Map

**RAM range:** `$02000000`-`$023FFFFF` (4 MB)
**EEPROM range:** `$07100000`-`$071001FF` (512 bytes)

---

## Overview

```
$02000000 ┌─────────────────────────────────────────┐
          │  (unused)                               │
$02000300 ├─────────────────────────────────────────┤
          │  VERA CCB - console control block       │  16 bytes
$02000310 ├─────────────────────────────────────────┤
          │  (unused)                               │
$02001400 ├─────────────────────────────────────────┤
          │  TICK_COUNT - system tick counter       │   4 bytes
$02001404 ├─────────────────────────────────────────┤
          │  (unused)                               │
$02004000 ├─────────────────────────────────────────┤
          │  LINE_BUF  - command input buffer       │  80 bytes
$02004050 ├─────────────────────────────────────────┤
          │  SAVE_D0   - D0-D7 register save area   │  32 bytes
$02004070 ├─────────────────────────────────────────┤
          │  SAVE_A0   - A0-A6 register save area   │  28 bytes
$0200408C ├─────────────────────────────────────────┤
          │  (implicit A7/SP slot - unused)         │   4 bytes
$02004090 ├─────────────────────────────────────────┤
          │  SAVE_PC   - user program counter       │   4 bytes
$02004094 ├─────────────────────────────────────────┤
          │  SAVE_SR   - user status register       │   2 bytes
$02004096 ├─────────────────────────────────────────┤
          │  (unused gap)                           │  ~16 KB
$02008000 ├─────────────────────────────────────────┤
          │  BASIC_RAM - EhBASIC interpreter area   │ 256 KB
$02050000 ├─────────────────────────────────────────┤
          │  User program / free RAM                │  ~3.8 MB
$023F0000 ├─────────────────────────────────────────┤
          │  Stack guard zone (reserved)            │  64 KB
$02400000 └─────────────────────────────────────────┘
             STACK_TOP  (supervisor stack grows ↓)
```

---

## Detailed Variable Table

### VERA Console Control Block (CCB) - `$02000300`-`$0200030F`

Defined in `docs/vera/vera_console_api.md`. A fixed 16-byte scratchpad holding persistent screen state for the VERA text console driver. Detected as initialized by `V_MAGIC`; `VERA_INIT` re-initializes the full block if the magic word is absent.

| Label | Address | Size | Type | Purpose |
|-------|---------|------|------|---------|
| `V_CUR_COL` | `$02000300` | 2 bytes | Word | Current cursor column (0-79). |
| `V_CUR_ROW` | `$02000302` | 2 bytes | Word | Current physical write row in the tilemap (0-63, circular). |
| `V_SCROLL_ROW` | `$02000304` | 2 bytes | Word | Physical tilemap row at the top of the visible display (0-63). |
| `V_MAGIC` | `$02000306` | 2 bytes | Word | Initialization signature. Set to `$A55A` by `VERA_INIT`. Any other value means the CCB is uninitialized. |
| `V_TEXT_ATTR` | `$02000308` | 1 byte | Byte | Current character color/attribute. Default: `$11` (white-on-blue). |
| `V_VIEW_H` | `$02000309` | 1 byte | Byte | Visible viewport height in rows. Default: `30`. |
| `V_MAP_H` | `$0200030A` | 1 byte | Byte | Total hardware tilemap height in rows. Default: `64`. |
| `V_FLAGS` | `$0200030B` | 1 byte | Byte | Bitmask: [7] Scroll Enable, [6] Wrap Enable, [0] Cursor Blink. |
| *(reserved)* | `$0200030C` | 4 bytes | - | Unallocated. Completes the 16-byte scratchpad. |

### Tick Counter - `$02001400`

| Label | Address | Size | Type | Purpose |
|-------|---------|------|------|---------|
| `TICK_COUNT` | `$02001400` | 4 bytes | Longword | System tick counter. Incremented by the active timer ISR - either the VERA VSYNC ISR at IPL-1 (~60 Hz) when VERA is installed, or VIA #1 T1 at IPL-4 (~60 Hz) when VERA is absent. Read via the GETTICKS API vector at $0424. |

### Monitor Variables - `$02004000`-`$02004095`

| Label | Address | Size | Type | Purpose |
|-------|---------|------|------|---------|
| `LINE_BUF` | `$02004000` | 80 bytes | Buffer | Command line input buffer. Filled by `GETLINE` on every monitor prompt cycle. NUL-terminated. Also aliased as `NS_DATA_BUF` for SCSI data reads (INQUIRY, READ CAPACITY, MODE SENSE responses) during the `S` (SCSI scan) command. |
| `SAVE_D0` | `$02004050` | 4 bytes | Register save | Saved value of D0. |
| `SAVE_D1` | `$02004054` | 4 bytes | Register save | Saved value of D1. |
| `SAVE_D2` | `$02004058` | 4 bytes | Register save | Saved value of D2. |
| `SAVE_D3` | `$0200405C` | 4 bytes | Register save | Saved value of D3. |
| `SAVE_D4` | `$02004060` | 4 bytes | Register save | Saved value of D4. |
| `SAVE_D5` | `$02004064` | 4 bytes | Register save | Saved value of D5. |
| `SAVE_D6` | `$02004068` | 4 bytes | Register save | Saved value of D6. |
| `SAVE_D7` | `$0200406C` | 4 bytes | Register save | Saved value of D7. |
| `SAVE_A0` | `$02004070` | 4 bytes | Register save | Saved value of A0. |
| `SAVE_A1` | `$02004074` | 4 bytes | Register save | Saved value of A1. |
| `SAVE_A2` | `$02004078` | 4 bytes | Register save | Saved value of A2. |
| `SAVE_A3` | `$0200407C` | 4 bytes | Register save | Saved value of A3. |
| `SAVE_A4` | `$02004080` | 4 bytes | Register save | Saved value of A4. |
| `SAVE_A5` | `$02004084` | 4 bytes | Register save | Saved value of A5. |
| `SAVE_A6` | `$02004088` | 4 bytes | Register save | Saved value of A6. |
| *(unnamed)* | `$0200408C` | 4 bytes | Padding | Implicit A7/SP slot. A7 is never saved here (SP is always reset to `STACK_TOP` on return). Exists to naturally align `SAVE_PC` to `$02004090`. |
| `SAVE_PC` | `$02004090` | 4 bytes | Register save | Saved user program counter. Set by `CMD_GO` when the user specifies an address. Also set automatically by `CMD_LOAD` (L command) to the address of the first S-record received - enabling a bare `G` command to jump to a freshly loaded program. |
| `SAVE_SR` | `$02004094` | 2 bytes | Register save | Saved user status register (condition codes + supervisor bits). |

---

## Memory Region Notes

### Below Monitor Workspace (`$02000000`-`$02003FFF`, 16 KB)

Mostly unused. The 68020 exception vector table lives in ROM at `$000000` (VBR = 0 at reset), so the bottom of RAM is not reserved for vectors.

The VERA Console Control Block occupies `$02000300`-`$0200030F` (16 bytes). `TICK_COUNT` occupies `$02001400`-`$02001403` (4 bytes) and is maintained by whichever timer ISR is active (VERA VSYNC at IPL-1 or VIA #1 T1 at IPL-4). The remainder of this region has no defined labels and is not referenced by any monitor code.

**Not covered by the boot RAM test** - `RAM_TEST_START` is `$02008000`, so this entire 16 KB zone (including the CCB) is never written to by `RAM_TEST_BOOT` or `CMD_TEST`. RAM content here after power-on is undefined until explicitly initialized. The CCB handles this via the `V_MAGIC` detection mechanism in `VERA_INIT`.

### VERA Console Control Block (`$02000300`-`$0200030F`, 16 bytes)

Defined by `docs/vera/vera_console_api.md`. Holds all persistent state for the VERA text console driver. Not initialized by the monitor cold start - `VERA_INIT` is responsible for detecting and initializing this block via `V_MAGIC`.

### Monitor Workspace (`$02004000`-`$02004095`, 150 bytes)

The entire monitor working area. Zeroed on cold start (`START`, lines 143-147). Contains the command buffer and the complete CPU register save state.

**Alias:** `NS_DATA_BUF` is defined as `EQU LINE_BUF`. The SCSI scanner (`CMD_SCSI`, `S` command) reuses the same 80-byte buffer to hold INQUIRY/READ CAPACITY/MODE SENSE response data, since user input is not needed during a SCSI probe.

### Unused Gap (`$02004096`-`$02007FFF`, ~16 KB)

No monitor variable or label is defined in this range. It is not tested at boot, not used by any monitor command.

### EhBASIC Interpreter RAM (`$02008000`-`$0204FFFF`, 256 KB)

Defined by `BASIC_RAM EQU $02008000` and `BASIC_SIZE EQU $00040000`. EhBASIC initialises its internal variable layout here on cold start (entered via `B` command). The monitor uses this boundary as `RAM_TEST_START` - the boot RAM test begins here to avoid disturbing the monitor workspace. EhBASIC internals are out of scope for this document.

### User Program / Free RAM (`$02050000`-`$023EFFFF`, ~14 MB)

General-purpose free RAM. User programs are loaded here via the `L` command (S-records are written to whatever address the records specify). There is no enforced lower bound - a user could load to `$02008000` if EhBASIC is not in use. The monitor imposes no constraints; the address is caller-determined by the S-record content.

### Stack Guard Zone (`$023F0000`-`$023FFFFF`, 64 KB)

Defined implicitly by `RAM_TEST_END EQU $023F0000`. The boot RAM test (`RAM_TEST_BOOT`) and the burn-in test (`CMD_TEST`) stop at this address, leaving the top 64 KB of RAM untouched to protect the stack. No label or variable is defined in this range - it is reserved as headroom for stack growth.

### Supervisor Stack (`STACK_TOP = $02400000`, grows downward)

The supervisor stack pointer is initialised to `$02400000` at cold start and reset to this value:
- On every return from `CMD_GO` (user program call)
- On every exception handler entry (Bus Error, Address Error, Illegal Instruction, Generic Handler)

`$02400000` is one byte past the top of the 4 MB RAM region (`$023FFFFF`). The first push to the stack writes to `$023FFFFC`. The stack grows downward into the 64 KB guard zone and potentially beyond, depending on call depth.

---

## Constants (Not RAM Addresses)

| Label | Value | Purpose |
|-------|-------|---------|
| `LINE_BUF_SZ` | `80` | Size of `LINE_BUF` in bytes |
| `BASIC_SIZE` | `$00040000` (256 KB) | Size of EhBASIC RAM area |
| `RAM_TEST_START` | `$02008000` | Start of boot/burn-in RAM test range |
| `RAM_TEST_END` | `$023F0000` | End (exclusive) of boot/burn-in RAM test range |
| `RAM_MAX_ERRS` | `8` | Max errors reported per pattern before suppression |

---

## Initialization at Cold Start

On every cold start (`START`, `$000460`):

1. **SP reset** - `MOVEA.L #STACK_TOP, SP` sets supervisor stack to `$02400000`.
2. **Monitor workspace zeroed** - `CLR.L` loop clears 72 bytes from `SAVE_D0` (`$02004050`) through `SAVE_SR+4` (`$02004097`), zeroing all register save slots.
   `LINE_BUF` (`$02004000`-`$0200404F`) is **not** zeroed at init - it is overwritten on the first `GETLINE` call.
3. **Boot RAM test** - `RAM_TEST_BOOT` writes `$AAAAAAAA` then `$55555555` across `$02008000`-`$023EFFFC`. Any failures are printed but do not halt the monitor.

---

## AGFA-MON NVRAM Configuration (Xicor X2804A EEPROM)

**Location:** `$07100000`-`$071001FF` (512 bytes, mirrored at `$071F0000`)
**Chip:** Xicor X2804AP-45 (parallel EEPROM, 45 ns access, ~10 ms write cycle)
**Interface:** Direct memory-mapped byte read/write at `$07100000`

### Layout

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| `$00` | 1 byte | `NV_MAGIC` | Magic byte. Value: `$A5`. If not `$A5`, NVRAM is uninitialized and defaults are used. |
| `$01` | 1 byte | `NV_CHECKSUM` | Simple checksum (sum of bytes `$02`-`$27`, truncated to 8 bits). If mismatch, defaults are used. |
| `$02` | 1 byte | `NV_BAUD_IDX` | Serial port CHA baudrate index. 0=9600, 1=19200, 2=38400, 3=57600, 4=115200. Data bits always 8, stop bits 1, parity none (8N1). |
| `$03` | 1 byte | `NV_DC_VIDEO` | VERA DC_VIDEO register. Bits [1:0] = OUT_MODE (0=disabled, 1=VGA, 2=NTSC, 3=RGB 15KHz). Bits [7:2] = layer/sprite enable flags. |
| `$04` | 1 byte | `NV_PS2_CONFIG` | PS/2 Keyboard configuration. Bit [7]=enabled, [5:4]=repeat delay index, [3:2]=repeat rate index, [1:0]=reserved. |
| `$05` | 1 byte | `NV_SCSI_BOOT_ID` | SCSI boot device ID (0-7). Value `$FF` = SCSI boot disabled. |
| `$06` | 1 byte | `NV_BOOT_MODE` | Default boot behavior. 0=CMD_LOOP, 1=BASIC, 2=BANK1, 3=BANK2, 4=BANK3, 5=BANK4. |
| `$07` | 1 byte | `NV_BOOT_DELAY` | Auto-boot delay preset. 0=no auto-boot, 1=1 second, 2=5 seconds, 3=10 seconds. |
| `$08` | 1 byte | `NV_TERM_WIDTH` | Terminal display width in columns (0-255). Default: 80. |
| `$09` | 1 byte | `NV_TERM_HEIGHT` | Terminal display height in rows (0-255). Default: 30. |
| `$0A` | 1 byte | `NV_DC_HSCALE` | VERA DC_HSCALE register. Horizontal scaling factor. Default: `$80` (1x, 640 pixels wide). |
| `$0B` | 1 byte | `NV_DC_VSCALE` | VERA DC_VSCALE register. Vertical scaling factor. Default: `$40` (2x, standard for 240p). |
| `$0C` | 1 byte | `NV_DC_VSTART` | VERA DC_VSTART register. Top edge of visible display. Default: `$00`. |
| `$0D` | 1 byte | `NV_DC_VSTOP` | VERA DC_VSTOP register. Bottom edge of visible display at row 240. Default: `$F0`. |
| `$0E` | 1 byte | `NV_DC_BORDER` | VERA DC_BORDER register. Border color index. Default: `$00` (black). |
| `$0F` | 1 byte | `NV_TEXT_ATTR` | Default text attribute (character color/background). Default: `$01` (white on black). |
| `$10` | 1 byte | `NV_CONSOLE_OUTPUT` | Console and input redirection flags. See bit layout below. Default: `$00` (Serial output, RS232 input). |
| `$11`-`$27` | 23 bytes | *(reserved)* | Reserved for future configuration fields. Initialized to `$00`. |
| `$28`-`$1FF` | 472 bytes | *(unused)* | Unallocated. Available for future expansion if needed. |

### Field Details

#### `NV_CONSOLE_OUTPUT` (`$10`) - Bit Layout

```
[1] Input source       0 = RS232 serial input, 1 = PS/2 keyboard input (if available)
[0] Output target      0 = Serial output, 1 = VERA output (if initialized)
```

**Behavior:**
- **Bit [0] (Output):** If set and VERA is detected + initialized, console output is redirected to VERA. Otherwise, output remains on serial.
- **Bit [1] (Input):** If set and PS/2 keyboard handler is active, `GETCHAR` reads from PS/2 buffer. Otherwise, `GETCHAR` polls RS232 serial.

#### `NV_PS2_CONFIG` (`$04`) - Bit Layout

```
[7] Enable flag        0 = PS/2 disabled, 1 = PS/2 enabled
[5:4] Repeat delay     00 = 250 ms, 01 = 500 ms, 10 = 750 ms, 11 = 1000 ms
[3:2] Repeat rate      00 = 30 cps, 01 = 20 cps, 10 = 15 cps, 11 = 10 cps
[1:0] Reserved         (must be 0)
```

#### `NV_BOOT_MODE` (`$06`) - Values

| Value | Boot Target |
|-------|-------------|
| 0 | Return to monitor command loop (`CMD_LOOP`) |
| 1 | Launch EhBASIC interpreter |
| 2 | Boot from BANK 1 ROM |
| 3 | Boot from BANK 2 ROM |
| 4 | Boot from BANK 3 ROM |
| 5 | Boot from BANK 4 ROM |

#### `NV_BOOT_DELAY` (`$07`) - Values

| Value | Behavior |
|-------|----------|
| 0 | No auto-boot. Require user intervention to select boot mode. |
| 1 | Auto-boot after 1 second |
| 2 | Auto-boot after 5 seconds |
| 3 | Auto-boot after 10 seconds |

### Initialization and Validation

On cold start (`START`), AGFA-MON should:

1. **Read and validate:** Check `NV_MAGIC` at `$07100000`. If not `$A5`, use hardcoded defaults and skip to step 3.
2. **Checksum verify:** Compute sum of bytes `$07100002`-`$07100027` (mod 256). Compare to `NV_CHECKSUM` at `$07100001`. If mismatch, use hardcoded defaults and skip to step 3.
3. **Apply settings:**
   - If valid magic + checksum: read all fields and configure accordingly.
   - If invalid: use hardcoded defaults below, optionally write them back to NVRAM to initialize it.

### Hardcoded Defaults (if NVRAM uninitialized)

```
NV_MAGIC        = $A5
NV_CHECKSUM     = $F5    (computed: sum of $02-$0F + $10 + reserved $00s)
NV_BAUD_IDX     = 0      (9600 baud)
NV_DC_VIDEO     = $9A    (non-interlaced NTSC, layers/sprites enabled)
NV_PS2_CONFIG   = $40    (disabled, 250ms delay, 30 cps rate)
NV_SCSI_BOOT_ID = $FF    (SCSI boot disabled)
NV_BOOT_MODE    = 0      (CMD_LOOP)
NV_BOOT_DELAY   = 0      (no auto-boot)
NV_TERM_WIDTH   = 80
NV_TERM_HEIGHT  = 30
NV_DC_HSCALE    = $80    (1x horizontal scale, 640 pixels)
NV_DC_VSCALE    = $40    (2x vertical scale, standard for 240p)
NV_DC_VSTART    = $00    (top of visible display)
NV_DC_VSTOP     = $F0    (bottom at row 240)
NV_DC_BORDER    = $00    (black border)
NV_TEXT_ATTR    = $01    (white text on black background)
NV_CONSOLE_OUTPUT = $00  (serial output, RS232 input)
```

### EEPROM Write Considerations

- **Write cycle:** The Xicor X2804AP-45 requires **~10 ms per byte write**. After each byte write to the EEPROM, software must wait **at least 10 ms** before the next write. Failure to wait results in partial or lost writes. Writing all 10 fields sequentially with proper delays takes ~100 ms total.
  - **Verified:** Program delay of 20,000 CPU cycles at 16 MHz is approximately correct for 10 ms.
  - **Recommended:** Use a calibrated delay routine or a hardware timer (VIA T1) if available.
- **Wear leveling:** The EEPROM supports ~10⁶ erase/write cycles per byte. Avoid writing settings on every boot. Implement a `SAVE_CONFIG` command that only writes if the user explicitly requests it.
- **Atomic updates:** When writing multiple bytes, consider whether a partial write (due to power loss) could corrupt the config. A simple mitigation: write `NV_MAGIC = $00` first, then write all fields, then write `NV_MAGIC = $A5` and `NV_CHECKSUM` last. On the next boot, if `NV_MAGIC != $A5`, defaults are used.

