;******************************************************************************
;
;  nvram_config.inc - NVRAM Configuration Utility (Include File)
;  For AGFA Compugraphic 9000PS  /  68020
;
;  This file is designed to be INCLUDEd into a parent source file.
;  The parent must define the following EQUs BEFORE the INCLUDE:
;
;    CONFIG_BUF  EQU  <address>   ; 40-byte RAM working copy of NVRAM
;    LINE_BUF    EQU  <address>   ; 80-byte scratch buffer for GETLINE
;
;  Standalone test:  CONFIG_BUF = $02000500,  LINE_BUF = $02000480
;  ROM integration:  CONFIG_BUF = $02004096,  LINE_BUF = $02004000
;
; ── Entry Points ─────────────────────────────────────────────────────────────
;
;  LOAD_CONFIG   - Read EEPROM into CONFIG_BUF (validate magic+checksum first).
;                  Call at boot.  If EEPROM invalid, loads factory defaults.
;
;  SAVE_EEPROM   - Write CONFIG_BUF to EEPROM (wear-levelled, verified).
;
;  NVRAM_CONFIG  - Run the interactive CMOS-SETUP style menu.
;                  Returns via RTS when user presses Q.
;
; ── Internal I/O ─────────────────────────────────────────────────────────────
;
;  All I/O uses NVC_* prefixed routines (NVC = NVRAM Config internal).
;  These call the monitor serial API ($0406 PUTCHAR, $040C GETCHAR)
;  so they work whether running standalone or integrated into the ROM.
;
; ── NVRAM Layout ($07100000, 40 bytes) ───────────────────────────────────────
;
;  Off  Len  Name                Description
;  $00   1   NV_MAGIC            $A5 = contents valid
;  $01   1   NV_CHECKSUM         Sum of bytes $02-$27 mod 256
;  $02   1   NV_BAUD_IDX         Serial baud: 0=9600 1=19200 2=38400 3=57600 4=115200
;  $03   1   NV_DC_VIDEO         VERA DC_VIDEO register ($91=VGA $9A=NTSC $93=RGB)
;  $04   1   NV_PS2_CONFIG       PS/2 keyboard config byte (bit7=enable, etc.)
;  $05   1   NV_SCSI_BOOT_ID     SCSI boot device ID (0-7) or $FF = disabled
;  $06   1   NV_BOOT_MODE        0=CMD_LOOP 1=BASIC 2-5=ROM bank 1-4
;  $07   1   NV_BOOT_DELAY       0=manual 1=1s 2=5s 3=10s auto-boot delay
;  $08   1   NV_TERM_WIDTH       Console character columns ($50 = 80)
;  $09   1   NV_TERM_HEIGHT      Console character rows ($1E = 30)
;  $0A   1   NV_DC_HSCALE        VERA horizontal pixel scale ($80 = 640px 1x)
;  $0B   1   NV_DC_VSCALE        VERA vertical pixel scale ($40 = 480px 2x)
;  $0C   1   NV_DC_VSTART        VERA first visible scanline ($00 = top)
;  $0D   1   NV_DC_VSTOP         VERA last visible scanline ($F0 = line 240)
;  $0E   1   NV_DC_BORDER        VERA border colour palette index ($00 = black)
;  $0F   1   NV_TEXT_ATTR        Default text attribute byte ($01 = white/black)
;  $10   1   NV_CONSOLE_OUTPUT   Bit[0]=VERA out, Bit[1]=PS/2 in ($00 = serial/RS232)
;  $11-$27   (23 bytes reserved for future use, zeroed on init)
;
; ── Checksum Algorithm ───────────────────────────────────────────────────────
;
;  checksum = (sum of bytes at offsets $02 through $27) & $FF
;
;******************************************************************************

; ─── Hardware Addresses ──────────────────────────────────────────────────────

EEPROM_BASE     EQU     $07100000   ; Xicor X2804AP-45, 512-byte parallel EEPROM

; Monitor serial API entry points (always valid - monitor is always running)
MON_PUTCHAR     EQU     $0406       ; Send D0.B to serial
MON_GETCHAR     EQU     $040C       ; Receive byte into D0.B from serial

; ─── NVRAM Field Offsets (relative to EEPROM_BASE / CONFIG_BUF) ──────────────

NV_MAGIC        EQU     0
NV_CHECKSUM     EQU     1
NV_BAUD_IDX     EQU     2
NV_DC_VIDEO     EQU     3
NV_PS2_CONFIG   EQU     4
NV_SCSI_BOOT_ID EQU     5
NV_BOOT_MODE    EQU     6
NV_BOOT_DELAY   EQU     7
NV_TERM_WIDTH   EQU     8
NV_TERM_HEIGHT  EQU     9
NV_DC_HSCALE    EQU     10
NV_DC_VSCALE    EQU     11
NV_DC_VSTART    EQU     12
NV_DC_VSTOP     EQU     13
NV_DC_BORDER    EQU     14
NV_TEXT_ATTR    EQU     15
NV_CONSOLE_OUTPUT EQU   16          ; Bit[0]=VERA output, Bit[1]=PS/2 input

; ─── Factory Defaults ────────────────────────────────────────────────────────

DEFAULT_BAUD_IDX        EQU 0       ; 9600 baud
DEFAULT_DC_VIDEO        EQU $9A     ; NTSC, 240p, layers/sprites enabled
DEFAULT_PS2_CONFIG      EQU $40     ; disabled, 250ms delay, 30 cps
DEFAULT_SCSI_BOOT       EQU $FF     ; disabled
DEFAULT_BOOT_MODE       EQU 0       ; CMD_LOOP
DEFAULT_BOOT_DELAY      EQU 0       ; no auto-boot
DEFAULT_TERM_WIDTH      EQU 80
DEFAULT_TERM_HEIGHT     EQU 30
DEFAULT_DC_HSCALE       EQU $80     ; 1x horizontal scale
DEFAULT_DC_VSCALE       EQU $40     ; 2x vertical scale
DEFAULT_DC_VSTART       EQU $00
DEFAULT_DC_VSTOP        EQU $F0
DEFAULT_DC_BORDER       EQU $00     ; black border
DEFAULT_TEXT_ATTR       EQU $01     ; white on black
DEFAULT_CONSOLE_OUTPUT  EQU $00     ; serial output, RS232 input

