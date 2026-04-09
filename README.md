# Agfa Compugraphic 9000PS Reverse Engineering Project

A from-scratch machine language monitor and replacement ROM firmware for the **Agfa Compugraphic 9000PS** RIP main processor board, built entirely through hardware reverse engineering.

As seen on a video series on [Adrian's Digital Basement](https://www.youtube.com/@adriansdigitalbasement)

[Original firmware and drive image](https://archive.org/details/agfa-computgraphi-9000-ps)

[Agfa I/O Board Reverse Engineering](https://github.com/misterblack1/agfa_ebs_pnafati)

---

## What is This?

The Agfa Compugraphic 9000PS PostScript Level 1 RIP (Raster Image Processor) designed in 1986 built around a **Motorola 68020** CPU at 16 MHz with 4 MB of RAM. This project is a **complete replacement firmware** that provides:

- **Interactive machine language monitor** - memory examine/edit, fill, move, hex dump, register display, S-record load/dump, and more
- **EhBASIC 3.54** - embedded 68K BASIC interpreter, launchable from the monitor prompt
- **VERA graphics controller support** - text mode, bitmap graphics, cursor, palette manipulation, shape drawing via a VERA installed into the VIA#1 socket.
- **SCSI bus scanning** - enumerate devices on the onboard SCSI bus
- **RAM burn-in testing** - multi-pattern memory test with error reporting
- **NVRAM configuration** - persistent settings for console output mode
- **CP/M and UNIX boot stubs** - jumps to CP/M-68K and Minix, in ROM

All of this was developed without any original documentation. The hardware was reverse engineered by analyzing the original ROMs, probing the board, and writing test code.

![Agfa Computergraphic 9000PS](https://github.com/misterblack1/agfa_ebs_pnafati/blob/main/images/Compugraphic9000ps.jpg?raw=true)

---

## Hardware

| Component | Details |
|-----------|---------|
| **CPU** | Motorola 68020 @ 16 MHz |
| **RAM** | 4 MB ($02000000-$023FFFFF), tested on cold boot |
| **ROM** | 128 KB, Bank 0 ($000000-$01FFFF), 4× AM27C256 EPROMs |
| **Serial** | Zilog Z85C30 SCC Channel A, DB-25 RS-232 at 9600 8N1 |
| **Graphics** | VERA (from the Commander X16) graphics controller |
| **SCSI** | AM5380 SCSI controller |
| **Timer/IO** | 6522 VIA |

---

## Monitor Commands

```
B                       Launch BASIC (CALL 1054 to exit)
C                       Boot CP/M
D [addr] [len]          Hex/ASCII memory dump (default 128 bytes)
E <addr>                Examine/edit memory (. to quit)
F <addr> <len> <byte>   Fill memory
G [addr]                Execute at address
H or ?                  Show help
L                       Load S-records from terminal
M <src> <dst> <len>     Copy memory
P <addr> <len>          Send S-records to terminal
R                       Show CPU registers
S                       Scan SCSI bus
T                       RAM burn-in test
U                       Boot UNIX
V                       NVRAM configuration
X                       Save BASIC program as S-records
```

---

## Repository Structure

```
.
├── agfa-monitor/                    Main ROM firmware source and build output
│   └── include/                     Assembly include files (VERA, VIA, NVRAM)
├── assembler/                       Build tools and vasm 68020 assembler
├── docs/                            Reference documentation
├── examples/                        Example programs and demos
└── fonts/                           Alternative 8×8 font data files
```

---

## Build the ROM

```powershell
cd agfa-monitor
.\build.ps1
```

**GNU Make** (Linux/Mac/Windows with Make):
```bash
cd agfa-monitor
make
```

Both produce four 32 KB EPROM images:
- `agfa-rom-HH.bin` - Byte lane D24-D31 (MSB)
- `agfa-rom-HM.bin` - Byte lane D16-D23
- `agfa-rom-LM.bin` - Byte lane D8-D15
- `agfa-rom-LL.bin` - Byte lane D0-D7 (LSB)

### Install on Hardware

Program each `.bin` file onto an **AM27C256** EPROM using a TL866-3G (or compatible) programmer in AM27C256 mode. Install the four chips into the ROM sockets on the Agfa 9000PS main board, matching the HH/HM/LM/LL silkscreen labels.

### Build Example Programs

```bash
assembler/vasmm68k_mot.exe -m68020 -Fsrec -o examples/hello_world.s19 examples/hello_world.m68
```

Load the resulting `.s19` file via the monitor's `L` command, then run with `G 02050000`.

---

## ROM Binary Layout

| Address Range | Content |
|--------------|---------|
| $000000-$0003FF | Exception vector table (256 × longword) |
| $000400-$00045F | API jump table (fixed-address `JMP` entries) |
| $000460-$0017FF | Monitor code: boot, command loop, SCC driver, utilities |
| $001800-$0058F1 | EhBASIC 68K v3.54 engine |
| $006000-$007FFF | Extended commands (burn-in test, SCSI scan) |

### API Jump Table

| Address | Entry | Description |
|---------|-------|-------------|
| $0400 | `START` | Cold start / reset |
| $0406 | `PUTCHAR` | Output D0.B to console |
| $040C | `GETCHAR` | Blocking input → D0.B |
| $0412 | `PRINTSTR` | Print NUL-terminated string at A0 |
| $0418 | `GETLINE` | Read line into buffer at A0 |
| $041E | `CMD_LOOP` | Return to monitor prompt |
| $0424 | `GETTICKS` | Tick count → D0.L (~60 Hz) |

User programs call these via absolute addresses. See [`docs/agfa_mon_api_digest.md`](docs/agfa_mon_api_digest.md) for full details.

---

## Testing

On real hardware:

1. Assemble your program to S-record format: `vasmm68k_mot.exe -m68020 -Fsrec -o test.s19 test.m68`
2. At the AGFA-MON prompt, type `L` and press Enter
3. Paste the `.s19` file content into the serial terminal
4. Run with `G 02050000`

Example programs in [`examples/`](examples/) demonstrate the ROM API, VERA graphics, palette manipulation, shape drawing, and font handling.

---

## Credits

- **EhBASIC** - 68K port of the EhBASIC 3.54 interpreter by Lee Davison
- **VERA** - Commander X16 VERA by Frank van den Hoef
- **vasm** - portable and retargetable assembler by Volker Barthelmann

---

## License

This project is provided for educational and preservation purposes. The AGFA-MON firmware source code (`agfa-mon.m68`, `agfa-basic.m68`, and associated includes) is original work by Adrian Black. EhBASIC is used under its original license terms. The vasm assembler is distributed under its own license (see the vasm project for details).
