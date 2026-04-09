;******************************************************************************
;
;  vera_init.inc - VERA Hardware Initialization (Include File)
;  For AGFA Compugraphic 9000PS  /  68020
;
;  This file is designed to be INCLUDEd into a parent source file.
;  The parent must define the following EQU BEFORE the INCLUDE:
;
;    CONFIG_BUF  EQU  <address>   ; 40-byte RAM working copy of NVRAM
;                                 ; Must be populated by LOAD_CONFIG first.
;
;  Standalone test:  CONFIG_BUF = $02000500
;  ROM integration:  CONFIG_BUF = $02004096
;
; ── Entry Points ─────────────────────────────────────────────────────────────
;
;  VERA_DETECT  - Check if VERA hardware is installed.
;                  D0.B returns: 0 = absent (all $FF), 1 = present
;                  Does NOT modify any VERA registers.
;
;  VERA_INIT    - Initialize VERA using settings from CONFIG_BUF.
;                  Call LOAD_CONFIG before calling this.
;                  D0.B returns: 0 = success, 1 = NVRAM bad (used defaults)
;                  On success: CCB initialized, font+palette loaded, Layer 0
;                  configured, tilemap cleared, banner printed to VERA.
;
;  VERA_PUTCHAR - Write single character (D0.B) to VERA console.
;                  Handles CR/LF, scrolling, word wrap (at col 70).
;                  Caller must have called VERA_INIT successfully first.
;
;  VERA_PRINTSTR - Print NUL-terminated string (A0) to VERA console.
;
; ── VERA Hardware ─────────────────────────────────────────────────────────────
;
;  VERA registers base: $04000020
;
;  Detection: Read 8 bytes from $04000020.
;    All $FF → VERA absent (bus returns open-bus value)
;    Any non-$FF → VERA present (chip drives the bus)
;
; ── CCB Location ─────────────────────────────────────────────────────────────
;
;  Console Control Block at $02000300 (defined in memory map).
;  Initialized by VERA_INIT; checked by VERA_PUTCHAR via V_MAGIC.
;
;******************************************************************************

; ─── VERA Hardware Registers (offset from VERA_BASE) ─────────────────────────

VI_VERA_BASE        EQU     $04000020
VI_ADDR0_L          EQU     $00
VI_ADDR0_M          EQU     $01
VI_ADDR0_H          EQU     $02
VI_DATA0            EQU     $03
VI_CTRL             EQU     $05
VI_DC_VIDEO         EQU     $09
VI_DC_HSCALE        EQU     $0A
VI_DC_VSCALE        EQU     $0B
VI_DC_BORDER        EQU     $0C
VI_L0_CONFIG        EQU     $0D
VI_L0_MAPBASE       EQU     $0E
VI_L0_TILEBASE      EQU     $0F
VI_L0_VSCROLL_L     EQU     $12
VI_L0_VSCROLL_H     EQU     $13

; ─── Console Control Block (CCB) at $02000300 ─────────────────────────────────

VI_CCB_BASE         EQU     $02000300
VI_V_CUR_COL        EQU     VI_CCB_BASE+0   ; Word  current cursor column (0-79)
VI_V_CUR_ROW        EQU     VI_CCB_BASE+2   ; Word  current physical write row (0-63)
VI_V_SCROLL_ROW     EQU     VI_CCB_BASE+4   ; Word  physical row at top of display (0-63)
VI_V_MAGIC          EQU     VI_CCB_BASE+6   ; Word  $A55A = initialized
VI_V_TEXT_ATTR      EQU     VI_CCB_BASE+8   ; Byte  character color attribute
VI_V_VIEW_H         EQU     VI_CCB_BASE+9   ; Byte  visible rows (from NVRAM)
VI_V_MAP_H          EQU     VI_CCB_BASE+10  ; Byte  tilemap height (always 64)
VI_V_FLAGS          EQU     VI_CCB_BASE+11  ; Byte  [7]=scroll [6]=wrap

VI_MAGIC_VAL        EQU     $A55A

; ─── NVRAM offsets into CONFIG_BUF ───────────────────────────────────────────

VI_NV_DC_VIDEO      EQU     3
VI_NV_TERM_HEIGHT   EQU     9
VI_NV_DC_HSCALE     EQU     10
VI_NV_DC_VSCALE     EQU     11
VI_NV_DC_VSTART     EQU     12
VI_NV_DC_VSTOP      EQU     13
VI_NV_DC_BORDER     EQU     14
VI_NV_TEXT_ATTR     EQU     15