;******************************************************************************
;  INIT_CONFIG - Initialize CONFIG_BUF with factory defaults
;******************************************************************************

INIT_CONFIG:
        MOVEA.L #CONFIG_BUF,A0
        MOVE.B  #$A5,(A0)+              ; Magic
        MOVE.B  #$00,(A0)+              ; Checksum (computed later)
        MOVE.B  #DEFAULT_BAUD_IDX,(A0)+
        MOVE.B  #DEFAULT_DC_VIDEO,(A0)+
        MOVE.B  #DEFAULT_PS2_CONFIG,(A0)+
        MOVE.B  #DEFAULT_SCSI_BOOT,(A0)+
        MOVE.B  #DEFAULT_BOOT_MODE,(A0)+
        MOVE.B  #DEFAULT_BOOT_DELAY,(A0)+
        MOVE.B  #DEFAULT_TERM_WIDTH,(A0)+
        MOVE.B  #DEFAULT_TERM_HEIGHT,(A0)+
        MOVE.B  #DEFAULT_DC_HSCALE,(A0)+
        MOVE.B  #DEFAULT_DC_VSCALE,(A0)+
        MOVE.B  #DEFAULT_DC_VSTART,(A0)+
        MOVE.B  #DEFAULT_DC_VSTOP,(A0)+
        MOVE.B  #DEFAULT_DC_BORDER,(A0)+
        MOVE.B  #DEFAULT_TEXT_ATTR,(A0)+
        MOVE.B  #DEFAULT_CONSOLE_OUTPUT,(A0)+   ; $10 NV_CONSOLE_OUTPUT
        ; Zero out reserved block ($11-$27) = 23 bytes
        MOVEQ   #23-1,D7
.icclr  MOVE.B  #0,(A0)+
        DBF     D7,.icclr
        RTS

;******************************************************************************
;  LOAD_CONFIG - Load from EEPROM if magic+checksum valid, else use defaults
;
;  Trashes: D0, D7, A0, A1
;******************************************************************************

LOAD_CONFIG:
        MOVE.B  EEPROM_BASE+NV_MAGIC,D0
        CMPI.B  #$A5,D0
        BNE.S   .lc_defaults

        ; Copy 40 bytes from EEPROM into CONFIG_BUF
        MOVEA.L #EEPROM_BASE,A0
        MOVEA.L #CONFIG_BUF,A1
        MOVEQ   #40-1,D7
.lccopy MOVE.B  (A0)+,(A1)+
        DBF     D7,.lccopy

        ; Verify checksum
        BSR     CALC_CHECKSUM
        CMP.B   CONFIG_BUF+NV_CHECKSUM,D0
        BNE.S   .lc_badchk

        LEA     NVC_STR_LOADED,A0
        BSR     NVC_PRINTSTR
        RTS

.lc_badchk:
        LEA     NVC_STR_BADCHK,A0
        BSR     NVC_PRINTSTR
        BRA.S   .lc_init

.lc_defaults:
        LEA     NVC_STR_DEFAULTS,A0
        BSR     NVC_PRINTSTR
.lc_init:
        BSR     INIT_CONFIG
        RTS

;******************************************************************************
;  CALC_CHECKSUM - Sum CONFIG_BUF bytes $02-$27 (38 bytes), return in D0.B
;  Trashes: D7, A0
;******************************************************************************

CALC_CHECKSUM:
        MOVEQ   #0,D0
        MOVEA.L #CONFIG_BUF+2,A0
        MOVEQ   #38-1,D7               ; 38 bytes: offsets $02-$27
.cssum  ADD.B   (A0)+,D0
        DBF     D7,.cssum
        RTS

;******************************************************************************
;  NVRAM_CONFIG - Interactive NVRAM editor (top-level entry point)
;  Prints banner, loads config, runs menu, returns via RTS.
;******************************************************************************

NVRAM_CONFIG:
        LEA     NVC_STR_BANNER,A0
        BSR     NVC_PRINTSTR
        BSR     LOAD_CONFIG
        BSR     NVC_MAIN_MENU
        RTS

;******************************************************************************
;  NVC_MAIN_MENU - Display menu and dispatch commands
;******************************************************************************

NVC_MAIN_MENU:
        LEA     NVC_STR_MENU,A0
        BSR     NVC_PRINTSTR
        BSR     NVC_DISPLAY_VALUES
        LEA     NVC_STR_MENU_OPTS,A0
        BSR     NVC_PRINTSTR

.prompt BSR     NVC_GETCHAR
        CMPI.B  #$0D,D0
        BNE.S   .cmd
        BRA.S   .prompt

