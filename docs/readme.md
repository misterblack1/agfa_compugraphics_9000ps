# Agfa Compugraphic 9000PS - System Overview

## System
- **System:** Agfa Compugraphic 9000PS
- **Original Purpose:** Raster Image Processor (PostScript Level 1 RIP) for Agfa imagesetters. Acted as "printer" to host computers, which would connect via RS-232, RS-422 (Appletalk) and Centronics Parallel.
- **Original Firmware Name:** Atlas Monitor (boot/diagnostic firmware)
- **Board Label:** REVISION II, part number 87506-502
- **Original Firmware Version:** PostScript Version 49.3 (Adobe PostScript Level 1, copyright 1984-1988). Version displayed on RS-232 after typing `Executive` at the prompt.
- **Date Code:** 1986, Week 45 (November 1986)

---

## Processor
- **CPU:** Motorola 68020 running at 16 MHz
- **Optional FPU:** Motorola 68881 (not installed on this unit; original firmware detects FPU presence at boot via routine at ROM 0x0051C)

### 68020 Bus Timing & Wait States

The 68020 (unlike the 68000) uses **DSACK0 and DSACK1** (Data Strobe Acknowledge) instead of a single /DTACK line. The PAL at **U275** generates these signals to manage timing between the 16 MHz CPU and slower peripherals like the 1 MHz 6522 VIAs.

**CPU bus control signals involved:**
- **/AS** (Address Strobe) - asserted by CPU when address is valid
- **/RW** (Read/Write) - CPU indicates read or write operation
- **/DS** (Data Strobe) - asserted when data bus is valid
- **DSACK0, DSACK1** (Data Strobe Acknowledge) - **asserted by PAL U275 to signal cycle completion and bus timing mode**
- **/BERR** (Bus Error) - asserted by PAL on illegal access

**DSACK encoding (both active-low):**
| DSACK0 | DSACK1 | Meaning |
|--------|--------|---------|
| 0 | 0 | Data ready, synchronous operation |
| 0 | 1 | Data ready, asynchronous operation |
| 1 | 0 | Device not ready (wait state, asynchronous) |
| 1 | 1 | Reserved / Bus error |

**How wait states work:**
1. CPU asserts /AS with address and /RW direction
2. PAL U232 decodes address and asserts appropriate chip select (/CS for target device)
3. Target device (e.g., 6522 VIA) performs the read or write operation
4. **PAL U275 monitors the cycle and asserts DSACK signals** - for slow devices (1 MHz VIA), it asserts DSACK1=1, DSACK0=0 (wait state), causing the CPU to insert wait cycles
5. When the device is ready, PAL U275 asserts DSACK1=0, DSACK0=1 (data ready, asynchronous)
6. CPU latches the data (on read) or confirms write, then continues to next cycle

**Note:** This board does NOT appear to have a hardware /BERR timeout mechanism (probing unknown addresses causes infinite hang, not a graceful bus error). The PAL must carefully manage /CS and DSACK to prevent accidental access to non-existent addresses.

