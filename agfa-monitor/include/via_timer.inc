;******************************************************************************
;
;  via_timer.inc -- VIA #1 T1 Fallback Timer (Include File)
;  For AGFA Compugraphic 9000PS / 68020
;
;  Provides a ~60Hz system tick when VERA is not installed.
;  Uses VIA #1 Timer 1 in free-running mode with IPL-4 autovector.
;
;  The TICK_COUNT variable is at the same address as vera_cursor.inc uses,
;  so GETTICKS works identically regardless of tick source.
;
; -- Entry Points --------------------------------------------------------------
;
;  VIA_TIMER_INIT     -- Copy vector table to RAM, install T1 ISR,
;                        configure VIA T1 for ~60Hz, enable interrupts.
;
;  VIA_TIMER_SHUTDOWN -- Disable VIA T1 interrupts.
;
; -- Dependencies --------------------------------------------------------------
;
;  Uses VC_RAM_VBR and VC_TICK_COUNT from vera_cursor.inc.
;  Both files must be INCLUDEd (vera_cursor.inc first).
;
;******************************************************************************

; --- VIA #1 registers ---------------------------------------------------------

VT1_VIA_BASE    EQU     $04000000
VT1_T1CL        EQU     $04             ; T1 Counter Low (also latch low on write)
VT1_T1CH        EQU     $05             ; T1 Counter High (starts timer on write)
VT1_ACR         EQU     $0B             ; Auxiliary Control Register
VT1_IFR         EQU     $0D             ; Interrupt Flag Register
VT1_IER         EQU     $0E             ; Interrupt Enable Register

; --- Timer constant -----------------------------------------------------------
;  VIA clock = 1 MHz.  T1 period = (latch + 2) us.
;  For ~60 Hz: 1000000 / 60 = 16667 us.  Latch = 16665 = $4119.

VT1_T1_LATCH_L  EQU     $19             ; low byte of 16665
VT1_T1_LATCH_H  EQU     $41             ; high byte of 16665


;******************************************************************************
;  VIA_TIMER_INIT -- Set up VIA #1 T1 for ~60Hz ticks
;
;  1. Copy ROM vector table to RAM (same as VERA_IRQ_INIT)
;  2. Install VIA_T1_ISR at IPL-4 autovector (+$070)
;  3. Set VBR to RAM copy
;  4. Initialize tick counter
;  5. Configure VIA T1 free-running, enable T1 interrupt
;  6. Lower IPL to allow level-4 interrupts
;
;  Note: VIA #1 interrupts at IPL level 4 (not level 1 like VERA).
;
;  Trashes: D0, A0, A1
;******************************************************************************

VIA_TIMER_INIT:
        ; 1. Copy 256 longwords (1KB) from ROM vectors to RAM
        LEA     $0,A0
        LEA     VC_RAM_VBR,A1
        MOVE.W  #255,D0
.cpvec  MOVE.L  (A0)+,(A1)+
        DBF     D0,.cpvec

        ; 2. Install T1 handler at IPL-4 autovector ($070)
        MOVE.L  #VIA_T1_ISR,VC_RAM_VBR+$070

        ; 3. Relocate VBR to RAM
        LEA     VC_RAM_VBR,A0
        MOVEC   A0,VBR

        ; 4. Initialize tick counter
        CLR.L   VC_TICK_COUNT

        ; 5. Configure VIA #1 T1 for free-running ~60Hz
        ;    Read ACR, set bit 6 (T1 free-run), preserve other bits
        MOVE.B  VT1_VIA_BASE+VT1_ACR,D0
        ORI.B   #$40,D0                ; bit 6 = T1 free-running mode
        MOVE.B  D0,VT1_VIA_BASE+VT1_ACR

        ; Load T1 latch and start timer
        MOVE.B  #VT1_T1_LATCH_L,VT1_VIA_BASE+VT1_T1CL
        MOVE.B  #VT1_T1_LATCH_H,VT1_VIA_BASE+VT1_T1CH  ; starts timer

        ; Clear T1 interrupt flag, then enable T1 interrupt
        MOVE.B  VT1_VIA_BASE+VT1_T1CL,D0   ; read T1C-L to clear IFR
        MOVE.B  #$C0,VT1_VIA_BASE+VT1_IER  ; set bit 7 (enable) + bit 6 (T1)

        ; 6. Lower IPL to allow level-4 interrupts
        ANDI.W  #$F8FF,SR

        RTS


;******************************************************************************
;  VIA_TIMER_SHUTDOWN -- Disable VIA #1 T1 interrupts
;
;  Trashes: D0
;******************************************************************************

VIA_TIMER_SHUTDOWN:
        ORI.W   #$0700,SR                      ; mask all interrupts
        MOVE.B  #$40,VT1_VIA_BASE+VT1_IER      ; clear bit 6 (T1), bit 7=0 = disable
        MOVE.B  VT1_VIA_BASE+VT1_T1CL,D0       ; read T1C-L to clear any pending IFR
        MOVEQ   #0,D0
        MOVEC   D0,VBR                          ; restore VBR to ROM
        RTS


;******************************************************************************
;  VIA_T1_ISR -- IPL-4 Interrupt Service Routine
;
;  Increments TICK_COUNT, acknowledges T1 interrupt.
;
;  Trashes: nothing (all regs saved/restored)
;******************************************************************************

VIA_T1_ISR:
        MOVEM.L D0,-(SP)
        MOVE.B  VT1_VIA_BASE+VT1_T1CL,D0   ; read T1C-L to clear IFR bit 6
        ADDQ.L  #1,VC_TICK_COUNT
        MOVEM.L (SP)+,D0
        RTE


; -- End of via_timer.inc -----------------------------------------------------