.cmd    CMPI.B  #'0',D0
        BEQ.W   NVC_EDIT_0
        CMPI.B  #'1',D0
        BEQ.W   NVC_EDIT_1
        CMPI.B  #'2',D0
        BEQ.W   NVC_EDIT_2
        CMPI.B  #'3',D0
        BEQ.W   NVC_EDIT_3
        CMPI.B  #'4',D0
        BEQ.W   NVC_EDIT_4
        CMPI.B  #'5',D0
        BEQ.W   NVC_EDIT_5
        CMPI.B  #'6',D0
        BEQ.W   NVC_EDIT_6
        CMPI.B  #'7',D0
        BEQ.W   NVC_EDIT_7
        CMPI.B  #'8',D0
        BEQ.W   NVC_EDIT_8
        CMPI.B  #'9',D0
        BEQ.W   NVC_EDIT_9
        CMPI.B  #'A',D0
        BEQ.W   NVC_EDIT_A
        CMPI.B  #'a',D0
        BEQ.W   NVC_EDIT_A
        CMPI.B  #'B',D0
        BEQ.W   NVC_EDIT_B
        CMPI.B  #'b',D0
        BEQ.W   NVC_EDIT_B
        CMPI.B  #'C',D0
        BEQ.W   NVC_EDIT_C
        CMPI.B  #'c',D0
        BEQ.W   NVC_EDIT_C
        CMPI.B  #'D',D0
        BEQ.W   NVC_EDIT_D
        CMPI.B  #'d',D0
        BEQ.W   NVC_EDIT_D
        CMPI.B  #'E',D0
        BEQ.W   NVC_EDIT_E
        CMPI.B  #'e',D0
        BEQ.W   NVC_EDIT_E
        CMPI.B  #'V',D0
        BEQ.W   NVC_CMD_VIEW
        CMPI.B  #'v',D0
        BEQ.W   NVC_CMD_VIEW
        CMPI.B  #'R',D0
        BEQ.W   NVC_CMD_RESET
        CMPI.B  #'r',D0
        BEQ.W   NVC_CMD_RESET
        CMPI.B  #'S',D0
        BEQ.W   NVC_CMD_SAVE
        CMPI.B  #'s',D0
        BEQ.W   NVC_CMD_SAVE
        CMPI.B  #'Q',D0
        BEQ.W   NVC_CMD_QUIT
        CMPI.B  #'q',D0
        BEQ.W   NVC_CMD_QUIT
        CMPI.B  #'?',D0
        BEQ.W   NVC_CMD_HELP

        BRA.W   NVC_MAIN_MENU

;******************************************************************************
;  NVC_DISPLAY_VALUES - Show current config values in compact table
;******************************************************************************

NVC_DISPLAY_VALUES:
        LEA     NVC_STR_CUR_VAL,A0
        BSR     NVC_PRINTSTR

        ; Row 1: Baud, Video, PS2, SCSI
        LEA     NVC_COL_02,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_BAUD_IDX,D0
        BSR     NVC_PRINT_HEX2
        LEA     NVC_COL_03,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_DC_VIDEO,D0
        BSR     NVC_PRINT_HEX2
        LEA     NVC_COL_04,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_PS2_CONFIG,D0
        BSR     NVC_PRINT_HEX2
        LEA     NVC_COL_05,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_SCSI_BOOT_ID,D0
        BSR     NVC_PRINT_HEX2
        BSR     NVC_PRINT_CRLF

        ; Row 2: Boot mode, Delay, Width, Height
        LEA     NVC_COL_06,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_BOOT_MODE,D0
        BSR     NVC_PRINT_HEX2
        LEA     NVC_COL_07,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_BOOT_DELAY,D0
        BSR     NVC_PRINT_HEX2
        LEA     NVC_COL_08,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_TERM_WIDTH,D0
        BSR     NVC_PRINT_HEX2
        LEA     NVC_COL_09,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_TERM_HEIGHT,D0
        BSR     NVC_PRINT_HEX2
        BSR     NVC_PRINT_CRLF

        ; Row 3: HScale, VScale, VStart, VStop
        LEA     NVC_COL_0A,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_DC_HSCALE,D0
        BSR     NVC_PRINT_HEX2
        LEA     NVC_COL_0B,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_DC_VSCALE,D0
        BSR     NVC_PRINT_HEX2
        LEA     NVC_COL_0C,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_DC_VSTART,D0
        BSR     NVC_PRINT_HEX2
        LEA     NVC_COL_0D,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_DC_VSTOP,D0
        BSR     NVC_PRINT_HEX2
        BSR     NVC_PRINT_CRLF

        ; Row 4: Border, Attr, Console Output, Checksum
        LEA     NVC_COL_0E,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_DC_BORDER,D0
        BSR     NVC_PRINT_HEX2
        LEA     NVC_COL_0F,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_TEXT_ATTR,D0
        BSR     NVC_PRINT_HEX2
        LEA     NVC_COL_10,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_CONSOLE_OUTPUT,D0
        BSR     NVC_PRINT_HEX2
        LEA     NVC_COL_CSUM,A0
        BSR     NVC_PRINTSTR
        BSR     CALC_CHECKSUM
        BSR     NVC_PRINT_HEX2
        LEA     NVC_STR_VALID,A0
        BSR     NVC_PRINTSTR
        BSR     NVC_PRINT_CRLF

        RTS

;******************************************************************************
;  Field editors - NVC_EDIT_0 through NVC_EDIT_E
;******************************************************************************

NVC_EDIT_0:
        LEA     NVC_STR_ED_BAUD,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_BAUD_IDX,D0
        BSR     NVC_PRINT_HEX2
        BSR     NVC_PRINT_CRLF
        LEA     NVC_STR_BAUD_OPTS,A0
        BSR     NVC_PRINTSTR
        LEA     LINE_BUF,A0
        BSR     NVC_GETLINE
        MOVEA.L A0,A1
        BSR     NVC_SKIP_SPACES
        MOVE.B  (A1),D0
        BEQ.S   .skip
        BSR     NVC_PARSE_HEX
        CMPI.B  #4,D0
        BHI.S   .invalid
        MOVEA.L #CONFIG_BUF+NV_BAUD_IDX,A2
        MOVE.B  D0,(A2)
        BRA.W   NVC_MAIN_MENU
.invalid:
        LEA     NVC_STR_INVALID,A0
        BSR     NVC_PRINTSTR
        BRA.W   NVC_MAIN_MENU
.skip   BRA.W   NVC_MAIN_MENU

NVC_EDIT_1:
        LEA     NVC_STR_ED_VIDEO,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_DC_VIDEO,D0
        BSR     NVC_PRINT_HEX2
        BSR     NVC_PRINT_CRLF
        LEA     NVC_STR_VIDEO_OPTS,A0
        BSR     NVC_PRINTSTR
        LEA     LINE_BUF,A0
        BSR     NVC_GETLINE
        MOVEA.L A0,A1
        BSR     NVC_SKIP_SPACES
        MOVE.B  (A1),D0
        BEQ.S   .skip
        CMPI.B  #'1',D0
        BEQ.S   .vga
        CMPI.B  #'2',D0
        BEQ.S   .ntsc
        CMPI.B  #'3',D0
        BEQ.S   .rgb
        BSR     NVC_PARSE_HEX
        MOVEA.L #CONFIG_BUF+NV_DC_VIDEO,A2
        MOVE.B  D0,(A2)
        BRA.W   NVC_MAIN_MENU
