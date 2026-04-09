;******************************************************************************
;
;  vera_text.inc -- VERA Text Console API (Include File)
;  For AGFA Compugraphic 9000PS / 68020
;
;  This file provides the VERA text console routines that are:
;    1. Compiled into ROM as part of agfa-mon.m68
;    2. Copied to RAM at $02004500 at boot for maximum performance
;    3. Reached via JMP thunks at $02004100+ (call with JSR addr)
;
;  No EQUs need to be defined by the parent -- all dependencies are on the
;  CCB at $02000300 (defined in the memory map).
;
; -- Public Entry Points -------------------------------------------------------
;
;  VERA_PUTCHAR   (D0.B)  -- Write one character to VERA console.
;                            Handles CR, LF, BS, printable chars.
;                            VT100-style pending wrap at col 80.
;
;  VERA_PRINTSTR  (A0)    -- Print NUL-terminated string to VERA console.
;
;  VERA_CLRSCR            -- Clear the entire visible viewport.
;
;  VERA_CLR_ROW   (D1.W)  -- Clear physical row D1 (0-63) in tilemap.
;
;  VERA_HANDLE_LF         -- Advance row with scroll logic (internal, also
;                            callable directly if needed).
;
;  VERA_STREAM_START      -- Set VERA ADDR0 to current row, stride=1 (for
;                            high-speed sequential writes via STREAM_CHAR).
;
;  STREAM_CHAR    (D0.B)  -- Write D0 + current attribute to VERA_DATA0.
;                            Assumes VERA ADDR0 is set with stride=1.
;
;  STREAM_HEX8    (D0.L)  -- Write 8 hex digits via streaming.
;
;  STREAM_HEX2    (D0.B)  -- Write 2 hex digits via streaming.
;
; -- Dispatch Vector Table (JMP Thunks) ----------------------------------------
;
;  Each entry at $02004100+ is a 6-byte JMP abs.L instruction that redirects
;  to the RAM copy of the routine. Call with plain JSR addr:
;
;    JSR VT_VEC_PUTCHAR     = JSR $02004100 = JMP to VERA_PUTCHAR in RAM
;    JSR VT_VEC_PRINTSTR    = JSR $02004108 = JMP to VERA_PRINTSTR in RAM
;    etc.
;
;  8 bytes per slot (6-byte JMP + 2 pad) for clean alignment.
;
; -- CCB Layout ($02000300) ----------------------------------------------------
;
;  +$00 V_CUR_COL    Word  Current cursor column (0-79)
;  +$02 V_CUR_ROW    Word  Current physical write row in tilemap (0-63)
;  +$04 V_SCROLL_ROW Word  Physical row at top of display (0-63)
;  +$06 V_MAGIC      Word  $A55A = initialized
;  +$08 V_TEXT_ATTR  Byte  Current character color attribute
;  +$09 V_VIEW_H     Byte  Visible viewport height in rows (from NVRAM)
;  +$0A V_MAP_H      Byte  Tilemap height in rows (always 64)
;  +$0B V_FLAGS      Byte  [7]=scroll [6]=wrap
;
; -- VRAM Layout ---------------------------------------------------------------
;
;  Tilemap:  VRAM $00000 -- 128 columns x 64 rows x 2 bytes = 16 KB
;  Font:     VRAM $0F000 -- 256 chars x 8 bytes = 2 KB
;  Palette:  VRAM $1FA00 -- 256 colors x 2 bytes = 512 bytes
;
;  Address formula: (row * 256) + (col * 2)
;
; -- Performance Notes ---------------------------------------------------------
;
;  These routines run from RAM ($02004500+) when loaded for maximum performance.
;  The copy is performed by agfa-mon.m68 at boot via VT_INIT_RAM.
;  JMP thunks at $02004100+ redirect calls to the RAM copies.
;
;  High-speed streaming (VERA_STREAM_START + STREAM_CHAR) uses VERA stride-1
;  auto-increment, eliminating per-character address calculation.
;
;******************************************************************************

; --- CCB Base and Field Offsets -----------------------------------------------

VT_CCB          EQU     $02000300
VT_CUR_COL      EQU     VT_CCB+0        ; Word
VT_CUR_ROW      EQU     VT_CCB+2        ; Word
VT_SCROLL_ROW   EQU     VT_CCB+4        ; Word
VT_MAGIC        EQU     VT_CCB+6        ; Word ($A55A)
VT_TEXT_ATTR    EQU     VT_CCB+8        ; Byte
VT_VIEW_H       EQU     VT_CCB+9        ; Byte
VT_MAP_H        EQU     VT_CCB+10       ; Byte
VT_FLAGS        EQU     VT_CCB+11       ; Byte
VT_CURSOR_STATE EQU     VT_CCB+12       ; Byte  $00=hidden, $FF=shown (inverted)
VT_BLINK_TIMER  EQU     VT_CCB+13       ; Byte  countdown to blink toggle
VT_CURSOR_EN    EQU     VT_CCB+14       ; Byte  $00=disabled, $01=enabled
VT_CURSOR_DIRTY EQU     VT_CCB+15       ; Byte  $FF=cursor needs redraw at new position (set by PUTCHAR, cleared by ISR)