**PAL Summary:**
- **U232 PAL ATS1+232B** (address decode / chip select):
  - Decodes A31:A0 address bus to generate chip selects for VIAs (confirmed) and possibly other devices
  - Handles /CS conditional gating (via the 74LS08 AND gates on VIA #1 and VIA #2 CS1)
  - Routes register select signals (RS0-RS3) through conditional gating logic

- **U275 PAL** (bus timing / DSACK control):
  - Generates DSACK0 and DSACK1 signals to manage wait states
  - Delays DSACK assertion for slow devices (1 MHz 6522 VIAs)
  - Asserts immediate DSACK for fast devices (ROM, some RAM regions)

- Others not analyzed yet.
  
---

## Firmware Versions

### Original Firmware ("Atlas Monitor" + PostScript)
- **Boot Phase:** Motorola 68000 Atlas Monitor (ROM bank 0) provides cold-boot diagnostics, hardware initialization, and a serial console
- **Primary Firmware:** Adobe PostScript Level 1 interpreter (ROM banks 2-4), launched by Atlas Monitor
- **ROM Banks:** All 5 banks (640 KB total, 0x00000-0x9FFFF)
- **Build Tool:** Ghidra disassembly available in `agfa_rom_disassembly.asm`

**Architecture:**
- Cold boot → Atlas Monitor diagnostic/boot phase
- User types "Executive" at console → launches PostScript interpreter
- PostScript interpreter renders documents to I/O board via FIFOs and Centronics interface
- Cannot be easily modified; would require desoldering and reprogramming all 20 EPROM chips

### AGFA-MON Firmware ("Machine Language Monitor" + EhBASIC)
- **Status:** Custom replacement firmware developed by Adrian Black for reverse-engineering and development. Runs independently of original firmware.
- **Purpose:** Lightweight machine language monitor with educational/development focus
- **Boot Phase:** Immediate hardware initialization → machine language monitor prompt
- **Serial Interface:** RS-232 on Channel A (pin 15), 9600 8N1
- **Features:**
  - Memory dump, examine, fill, move, go (jump to address)
  - S-record load/save for program transport
  - RAM test with multiple patterns (AA/55 quick boot test, full memory test burn-in possible with command)
  - SCSI bus scanner (INQUIRY, READ CAPACITY, MODE SENSE)
  - EhBASIC 68k v3.54 interpreter
  - Exception vector setup and jump table API
- **ROM Footprint:** 32 KB (fits in bank 0), but currently runs from RAM after warm boot
- **Build Tool:** vasmm68k_mot assembler (Motorola syntax)
- **Source Files:**
  - `agfa-monitor/agfa-mon.m68` - main monitor (4.6 KB source)
  - `agfa-monitor/agfa-basic.m68` - EhBASIC wrapper (embedded jump table vectors)
  - `agfa-monitor/ehbasic68k.inc` - EhBASIC engine (extracted from I/O board basic ROM)

---

## Memory

- **RAM:** 4 MB DRAM installed. The board supports a maximum of 6 MB (Banks 0-5, two banks currently unpopulated). The original firmware tests for up to 16 MB in 1 MB increments, showing the memory map allows for this much RAM, however it does not appear to be decoded on this board as there are RAM mirrors.
- **RAM Type:** ZIP (Zigzag Inline Package) DRAM, 256K x 1 bit per chip, soldered
- **RAM Organization:** 32 chips per bank x 4 banks = 128 ZIP chips total
  - Each bank is 32 chips wide (one chip per data bit, D0-D31) x 256K deep = 1 MB per bank
  - Bank 0: D0-D31, address 0x02000000-0x020FFFFF
  - Bank 1: D0-D31, address 0x02100000-0x021FFFFF
  - Bank 2: D0-D31, address 0x02200000-0x022FFFFF
  - Bank 3: D0-D31, address 0x02300000-0x023FFFFF
- **RAM Address Range:** 0x02000000 - 0x023FFFFF (4 MB installed), 0x02000000 - 0x025FFFFF (6 MB physical board maximum)
- **Address Mapping:** Fixed - ROM and RAM addresses are hardwired via PAL address decoding. No evidence of bank switching, overlays, or memory remapping anywhere in the firmware.

### Physical RAM Layout (from board inspection and photo)

```
TOP OF RAM AREA
  [LS244] [LS244] [LS244] [LS244]   <- DRAM data input buffers (8 bits each, D to CPU)
  [LS373] [LS373] [LS373] [LS373]   <- DRAM data output latches (8 bits each, Q from CPU)
  Bank 5  (not installed - empty sockets)
  Bank 4  (not installed - empty sockets)
  Bank 3  D31-D0  3MB-4MB  (32 ZIP chips)
  Bank 2  D31-D0  2MB-3MB  (32 ZIP chips)
  Bank 1  D31-D0  1MB-2MB  (32 ZIP chips)
  Bank 0  D31-D0  0MB-1MB  (32 ZIP chips)
BOTTOM EDGE OF BOARD
```

- **D0** is on the right side of each bank row; **D31** is on the left edge
- Data bit numbering increases right-to-left across each row of 32 chips
- **Four LS244/LS373 pairs** sit above the empty banks, one pair per byte lane (D31-D24, D23-D16, D15-D8, D7-D0)
  - LS244: buffers CPU data bus onto RAM data-in (D) pins when writing
  - LS373: latches RAM data-out (Q) pins onto CPU data bus when reading
- The ROM chips (EPROMs) are located on the right edge of the board

---

## ROM

- **Total ROM:** 640 KB - 20x AM27C256 EPROMs (32 KB each)
- **Organization:** 4-wide interleave (HH, HM, LM, LL) to provide 32-bit data bus width; 5 banks of 4 chips each
- **ROM Address Range:** 0x00000000 - 0x0009FFFF
- **ROM Dumps of original firmware:** Files 0.bin through 4.bin (128 KB each, one per bank)

---

## I/O Ports

- **RS-232** - Serial port via Z85C30 Channel A (0x07000000): PostScript job input, Atlas Monitor, diagnostic output
- **RS-422 (AppleTalk)** - Apple network interface via Z85C30 Channel B (0x07000000/0x07000001, pin 25)
- **Centronics Parallel** - Parallel printer port. One MK4501 FIFO, two LS374 latch chips, and a PAL are located in this area of the board. See [parallel_port.md](parallel_port.md).
- **SCSI Connector** - 50-pin SCSI external device interface (via AMD AM5380)
- **I/O Processor Connector** - 50-pin ribbon cable to I/O processor board. Carries R6522 VIA parallel I/O signals for board-to-board communication.

---

## Hard Drive

- **Drive:** Quantum ProDrive P40S, 40 MB, SCSI (physically present on this unit)
- **SCSI ID:** 0
- **Interface:** AMD AM5380 SCSI controller on main board
- **Not required for boot:** The system boots PostScript fully without the SCSI disk connected. All testing was done with the disk disconnected.

---

## Notes

- FPU not installed on this unit; but it can accept a Motorola MC68881