.vga:   MOVE.B  #$91,CONFIG_BUF+NV_DC_VIDEO
        BRA.W   NVC_MAIN_MENU
.ntsc:  MOVE.B  #$9A,CONFIG_BUF+NV_DC_VIDEO
        BRA.W   NVC_MAIN_MENU
.rgb:   MOVE.B  #$93,CONFIG_BUF+NV_DC_VIDEO
        BRA.W   NVC_MAIN_MENU
.skip   BRA.W   NVC_MAIN_MENU

NVC_EDIT_2:
        LEA     NVC_STR_ED_PS2,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_PS2_CONFIG,D0
        BSR     NVC_PRINT_HEX2
        BSR     NVC_PRINT_CRLF
        LEA     NVC_STR_PS2_OPTS,A0
        BSR     NVC_PRINTSTR
        LEA     LINE_BUF,A0
        BSR     NVC_GETLINE
        MOVEA.L A0,A1
        BSR     NVC_SKIP_SPACES
        MOVE.B  (A1),D0
        BEQ.S   .skip
        BSR     NVC_PARSE_HEX
        MOVEA.L #CONFIG_BUF+NV_PS2_CONFIG,A2
        MOVE.B  D0,(A2)
        BRA.W   NVC_MAIN_MENU
.skip   BRA.W   NVC_MAIN_MENU

NVC_EDIT_3:
        LEA     NVC_STR_ED_SCSI,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_SCSI_BOOT_ID,D0
        BSR     NVC_PRINT_HEX2
        BSR     NVC_PRINT_CRLF
        LEA     NVC_STR_SCSI_OPTS,A0
        BSR     NVC_PRINTSTR
        LEA     LINE_BUF,A0
        BSR     NVC_GETLINE
        MOVEA.L A0,A1
        BSR     NVC_SKIP_SPACES
        MOVE.B  (A1),D0
        BEQ.S   .skip
        BSR     NVC_PARSE_HEX
        CMPI.B  #7,D0
        BLS.S   .ok
        CMPI.B  #$FF,D0
        BNE.S   .invalid
.ok:    MOVEA.L #CONFIG_BUF+NV_SCSI_BOOT_ID,A2
        MOVE.B  D0,(A2)
        BRA.W   NVC_MAIN_MENU
.invalid:
        LEA     NVC_STR_INVALID,A0
        BSR     NVC_PRINTSTR
        BRA.W   NVC_MAIN_MENU
.skip   BRA.W   NVC_MAIN_MENU

NVC_EDIT_4:
        LEA     NVC_STR_ED_BMODE,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_BOOT_MODE,D0
        BSR     NVC_PRINT_HEX2
        BSR     NVC_PRINT_CRLF
        LEA     NVC_STR_BMODE_OPTS,A0
        BSR     NVC_PRINTSTR
        LEA     LINE_BUF,A0
        BSR     NVC_GETLINE
        MOVEA.L A0,A1
        BSR     NVC_SKIP_SPACES
        MOVE.B  (A1),D0
        BEQ.S   .skip
        BSR     NVC_PARSE_HEX
        CMPI.B  #5,D0
        BHI.S   .invalid
        MOVEA.L #CONFIG_BUF+NV_BOOT_MODE,A2
        MOVE.B  D0,(A2)
        BRA.W   NVC_MAIN_MENU
.invalid:
        LEA     NVC_STR_INVALID,A0
        BSR     NVC_PRINTSTR
        BRA.W   NVC_MAIN_MENU
.skip   BRA.W   NVC_MAIN_MENU

NVC_EDIT_5:
        LEA     NVC_STR_ED_BDLY,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_BOOT_DELAY,D0
        BSR     NVC_PRINT_HEX2
        BSR     NVC_PRINT_CRLF
        LEA     NVC_STR_BDLY_OPTS,A0
        BSR     NVC_PRINTSTR
        LEA     LINE_BUF,A0
        BSR     NVC_GETLINE
        MOVEA.L A0,A1
        BSR     NVC_SKIP_SPACES
        MOVE.B  (A1),D0
        BEQ.S   .skip
        BSR     NVC_PARSE_HEX
        CMPI.B  #3,D0
        BHI.S   .invalid
        MOVEA.L #CONFIG_BUF+NV_BOOT_DELAY,A2
        MOVE.B  D0,(A2)
        BRA.W   NVC_MAIN_MENU
.invalid:
        LEA     NVC_STR_INVALID,A0
        BSR     NVC_PRINTSTR
        BRA.W   NVC_MAIN_MENU
.skip   BRA.W   NVC_MAIN_MENU

NVC_EDIT_6:
        LEA     NVC_STR_ED_WIDTH,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_TERM_WIDTH,D0
        BSR     NVC_PRINT_HEX2
        BSR     NVC_PRINT_CRLF
        LEA     NVC_STR_WIDTH_OPTS,A0
        BSR     NVC_PRINTSTR
        LEA     LINE_BUF,A0
        BSR     NVC_GETLINE
        MOVEA.L A0,A1
        BSR     NVC_SKIP_SPACES
        MOVE.B  (A1),D0
        BEQ.S   .skip
        BSR     NVC_PARSE_HEX
        MOVEA.L #CONFIG_BUF+NV_TERM_WIDTH,A2
        MOVE.B  D0,(A2)
        BRA.W   NVC_MAIN_MENU
.skip   BRA.W   NVC_MAIN_MENU