VT_MAGIC_VAL    EQU     $A55A
VT_VF_PEND_WRAP EQU     $20            ; bit 5 of V_FLAGS: pending wrap

; --- VERA Hardware ------------------------------------------------------------

VT_VERA_BASE    EQU     $04000020
VT_ADDR0_L      EQU     $00
VT_ADDR0_M      EQU     $01
VT_ADDR0_H      EQU     $02
VT_DATA0        EQU     $03
VT_CTRL         EQU     $05
VT_L0_VSCROLL_L EQU     $12
VT_L0_VSCROLL_H EQU     $13

; --- Dispatch Vector Table (JMP Thunks) ---------------------------------------
; Each slot is 8 bytes: 6-byte JMP abs.L + 2 bytes padding.
; Call with: JSR VT_VEC_xxx  (NOT JSR (VT_VEC_xxx) -- no parens)
; VT_INIT_VECTORS writes JMP instructions here at boot.

VT_VEC_PUTCHAR      EQU     $02004100   ; JMP -> VERA_PUTCHAR  in RAM
VT_VEC_PRINTSTR     EQU     $02004108   ; JMP -> VERA_PRINTSTR in RAM
VT_VEC_CLRSCR       EQU     $02004110   ; JMP -> VERA_CLRSCR   in RAM
VT_VEC_CLR_ROW      EQU     $02004118   ; JMP -> VERA_CLR_ROW  in RAM
VT_VEC_HANDLE_LF    EQU     $02004120   ; JMP -> VERA_HANDLE_LF in RAM
VT_VEC_STREAM_START EQU     $02004128   ; JMP -> VERA_STREAM_START in RAM
VT_VEC_STREAM_CHAR  EQU     $02004130   ; JMP -> STREAM_CHAR   in RAM
; $02004138-$0200417F reserved for future vectors (8 more 8-byte slots)
; --- Extended API (new) ---
VT_VEC_GOTOXY       EQU     $02004138   ; JMP -> VERA_GOTOXY   in RAM
VT_VEC_SET_ATTR     EQU     $02004140   ; JMP -> VERA_SET_ATTR in RAM
VT_VEC_CURSOR_ON    EQU     $02004148   ; JMP -> VERA_CURSOR_ON in RAM
VT_VEC_CURSOR_OFF   EQU     $02004150   ; JMP -> VERA_CURSOR_OFF in RAM
VT_VEC_SCROLL_UP    EQU     $02004158   ; JMP -> VERA_SCROLL_UP in RAM
VT_VEC_SCROLL_DOWN  EQU     $02004160   ; JMP -> VERA_SCROLL_DOWN in RAM
VT_VEC_CLR_EOL      EQU     $02004168   ; JMP -> VERA_CLR_EOL  in RAM
VT_VEC_CLR_EOS      EQU     $02004170   ; JMP -> VERA_CLR_EOS  in RAM
VT_VEC_INSERT_LINE  EQU     $02004178   ; JMP -> VERA_INSERT_LINE in RAM
VT_VEC_DELETE_LINE  EQU     $02004180   ; JMP -> VERA_DELETE_LINE in RAM
VT_VEC_CURSOR_TOGGLE EQU    $02004188   ; JMP -> CURSOR_TOGGLE   in RAM

; --- RAM Copy Destination -----------------------------------------------------

VT_RAM_BASE     EQU     $02004500       ; VERA text code copied here at boot

;******************************************************************************
;  VT_INIT_RAM -- Copy VERA text routines from ROM to RAM
;
;  Call once at boot after VERA_INIT.  Copies from VT_ROM_START to
;  VT_ROM_END into RAM at VT_RAM_BASE.
;
;  Trashes: D0, A0, A1
;******************************************************************************

VT_INIT_RAM:
        LEA     VT_ROM_START,A0         ; Source: ROM (or test image in RAM)
        MOVEA.L #VT_RAM_BASE,A1         ; Destination: RAM
        MOVE.L  #VT_ROM_END-VT_ROM_START-1,D0
.vtcpy  MOVE.B  (A0)+,(A1)+
        DBF     D0,.vtcpy
        RTS

;******************************************************************************
;  VT_INIT_VECTORS -- Populate dispatch vector table with JMP thunks
;
;  Writes a 6-byte JMP abs.L instruction into each 8-byte vector slot.
;  The JMP targets the RAM copy of each routine.
;  Must be called AFTER VT_INIT_RAM.
;
;  Callers use: JSR VT_VEC_PUTCHAR  (no parens -- JSR to the thunk address)
;
;  Trashes: A0
;******************************************************************************

; JMP abs.L opcode
VT_JMP_OP       EQU     $4EF9

