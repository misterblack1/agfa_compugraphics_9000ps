;******************************************************************************
;
;  vera_cursor.inc -- VERA VSYNC IRQ + Blinking Cursor (Include File)
;  For AGFA Compugraphic 9000PS / 68020
;
;  Provides VSYNC-driven cursor blink and a system tick counter.
;  Ported from the validated standalone test (vera_vsync_irq_test.m68 v6).
;
; -- Entry Points (called from agfa-mon.m68) -----------------------------------
;
;  VERA_IRQ_INIT     -- Copy vector table to RAM, install VSYNC ISR,
;                       enable VERA VSYNC interrupt, lower IPL.
;                       Call AFTER VERA_INIT and VT_INIT_VECTORS.
;
;  VERA_IRQ_SHUTDOWN -- Disable VERA VSYNC interrupt, restore VBR to 0.
;
;  GETTICKS          -- Return TICK_COUNT in D0.L.  API entry at $0424.
;
;  Note: CURSOR_TOGGLE, CURSOR_FORCE_ON, CURSOR_FORCE_OFF, SET_CURSOR_ADDR
;  have been moved to vera_text.inc (inside the VT_ROM_START..VT_ROM_END
;  RAM-copied block).  The ISR calls CURSOR_TOGGLE via VT_VEC_CURSOR_TOGGLE.
;
; -- Critical Rules ------------------------------------------------------------
;
;  1. NEVER read VERA DATA0 in cursor routines (auto-increments ADDR0).
;  2. Track cursor state in VT_CURSOR_STATE - write-only pattern.
;  3. Block IPL-1 during ALL VERA register access (ORI.W #$0100,SR).
;  4. Do NOT block IPL for entire loops - only around VERA operations.
;  5. CURSOR_FORCE_ON always writes (cursor may have moved to new cell).
;  6. SET_CURSOR_ADDR clobbers D0 and D2 - save character in D1 first.
;  7. Hide-before-write, show-after-write pattern in PUTCHAR.
;
; -- Dependencies --------------------------------------------------------------
;
;  Requires vera_text.inc to be included first (provides VT_CCB field EQUs,
;  VT_VERA_BASE, VT_ADDR0_L/M/H, VT_DATA0, VT_CTRL, VT_TEXT_ATTR, etc.)
;
;******************************************************************************

; --- VERA IRQ registers -------------------------------------------------------

VC_VERA_IEN     EQU     $04000026       ; VERA Interrupt Enable
VC_VERA_ISR     EQU     $04000027       ; VERA Interrupt Status

; --- RAM addresses for VBR and tick counter -----------------------------------

VC_RAM_VBR      EQU     $02001000       ; Relocated vector table (1KB)
                                        ; $02001000-$020013FF - NO VARIABLES HERE
VC_TICK_COUNT   EQU     $02001400       ; 32-bit tick counter, ~60Hz

; --- Constants ----------------------------------------------------------------

VC_BLINK_RATE   EQU     30              ; frames between blink toggles (~500ms)


;******************************************************************************
;  VERA_IRQ_INIT -- Set up VSYNC interrupt for cursor blink and tick counter
;
;  1. Copy ROM vector table ($000000) to RAM at VC_RAM_VBR
;  2. Install VSYNC_HANDLER at the IPL-1 autovector slot (+$064)
;  3. Set VBR to RAM copy
;  4. Initialize cursor and tick state
;  5. Enable VERA VSYNC interrupt (IEN bit 0 ONLY - bit 7 is NOT master enable)
;  6. Lower IPL to allow level-1 interrupts
;
;  Trashes: D0, A0, A1
;******************************************************************************

VERA_IRQ_INIT:
        ; 1. Copy 256 longwords (1KB) from ROM vectors to RAM
        LEA     $0,A0
        LEA     VC_RAM_VBR,A1
        MOVE.W  #255,D0
.cpvec  MOVE.L  (A0)+,(A1)+
        DBF     D0,.cpvec

        ; 2. Install VSYNC handler at IPL-1 autovector ($064)
        MOVE.L  #VSYNC_HANDLER,VC_RAM_VBR+$064

        ; 3. Relocate VBR to RAM
        LEA     VC_RAM_VBR,A0
        MOVEC   A0,VBR

        ; 4. Initialize state
        CLR.L   VC_TICK_COUNT
        MOVE.B  #VC_BLINK_RATE,VT_BLINK_TIMER
        CLR.B   VT_CURSOR_STATE
        CLR.B   VT_CURSOR_DIRTY
        MOVE.B  #$01,VT_CURSOR_EN

        ; 5. Clear any pending VERA interrupts, then enable VSYNC only
        MOVE.B  #$07,VC_VERA_ISR       ; clear all flags
        MOVE.B  #$01,VC_VERA_IEN       ; enable VSYNC (bit 0 ONLY)

        ; 6. Lower IPL to allow level-1 interrupts
        ANDI.W  #$F8FF,SR

        RTS


;******************************************************************************
;  VERA_IRQ_SHUTDOWN -- Disable VERA VSYNC interrupt, restore VBR
;
;  Call before exiting to a program that manages its own vectors.
;
;  Trashes: D0
;******************************************************************************

VERA_IRQ_SHUTDOWN:
        ORI.W   #$0700,SR              ; mask all interrupts
        MOVE.B  #$00,VC_VERA_IEN       ; disable VERA interrupts
        MOVE.B  #$07,VC_VERA_ISR       ; clear pending flags
        MOVEQ   #0,D0
        MOVEC   D0,VBR                 ; restore VBR to ROM
        RTS


;******************************************************************************
;  VSYNC_HANDLER -- IPL-1 Interrupt Service Routine
;
;  Called ~60 times per second by VERA VSYNC.
;  Increments TICK_COUNT, decrements blink timer, toggles cursor on expiry.
;
;  Trashes: nothing (all regs saved/restored)
;******************************************************************************

VSYNC_HANDLER:
        MOVEM.L D0-D2/A0,-(SP)

        ; Check if this is actually a VSYNC interrupt
        MOVE.B  VC_VERA_ISR,D0
        BTST    #0,D0
        BEQ.S   .vh_spurious

        ; Acknowledge VSYNC
        MOVE.B  #$01,VC_VERA_ISR

        ; Increment system tick counter (always, regardless of busy state)
        ADDQ.L  #1,VC_TICK_COUNT

        ; Skip cursor if disabled
        TST.B   VT_CURSOR_EN
        BEQ.S   .vh_exit                ; cursor disabled

        ; Check if PUTCHAR flagged cursor for immediate redraw
        TST.B   VT_CURSOR_DIRTY
        BEQ.S   .vh_blink               ; not dirty - normal blink logic
        CLR.B   VT_CURSOR_DIRTY         ; clear the flag
        JSR     VT_VEC_CURSOR_ON        ; force cursor on + reset blink timer
        BRA.S   .vh_exit

        ; Normal cursor blink: decrement timer, toggle on expiry
.vh_blink:
        SUBQ.B  #1,VT_BLINK_TIMER
        BNE.S   .vh_exit
        MOVE.B  #VC_BLINK_RATE,VT_BLINK_TIMER
        JSR     VT_VEC_CURSOR_TOGGLE    ; call into RAM-copied block via thunk

.vh_exit:
        MOVEM.L (SP)+,D0-D2/A0
        RTE

.vh_spurious:
        ; Clear all ISR flags to prevent stuck interrupts
        MOVE.B  #$07,VC_VERA_ISR
        MOVEM.L (SP)+,D0-D2/A0
        RTE


;******************************************************************************
;  Cursor routines (CURSOR_TOGGLE, CURSOR_FORCE_ON, CURSOR_FORCE_OFF,
;  SET_CURSOR_ADDR) have been moved to vera_text.inc inside the
;  VT_ROM_START..VT_ROM_END block so they run from RAM.
;  The ISR reaches CURSOR_TOGGLE via JMP thunk VT_VEC_CURSOR_TOGGLE.
;******************************************************************************


;******************************************************************************
;  GETTICKS -- Return system tick count in D0.L
;
;  API entry point at $0424.  ~60Hz regardless of tick source
;  (VERA VSYNC or VIA T1 fallback).
;
;  Returns: D0.L = current tick count
;  Trashes: D0 only
;******************************************************************************

GETTICKS:
        MOVE.L  VC_TICK_COUNT,D0
        RTS


; -- End of vera_cursor.inc ---------------------------------------------------