NVC_EDIT_7:
        LEA     NVC_STR_ED_HEIGHT,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_TERM_HEIGHT,D0
        BSR     NVC_PRINT_HEX2
        BSR     NVC_PRINT_CRLF
        LEA     NVC_STR_HEIGHT_OPTS,A0
        BSR     NVC_PRINTSTR
        LEA     LINE_BUF,A0
        BSR     NVC_GETLINE
        MOVEA.L A0,A1
        BSR     NVC_SKIP_SPACES
        MOVE.B  (A1),D0
        BEQ.S   .skip
        BSR     NVC_PARSE_HEX
        MOVEA.L #CONFIG_BUF+NV_TERM_HEIGHT,A2
        MOVE.B  D0,(A2)
        BRA.W   NVC_MAIN_MENU
.skip   BRA.W   NVC_MAIN_MENU

NVC_EDIT_8:
        LEA     NVC_STR_ED_HSCALE,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_DC_HSCALE,D0
        BSR     NVC_PRINT_HEX2
        BSR     NVC_PRINT_CRLF
        LEA     NVC_STR_HSCALE_OPTS,A0
        BSR     NVC_PRINTSTR
        LEA     LINE_BUF,A0
        BSR     NVC_GETLINE
        MOVEA.L A0,A1
        BSR     NVC_SKIP_SPACES
        MOVE.B  (A1),D0
        BEQ.S   .skip
        BSR     NVC_PARSE_HEX
        MOVEA.L #CONFIG_BUF+NV_DC_HSCALE,A2
        MOVE.B  D0,(A2)
        BRA.W   NVC_MAIN_MENU
.skip   BRA.W   NVC_MAIN_MENU

NVC_EDIT_9:
        LEA     NVC_STR_ED_VSCALE,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_DC_VSCALE,D0
        BSR     NVC_PRINT_HEX2
        BSR     NVC_PRINT_CRLF
        LEA     NVC_STR_VSCALE_OPTS,A0
        BSR     NVC_PRINTSTR
        LEA     LINE_BUF,A0
        BSR     NVC_GETLINE
        MOVEA.L A0,A1
        BSR     NVC_SKIP_SPACES
        MOVE.B  (A1),D0
        BEQ.S   .skip
        BSR     NVC_PARSE_HEX
        MOVEA.L #CONFIG_BUF+NV_DC_VSCALE,A2
        MOVE.B  D0,(A2)
        BRA.W   NVC_MAIN_MENU
.skip   BRA.W   NVC_MAIN_MENU

NVC_EDIT_A:
        LEA     NVC_STR_ED_VSTART,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_DC_VSTART,D0
        BSR     NVC_PRINT_HEX2
        BSR     NVC_PRINT_CRLF
        LEA     NVC_STR_VSTART_OPTS,A0
        BSR     NVC_PRINTSTR
        LEA     LINE_BUF,A0
        BSR     NVC_GETLINE
        MOVEA.L A0,A1
        BSR     NVC_SKIP_SPACES
        MOVE.B  (A1),D0
        BEQ.S   .skip
        BSR     NVC_PARSE_HEX
        MOVEA.L #CONFIG_BUF+NV_DC_VSTART,A2
        MOVE.B  D0,(A2)
        BRA.W   NVC_MAIN_MENU
.skip   BRA.W   NVC_MAIN_MENU

NVC_EDIT_B:
        LEA     NVC_STR_ED_VSTOP,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_DC_VSTOP,D0
        BSR     NVC_PRINT_HEX2
        BSR     NVC_PRINT_CRLF
        LEA     NVC_STR_VSTOP_OPTS,A0
        BSR     NVC_PRINTSTR
        LEA     LINE_BUF,A0
        BSR     NVC_GETLINE
        MOVEA.L A0,A1
        BSR     NVC_SKIP_SPACES
        MOVE.B  (A1),D0
        BEQ.S   .skip
        BSR     NVC_PARSE_HEX
        MOVEA.L #CONFIG_BUF+NV_DC_VSTOP,A2
        MOVE.B  D0,(A2)
        BRA.W   NVC_MAIN_MENU
.skip   BRA.W   NVC_MAIN_MENU

NVC_EDIT_C:
        LEA     NVC_STR_ED_BORDER,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_DC_BORDER,D0
        BSR     NVC_PRINT_HEX2
        BSR     NVC_PRINT_CRLF
        LEA     NVC_STR_BORDER_OPTS,A0
        BSR     NVC_PRINTSTR
        LEA     LINE_BUF,A0
        BSR     NVC_GETLINE
        MOVEA.L A0,A1
        BSR     NVC_SKIP_SPACES
        MOVE.B  (A1),D0
        BEQ.S   .skip
        BSR     NVC_PARSE_HEX
        MOVEA.L #CONFIG_BUF+NV_DC_BORDER,A2
        MOVE.B  D0,(A2)
        BRA.W   NVC_MAIN_MENU
.skip   BRA.W   NVC_MAIN_MENU

NVC_EDIT_D:
        LEA     NVC_STR_ED_ATTR,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_TEXT_ATTR,D0
        BSR     NVC_PRINT_HEX2
        BSR     NVC_PRINT_CRLF
        LEA     NVC_STR_ATTR_OPTS,A0
        BSR     NVC_PRINTSTR
        LEA     LINE_BUF,A0
        BSR     NVC_GETLINE
        MOVEA.L A0,A1
        BSR     NVC_SKIP_SPACES
        MOVE.B  (A1),D0
        BEQ.S   .skip
        BSR     NVC_PARSE_HEX
        MOVEA.L #CONFIG_BUF+NV_TEXT_ATTR,A2
        MOVE.B  D0,(A2)
        BRA.W   NVC_MAIN_MENU
.skip   BRA.W   NVC_MAIN_MENU