VT_INIT_VECTORS:
        ; VERA_PUTCHAR
        MOVE.W  #VT_JMP_OP,VT_VEC_PUTCHAR
        MOVE.L  #VT_RAM_BASE+(VERA_PUTCHAR-VT_ROM_START),VT_VEC_PUTCHAR+2

        ; VERA_PRINTSTR
        MOVE.W  #VT_JMP_OP,VT_VEC_PRINTSTR
        MOVE.L  #VT_RAM_BASE+(VERA_PRINTSTR-VT_ROM_START),VT_VEC_PRINTSTR+2

        ; VERA_CLRSCR
        MOVE.W  #VT_JMP_OP,VT_VEC_CLRSCR
        MOVE.L  #VT_RAM_BASE+(VERA_CLRSCR-VT_ROM_START),VT_VEC_CLRSCR+2

        ; VERA_CLR_ROW
        MOVE.W  #VT_JMP_OP,VT_VEC_CLR_ROW
        MOVE.L  #VT_RAM_BASE+(VERA_CLR_ROW-VT_ROM_START),VT_VEC_CLR_ROW+2

        ; VERA_HANDLE_LF
        MOVE.W  #VT_JMP_OP,VT_VEC_HANDLE_LF
        MOVE.L  #VT_RAM_BASE+(VERA_HANDLE_LF-VT_ROM_START),VT_VEC_HANDLE_LF+2

        ; VERA_STREAM_START
        MOVE.W  #VT_JMP_OP,VT_VEC_STREAM_START
        MOVE.L  #VT_RAM_BASE+(VERA_STREAM_START-VT_ROM_START),VT_VEC_STREAM_START+2

        ; STREAM_CHAR
        MOVE.W  #VT_JMP_OP,VT_VEC_STREAM_CHAR
        MOVE.L  #VT_RAM_BASE+(STREAM_CHAR-VT_ROM_START),VT_VEC_STREAM_CHAR+2

        ; VERA_GOTOXY
        MOVE.W  #VT_JMP_OP,VT_VEC_GOTOXY
        MOVE.L  #VT_RAM_BASE+(VERA_GOTOXY-VT_ROM_START),VT_VEC_GOTOXY+2

        ; VERA_SET_ATTR
        MOVE.W  #VT_JMP_OP,VT_VEC_SET_ATTR
        MOVE.L  #VT_RAM_BASE+(VERA_SET_ATTR-VT_ROM_START),VT_VEC_SET_ATTR+2

        ; VERA_CURSOR_ON
        MOVE.W  #VT_JMP_OP,VT_VEC_CURSOR_ON
        MOVE.L  #VT_RAM_BASE+(VERA_CURSOR_ON-VT_ROM_START),VT_VEC_CURSOR_ON+2

        ; VERA_CURSOR_OFF
        MOVE.W  #VT_JMP_OP,VT_VEC_CURSOR_OFF
        MOVE.L  #VT_RAM_BASE+(VERA_CURSOR_OFF-VT_ROM_START),VT_VEC_CURSOR_OFF+2

        ; VERA_SCROLL_UP
        MOVE.W  #VT_JMP_OP,VT_VEC_SCROLL_UP
        MOVE.L  #VT_RAM_BASE+(VERA_SCROLL_UP-VT_ROM_START),VT_VEC_SCROLL_UP+2

        ; VERA_SCROLL_DOWN
        MOVE.W  #VT_JMP_OP,VT_VEC_SCROLL_DOWN
        MOVE.L  #VT_RAM_BASE+(VERA_SCROLL_DOWN-VT_ROM_START),VT_VEC_SCROLL_DOWN+2

        ; VERA_CLR_EOL
        MOVE.W  #VT_JMP_OP,VT_VEC_CLR_EOL
        MOVE.L  #VT_RAM_BASE+(VERA_CLR_EOL-VT_ROM_START),VT_VEC_CLR_EOL+2

        ; VERA_CLR_EOS
        MOVE.W  #VT_JMP_OP,VT_VEC_CLR_EOS
        MOVE.L  #VT_RAM_BASE+(VERA_CLR_EOS-VT_ROM_START),VT_VEC_CLR_EOS+2

        ; VERA_INSERT_LINE
        MOVE.W  #VT_JMP_OP,VT_VEC_INSERT_LINE
        MOVE.L  #VT_RAM_BASE+(VERA_INSERT_LINE-VT_ROM_START),VT_VEC_INSERT_LINE+2

        ; VERA_DELETE_LINE
        MOVE.W  #VT_JMP_OP,VT_VEC_DELETE_LINE
        MOVE.L  #VT_RAM_BASE+(VERA_DELETE_LINE-VT_ROM_START),VT_VEC_DELETE_LINE+2

        ; CURSOR_TOGGLE (for VSYNC ISR to call from ROM into RAM)
        MOVE.W  #VT_JMP_OP,VT_VEC_CURSOR_TOGGLE
        MOVE.L  #VT_RAM_BASE+(CURSOR_TOGGLE-VT_ROM_START),VT_VEC_CURSOR_TOGGLE+2

        RTS

;******************************************************************************
;  VERA TEXT ROUTINES BEGIN (VT_ROM_START marks start of copyable block)
;******************************************************************************

VT_ROM_START:

;******************************************************************************
;  VERA_PUTCHAR -- Write character D0.B to VERA console
;
;  Handles:  $0D (CR) -> column = 0
;            $0A (LF) -> call VERA_HANDLE_LF
;            Printable -> write to VRAM, advance column; wrap at col 80
;
;  Trashes: D0-D2, A0
;******************************************************************************

