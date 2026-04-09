# Agfa 9000PS - 68020 Interrupt Map

All interrupts use **autovector** mode. The board asserts VPA during INTACK for all tested devices.

| IPL Level | Source | Status | Notes |
|-----------|--------|--------|-------|
| 1 | R6522 VIA #2 /IRQ | **Confirmed** | VERA is using this socket, so it is on IPL 1 |
| 2 | SCSI Controller | **Confirmed** | Asserted whenever SCSI controller is accessed |
| 3 | Unknown | Likely unused | IRQ monitor: 0 hits in 10 s. No known device candidate. |
| 4 | R6522 VIA #1 /IRQ | **Confirmed** | T1 free-running, 0x20 hits/~1.6 s. **AGFA-MON use:** When VERA is **not** installed, `VIA_TIMER_INIT` configures VIA #1 T1 for ~60 Hz at IPL-4 (autovector $070). The T1 ISR increments `TICK_COUNT` at `$02001400`, providing the same time base as the VERA VSYNC ISR in VERA-less configurations. |
| 5 | Unknown | Likely unused | IRQ monitor: 0 hits in 10 s. No known device candidate. |
| 6 | Z85C30 SCC /INT | **Confirmed** | NV=1 autovector; TX-empty trigger; 1 hit then ISR self-disables |
| 7 | NMI | Untested | Non-maskable; source unknown |

Centronics port may be using one of the unused IRQs, but this isn't known at this time.