NVC_EDIT_E:
        LEA     NVC_STR_ED_CONSOLE,A0
        BSR     NVC_PRINTSTR
        MOVE.B  CONFIG_BUF+NV_CONSOLE_OUTPUT,D0
        BSR     NVC_PRINT_HEX2
        BSR     NVC_PRINT_CRLF
        LEA     NVC_STR_CONSOLE_OPTS,A0
        BSR     NVC_PRINTSTR
        LEA     LINE_BUF,A0
        BSR     NVC_GETLINE
        MOVEA.L A0,A1
        BSR     NVC_SKIP_SPACES
        MOVE.B  (A1),D0
        BEQ.S   .skip
        BSR     NVC_PARSE_HEX
        ANDI.B  #$03,D0             ; Only bits [1:0] valid
        MOVEA.L #CONFIG_BUF+NV_CONSOLE_OUTPUT,A2
        MOVE.B  D0,(A2)
        BRA.W   NVC_MAIN_MENU
.skip   BRA.W   NVC_MAIN_MENU

;******************************************************************************
;  Command handlers
;******************************************************************************

NVC_CMD_VIEW:
        LEA     NVC_STR_VIEW_HDR,A0
        BSR     NVC_PRINTSTR
        BSR     NVC_DISPLAY_VALUES
        LEA     NVC_STR_VIEW_FTR,A0
        BSR     NVC_PRINTSTR
        BSR     NVC_GETCHAR
        BRA.W   NVC_MAIN_MENU

NVC_CMD_RESET:
        LEA     NVC_STR_RST_CONF,A0
        BSR     NVC_PRINTSTR
        BSR     NVC_GETCHAR
        CMPI.B  #'Y',D0
        BEQ.S   .do_reset
        CMPI.B  #'y',D0
        BEQ.S   .do_reset
        BRA.W   NVC_MAIN_MENU
.do_reset:
        BSR     INIT_CONFIG
        BRA.W   NVC_MAIN_MENU

NVC_CMD_SAVE:
        BSR     SAVE_EEPROM
        BRA.W   NVC_MAIN_MENU

NVC_CMD_QUIT:
        RTS

NVC_CMD_HELP:
        LEA     NVC_STR_HELP,A0
        BSR     NVC_PRINTSTR
        BRA.W   NVC_MAIN_MENU

;******************************************************************************
;  SAVE_EEPROM - Write CONFIG_BUF to EEPROM (wear-levelled, verified)
;******************************************************************************

SAVE_EEPROM:
        LEA     NVC_STR_SAVING,A0
        BSR     NVC_PRINTSTR

        BSR     CALC_CHECKSUM
        MOVE.B  D0,CONFIG_BUF+NV_CHECKSUM

        MOVEA.L #CONFIG_BUF,A0
        MOVEA.L #EEPROM_BASE,A1
        MOVEQ   #40-1,D7
.sewr   MOVE.B  (A0),D0
        CMP.B   (A1),D0
        BEQ.S   .sesame
        MOVE.B  D0,(A1)
        BSR     NVC_EEPROM_DELAY
.sesame ADDQ.L  #1,A0
        ADDQ.L  #1,A1
        DBF     D7,.sewr

        MOVEA.L #CONFIG_BUF,A0
        MOVEA.L #EEPROM_BASE,A1
        MOVEQ   #40-1,D7
.sever  MOVE.B  (A0)+,D0
        CMP.B   (A1)+,D0
        BNE.S   .sefail
        DBF     D7,.sever

        LEA     NVC_STR_SAVED,A0
        BSR     NVC_PRINTSTR
        RTS

.sefail:
        LEA     NVC_STR_SAVEFAIL,A0
        BSR     NVC_PRINTSTR
        RTS

;******************************************************************************
;  NVC_EEPROM_DELAY - ~100 ms delay for EEPROM write cycle
;******************************************************************************

NVC_EEPROM_DELAY:
        MOVE.L  #400000,D0
.edl    SUBQ.L  #1,D0
        BNE.S   .edl
        RTS

;******************************************************************************
;  NVC I/O Routines - call through monitor API for serial consistency
;******************************************************************************

NVC_GETCHAR:
        JSR     MON_GETCHAR
        RTS

NVC_PUTCHAR:
        JSR     MON_PUTCHAR
        RTS

NVC_PRINTSTR:
.lp     MOVE.B  (A0)+,D0
        BEQ.S   .end
        BSR     NVC_PUTCHAR
        BRA.S   .lp
.end    RTS

NVC_PRINT_CRLF:
        MOVE.B  #$0D,D0
        BSR     NVC_PUTCHAR
        MOVE.B  #$0A,D0
        BSR     NVC_PUTCHAR
        RTS

NVC_PRINT_HEX2:
        MOVEM.L D1,-(SP)
        MOVE.B  D0,D1
        LSR.B   #4,D0
        BSR     NVC_HEX_NYBL
        MOVE.B  D1,D0
        AND.B   #$0F,D0
        BSR     NVC_HEX_NYBL
        MOVEM.L (SP)+,D1
        RTS

NVC_HEX_NYBL:
        CMP.B   #10,D0
        BLT.S   .dec
        ADD.B   #7,D0
.dec    ADD.B   #'0',D0
        BSR     NVC_PUTCHAR
        RTS

NVC_SKIP_SPACES:
.lp     MOVE.B  (A1),D0
        CMPI.B  #$20,D0
        BNE.S   .end
        ADDQ.L  #1,A1
        BRA.S   .lp
.end    RTS

NVC_PARSE_HEX:
        MOVEQ   #0,D0
.lp     MOVE.B  (A1)+,D1
        CMPI.B  #'0',D1
        BCS.S   .end
        CMPI.B  #'9',D1
        BLS.S   .dec
        CMPI.B  #'A',D1
        BCS.S   .end
        CMPI.B  #'F',D1
        BLS.S   .uc
        CMPI.B  #'a',D1
        BCS.S   .end
        CMPI.B  #'f',D1
        BHI.S   .end
        SUB.B   #'a'-10,D1
        BRA.S   .store
.dec:   SUB.B   #'0',D1
        BRA.S   .store
.uc:    SUB.B   #'A'-10,D1
.store  LSL.L   #4,D0
        OR.B    D1,D0
        BRA.S   .lp
.end    SUBQ.L  #1,A1
        RTS