VERA_PUTCHAR:
        MOVEM.L D0-D2/A0,-(SP)
        ORI.W   #$0100,SR              ; block IPL-1 while touching VERA
        MOVE.B  D0,D1

        CMPI.B  #$0D,D1
        BNE.S   .vpc_lf
        BSR     CURSOR_FORCE_OFF        ; hide cursor at old position before CR moves it
        CLR.W   VT_CUR_COL
        ANDI.B  #$DF,VT_FLAGS
        BRA.W   .vpc_done

.vpc_lf:
        CMPI.B  #$0A,D1
        BNE.S   .vpc_bs
        BTST    #5,VT_FLAGS
        BNE.S   .vpc_lf_absorb
        BSR     VERA_HANDLE_LF
        BRA.W   .vpc_done
.vpc_lf_absorb:
        ANDI.B  #$DF,VT_FLAGS
        BRA.W   .vpc_done

.vpc_bs:
        CMPI.B  #$08,D1
        BNE.W   .vpc_char
        BSR     CURSOR_FORCE_OFF        ; hide cursor at old position before BS moves it
        ANDI.B  #$DF,VT_FLAGS
        MOVE.W  VT_CUR_COL,D0
        BNE.S   .vpc_bs_col
        MOVE.W  VT_CUR_ROW,D0
        BEQ.W   .vpc_done
        SUBQ.W  #1,VT_CUR_ROW
        ANDI.W  #$3F,VT_CUR_ROW
        MOVE.W  #79,VT_CUR_COL
        BRA.S   .vpc_bs_erase
.vpc_bs_col:
        SUBQ.W  #1,VT_CUR_COL
.vpc_bs_erase:
        MOVEQ   #0,D2
        MOVE.W  VT_CUR_ROW,D2
        LSL.L   #8,D2
        MOVEQ   #0,D0
        MOVE.W  VT_CUR_COL,D0
        ADD.L   D0,D0
        ADD.L   D0,D2
        MOVE.B  #$00,VT_VERA_BASE+VT_CTRL
        MOVE.B  D2,VT_VERA_BASE+VT_ADDR0_L
        LSR.L   #8,D2
        MOVE.B  D2,VT_VERA_BASE+VT_ADDR0_M
        MOVE.B  #$10,VT_VERA_BASE+VT_ADDR0_H
        MOVE.B  #$20,VT_VERA_BASE+VT_DATA0
        MOVE.B  VT_TEXT_ATTR,VT_VERA_BASE+VT_DATA0
        BRA.W   .vpc_done

.vpc_char:
        BTST    #5,VT_FLAGS
        BEQ.S   .vpc_write
        ANDI.B  #$DF,VT_FLAGS
        MOVE.B  D1,-(SP)
        BSR     VERA_HANDLE_LF
        MOVE.B  (SP)+,D1

.vpc_write:
        MOVEQ   #0,D2
        MOVE.W  VT_CUR_ROW,D2
        LSL.L   #8,D2
        MOVEQ   #0,D0
        MOVE.W  VT_CUR_COL,D0
        ADD.L   D0,D0
        ADD.L   D0,D2

        MOVE.B  #$00,VT_VERA_BASE+VT_CTRL
        MOVE.B  D2,VT_VERA_BASE+VT_ADDR0_L
        LSR.L   #8,D2
        MOVE.B  D2,VT_VERA_BASE+VT_ADDR0_M
        MOVE.B  #$10,VT_VERA_BASE+VT_ADDR0_H

        MOVE.B  D1,VT_VERA_BASE+VT_DATA0
        MOVE.B  VT_TEXT_ATTR,VT_VERA_BASE+VT_DATA0

        ADDQ.W  #1,VT_CUR_COL
        CMPI.W  #80,VT_CUR_COL
        BLT.S   .vpc_done
        ORI.B   #VT_VF_PEND_WRAP,VT_FLAGS

.vpc_done:
        ANDI.W  #$F8FF,SR              ; re-enable IPL-1
        MOVEM.L (SP)+,D0-D2/A0
        RTS

;******************************************************************************
;  VERA_HANDLE_LF -- Advance write row; hardware-scroll if viewport full
;
;  Per docs/vera/vera_console_api.md:
;    V_CUR_COL = 0
;    V_CUR_ROW = (V_CUR_ROW + 1) & 63  (circular wrap)
;    Distance = (V_CUR_ROW - V_SCROLL_ROW) & 63
;    If Distance >= V_VIEW_H: scroll hardware, clear new bottom row
;
;  Trashes: D0, D1
;******************************************************************************