; ─── Monitor serial API ───────────────────────────────────────────────────────

VI_MON_PUTCHAR      EQU     $0406

;******************************************************************************
;  VERA_DETECT - Check if VERA hardware is installed
;
;  Reads 8 bytes from the VERA register window ($04000020).
;  If ALL bytes are $FF, VERA is absent (open bus).
;  If ANY byte differs from $FF, VERA is present.
;
;  Returns: D0.B = 0 absent, 1 present
;  Trashes: D0, D1, A0
;******************************************************************************

VERA_DETECT:
        MOVEA.L #VI_VERA_BASE,A0
        MOVEQ   #8-1,D1
.vdchk  MOVE.B  (A0)+,D0
        CMPI.B  #$FF,D0
        BNE.S   .vdpres         ; Found non-FF byte - VERA is here
        DBF     D1,.vdchk
        ; All $FF - VERA absent
        MOVEQ   #0,D0
        RTS
.vdpres:
        MOVEQ   #1,D0
        RTS

;******************************************************************************
;  VERA_INIT - Initialize VERA from CONFIG_BUF settings
;
;  Assumes CONFIG_BUF has been populated by LOAD_CONFIG.
;  Does NOT call VERA_DETECT - caller is responsible for detecting first.
;
;  Sequence:
;    1. Initialize CCB
;    2. Soft-reset VERA
;    3. Program Display Composer from CONFIG_BUF
;    4. Upload palette
;    5. Upload font
;    6. Configure Layer 0
;    7. Clear tilemap
;    8. Print banner to VERA
;
;  Returns: D0.B = 0 (always succeeds if VERA is present)
;  Trashes: D0-D2, A0-A1
;******************************************************************************

VERA_INIT:
        MOVEM.L D0-D4/A0-A1,-(SP)

        ; 1. INITIALIZE CCB
        MOVE.W  #0,VI_V_CUR_COL
        MOVE.W  #0,VI_V_CUR_ROW
        MOVE.W  #0,VI_V_SCROLL_ROW
        MOVE.B  CONFIG_BUF+VI_NV_TEXT_ATTR,VI_V_TEXT_ATTR
        MOVE.B  CONFIG_BUF+VI_NV_TERM_HEIGHT,VI_V_VIEW_H
        MOVE.B  #64,VI_V_MAP_H
        MOVE.B  #$C0,VI_V_FLAGS         ; Scroll enable, wrap enable
        MOVE.W  #VI_MAGIC_VAL,VI_V_MAGIC

        ; 2. SOFT RESET VERA
        MOVE.B  #$80,VI_VERA_BASE+VI_CTRL
        MOVE.L  #$3FFFF,D1
.vrst1  SUBQ.L  #1,D1
        BNE.S   .vrst1
        MOVE.B  #$00,VI_VERA_BASE+VI_CTRL
        MOVE.L  #$3FFFF,D1
.vrst2  SUBQ.L  #1,D1
        BNE.S   .vrst2

        ; 3. DISPLAY COMPOSER - DCSEL=0 (scale registers)
        MOVE.B  #$00,VI_VERA_BASE+VI_CTRL
        MOVE.B  CONFIG_BUF+VI_NV_DC_HSCALE,VI_VERA_BASE+VI_DC_HSCALE
        MOVE.B  CONFIG_BUF+VI_NV_DC_VSCALE,VI_VERA_BASE+VI_DC_VSCALE
        MOVE.B  CONFIG_BUF+VI_NV_DC_BORDER,VI_VERA_BASE+VI_DC_BORDER

        ; DCSEL=1 (H/V start/stop)
        MOVE.B  #$02,VI_VERA_BASE+VI_CTRL
        MOVE.B  #0,VI_VERA_BASE+VI_DC_VIDEO     ; HSTART = 0
        MOVE.B  #160,VI_VERA_BASE+VI_DC_HSCALE  ; HSTOP = 160 (640px)
        MOVE.B  CONFIG_BUF+VI_NV_DC_VSTART,VI_VERA_BASE+VI_DC_VSCALE
        MOVE.B  CONFIG_BUF+VI_NV_DC_VSTOP,VI_VERA_BASE+VI_DC_BORDER

        ; DCSEL=0, set video mode (with Layer 0 enable bit forced on)
        MOVE.B  #$00,VI_VERA_BASE+VI_CTRL
        MOVE.B  CONFIG_BUF+VI_NV_DC_VIDEO,D0
        ORI.B   #$10,D0                         ; Force Layer 0 enable
        MOVE.B  D0,VI_VERA_BASE+VI_DC_VIDEO

        ; 4. UPLOAD PALETTE
        MOVE.B  #$00,VI_VERA_BASE+VI_ADDR0_L
        MOVE.B  #$FA,VI_VERA_BASE+VI_ADDR0_M
        MOVE.B  #$11,VI_VERA_BASE+VI_ADDR0_H    ; VRAM $1FA00, stride=1
        LEA     VI_PAL_DATA,A1
        MOVE.W  #511,D1