NVC_GETLINE:
        MOVEA.L A0,A2
        MOVEQ   #78,D7
.read   BSR     NVC_GETCHAR
        CMPI.B  #$0D,D0
        BEQ.S   .done
        CMPI.B  #$08,D0
        BEQ.S   .backsp
        CMPI.B  #$7F,D0
        BEQ.S   .backsp
        CMPI.B  #$20,D0
        BLT.S   .read
        CMPI.B  #$7E,D0
        BGT.S   .read
        MOVE.B  D0,(A0)+
        BSR     NVC_PUTCHAR
        DBRA    D7,.read
        BRA.S   .done
.backsp CMPA.L  A2,A0
        BLE.S   .read
        SUBQ.L  #1,A0
        MOVE.B  #$08,D0
        BSR     NVC_PUTCHAR
        MOVE.B  #$20,D0
        BSR     NVC_PUTCHAR
        MOVE.B  #$08,D0
        BSR     NVC_PUTCHAR
        BRA.S   .read
.done   MOVE.B  #$0D,D0
        BSR     NVC_PUTCHAR
        MOVE.B  #$0A,D0
        BSR     NVC_PUTCHAR
        MOVE.B  #$00,(A0)
        MOVEA.L A2,A0
        RTS

;******************************************************************************
;  String Constants
;******************************************************************************

NVC_STR_BANNER:
        DC.B    13,10,"====================================",13,10
        DC.B    "  AGFA NVRAM Configuration Utility",13,10
        DC.B    "====================================",13,10,0
        ALIGN   2

NVC_STR_LOADED:
        DC.B    "Loaded from EEPROM.",13,10,0
        ALIGN   2

NVC_STR_BADCHK:
        DC.B    "EEPROM checksum bad, using defaults.",13,10,0
        ALIGN   2

NVC_STR_DEFAULTS:
        DC.B    "EEPROM not initialized, using defaults.",13,10,0
        ALIGN   2

NVC_STR_MENU:
        DC.B    13,10,"-------------------------------------",13,10,0
        ALIGN   2

NVC_STR_CUR_VAL:
        DC.B    "[Current Values]",13,10,0
        ALIGN   2

NVC_COL_02:
        DC.B    "$02 Bau=",0
        ALIGN   2
NVC_COL_03:
        DC.B    "  $03 Vid=",0
        ALIGN   2
NVC_COL_04:
        DC.B    "  $04 PS2=",0
        ALIGN   2
NVC_COL_05:
        DC.B    "  $05 SCS=",0
        ALIGN   2
NVC_COL_06:
        DC.B    "$06 Mod=",0
        ALIGN   2
NVC_COL_07:
        DC.B    "  $07 Dly=",0
        ALIGN   2
NVC_COL_08:
        DC.B    "  $08 Wid=",0
        ALIGN   2
NVC_COL_09:
        DC.B    "  $09 Hgt=",0
        ALIGN   2
NVC_COL_0A:
        DC.B    "$0A HSc=",0
        ALIGN   2
NVC_COL_0B:
        DC.B    "  $0B VSc=",0
        ALIGN   2
NVC_COL_0C:
        DC.B    "  $0C VSr=",0
        ALIGN   2
NVC_COL_0D:
        DC.B    "  $0D VSp=",0
        ALIGN   2
NVC_COL_0E:
        DC.B    "$0E Brd=",0
        ALIGN   2
NVC_COL_0F:
        DC.B    "  $0F Att=",0
        ALIGN   2
NVC_COL_10:
        DC.B    "  $10 Con=",0
        ALIGN   2
NVC_COL_CSUM:
        DC.B    "  Chk=",0
        ALIGN   2

NVC_STR_VALID:
        DC.B    " (ok)",13,10,0
        ALIGN   2

NVC_STR_MENU_OPTS:
        DC.B    13,10,"NVRAM SETUP MENU",13,10
        DC.B    "0 - CHA Baud Rate       8 - VERA H-Scale",13,10
        DC.B    "1 - VERA Video Config   9 - VERA V-Scale",13,10
        DC.B    "2 - PS2 Keyboard        A - VERA V-Start",13,10
        DC.B    "3 - SCSI Boot Option    B - VERA V-Stop",13,10
        DC.B    "4 - Boot Mode           C - VERA Border",13,10
        DC.B    "5 - Boot Delay          D - Text Attribute",13,10
        DC.B    "6 - VERA Buffer Width   E - Console I/O Mode",13,10
        DC.B    "7 - VERA Buffer Height  V - View current settings",13,10
        DC.B    "S - Save to EEPROM      R - Reset to defaults",13,10
        DC.B    "Q - Quit without saving ? - Help, show this menu",13,10,13,10
        DC.B    "Choice: ",0
        ALIGN   2

NVC_STR_ED_BAUD:
        DC.B    13,10,"Edit Baud index",13,10,"Current value: ",0
        ALIGN   2
NVC_STR_BAUD_OPTS:
        DC.B    13,10,"Valid range: 0-4",13,10
        DC.B    "  0=9600  1=19200  2=38400  3=57600  4=115200",13,10
        DC.B    "Enter new value (or blank to skip): ",0
        ALIGN   2

NVC_STR_ED_VIDEO:
        DC.B    13,10,"Edit DC_VIDEO (VERA Video Output Mode)",13,10,"Current value: ",0
        ALIGN   2
NVC_STR_VIDEO_OPTS:
        DC.B    13,10,"Presets: 1=VGA ($91)  2=NTSC ($9A)  3=RGB ($93)",13,10
        DC.B    "Or enter hex $00-$FF: ",0
        ALIGN   2

NVC_STR_ED_PS2:
        DC.B    13,10,"Edit PS2 config",13,10,"Current value: ",0
        ALIGN   2
NVC_STR_PS2_OPTS:
        DC.B    13,10,"Bit[7]=enable  Bits[5:4]=delay  Bits[3:2]=rate",13,10
        DC.B    "Enter hex value: ",0
        ALIGN   2