VERA_HANDLE_LF:
        BSR     CURSOR_FORCE_OFF        ; hide cursor at old position before moving
        CLR.W   VT_CUR_COL

        ADDQ.W  #1,VT_CUR_ROW
        ANDI.W  #$3F,VT_CUR_ROW

        MOVE.W  VT_CUR_ROW,D0
        SUB.W   VT_SCROLL_ROW,D0
        ANDI.W  #$3F,D0

        MOVEQ   #0,D1
        MOVE.B  VT_VIEW_H,D1            ; Viewport height from CCB (not hardcoded)
        CMP.W   D1,D0
        BLT.S   .vhlf_noscroll

        ADDQ.W  #1,VT_SCROLL_ROW
        ANDI.W  #$3F,VT_SCROLL_ROW
        MOVE.W  VT_SCROLL_ROW,D0
        LSL.W   #3,D0                   ; row * 8 pixels
        MOVE.B  D0,VT_VERA_BASE+VT_L0_VSCROLL_L
        LSR.W   #8,D0
        MOVE.B  D0,VT_VERA_BASE+VT_L0_VSCROLL_H

        ; Clear the new row at the bottom of the viewport
        MOVE.W  VT_SCROLL_ROW,D1
        MOVEQ   #0,D0
        MOVE.B  VT_VIEW_H,D0
        ADD.W   D0,D1
        SUBQ.W  #1,D1                   ; bottom row = SCROLL_ROW + VIEW_H - 1
        ANDI.W  #$3F,D1
        BSR     VERA_CLR_ROW

.vhlf_noscroll:
        RTS

;******************************************************************************
;  VERA_PRINTSTR -- Print NUL-terminated string (A0) to VERA console
;  Trashes: D0, A0
;******************************************************************************

VERA_PRINTSTR:
        MOVE.B  (A0)+,D0
        BEQ.S   .vps_done
        BSR     VERA_PUTCHAR
        BRA.S   VERA_PRINTSTR
.vps_done:
        RTS

;******************************************************************************
;  VERA_CLRSCR -- Clear entire visible viewport and reset cursor to (0,0)
;  Trashes: D0, D1
;******************************************************************************

VERA_CLRSCR:
        MOVEM.L D0-D1,-(SP)

        ; Reset scroll and cursor
        CLR.W   VT_CUR_COL
        CLR.W   VT_CUR_ROW
        CLR.W   VT_SCROLL_ROW
        ANDI.B  #$DF,VT_FLAGS
        MOVE.B  #$00,VT_VERA_BASE+VT_L0_VSCROLL_L
        MOVE.B  #$00,VT_VERA_BASE+VT_L0_VSCROLL_H
        MOVE.B  #$00,VT_VERA_BASE+VT_L0_VSCROLL_H

        ; Clear all 64 rows
        CLR.W   D1
.vcs_loop:
        BSR     VERA_CLR_ROW
        ADDQ.W  #1,D1
        CMPI.W  #64,D1
        BLT.S   .vcs_loop

        MOVEM.L (SP)+,D0-D1
        RTS

;******************************************************************************
;  VERA_CLR_ROW -- Clear physical row D1.W (0-63) in the tilemap
;
;  Writes 128 space+attribute pairs, covering all 128 columns.
;  (Only 80 are visible, but clearing the full 128 keeps VRAM clean.)
;
;  Trashes: D0
;******************************************************************************

VERA_CLR_ROW:
        MOVEM.L D0-D2,-(SP)
        MOVEQ   #0,D0
        MOVE.W  D1,D0
        LSL.L   #8,D0                   ; row * 256 = VRAM start of row
        MOVE.B  #$00,VT_VERA_BASE+VT_CTRL
        MOVE.B  D0,VT_VERA_BASE+VT_ADDR0_L
        LSR.L   #8,D0
        MOVE.B  D0,VT_VERA_BASE+VT_ADDR0_M
        MOVE.B  #$10,VT_VERA_BASE+VT_ADDR0_H   ; stride=1
        MOVE.W  #127,D0
        MOVE.B  VT_TEXT_ATTR,D2
.vcr_lp:
        MOVE.B  #$20,VT_VERA_BASE+VT_DATA0     ; Space
        MOVE.B  D2,VT_VERA_BASE+VT_DATA0        ; Attribute
        DBRA    D0,.vcr_lp
        MOVEM.L (SP)+,D0-D2
        RTS

;******************************************************************************
;  VERA_STREAM_START -- Set VERA ADDR0 to start of current row, stride=1
;
;  After calling this, use STREAM_CHAR for fast sequential writes.
;  Each STREAM_CHAR call advances the VERA internal address by 2 automatically.
;
;  Trashes: D0, D1
;******************************************************************************

VERA_STREAM_START:
        MOVEQ   #0,D1
        MOVE.W  VT_CUR_ROW,D1
        LSL.L   #8,D1
        MOVEQ   #0,D0
        MOVE.W  VT_CUR_COL,D0
        ADD.L   D0,D0
        ADD.L   D0,D1
        MOVE.B  #$00,VT_VERA_BASE+VT_CTRL
        MOVE.B  D1,VT_VERA_BASE+VT_ADDR0_L
        LSR.L   #8,D1
        MOVE.B  D1,VT_VERA_BASE+VT_ADDR0_M
        MOVE.B  #$10,VT_VERA_BASE+VT_ADDR0_H   ; stride=1
        RTS

;******************************************************************************
;  STREAM_HEX8 -- Write D0.L as 8 hex digits (streaming)
;  Trashes: D0-D2, A0
;******************************************************************************

STREAM_HEX8:
        MOVEM.L D1-D2,-(SP)
        MOVE.L  D0,D1
        MOVEQ   #7,D2