.vpal   MOVE.B  (A1)+,VI_VERA_BASE+VI_DATA0
        DBRA    D1,.vpal

        ; 5. UPLOAD FONT
        MOVE.B  #$00,VI_VERA_BASE+VI_ADDR0_L
        MOVE.B  #$F0,VI_VERA_BASE+VI_ADDR0_M
        MOVE.B  #$10,VI_VERA_BASE+VI_ADDR0_H    ; VRAM $0F000, stride=1
        LEA     VI_FONT_DATA,A1
        MOVE.W  #2047,D1
.vfont  MOVE.B  (A1)+,VI_VERA_BASE+VI_DATA0
        DBRA    D1,.vfont

        ; 6. LAYER 0 SETUP
        MOVE.B  #$60,VI_VERA_BASE+VI_L0_CONFIG  ; 128x64 map, 1-bit tile
        MOVE.B  #$00,VI_VERA_BASE+VI_L0_MAPBASE ; Map at VRAM $00000
        MOVE.B  #$78,VI_VERA_BASE+VI_L0_TILEBASE; Tiles at VRAM $0F000

        ; 7. CLEAR TILEMAP (8192 char+attr pairs)
        MOVE.B  #$00,VI_VERA_BASE+VI_ADDR0_L
        MOVE.B  #$00,VI_VERA_BASE+VI_ADDR0_M
        MOVE.B  #$10,VI_VERA_BASE+VI_ADDR0_H    ; stride=1
        MOVE.W  #8191,D1
        MOVE.B  VI_V_TEXT_ATTR,D2
.vclr   MOVE.B  #$20,VI_VERA_BASE+VI_DATA0      ; Space
        MOVE.B  D2,VI_VERA_BASE+VI_DATA0         ; Attribute
        DBRA    D1,.vclr

        ; 8. WRITE SPLASH SCREEN TO VERA TILEMAP (direct VRAM, no text API needed)
        ;
        ;    All text starts at column 10 (VRAM offset +$14 per row).
        ;    Row 5:   "AGFA COMPUGRAPHIC 9000PS"
        ;    Row 6:   "========================"
        ;    Row 8:   "VERA detected and fully configured from NVRAM."
        ;    Row 10+: All font characters $20-$FF, 60 per row (cols 10-69)
        ;             224 chars / 60 per row = 4 rows (rows 10-13)
        ;
        ;    After splash: VI_V_CUR_ROW set to D3+2 so new text starts below.
        ;
        ;    VRAM address: (row * 256) + (col * 2)
        ;    Col 10 start = byte offset 20 = $14 within each row.

        MOVE.B  VI_V_TEXT_ATTR,D2       ; D2 = attribute byte for all writes

        ; Row 5 col 10 (VRAM $00514)
        MOVE.B  #$00,VI_VERA_BASE+VI_CTRL
        MOVE.B  #$14,VI_VERA_BASE+VI_ADDR0_L
        MOVE.B  #$05,VI_VERA_BASE+VI_ADDR0_M
        MOVE.B  #$10,VI_VERA_BASE+VI_ADDR0_H    ; stride=1
        LEA     VI_STR_VERA_L0,A1
.vban0  MOVE.B  (A1)+,D0
        BEQ.S   .vban0d
        MOVE.B  D0,VI_VERA_BASE+VI_DATA0
        MOVE.B  D2,VI_VERA_BASE+VI_DATA0
        BRA.S   .vban0
.vban0d:

        ; Row 6 col 10 (VRAM $00614)
        MOVE.B  #$00,VI_VERA_BASE+VI_CTRL
        MOVE.B  #$14,VI_VERA_BASE+VI_ADDR0_L
        MOVE.B  #$06,VI_VERA_BASE+VI_ADDR0_M
        MOVE.B  #$10,VI_VERA_BASE+VI_ADDR0_H
        LEA     VI_STR_VERA_L1,A1