NVC_STR_ED_SCSI:
        DC.B    13,10,"Edit SCSI Boot ID",13,10,"Current value: ",0
        ALIGN   2
NVC_STR_SCSI_OPTS:
        DC.B    13,10,"Valid: 0-7 (device ID) or FF (disabled)",13,10
        DC.B    "Enter hex value: ",0
        ALIGN   2

NVC_STR_ED_BMODE:
        DC.B    13,10,"Edit Boot Mode",13,10,"Current value: ",0
        ALIGN   2
NVC_STR_BMODE_OPTS:
        DC.B    13,10,"0=CMD_LOOP  1=BASIC  2=BANK1  3=BANK2  4=BANK3  5=BANK4",13,10
        DC.B    "Enter value (0-5): ",0
        ALIGN   2

NVC_STR_ED_BDLY:
        DC.B    13,10,"Edit Boot Delay",13,10,"Current value: ",0
        ALIGN   2
NVC_STR_BDLY_OPTS:
        DC.B    13,10,"0=Manual  1=1s  2=5s  3=10s",13,10
        DC.B    "Enter value (0-3): ",0
        ALIGN   2

NVC_STR_ED_WIDTH:
        DC.B    13,10,"Edit Terminal Width",13,10,"Current value: ",0
        ALIGN   2
NVC_STR_WIDTH_OPTS:
        DC.B    13,10,"Range $00-$FF  (50 hex = 80 columns)",13,10
        DC.B    "Enter hex value: ",0
        ALIGN   2

NVC_STR_ED_HEIGHT:
        DC.B    13,10,"Edit Terminal Height",13,10,"Current value: ",0
        ALIGN   2
NVC_STR_HEIGHT_OPTS:
        DC.B    13,10,"Range $00-$FF  (1E hex = 30 rows)",13,10
        DC.B    "Enter hex value: ",0
        ALIGN   2

NVC_STR_ED_HSCALE:
        DC.B    13,10,"Edit DC_HSCALE (Horizontal Scale)",13,10,"Current value: ",0
        ALIGN   2
NVC_STR_HSCALE_OPTS:
        DC.B    13,10,"Range $00-$FF  ($80 = 1x, 640px wide)",13,10
        DC.B    "Enter hex value: ",0
        ALIGN   2

NVC_STR_ED_VSCALE:
        DC.B    13,10,"Edit DC_VSCALE (Vertical Scale)",13,10,"Current value: ",0
        ALIGN   2
NVC_STR_VSCALE_OPTS:
        DC.B    13,10,"Range $00-$FF  ($40 = 2x, 240p->480p)",13,10
        DC.B    "Enter hex value: ",0
        ALIGN   2

NVC_STR_ED_VSTART:
        DC.B    13,10,"Edit DC_VSTART (Top of Display)",13,10,"Current value: ",0
        ALIGN   2
NVC_STR_VSTART_OPTS:
        DC.B    13,10,"Range $00-$FF (scanline)  $00 = top",13,10
        DC.B    "Enter hex value: ",0
        ALIGN   2

NVC_STR_ED_VSTOP:
        DC.B    13,10,"Edit DC_VSTOP (Bottom of Display)",13,10,"Current value: ",0
        ALIGN   2
NVC_STR_VSTOP_OPTS:
        DC.B    13,10,"Range $00-$FF (scanline)  $F0 = line 240",13,10
        DC.B    "Enter hex value: ",0
        ALIGN   2

NVC_STR_ED_BORDER:
        DC.B    13,10,"Edit DC_BORDER (Border Color)",13,10,"Current value: ",0
        ALIGN   2
NVC_STR_BORDER_OPTS:
        DC.B    13,10,"Range $00-$FF (palette index)  $00 = black",13,10
        DC.B    "Enter hex value: ",0
        ALIGN   2

NVC_STR_ED_ATTR:
        DC.B    13,10,"Edit TEXT_ATTR (Default Text Color)",13,10,"Current value: ",0
        ALIGN   2
NVC_STR_ATTR_OPTS:
        DC.B    13,10,"Range $00-$FF (color attribute)  $01 = white/black",13,10
        DC.B    "Enter hex value: ",0
        ALIGN   2

NVC_STR_ED_CONSOLE:
        DC.B    13,10,"Edit Console I/O Mode ($10)",13,10,"Current value: ",0
        ALIGN   2
NVC_STR_CONSOLE_OPTS:
        DC.B    13,10,"Bitmask -- enter hex value 0-3:",13,10
        DC.B    "  Bit[0] Output: 0=Serial  1=VERA (if initialized)",13,10
        DC.B    "  Bit[1] Input:  0=RS232   1=PS/2 (if available)",13,10
        DC.B    "  $00=Serial/RS232  $01=VERA/RS232",13,10
        DC.B    "  $02=Serial/PS2   $03=VERA/PS2",13,10
        DC.B    "Enter value (0-3): ",0
        ALIGN   2

NVC_STR_INVALID:
        DC.B    "Invalid input! Try again.",13,10,0
        ALIGN   2

NVC_STR_VIEW_HDR:
        DC.B    13,10,"=== ALL NVRAM FIELDS ===",13,10,0
        ALIGN   2

NVC_STR_VIEW_FTR:
        DC.B    13,10,"Press any key to return to menu...",0
        ALIGN   2

NVC_STR_RST_CONF:
        DC.B    "Reset all values to defaults? (Y/N): ",0
        ALIGN   2

NVC_STR_SAVING:
        DC.B    "Saving to EEPROM...",0
        ALIGN   2

NVC_STR_SAVED:
        DC.B    "Saved OK.",13,10,0
        ALIGN   2

NVC_STR_SAVEFAIL:
        DC.B    "SAVE FAILED: verify mismatch!",13,10,0
        ALIGN   2

NVC_STR_HELP:
        DC.B    "Use 0-9/A-E to edit fields.",13,10
        DC.B    "V=View all, R=Reset, S=Save, Q=Quit",13,10,0
        ALIGN   2

; ── End of nvram_config.inc ──────────────────────────────────────────────────