.sh8lp  ROL.L   #4,D1
        MOVE.B  D1,D0
        BSR.S   STREAM_NYBBLE
        DBF     D2,.sh8lp
        MOVEM.L (SP)+,D1-D2
        RTS

;******************************************************************************
;  STREAM_HEX2 -- Write D0.B as 2 hex digits (streaming)
;  Trashes: D0
;******************************************************************************

STREAM_HEX2:
        MOVE.B  D0,-(SP)
        LSR.B   #4,D0
        BSR.S   STREAM_NYBBLE
        MOVE.B  (SP)+,D0
        BSR.S   STREAM_NYBBLE
        RTS

;******************************************************************************
;  STREAM_NYBBLE -- Write low nybble of D0.B as hex digit (falls into STREAM_CHAR)
;******************************************************************************

STREAM_NYBBLE:
        ANDI.L  #$0F,D0
        LEA     VT_HEX_TABLE,A0
        MOVE.B  (A0,D0.W),D0
        ; fall through to STREAM_CHAR

;******************************************************************************
;  STREAM_CHAR -- Write D0.B + current attribute to VERA_DATA0 (streaming)
;
;  Assumes VERA ADDR0 is already set (via VERA_STREAM_START or manually).
;  The VERA auto-increments ADDR0 by 1 after each write, so character byte
;  and attribute byte are written in two consecutive calls.
;
;  Trashes: nothing (D0 is consumed but the char was already in D0)
;******************************************************************************

STREAM_CHAR:
        MOVE.B  D0,VT_VERA_BASE+VT_DATA0        ; ASCII
        MOVE.B  VT_TEXT_ATTR,VT_VERA_BASE+VT_DATA0 ; Attribute
        RTS

;******************************************************************************
;  EXTENDED VERA TEXT API - Stubs
;
;  These stubs are placeholders.  Each returns immediately with RTS.
;  Implement as needed for cursor support, scrolling, and VT100 features.
;
;  All are inside VT_ROM_START..VT_ROM_END so they are copied to RAM and
;  reachable via JMP thunks at VT_VEC_xxx.
;******************************************************************************

;******************************************************************************
;  VERA_GOTOXY -- Move cursor to absolute position
;
;  Input:  D0.W = column (0-79), D1.W = row (0-63, physical tilemap row)
;  Effect: Sets VT_CUR_COL and VT_CUR_ROW, clears pending-wrap flag.
;  Note:   Caller is responsible for translating logical (viewport) row to
;          physical tilemap row if needed (add VT_SCROLL_ROW, mask with 63).
;
;  Trashes: nothing
;******************************************************************************
VERA_GOTOXY:
        BSR     CURSOR_FORCE_OFF        ; hide cursor at old position
        MOVE.W  D0,VT_CUR_COL
        MOVE.W  D1,VT_CUR_ROW
        ANDI.B  #$DF,VT_FLAGS          ; clear pending-wrap flag
        RTS

;******************************************************************************
;  VERA_SET_ATTR -- Set current text attribute
;
;  Input:  D0.B = new attribute byte (upper nibble = bg color 0-15,
;          lower nibble = fg color 0-15)
;  Effect: Stores to VT_TEXT_ATTR in CCB.  Future writes use this attribute.
;
;  Trashes: nothing
;******************************************************************************
VERA_SET_ATTR:
        MOVE.B  D0,VT_TEXT_ATTR
        RTS

;******************************************************************************
;  VERA_CURSOR_ON -- Enable and show blinking cursor
;
;  Delegates to CURSOR_FORCE_ON in vera_cursor.inc.
;  Also sets VT_CURSOR_EN=1 to enable ISR-driven blinking.
;
;  Trashes: D0, D2, A0
;******************************************************************************
VERA_CURSOR_ON:
        MOVE.B  #$01,VT_CURSOR_EN
        BSR     CURSOR_FORCE_ON
        RTS

;******************************************************************************
;  VERA_CURSOR_OFF -- Hide cursor and disable blinking
;
;  Delegates to CURSOR_FORCE_OFF in vera_cursor.inc.
;  Also sets VT_CURSOR_EN=0 to prevent ISR from re-showing cursor.
;
;  Trashes: D0, D2, A0
;******************************************************************************
VERA_CURSOR_OFF:
        BSR     CURSOR_FORCE_OFF
        CLR.B   VT_CURSOR_EN
        RTS

;******************************************************************************
;  VERA_SCROLL_UP -- Scroll viewport up one line (public API)
;
;  Effect: Advances VT_SCROLL_ROW by 1, updates VERA L0_VSCROLL,
;          clears the new bottom row of the viewport.
;          Does NOT change VT_CUR_COL or VT_CUR_ROW.
;
;  Trashes: D0-D2
;******************************************************************************
VERA_SCROLL_UP:
        MOVEM.L D0-D1,-(SP)
        ORI.W   #$0100,SR

        ADDQ.W  #1,VT_SCROLL_ROW
        ANDI.W  #$3F,VT_SCROLL_ROW

        ; Update hardware scroll register
        MOVE.W  VT_SCROLL_ROW,D0
        LSL.W   #3,D0                  ; row * 8 pixels
        MOVE.B  D0,VT_VERA_BASE+VT_L0_VSCROLL_L
        LSR.W   #8,D0
        MOVE.B  D0,VT_VERA_BASE+VT_L0_VSCROLL_H

        ; Clear new bottom row
        MOVE.W  VT_SCROLL_ROW,D1
        MOVEQ   #0,D0
        MOVE.B  VT_VIEW_H,D0
        ADD.W   D0,D1
        SUBQ.W  #1,D1
        ANDI.W  #$3F,D1
        BSR     VERA_CLR_ROW

        ANDI.W  #$F8FF,SR
        MOVEM.L (SP)+,D0-D1
        RTS