.vban1  MOVE.B  (A1)+,D0
        BEQ.S   .vban1d
        MOVE.B  D0,VI_VERA_BASE+VI_DATA0
        MOVE.B  D2,VI_VERA_BASE+VI_DATA0
        BRA.S   .vban1
.vban1d:

        ; Row 8 col 10 (VRAM $00814)
        MOVE.B  #$00,VI_VERA_BASE+VI_CTRL
        MOVE.B  #$14,VI_VERA_BASE+VI_ADDR0_L
        MOVE.B  #$08,VI_VERA_BASE+VI_ADDR0_M
        MOVE.B  #$10,VI_VERA_BASE+VI_ADDR0_H
        LEA     VI_STR_VERA_L2,A1
.vban2  MOVE.B  (A1)+,D0
        BEQ.S   .vban2d
        MOVE.B  D0,VI_VERA_BASE+VI_DATA0
        MOVE.B  D2,VI_VERA_BASE+VI_DATA0
        BRA.S   .vban2
.vban2d:

        ; Character table: $20-$FF, 60 chars per row, cols 10-69
        ; Starting at row 10. D3 = current row, D4 = col counter (DBF).
        MOVEQ   #10,D3                  ; start at tilemap row 10
        MOVEQ   #$20,D0                 ; first char to print ($20 = space)

.vcht_row:
        ; VRAM address for this row, col 10: (D3 * 256) + 20
        MOVE.L  D3,D1
        LSL.L   #8,D1                   ; row * 256
        ADDI.L  #20,D1                  ; + col 10 (10 * 2 bytes)
        MOVE.B  #$00,VI_VERA_BASE+VI_CTRL
        MOVE.B  D1,VI_VERA_BASE+VI_ADDR0_L
        LSR.L   #8,D1
        MOVE.B  D1,VI_VERA_BASE+VI_ADDR0_M
        MOVE.B  #$10,VI_VERA_BASE+VI_ADDR0_H    ; stride=1

        MOVEQ   #60-1,D4               ; 60 chars per row
.vcht_col:
        MOVE.B  D0,VI_VERA_BASE+VI_DATA0        ; char
        MOVE.B  D2,VI_VERA_BASE+VI_DATA0        ; attribute
        ADDQ.W  #1,D0
        CMPI.W  #$100,D0               ; past $FF?
        BEQ.S   .vcht_done
        DBF     D4,.vcht_col

        ADDQ.L  #1,D3                  ; next row
        BRA.S   .vcht_row

.vcht_done:
        ; Position cursor two rows below the last splash row so new text
        ; (from vera_text_test or any subsequent output) starts below splash.
        ADDQ.L  #2,D3
        MOVE.W  D3,VI_V_CUR_ROW
        MOVE.W  #0,VI_V_CUR_COL

        ; 9. PRINT INIT STATUS TO SERIAL
        LEA     VI_STR_BANNER,A0
        BSR     VI_PRINTSTR_SERIAL

        MOVEM.L (SP)+,D0-D4/A0-A1
        MOVEQ   #0,D0                            ; Return success
        RTS

;******************************************************************************
;  VI_PRINTSTR_SERIAL - Print NUL-terminated string at A0 to serial only
;  Used for init status messages on serial before VERA is running.
;  Trashes: D0, A0
;******************************************************************************

VI_PRINTSTR_SERIAL:
        MOVE.B  (A0)+,D0
        BEQ.S   .vipdone
        JSR     VI_MON_PUTCHAR
        BRA.S   VI_PRINTSTR_SERIAL
.vipdone:
        RTS

;******************************************************************************
;  String Constants
;******************************************************************************

VI_STR_BANNER:
        DC.B    "AGFA COMPUGRAPHIC 9000PS",$0D,$0A
        DC.B    "========================",$0D,$0A,$0D,$0A
        DC.B    "AGFA-MON Ready.",$0D,$0A,0
        ALIGN   2

; VERA tilemap splash lines (NUL-terminated, no CR/LF)
VI_STR_VERA_L0:
        DC.B    "AGFA COMPUGRAPHIC 9000PS",0
        ALIGN   2
VI_STR_VERA_L1:
        DC.B    "========================",0
        ALIGN   2
VI_STR_VERA_L2:
        DC.B    "VERA detected and fully configured from NVRAM.",0
        ALIGN   2

;******************************************************************************
;  Data: Palette and Font
;******************************************************************************

        ALIGN   4
