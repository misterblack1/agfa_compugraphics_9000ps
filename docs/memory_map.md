# Agfa 9000PS - Memory Map

Derived from ROM disassembly combined with hardware bus probing.

## Memory Map

Summary of all confirmed addresses based on bus-probe.m68, phase0_ram_mirrors.m68 v1/v2, and phase1_eeprom_probe.m68 hardware results:

| Address Range | Device | How Confirmed |
|--------------|--------|---------------|
| $00000000-$0009FFFF | ROM (5 banks, 128 KB each) | Bus probe, ROM signatures match |
| $00100000-$01FFFFFF | ROM mirror | Phase 1: reads identical to $00000000 |
| $02000000-$023FFFFF | RAM (4 MB installed) | Sentinel write/read, RAM test |
| $02800000-$02BFFFFF | RAM mirror | Phase 0: sentinel $DEADBEEF reads back |
| $03000000-$033FFFFF | RAM mirror | Phase 0: sentinel $CAFEBABE reads back |
| $03800000-$03BFFFFF | RAM mirror | Phase 0: sentinel reads back |
| $02400000, $02C00000, $03400000, $03C00000 | NOT mirrors (MK4501 FIFO / open bus) | Phase 0: different content, not writable |
| $04000000-$04FFFFFF | VIA mirror (all of it) | Phase 1: slow scan due to wait states throughout |
| $05000000-$05FFFFFF | SCSI mirror (all of it) | Phase 1: SCSI registers mirror throughout |
| $06000000-$06FFFFFF | Centronics parallel port area | Phase 1: reads $FF (74LS374 undriven); freeze holes where A21=1,A20=0 |
| $07000000-$0700FFFF | Z85C30 SCC (primary) | Phase 1: reads `54 00 50 0D` (SCC status pattern) |
| $07100000-$071FFFFF | Xicor X2804A EEPROM (primary) | Phase 1: distinctive `01 4A`/`01 49` wear-level pattern; write confirmed |
| $071F0000-$071F01FF | Xicor EEPROM mirror (within block) | Phase 1: identical dump to $07100000 |
| $07200000-$073FFFFF | Unmapped - Freeze | Phase 1: no DSACK |
| $07400000-$074FFFFF | SCC mirror | Phase 1 |
| $07500000-$075FFFFF | Xicor mirror | Phase 1 |
| $07600000 | Freeze | Phase 1 |
| $07700000 | Open bus ($FF) | Phase 1: reads FF, function unknown |
| $07800000 | SCC mirror | Phase 1 |
| $07900000 | Xicor mirror | Phase 1 |
| $07A00000-$07FFFFFF | Mirrors of $07200000-$07700000 | Phase 1 |
| $08000000+ | Freeze - no DSACK | Phase 0/1: hangs. Usable space ends at $07FFFFFF |

**Note on RAM mirrors:** Phase 0 v2 confirmed that some mirror addresses respond differently to MOVE.L vs MOVE.B access (PAL uses SIZ0/SIZ1 in decode). $02C00000 returns MOVE.L=$04122643 but byte reads give $00000572. $03000000/$03800000 are MK4501 FIFOs, not RAM mirrors - confirmed by scope: /W toggles on all four MK4501N chips on access.

## $06xxxxxx Parallel Port / Rendering Pipeline Analysis

### Address Decode Pattern (confirmed Phase 1 probe, 2026-03-31)

The entire $06000000-$06FFFFFF window is the Centronics parallel input port area and rendering pipeline. Within this space, addresses respond or freeze based on address bits A21 and A20:

| A21 | A20 | Behavior | Example addresses |
|-----|-----|----------|-------------------|
| 0   | 0   | Responds (all $FF) | $06000000, $06400000, $06800000, $06C00000 |
| 0   | 1   | Responds (first byte $06, rest $FF) | $06100000, $06500000, $06900000, $06D00000 |
| 1   | 1   | Responds ($06+$FF pattern) | $06300000, $06700000, $06B00000, $06F00000 |
| 1   | 0   | **FREEZE** (no DSACK) | $06200000, $06600000, $06A00000, $06E00000 |

The "first byte = $06" pattern at certain addresses is likely an address bus artifact: when the device doesn't drive all 32 data bus bits, the upper byte (A23:A16 = $06) bleeds onto the high byte of the data bus from capacitive remnants or bus pre-charge. The FF bytes are pull-ups on undriven data lines.

### Key Observations

- RAM mirrors at 8 MB stride (A23 not used in RAM CS decode)
- **$06000000 → parallel port:** reads toggle /OE (pin 1) on the LS374 directly adjacent to the parallel port connector. This is the strongest chip select evidence yet for any of these addresses.
- **$03000000 / $03800000 → MK4501N FIFOs confirmed:** reads AND writes toggle /W on all four MK4501N chips simultaneously. These are the I/O board FIFO data paths.
- **$02C00000 and $06100000 → LS244 + U309 LS245 pair north of XICOR:** both addresses toggle /OE on a chip previously misidentified as LS374 - confirmed to be an **LS244** (unidirectional octal buffer). It works in conjunction with **U309**, an LS245 (bidirectional transceiver) directly adjacent to it. This LS244+LS245 pair also activates during **SCC and SCSI accesses**, which means it is NOT uniquely selected by $02C00000/$06100000 - it is part of the peripheral bus buffer infrastructure that gates all CPU↔peripheral traffic (SCC, SCSI, and possibly others). The activation seen at those two addresses is likely a side-effect of peripheral bus enable, not a unique decode. Downstream connection still unidentified.