;******************************************************************************
;  VERA_SCROLL_DOWN -- Scroll viewport down one line (reverse scroll)
;
;  Effect: Decrements VT_SCROLL_ROW by 1 (wrapping), updates VERA L0_VSCROLL,
;          clears the new top row of the viewport.
;          Does NOT change VT_CUR_COL or VT_CUR_ROW.
;
;  Trashes: D0-D2
;******************************************************************************
VERA_SCROLL_DOWN:
        MOVEM.L D0-D1,-(SP)
        ORI.W   #$0100,SR

        SUBQ.W  #1,VT_SCROLL_ROW
        ANDI.W  #$3F,VT_SCROLL_ROW

        ; Update hardware scroll register
        MOVE.W  VT_SCROLL_ROW,D0
        LSL.W   #3,D0                  ; row * 8 pixels
        MOVE.B  D0,VT_VERA_BASE+VT_L0_VSCROLL_L
        LSR.W   #8,D0
        MOVE.B  D0,VT_VERA_BASE+VT_L0_VSCROLL_H

        ; Clear new top row (= VT_SCROLL_ROW itself)
        MOVE.W  VT_SCROLL_ROW,D1
        BSR     VERA_CLR_ROW

        ANDI.W  #$F8FF,SR
        MOVEM.L (SP)+,D0-D1
        RTS

;******************************************************************************
;  VERA_CLR_EOL -- Clear from cursor to end of current line
;
;  Effect: Writes spaces + current attribute from VT_CUR_COL to column 79
;          on the current row.  Does NOT move the cursor.
;
;  Trashes: D0-D2
;******************************************************************************
VERA_CLR_EOL:
        MOVEM.L D0-D2,-(SP)
        ORI.W   #$0100,SR              ; block IPL-1

        ; Calculate VRAM address: (VT_CUR_ROW * 256) + (VT_CUR_COL * 2)
        MOVEQ   #0,D2
        MOVE.W  VT_CUR_ROW,D2
        LSL.L   #8,D2                  ; row * 256
        MOVEQ   #0,D0
        MOVE.W  VT_CUR_COL,D0
        ADD.L   D0,D0                  ; col * 2
        ADD.L   D0,D2

        ; Set VERA ADDR0
        MOVE.B  #$00,VT_VERA_BASE+VT_CTRL
        MOVE.B  D2,VT_VERA_BASE+VT_ADDR0_L
        LSR.L   #8,D2
        MOVE.B  D2,VT_VERA_BASE+VT_ADDR0_M
        MOVE.B  #$10,VT_VERA_BASE+VT_ADDR0_H   ; stride=1

        ; Write space + attribute for columns VT_CUR_COL through 79
        MOVE.W  #79,D0
        SUB.W   VT_CUR_COL,D0         ; D0 = columns remaining (0-based count)
        BMI.S   .vce_done              ; cursor past col 79 - nothing to clear
        MOVE.B  VT_TEXT_ATTR,D2
.vce_lp:
        MOVE.B  #$20,VT_VERA_BASE+VT_DATA0     ; space
        MOVE.B  D2,VT_VERA_BASE+VT_DATA0        ; attribute
        DBF     D0,.vce_lp

.vce_done:
        ANDI.W  #$F8FF,SR              ; re-enable IPL-1
        MOVEM.L (SP)+,D0-D2
        RTS

;******************************************************************************
;  VERA_CLR_EOS -- Clear from cursor to end of screen
;
;  Effect: Clears from cursor position to end of current line, then clears
;          all lines below to the bottom of the viewport.
;          Does NOT move the cursor.
;
;  Trashes: D0-D2
;******************************************************************************
VERA_CLR_EOS:
        MOVEM.L D0-D2,-(SP)

        ; Clear from cursor to end of current line
        BSR     VERA_CLR_EOL

        ; Clear all rows below cursor to bottom of viewport
        ; Bottom row = (VT_SCROLL_ROW + VT_VIEW_H - 1) & $3F
        MOVE.W  VT_CUR_ROW,D1
        ADDQ.W  #1,D1
        ANDI.W  #$3F,D1               ; D1 = first row to clear (circular)

        MOVE.W  VT_SCROLL_ROW,D2
        MOVEQ   #0,D0
        MOVE.B  VT_VIEW_H,D0
        ADD.W   D0,D2                  ; D2 = VT_SCROLL_ROW + VT_VIEW_H
        ANDI.W  #$3F,D2               ; D2 = one past bottom row (circular)