VI_PAL_DATA:
    DC.B $00,$00,$FF,$0F,$00,$08,$FE,$0A,$4C,$0C,$C5,$00,$0A,$00,$E7,$0E,$85,$0D,$40,$06,$77,$0F,$33,$03,$77,$07,$F6,$0A,$8F,$00,$BB,$0B
    DC.B $00,$00,$11,$01,$22,$02,$33,$03,$44,$04,$55,$05,$66,$06,$77,$07,$88,$08,$99,$09,$AA,$0A,$BB,$0B,$CC,$0C,$DD,$0D,$EE,$0E,$FF,$0F
    DC.B $11,$02,$33,$04,$44,$06,$66,$08,$88,$0A,$99,$0C,$BB,$0F,$11,$02,$22,$04,$33,$06,$44,$08,$55,$0A,$66,$0C,$77,$0F,$00,$02,$11,$04
    DC.B $11,$06,$22,$08,$22,$0A,$33,$0C,$33,$0F,$00,$02,$00,$04,$00,$06,$00,$08,$00,$0A,$00,$0C,$00,$0F,$21,$02,$43,$04,$64,$06,$86,$08
    DC.B $A8,$0A,$C9,$0C,$EB,$0F,$11,$02,$32,$04,$53,$06,$74,$08,$95,$0A,$B6,$0C,$D7,$0F,$10,$02,$31,$04,$51,$06,$62,$08,$82,$0A,$A3,$0C
    DC.B $C3,$0F,$10,$02,$30,$04,$40,$06,$60,$08,$80,$0A,$90,$0C,$B0,$0F,$21,$01,$43,$03,$64,$05,$86,$07,$A8,$09,$C9,$0B,$FB,$0D,$21,$01
    DC.B $42,$03,$63,$04,$84,$06,$A5,$08,$C6,$09,$F7,$0B,$20,$01,$41,$02,$61,$04,$82,$05,$A2,$06,$C3,$08,$F3,$09,$20,$01,$40,$02,$60,$03
    DC.B $80,$04,$A0,$05,$C0,$06,$F0,$07,$21,$01,$43,$03,$65,$04,$86,$06,$A8,$08,$CA,$09,$FC,$0B,$21,$01,$42,$02,$64,$03,$85,$04,$A6,$05
    DC.B $C8,$06,$F9,$07,$20,$00,$41,$01,$62,$01,$83,$02,$A4,$02,$C5,$03,$F6,$03,$20,$00,$41,$00,$61,$00,$82,$00,$A2,$00,$C3,$00,$F3,$00
    DC.B $22,$01,$44,$03,$66,$04,$88,$06,$AA,$08,$CC,$09,$FF,$0B,$22,$01,$44,$02,$66,$03,$88,$04,$AA,$05,$CC,$06,$FF,$07,$22,$00,$44,$01
    DC.B $66,$01,$88,$02,$AA,$02,$CC,$03,$FF,$03,$22,$00,$44,$00,$66,$00,$88,$00,$AA,$00,$CC,$00,$FF,$00,$12,$01,$34,$03,$56,$04,$68,$06
    DC.B $8A,$08,$AC,$09,$CF,$0B,$12,$01,$24,$02,$46,$03,$58,$04,$6A,$05,$8C,$06,$9F,$07,$02,$00,$14,$01,$26,$01,$38,$02,$4A,$02,$5C,$03
    DC.B $6F,$03,$02,$00,$14,$00,$16,$00,$28,$00,$2A,$00,$3C,$00,$3F,$00,$12,$01,$34,$03,$46,$05,$68,$07,$8A,$09,$9C,$0B,$BF,$0D,$12,$01
    DC.B $24,$03,$36,$04,$48,$06,$5A,$08,$6C,$09,$7F,$0B,$02,$01,$14,$02,$16,$04,$28,$05,$2A,$06,$3C,$08,$3F,$09,$02,$01,$04,$02,$06,$03
    DC.B $08,$04,$0A,$05,$0C,$06,$0F,$07,$12,$02,$34,$04,$46,$06,$68,$08,$8A,$0A,$9C,$0C,$BE,$0F,$11,$02,$23,$04,$35,$06,$47,$08,$59,$0A
    DC.B $6B,$0C,$7D,$0F,$01,$02,$13,$04,$15,$06,$26,$08,$28,$0A,$3A,$0C,$3C,$0F,$01,$02,$03,$04,$04,$06,$06,$08,$08,$0A,$09,$0C,$0B,$0F

        ALIGN   4
VI_FONT_DATA:
        INCBIN  "agfa-monitor/vera/font_code437_8x8.bin"

; ── End of vera_init.inc ─────────────────────────────────────────────────────