.vcs_lp:
        CMP.W   D2,D1
        BEQ.S   .vcs_done
        BSR     VERA_CLR_ROW           ; clear physical row D1
        ADDQ.W  #1,D1
        ANDI.W  #$3F,D1
        BRA.S   .vcs_lp

.vcs_done:
        MOVEM.L (SP)+,D0-D2
        RTS

;******************************************************************************
;  VERA_INSERT_LINE -- Insert blank line at cursor row
;
;  Effect: Shifts all rows from cursor row to bottom of viewport down by one.
;          The bottom row is lost (scrolled off).  Cursor row is cleared.
;          Does NOT move the cursor.
;
;  Trashes: D0-D2, A0
;******************************************************************************
VERA_INSERT_LINE:
        RTS

;******************************************************************************
;  VERA_DELETE_LINE -- Delete cursor row, pull lines below up
;
;  Effect: Shifts all rows below cursor row up by one.  A blank line appears
;          at the bottom of the viewport.  Does NOT move the cursor.
;
;  Trashes: D0-D2, A0
;******************************************************************************
VERA_DELETE_LINE:
        RTS

;******************************************************************************
;  CURSOR ROUTINES (inside VT_ROM_START..VT_ROM_END for RAM execution)
;
;  Moved here from vera_cursor.inc so BSR calls from VERA_CURSOR_ON/OFF
;  and the ISR thunk (VT_VEC_CURSOR_TOGGLE) work correctly from RAM.
;
;  All cursor routines clobber: D0, D2, A0
;******************************************************************************

;----------------------------------------------------------------------
;  CURSOR_TOGGLE -- Flip cursor between shown (inverted) and hidden
;  Called by VSYNC ISR via VT_VEC_CURSOR_TOGGLE thunk.
;  Trashes: D0, D2, A0
;----------------------------------------------------------------------
CURSOR_TOGGLE:
        NOT.B   VT_CURSOR_STATE
        TST.B   VT_CURSOR_STATE
        BNE.S   .ct_show
        ; Hide: write normal attribute
        BSR     SET_CURSOR_ADDR
        LEA     VT_VERA_BASE,A0
        MOVE.B  VT_TEXT_ATTR,VT_DATA0(A0)
        RTS
.ct_show:
        ; Show: write inverted attribute
        BSR     SET_CURSOR_ADDR
        LEA     VT_VERA_BASE,A0
        MOVE.B  VT_TEXT_ATTR,D0
        NOT.B   D0
        MOVE.B  D0,VT_DATA0(A0)
        RTS

;----------------------------------------------------------------------
;  CURSOR_FORCE_ON -- Unconditionally show cursor at current position
;  Resets blink timer so cursor stays solid while typing.
;  Trashes: D0, D2, A0
;----------------------------------------------------------------------
CURSOR_FORCE_ON:
        BSR     SET_CURSOR_ADDR
        LEA     VT_VERA_BASE,A0
        MOVE.B  VT_TEXT_ATTR,D0
        NOT.B   D0
        MOVE.B  D0,VT_DATA0(A0)
        MOVE.B  #$FF,VT_CURSOR_STATE
        MOVE.B  #VC_BLINK_RATE,VT_BLINK_TIMER
        RTS

;----------------------------------------------------------------------
;  CURSOR_FORCE_OFF -- Hide cursor if currently shown
;  Trashes: D0, D2, A0
;----------------------------------------------------------------------
CURSOR_FORCE_OFF:
        TST.B   VT_CURSOR_STATE
        BEQ.S   .cfo_ret
        BSR     SET_CURSOR_ADDR
        LEA     VT_VERA_BASE,A0
        MOVE.B  VT_TEXT_ATTR,VT_DATA0(A0)
        CLR.B   VT_CURSOR_STATE
.cfo_ret:
        RTS

;----------------------------------------------------------------------
;  SET_CURSOR_ADDR -- Set VERA ADDR0 to attribute byte at (row, col)
;  Address = (row * 256) + (col * 2) + 1
;  Trashes: D0, D2, A0
;----------------------------------------------------------------------
SET_CURSOR_ADDR:
        MOVEQ   #0,D2
        MOVE.W  VT_CUR_ROW,D2
        LSL.L   #8,D2                  ; row * 256
        MOVEQ   #0,D0
        MOVE.W  VT_CUR_COL,D0
        ADD.L   D0,D0                  ; col * 2
        ADDQ.L  #1,D2                  ; +1 for attribute byte
        ADD.L   D0,D2                  ; final VRAM address
        LEA     VT_VERA_BASE,A0
        MOVE.B  #$00,VT_CTRL(A0)       ; DCSEL=0
        MOVE.B  D2,VT_ADDR0_L(A0)
        LSR.L   #8,D2
        MOVE.B  D2,VT_ADDR0_M(A0)
        MOVE.B  #$10,VT_ADDR0_H(A0)   ; stride=1, bit16=0
        RTS

;******************************************************************************
;  VERA TEXT ROUTINES END (VT_ROM_END marks end of copyable block)
;******************************************************************************

VT_HEX_TABLE:
        DC.B    "0123456789ABCDEF"
        ALIGN   2

VT_ROM_END:

; -- End of vera_text.inc -----------------------------------------------------
