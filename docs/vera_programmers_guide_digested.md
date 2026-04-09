# VERA Programmer's Reference - Concise Edition

> **Platform:** Agfa Compugraphic 9000PS
> **Register base:** `$04000020` - `$0400003F` (replaces original CX16 `$9F20`-`$9F3F`)
> **VRAM:** 128 KB, accessed indirectly via address/data port registers

## Overview

VERA (Versatile Embedded Retro Adapter) provides:

- Video: 640×480 @ 60 Hz, VGA / NTSC composite / S-Video / RGB output
- 2 layers (tile or bitmap mode each)
- 128 sprites, up to 64×64 px
- 256-color palette from 4096 colors (12-bit RGB)
- 16-channel PSG (Pulse, Saw, Triangle, Noise)
- PCM audio: 4 KB FIFO, up to 48 kHz 16-bit stereo
- SPI controller for SD card / flash

---

## Register Map

### Core Registers ($04000020-$04000028)

| Address     | Name             | R/W   | Bits 7-0 description                                                         |
|-------------|------------------|-------|------------------------------------------------------------------------------|
| $04000020   | ADDRx_L          | R/W   | VRAM Address bits 7:0 (for the data port selected by ADDRSEL)               |
| $04000021   | ADDRx_M          | R/W   | VRAM Address bits 15:8                                                       |
| $04000022   | ADDRx_H          | R/W   | Bits 7:4 = Address Increment; Bit 3 = DECR; Bit 2 = Nibble Increment; Bit 1 = Nibble Address; Bit 0 = VRAM Address bit 16 |
| $04000023   | DATA0            | R/W   | VRAM Data port 0                                                             |
| $04000024   | DATA1            | R/W   | VRAM Data port 1                                                             |
| $04000025   | CTRL             | R/W   | Bit 7 = Reset; Bits 6:1 = DCSEL; Bit 0 = ADDRSEL                             |
| $04000026   | IEN              | R/W   | Bit 7 = IRQ_LINE bit 8; Bit 6 = SCANLINE bit 8; Bits 5:4 = unused; Bit 3 = AFLOW; Bit 2 = SPRCOL; Bit 1 = LINE; Bit 0 = VSYNC |
| $04000027   | ISR              | R/W   | Bits 7:4 = Sprite collisions (read-only); Bit 3 = AFLOW; Bit 2 = SPRCOL; Bit 1 = LINE; Bit 0 = VSYNC (write 1 to clear bits 2:0) |
| $04000028   | IRQLINE_L        | Write | IRQ line bits 7:0                                                            |
| $04000028   | SCANLINE_L       | Read  | Current scanline bits 7:0                                                    |

**ADDRSEL / DCSEL multiplexing:**
- `ADDRSEL` (CTRL bit 0): selects which address register set is active (0 = ADDR0, 1 = ADDR1)
- `DCSEL` (CTRL bits 6:1): selects which display-composer register bank is visible at $04000029-$0400002C

### Display Composer Registers ($04000029-$0400002C) - banked by DCSEL

#### DCSEL=0: Video / Scaling / Border

| Address     | Name       | R/W | Bits                                                                     |
|-------------|------------|-----|--------------------------------------------------------------------------|
| $04000029   | DC_VIDEO   | R/W | Bit 7 = Current Field (RO); Bit 6 = Sprites Enable; Bit 5 = Layer1 Enable; Bit 4 = Layer0 Enable; Bit 3 = 240P (NTSC/RGB); Bit 2 = Chroma Disable (NTSC) / HV Sync (RGB); Bits 1:0 = Output Mode |
| $0400002A   | DC_HSCALE  | R/W | Active Display H-Scale (128 = 1:1, 64 = 2×)                              |
| $0400002B   | DC_VSCALE  | R/W | Active Display V-Scale (128 = 1:1, 64 = 2×)                              |
| $0400002C   | DC_BORDER  | R/W | Border color (palette index)                                             |

**Output Mode values:** 0 = disabled, 1 = VGA, 2 = NTSC composite/S-Video, 3 = RGB 15 KHz

#### DCSEL=1: Active Display Position

| Address     | Name       | R/W | Bits                                          |
|-------------|------------|-----|------------------------------------------------|
| $04000029   | DC_HSTART  | R/W | Active Display H-Start bits 9:2 (step of 4 px) |
| $0400002A   | DC_HSTOP   | R/W | Active Display H-Stop bits 9:2 (step of 4 px)  |
| $0400002B   | DC_VSTART  | R/W | Active Display V-Start bits 8:1 (step of 2 px) |
| $0400002C   | DC_VSTOP   | R/W | Active Display V-Stop bits 8:1 (step of 2 px)  |

Full active area = HSTART 0, HSTOP 640, VSTART 0, VSTOP 480.

#### DCSEL=2: FX Control

| Address     | Name          | R/W   | Bits                                                                                                                                   |
|-------------|---------------|-------|----------------------------------------------------------------------------------------------------------------------------------------|
| $04000029   | FX_CTRL       | R/W   | Bit 7 = Transparent Writes; Bit 6 = Cache Write Enable; Bit 5 = Cache Fill Enable; Bit 4 = One-byte Cache Cycling; Bit 3 = 16-bit Hop; Bit 2 = 4-bit Mode; Bits 1:0 = Addr1 Mode |
| $0400002A   | FX_TILEBASE   | W     | Bits 7:2 = FX Tile Base Address bits 16:11; Bit 1 = Affine Clip Enable; Bit 0 = 2-bit Polygon                                        |
| $0400002B   | FX_MAPBASE    | W     | Bits 7:2 = FX Map Base Address bits 16:11; Bits 1:0 = Map Size                                                                        |
| $0400002C   | FX_MULT       | W     | Bit 7 = Reset Accumulator; Bit 6 = Accumulate; Bit 5 = Subtract Enable; Bit 4 = Multiplier Enable; Bits 3:2 = Cache Byte Index; Bit 1 = Cache Nibble Index; Bit 0 = Two-byte Cache Increment Mode |

#### DCSEL=3: FX Increment

| Address     | Name          | R/W | Bits                                                                    |
|-------------|---------------|-----|-------------------------------------------------------------------------|
| $04000029   | FX_X_INCR_L  | W   | X Increment bits -2:-9 (signed, fixed-point)                           |
| $0400002A   | FX_X_INCR_H  | W   | Bit 7 = X Incr 32×; Bits 6:0 = X Increment bits 5:-1 (signed)         |
| $0400002B   | FX_Y_INCR_L  | W   | Y/X2 Increment bits -2:-9 (signed, fixed-point)                        |
| $0400002C   | FX_Y_INCR_H  | W   | Bit 7 = Y/X2 Incr 32×; Bits 6:0 = Y/X2 Increment bits 5:-1 (signed)   |

#### DCSEL=4: FX Position

| Address     | Name          | R/W | Bits                                                                   |
|-------------|---------------|-----|------------------------------------------------------------------------|
| $04000029   | FX_X_POS_L    | W   | X Position bits 7:0                                                    |
| $0400002A   | FX_X_POS_H    | W   | Bit 7 = X Pos bit -9; Bits 6:3 = unused; Bits 2:0 = X Position bits 10:8 |
| $0400002B   | FX_Y_POS_L    | W   | Y/X2 Position bits 7:0                                                 |
| $0400002C   | FX_Y_POS_H    | W   | Bit 7 = Y/X2 Pos bit -9; Bits 6:3 = unused; Bits 2:0 = Y/X2 Position bits 10:8 |

#### DCSEL=5: FX Sub-pixel Position / Polygon Fill

| Address     | Name               | R/W | Bits                                                                                                                   |
|-------------|--------------------|-----|------------------------------------------------------------------------------------------------------------------------|
| $04000029   | FX_X_POS_S         | W   | X Position sub-pixel bits -1:-8                                                                                        |
| $0400002A   | FX_Y_POS_S         | W   | Y/X2 Position sub-pixel bits -1:-8                                                                                     |
| $0400002B   | FX_POLY_FILL_L     | R   | (varies by 4-bit Mode and 2-bit Polygon flags; returns fill length and X position info)                                |
| $0400002C   | FX_POLY_FILL_H     | R   | Fill Length bits 9:3; Bit 0 = always 0                                                                                 |

**FX_POLY_FILL_L variants (read-only):**
- 4-bit Mode=0: Bit 7 = Fill Len ≥ 16; Bits 6:5 = X Pos 1:0; Bits 4:1 = Fill Len 3:0; Bit 0 = 0
- 4-bit Mode=1, 2-bit Polygon=0: Bit 7 = Fill Len ≥ 8; Bits 6:5 = X Pos 1:0; Bit 4 = X Pos 2; Bits 3:1 = Fill Len 2:0; Bit 0 = 0
- 4-bit Mode=1, 2-bit Polygon=1: Bit 7 = X2 Pos -1; Bits 6:5 = X Pos 1:0; Bit 4 = X Pos 2; Bits 3:1 = Fill Len 2:0; Bit 0 = X Pos -1

#### DCSEL=6: FX Cache / Accumulator

| Address     | Name            | R/W | Bits                                                        |
|-------------|-----------------|-----|-------------------------------------------------------------|
| $04000029   | FX_CACHE_L      | W   | Cache bits 7:0 / Multiplicand bits 7:0 (signed)            |
| $04000029   | FX_ACCUM_RESET  | R   | Reading resets the accumulator                              |
| $0400002A   | FX_CACHE_M      | W   | Cache bits 15:8 / Multiplicand bits 15:8 (signed)          |
| $0400002A   | FX_ACCUM        | R   | Reading performs an accumulate operation                    |
| $0400002B   | FX_CACHE_H      | W   | Cache bits 23:16 / Multiplier bits 7:0 (signed)            |
| $0400002C   | FX_CACHE_U      | W   | Cache bits 31:24 / Multiplier bits 15:8 (signed)           |

#### DCSEL=63: Version (Read-only)

| Address     | Name     | Description                    |
|-------------|----------|--------------------------------|
| $04000029   | DC_VER0  | ASCII "V" ($56) if valid       |
| $0400002A   | DC_VER1  | Major release                  |
| $0400002B   | DC_VER2  | Minor release                  |
| $0400002C   | DC_VER3  | Build number                   |

### Layer / Audio / SPI Registers ($0400002D-$0400003F)

Layer 0 and Layer 1 have identical register layouts (L0 at $2D-$33, L1 at $34-$3A).

**Layer 0:**

| Address     | Name           | Bits                                                                                                          |
|-------------|----------------|---------------------------------------------------------------------------------------------------------------|
| $0400002D   | L0_CONFIG      | Bits 7:6 = Map Height; Bits 5:4 = Map Width; Bit 3 = T256C; Bit 2 = Bitmap Mode; Bits 1:0 = Color Depth      |
| $0400002E   | L0_MAPBASE     | Map Base Address bits 16:9 (aligned to 512 bytes)                                                             |
| $0400002F   | L0_TILEBASE    | Bits 7:2 = Tile Base Address bits 16:11 (aligned to 2048 bytes); Bit 1 = Tile Height (0=8, 1=16); Bit 0 = Tile Width (0=8, 1=16) |
| $04000030   | L0_HSCROLL_L   | H-Scroll bits 7:0                                                                                             |
| $04000031   | L0_HSCROLL_H   | Bits 7:4 = unused; Bits 3:0 = H-Scroll bits 11:8 (in tile mode; bitmap mode = palette offset)                |
| $04000032   | L0_VSCROLL_L   | V-Scroll bits 7:0                                                                                             |
| $04000033   | L0_VSCROLL_H   | Bits 7:4 = unused; Bits 3:0 = V-Scroll bits 11:8                                                             |

**Layer 1:**

| Address     | Name           | Bits (same format as Layer 0)           |
|-------------|----------------|-----------------------------------------|
| $04000034   | L1_CONFIG      | Map Height / Width, T256C, Bitmap Mode, Color Depth |
| $04000035   | L1_MAPBASE     | Map Base Address bits 16:9              |
| $04000036   | L1_TILEBASE    | Tile Base Address bits 16:11 + Tile H/W |
| $04000037   | L1_HSCROLL_L   | H-Scroll bits 7:0                                                |
| $04000038   | L1_HSCROLL_H   | Bits 7:4 = unused; Bits 3:0 = H-Scroll bits 11:8                 |
| $04000039   | L1_VSCROLL_L   | V-Scroll bits 7:0                                                |
| $0400003A   | L1_VSCROLL_H   | Bits 7:4 = unused; Bits 3:0 = V-Scroll bits 11:8                 |

**Audio / SPI:**

| Address     | Name        | Bits                                                                                                          |
|-------------|-------------|---------------------------------------------------------------------------------------------------------------|
| $0400003B   | AUDIO_CTRL  | Bit 7 = FIFO Full (RO) / FIFO Reset (write 1); Bit 6 = FIFO Empty (RO); Bit 5 = 16-bit; Bit 4 = Stereo; Bits 3:0 = PCM Volume |
| $0400003C   | AUDIO_RATE  | PCM Sample Rate (128 = 48828 Hz, 64 = 24414 Hz, 32 = 12207 Hz, 0 = stop, >128 = invalid)                     |
| $0400003D   | AUDIO_DATA  | Audio FIFO data (write-only; writes add one byte to FIFO)                                                     |
| $0400003E   | SPI_DATA    | SPI data byte (write to start transfer, read to get received byte)                                            |
| $0400003F   | SPI_CTRL    | Bit 7 = Busy (RO); Bits 6:3 = unused; Bit 2 = Auto-TX; Bit 1 = Slow Clock (1=390 kHz, 0=12.5 MHz); Bit 0 = Select (1=assert CS) |

---

## VRAM Address Space Layout

| Address Range        | Description                   |
|----------------------|-------------------------------|
| $0:0000 - $1:F9BF    | General-purpose Video RAM     |
| $1:F9C0 - $1:F9FF    | PSG registers (16 voices × 4 bytes) |
| $1:FA00 - $1:FBFF    | Palette (256 entries × 2 bytes)     |
| $1:FC00 - $1:FFFF    | Sprite attributes (128 × 8 bytes)   |

**Important:** $1:F9C0-$1:FFFF are write-only registers. Reading returns the last value written by the host, not the actual register state. Initialize fully before use. The KERNAL normally handles this, but cartridge programs, custom ROMs, or standalone VERA use require manual initialization.

**Default KERNAL layout** (can be freely re-arranged if not using KERNAL text):

| Addresses            | Description                            |
|----------------------|----------------------------------------|
| $0:0000 - $1:2BFF    | 320×240 @ 256-color bitmap             |
| $1:2C00 - $1:2FFF    | unused (1024 bytes)                    |
| $1:3000 - $1:AFFF    | Sprite image data                      |
| $1:B000 - $1:EBFF    | Text mode                              |
| $1:EC00 - $1:EFFF    | unused (1024 bytes)                    |
| $1:F000 - $1:F7FF    | Charset                                |
| $1:F800 - $1:F9BF    | unused (448 bytes)                     |
| $1:F9C0 - $1:F9FF    | PSG registers                          |
| $1:FA00 - $1:FBFF    | Color palette                          |
| $1:FC00 - $1:FFFF    | Sprite attributes                      |

Call `CINT` ($FF81) to restore default text mode (uses NVRAM settings if configured).

**This memory map is not fixed:** All of $0:0000-$1:F9BF is freely usable if you don't need KERNAL/BASIC text. You can allocate multiple text/graphic buffers or rearrange tile layouts. Once rearranged, you must fully manage your own bitmaps, tiles, and text/tile buffers.

---

## VRAM Access

VRAM is accessed indirectly through the address/data port registers.

**Procedure:**
1. Set ADDRSEL in CTRL to select address port 0 or 1
2. Write the 17-bit VRAM address to ADDRx_L, ADDRx_M, ADDRx_H (bit 0)
3. Set the increment value in ADDRx_H bits 7:4
4. Read or write DATA0 or DATA1 - address auto-increments after each access

**Address Increment Values (ADDRx_H bits 7:4):**

| Value | Increment | Value | Increment |
|------:|----------:|------:|----------:|
| 0     | 0         | 8     | 128       |
| 1     | 1         | 9     | 256       |
| 2     | 2         | 10    | 512       |
| 3     | 4         | 11    | 40        |
| 4     | 8         | 12    | 80        |
| 5     | 16        | 13    | 160       |
| 6     | 32        | 14    | 320       |
| 7     | 64        | 15    | 640       |

**DECR** bit (ADDRx_H bit 3): set to decrement instead of increment.

**Nibble Increment** (bit 2) and **Nibble Address** (bit 1): control sub-byte addressing (see FX documentation for details).

---

## Reset

Writing 1 to **RESET** (CTRL bit 7) reconfigures the FPGA. All registers reset, palette returns to defaults.

---

## Interrupts

**IEN** ($04000026) enables interrupt sources; **ISR** ($04000027) reports active interrupts.

| Interrupt | IEN Bit | ISR Bit | Description                                                       |
|-----------|---------|---------|-------------------------------------------------------------------|
| VSYNC     | 0       | 0       | Vertical blank (start of frame)                                   |
| LINE      | 1       | 1       | Scanline matches IRQLINE value                                    |
| SPRCOL    | 2       | 2       | Sprite collision detected                                         |
| AFLOW     | 3       | 3       | Audio FIFO < ¼ full                                               |

- Write 1 to ISR bits 2:0 to clear. AFLOW clears when FIFO fills past ¼.
- IRQLINE_L ($04000028, write-only) + IEN bit 7 = 9-bit line number for LINE interrupt. For interlaced modes, the interrupt fires each field and bit 0 of IRQLINE is ignored.
- SCANLINE_L ($04000028, read-only) + IEN bit 6 = current scanline (0-479 in visible area, $1FF for lines 512-524). SCANLINE is not affected by interlaced modes and returns either all even or all odd values during even or odd fields. VERA renders lines ahead of scanout: line 1 is being rendered while line 0 is scanned out, so visible changes may be delayed one scanline.
- ISR bits 7:4 = sprite collision mask (read-only).

---

## Display Composer

Combines Layer 0, Layer 1, and sprites into the final output.

**DC_VIDEO** ($04000029, DCSEL=0):

| Bit | Field            | Description                                                                |
|-----|------------------|----------------------------------------------------------------------------|
| 7   | Current Field    | RO: 0 = even field/line, 1 = odd (interlaced modes)                        |
| 6   | Sprites Enable   | Show sprites                                                               |
| 5   | Layer1 Enable    | Show layer 1                                                               |
| 4   | Layer0 Enable    | Show layer 0                                                               |
| 3   | 240P             | Progressive mode for NTSC/RGB (263 lines instead of 262.5)                 |
| 2   | Chroma Dis / HVS | NTSC: disable chroma for mono (also disables S-video chroma); RGB: enable separate H/V sync               |
| 1:0 | Output Mode      | 0=disabled, 1=VGA, 2=NTSC, 3=RGB                                                                          |

**Scaling:** DC_HSCALE / DC_VSCALE - fractional scale factor. 128 = 1 output pixel per input pixel (1:1), 64 = 2 output pixels per input pixel (2×), etc.

**Border:** DC_BORDER - palette index for non-active screen area.

**Active area:** DC_HSTART/HSTOP/VSTART/VSTOP (DCSEL=1) - in native 640×480 space. H values step by 4 px, V values step by 2 px. Full active area = HSTART 0, HSTOP 640, VSTART 0, VSTOP 480.

**240P mode:** Enables 240P progressive over NTSC or RGB (263 scanlines per field instead of 262.5). No effect in VGA mode. On CRTs, both even and odd field scanlines display on even scanlines.

**Version (DCSEL=63):** DC_VER0-DC_VER3 are read-only. If DC_VER0 returns $56 ('V'), remaining registers give major, minor, and build numbers. If DC_VER0 returns anything else, version is undefined.

---

## Layer Modes

Both layers share the same feature set. Configured via `Lx_CONFIG`.

**Lx_CONFIG fields:**

| Bits  | Field        | Values                                                                 |
|-------|--------------|------------------------------------------------------------------------|
| 7:6   | Map Height   | 0=32, 1=64, 2=128, 3=256 tiles                                        |
| 5:4   | Map Width    | 0=32, 1=64, 2=128, 3=256 tiles                                        |
| 3     | T256C        | 1 bpp only: 0 = 16-color text, 1 = 256-color text                     |
| 2     | Bitmap Mode  | 0 = tile mode, 1 = bitmap mode                                         |
| 1:0   | Color Depth  | 0=1 bpp, 1=2 bpp, 2=4 bpp, 3=8 bpp                                    |

### Tile Mode - 1 bpp, 16-color text (T256C=0)

Each tile map entry is 2 bytes:

| Byte | Bits 7:0                              |
|------|---------------------------------------|
| 0    | Character index                       |
| 1    | Nibble high = Background color (4-bit palette index); Nibble low = Foreground color (4-bit palette index) |

Tile data: 1 bit per pixel. 1 = foreground color, 0 = background color.

### Tile Mode - 1 bpp, 256-color text (T256C=1)

Each tile map entry is 2 bytes:

| Byte | Bits 7:0                |
|------|-------------------------|
| 0    | Character index         |
| 1    | Foreground color (8-bit palette index) |

Tile data: 1 bit per pixel. 1 = foreground color, 0 = color 0 (transparent).

### Tile Mode - 2/4/8 bpp

Each tile map entry is 2 bytes:

| Byte | Bits                                                                                |
|------|-------------------------------------------------------------------------------------|
| 0    | Tile index bits 7:0                                                                 |
| 1    | Bits 7:4 = Palette offset; Bit 3 = V-flip; Bit 2 = H-flip; Bits 1:0 = Tile index bits 9:8 |

**Palette offset logic:** Color index 0 = transparent (unmodified). Index 1-15 becomes 1-15 + (16 × palette_offset). Index 16-255 unmodified. T256C bit forces bit 7 of color index to 1.

**Packed pixel ordering:** 2 bpp = 4 pixels/byte, 4 bpp = 2 pixels/byte. Bit 7 = leftmost pixel, bit 0 = rightmost pixel.

**H-Scroll:** 12-bit value (0-4095). Increasing moves the picture left; decreasing moves right. Only functional in tile mode.

**V-Scroll:** 12-bit value (0-4095). Increasing moves the picture up; decreasing moves down. Only functional in tile mode.

### Bitmap Mode - 1/2/4/8 bpp

- MAP_BASE is unused.
- TILE_BASE points to bitmap pixel data.
- TILEW bit: 0 = 320 px wide, 1 = 640 px wide.
- H-Scroll and V-Scroll registers are NOT functional for scrolling.
- H-Scroll bits 11:8 (Lx_HSCROLL_H lower nibble, bits 3:0) serves as the **palette offset** instead.
- Palette offset and T256C modify color indexes the same way as tile modes.

---

## Palette

256 entries at VRAM $1:FA00-$1:FBFF. Each entry is 2 bytes:

| Byte | Bits 7:4 | Bits 3:0     |
|------|----------|--------------|
| 0    | Green    | Blue         |
| 1    | unused   | Red          |

Each color component is 4 bits (0-15), giving 12-bit color (4096 possible colors).

Default palette: indexes 0-15 ≈ C64 colors; 16-31 = grayscale ramp; 32-255 = various hues/saturations.

---

## Sprites

128 sprites, each described by 8 bytes at VRAM $1:FC00-$1:FFFF:

| Offset | Bits                                                                                                  |
|--------|-------------------------------------------------------------------------------------------------------|
| 0      | Address bits 12:5                                                                                     |
| 1      | Bit 7 = Mode (0=4 bpp, 1=8 bpp); Bits 6:3 = unused; Bits 2:0 = Address bits 16:13                   |
| 2      | X position bits 7:0                                                                                   |
| 3      | Bits 7:2 = unused; Bits 1:0 = X position bits 9:8                                                    |
| 4      | Y position bits 7:0                                                                                   |
| 5      | Bits 7:2 = unused; Bits 1:0 = Y position bits 9:8                                                    |
| 6      | Bits 7:4 = Collision mask; Bits 3:2 = Z-depth; Bit 1 = V-flip; Bit 0 = H-flip                        |
| 7      | Bits 7:6 = Sprite height; Bits 5:4 = Sprite width; Bits 3:0 = Palette offset                         |

**Sprite size values:** 0=8 px, 1=16 px, 2=32 px, 3=64 px (same encoding for width and height).

**Z-depth:** 0=disabled, 1=behind Layer 0, 2=between Layer 0 and Layer 1, 3=in front of Layer 1.

**Rendering priority:** Lower memory address = drawn in front. Sprite 0 (lowest) is frontmost.

**Palette offset:** Same logic as tile layers (adds 16 × offset to color indexes 1-15).

---

## Sprite Collisions

At start of vertical blank, ISR bits 7:4 are updated with the collision mask. Non-zero sets SPRCOL interrupt. Generated once per frame; cleared when sprites no longer collide. Only detected on actually-rendered scanlines (may differ between interlaced and non-interlaced modes).

---

## SPI Controller

Connected to SD card and VERA flash chip (JP1 selects which).

| Operation     | Details                                                                         |
|---------------|---------------------------------------------------------------------------------|
| Transfer      | Write to SPI_DATA to start; read SPI_DATA for received byte; BUSY while active |
| Clock speed   | Slow Clock bit: 0 = 12.5 MHz, 1 = ~390 kHz (use during SD init; some SD cards require < 400 kHz)               |
| Chip select   | SELECT bit: 1 = assert (active low), 0 = release                               |
| Auto-TX       | When set, reading SPI_DATA auto-starts next transfer with dummy byte $FF        |

---

## Audio

### PSG (Programmable Sound Generator)

16 voices, each with 4 registers at VRAM $1:F9C0 + (voice × 4):

| Offset | Field          | Description                                                                                        |
|--------|----------------|----------------------------------------------------------------------------------------------------|
| 0-1    | Frequency word | 16-bit value. Output freq = (25 MHz / 512) / 2^17 × freq_word ≈ freq_word × 0.373 Hz              |
| 2      | Volume/pan     | Bit 7 = Right enable; Bit 6 = Left enable; Bits 5:0 = Volume (0-63, logarithmic)                  |
| 3      | Waveform/PW    | Bits 7:6 = Waveform (0=Pulse, 1=Sawtooth, 2=Triangle, 3=Noise); Bits 5:0 = Pulse Width / XOR      |

**Frequency example:** 440 Hz (A4) → freq_word = 440 / (48828.125 / 131072) ≈ 1181

**Pulse Width:** 63 ($3F) = 50% duty cycle (square wave), 0 = very narrow pulse.

**XOR (triangle/saw):** Modifies waveform via XOR at the given position. PW=63 = original waveform, PW=0 = inverted. Creates overtones and fuzz effects. Most noticeable with triangle (NES-like fuzzy triangle, similar to VRC6 chip). With saw, more subtle - adds overtones. Be aware of phasing effects.

**Noise:** Frequency controls brightness (higher = brighter, lower = darker). PWM/XOR values do not influence noise shape.

### PCM Audio

4 KB FIFO buffer for sample playback. Registers at $0400003B-$0400003D.

**AUDIO_CTRL** ($0400003B):
- Bit 7: FIFO Full (RO) / write 1 to reset FIFO
- Bit 6: FIFO Empty (RO)
- Bit 5: 16-bit mode (0 = 8-bit)
- Bit 4: Stereo (0 = mono, same data to both channels)
- Bits 3:0: PCM Volume (0-15, logarithmic)

Note: Writes to AUDIO_DATA while FIFO is full are ignored.

**AUDIO_RATE** ($0400003C): 128 = 48828 Hz (best quality, 1:1 input:output), 64 = 24414 Hz, 32 = 12207 Hz, 0 = stopped. Values > 128 are invalid. At rate 128, every output sample reads one input sample from FIFO. Lower rates duplicate samples to the DAC. Input samples are always read as a complete set (1, 2, or 4 bytes depending on mode).

**AUDIO_DATA** ($0400003D): Write-only. Each write adds one byte to FIFO. Writes while FIFO is full are ignored.

**FIFO write order by mode:**

| Mode          | Byte order                                                                         |
|---------------|------------------------------------------------------------------------------------|
| 8-bit mono    | `<mono>`                                                                           |
| 8-bit stereo  | `<left>` `<right>`                                                                 |
| 16-bit mono   | `<mono_lo>` `<mono_hi>`                                                            |
| 16-bit stereo | `<left_lo>` `<left_hi>` `<right_lo>` `<right_hi>`                                 |

All sample data is two's complement signed.

**Setup procedure:** Set rate to 0 → fill FIFO with initial data → set desired rate. Prevents underruns.

**AFLOW interrupt:** Fires when FIFO < ¼ full. Cleared by filling past ¼.

---

## FX (VERA Firmware v0.3.1+)

**Preliminary documentation - specification can still change.**

FX extends VERA addressing modes. It adds "helpers" that assist the CPU with time-consuming inner-loop operations (line drawing, polygon filling, affine transforms, multiplication). The CPU remains the orchestrator; FX does not add or extend hardware renderers.

Available in VERA firmware v0.3.1+ (CX16 emulators from R44).

**Backwards compatibility:** If only DCSEL values 0 and 1 are used, VERA behaves exactly as before the FX update.

**Important:** Interrupt handlers that access VERA registers/VRAM while FX is active must save FX_CTRL, write 0 to FX_CTRL before accessing VERA normally, then restore FX state after.

### DCSEL Extension

CTRL ($04000025) DCSEL field is extended to 6 bits (bits 6:1). FX uses DCSEL values 2-6, giving 20 additional 8-bit registers (15 write-only, 2 read-only, 3 read/write). DCSEL value 63 is read-only and reports VERA version. DCSEL values 7-62 are currently unused but behave like DCSEL 63.

### FX_CTRL Register (DCSEL=2, $04000029) - R/W

| Bit | Field                  | Description                                                       |
|-----|------------------------|-------------------------------------------------------------------|
| 7   | Transparent Writes     | Writing zero leaves target byte/nibble intact (affects DATA0/1)   |
| 6   | Cache Write Enable     | Write cache contents to VRAM on DATA0/DATA1 write                |
| 5   | Cache Fill Enable      | Fill 32-bit cache on DATA0/DATA1 read                            |
| 4   | One-byte Cache Cycling | Single-byte cache write + cycling behavior                        |
| 3   | 16-bit Hop             | Special ADDR1 alternating increment mode                          |
| 2   | 4-bit Mode             | Enable 4-bit (nibble) addressing                                  |
| 1:0 | Addr1 Mode             | 0=traditional, 1=line draw, 2=polygon fill, 3=affine             |

### Addr1 Modes

| Mode | Name               | Description                        |
|------|--------------------|------------------------------------|
| 0    | Traditional        | Normal ADDR1 behavior              |
| 1    | Line draw helper   | ADDR1 auto-stepped for line pixels |
| 2    | Polygon filler     | ADDR1 set per scanline of triangle |
| 3    | Affine helper      | ADDR1 reads from FX tile map       |

By default, Addr1 Mode is 0 (traditional VERA behavior).

---

### Line Draw Helper (Addr1 Mode=1)

Draws pixels along a line using Bresenham-style addressing. The "X increment" register tracks the fractional subpixel position.

#### Setup

1. Set ADDR1 to the address of the starting pixel.
2. Determine the octant (see table below) for the direction of the line.
3. Set ADDR1 increment to the **always-increment** direction:
   - 8-bit mode: +1, −1, −320, or +320
   - 4-bit mode: +0.5, −0.5, −160, or +160
4. Set ADDR0 increment to the **sometimes-increment** direction (same set of values). In line draw mode, ADDR0's increment is applied to ADDR1 when the subpixel accumulator overflows.
5. For 4-bit mode half-increments: the Nibble Increment bit (ADDRx_H bit 2) and optionally DECR bit (bit 3) provide ±0.5 addressing. The main Address Increment (bits 7:4) must be 0 and 4-bit Mode must be set in FX_CTRL.

ADDRx_H ($04000022): Bits 7:4 = Address Increment; Bit 3 = DECR; Bit 2 = Nibble Increment; Bit 1 = Nibble Address; Bit 0 = VRAM Address bit 16.

#### Octant Table

| Octant | 8-bit ADDR1 incr | 8-bit ADDR0 incr | 4-bit ADDR1 incr | 4-bit ADDR0 incr |
|--------|-------------------|-------------------|-------------------|-------------------|
| 0      | +1                | −320              | +0.5              | −160              |
| 1      | −320              | +1                | −160              | +0.5              |
| 2      | −320              | −1                | −160              | −0.5              |
| 3      | −1                | −320              | −0.5              | −160              |
| 4      | −1                | +320              | −0.5              | +160              |
| 5      | +320              | −1                | +160              | −0.5              |
| 6      | +320              | +1                | +160              | +0.5              |
| 7      | +1                | +320              | +0.5              | +160              |

#### Line Draw Increment Registers (DCSEL=3) - Write-only

| Address     | Name            | Bits                                                                                                     |
|-------------|-----------------|----------------------------------------------------------------------------------------------------------|
| $04000029   | FX_X_INCR_L     | X Increment bits −2:−9 (signed fractional)                                                               |
| $0400002A   | FX_X_INCR_H     | Bit 7 = X Incr. 32×; Bits 6:2 = X Increment bits 5:1 (signed integer); Bit 1 = X Incr. bit 0; Bit 0 = X Incr. bit −1 |

Increment registers are 15-bit signed fixed-point numbers. For line draw mode, the range should be 0.0 to 1.0 inclusive. Only the X incrementer is used; depending on octant it may represent either x or y pixel increments.

**Side effect:** Writing FX_X_INCR_H automatically sets the fractional part (lower 9 bits) of X Position to 0.5 (half a pixel) and clears the lowest pixel position bit (overflow bit). This centers the starting position at pixel center. There is no need to set the higher bits of X position, since the FX X position accumulator only tracks the fractional (subpixel) part during line drawing.

---

### Polygon Filler Helper (Addr1 Mode=2)

Fills horizontal spans of a triangle. ADDR0 serves as the scanline base address; VERA calculates ADDR1 for each span.

#### Setup (assuming 320-pixel-wide screen)

1. Set ADDR0 to the y-position of the top point of the triangle at x=0 (left edge). Increment = +320 (8-bit) or +160 (4-bit). ADDR0 increments one scanline per step; VERA uses it as the base for computing ADDR1.
2. No need to set ADDR1 - VERA sets it automatically.
3. Calculate slopes (dx/dy) for left and right edges. Slopes can be negative and exceed 1.0, covering the full 180° downward range.
4. Set ADDR1 increment to +1 (8-bit) or +0.5 (4-bit). Alternatively +4 if using 32-bit cache writes.
5. Set left slope into X increment registers, right slope into Y/X2 increment registers (DCSEL=3).
   - **Important:** Slopes must be set to **half** the actual per-line increment, because the polygon filler increments in two steps per line.
6. Increment registers are 15-bit signed fixed-point: 6 integer bits, 9 fractional bits, plus 1 bit for 32× multiplier.

#### Polygon/Affine Increment Registers (DCSEL=3) - Write-only

| Address     | Name            | Bits                                                                                              |
|-------------|-----------------|---------------------------------------------------------------------------------------------------|
| $04000029   | FX_X_INCR_L     | X Increment bits −2:−9 (signed fractional)                                                        |
| $0400002A   | FX_X_INCR_H     | Bit 7 = X Incr. 32×; Bits 6:1 = X Increment bits 5:0 (signed integer); Bit 0 = X Incr. bit −1   |
| $0400002B   | FX_Y_INCR_L     | Y/X2 Increment bits −2:−9 (signed fractional)                                                     |
| $0400002C   | FX_Y_INCR_H     | Bit 7 = Y/X2 Incr. 32×; Bits 6:1 = Y/X2 Increment bits 5:0 (signed integer); Bit 0 = Y/X2 Incr. bit −1 |

**Note:** The source document presents this register differently for line draw vs polygon/affine - in line draw, bit 1 ("X Incr. (0)") is shown as a separate named field, while here it is grouped into the "X Increment (5:0)" field. The underlying hardware register format is the same in both modes.

**Side effect:** Writing the high byte of either increment register automatically sets the lower 9 bits of the corresponding position register to 0.5 (half-pixel center).

#### Position Registers (DCSEL=4) - Write-only

| Address     | Name          | Bits                                                                       |
|-------------|---------------|----------------------------------------------------------------------------|
| $04000029   | FX_X_POS_L    | X Position bits 7:0                                                        |
| $0400002A   | FX_X_POS_H    | Bit 7 = X Pos bit −9; Bits 6:3 = unused; Bits 2:0 = X Position bits 10:8  |
| $0400002B   | FX_Y_POS_L    | Y/X2 Position bits 7:0                                                     |
| $0400002C   | FX_Y_POS_H    | Bit 7 = Y/X2 Pos bit −9; Bits 6:3 = unused; Bits 2:0 = Y/X2 Position bits 10:8 |

Set X position and Y/X2 position to the x-pixel-position of the top triangle point.

#### Triangle Fill Loop

1. **Read DATA1** - returns no useful data but performs:
   - Increment/decrement X1 and X2 positions by their increment values
   - Set ADDR1 = ADDR0 + X1
2. **Read FX_POLY_FILL_L** ($0400002B, DCSEL=5) - returns fill length info:

**FX_POLY_FILL_L variants (read-only):**

| Mode                              | Bit 7           | Bits 6:5      | Bit 4        | Bits 3:1        | Bit 0 |
|-----------------------------------|-----------------|---------------|--------------|-----------------|-------|
| 8-bit (4-bit Mode=0)              | Fill Len ≥ 16   | X Position 1:0| Fill Len 3:0 | (part of above) | 0     |
| 4-bit (4-bit Mode=1, 2-bit Poly=0)| Fill Len ≥ 8    | X Position 1:0| X Position 2 | Fill Len 2:0    | 0     |

8-bit mode bit layout: Bit 7 = Fill Len ≥ 16; Bits 6:5 = X Position 1:0; Bits 4:1 = Fill Len 3:0; Bit 0 = 0.

4-bit mode bit layout: Bit 7 = Fill Len ≥ 8; Bits 6:5 = X Position 1:0; Bit 4 = X Position 2; Bits 3:1 = Fill Len 2:0; Bit 0 = 0.

3. If Fill Len ≥ 16 (8-bit) or ≥ 8 (4-bit), also **read FX_POLY_FILL_H** ($0400002C, DCSEL=5):
   - Bits 7:1 = Fill Len 9:3; Bit 0 = always 0.
4. Combined, these give 10 bits of fill length. **Important:** When bits 8 and 9 are both 1, the fill length is negative - do not draw the line.
5. **Write to DATA1** Fill Len times (ADDR1 auto-increments each write).
6. **Read DATA0** - increments X1 and X2 again.
7. Repeat from step 1 until all lines of the triangle part are drawn.

There is also a 2-bit polygon mode (2-bit Polygon bit in FX_TILEBASE), detailed in the FX tutorial.

---

### Affine Helper (Addr1 Mode=3)

Reads tile data from an FX-specific tile area when ADDR1 is read. Used for texture mapping, rotation, and scaling.

#### FX Tile/Map Registers (DCSEL=2) - Write-only

| Address     | Name          | Bits                                                                        |
|-------------|---------------|-----------------------------------------------------------------------------|
| $0400002A   | FX_TILEBASE   | Bits 7:2 = FX Tile Base Address bits 16:11; Bit 1 = Affine Clip Enable; Bit 0 = 2-bit Polygon |
| $0400002B   | FX_MAPBASE    | Bits 7:2 = FX Map Base Address bits 16:11; Bits 1:0 = Map Size              |

- **FX_TILEBASE** points to a set of 8×8 tiles in either 4-bit or 8-bit depth. Supports up to 256 tile definitions. Can overlap traditional layer tile bases.
- **FX_MAPBASE** points to a square tile map, one byte per tile, no attribute bytes (unlike traditional layer tile maps).
- **Affine Clip Enable** (FX_TILEBASE bit 1): when X/Y positions are outside the tile map, always reads data from tile 0. Default behavior wraps positions to the opposite side of the map.
- **Map Size:**

| Map Size | Dimensions |
|----------|------------|
| 0        | 2×2        |
| 1        | 8×8        |
| 2        | 32×32      |
| 3        | 128×128    |

#### Transparent Writes

The **Transparent Writes** toggle (FX_CTRL bit 7) is especially useful in affine mode. When set, writing a zero leaves the byte (or nibble in 4-bit mode) at the target address intact. This toggle is not limited to affine mode and affects writes to both DATA0 and DATA1.

#### Position and Increment Registers

The X and Y position registers (DCSEL=4) indirectly set ADDR1 to the source pixel in the tile map. The X and Y increments (DCSEL=3) determine the step after each ADDR1 read. The affine helper supports the full range of increment values, including negative values.

Position registers (DCSEL=4, write-only):

| Address     | Name          | Bits                                                                       |
|-------------|---------------|----------------------------------------------------------------------------|
| $04000029   | FX_X_POS_L    | X Position bits 7:0                                                        |
| $0400002A   | FX_X_POS_H    | Bit 7 = X Pos bit −9; Bits 6:3 = unused; Bits 2:0 = X Position bits 10:8  |
| $0400002B   | FX_Y_POS_L    | Y/X2 Position bits 7:0                                                     |
| $0400002C   | FX_Y_POS_H    | Bit 7 = Y/X2 Pos bit −9; Bits 6:3 = unused; Bits 2:0 = Y/X2 Position bits 10:8 |

Increment registers (DCSEL=3, write-only) - same format as polygon filler (6 contiguous integer bits):

| Address     | Name            | Bits                                                                                              |
|-------------|-----------------|---------------------------------------------------------------------------------------------------|
| $04000029   | FX_X_INCR_L     | X Increment bits −2:−9 (signed fractional)                                                        |
| $0400002A   | FX_X_INCR_H     | Bit 7 = X Incr. 32×; Bits 6:1 = X Increment bits 5:0 (signed integer); Bit 0 = X Incr. bit −1   |
| $0400002B   | FX_Y_INCR_L     | Y/X2 Increment bits −2:−9 (signed fractional)                                                     |
| $0400002C   | FX_Y_INCR_H     | Bit 7 = Y/X2 Incr. 32×; Bits 6:1 = Y/X2 Increment bits 5:0 (signed integer); Bit 0 = Y/X2 Incr. bit −1 |

---

### 32-bit Cache

A 32-bit (4-byte) cache that can be filled from VRAM reads, set directly, and written back to VRAM.

#### Cache Fill

When the CPU reads DATA0 or DATA1 with **Cache Fill Enable** set (FX_CTRL bit 5), the value read is copied into an indexed location in the 32-bit cache:
- 8-bit mode: caches one byte per read
- 4-bit mode: caches one nibble per read

After caching, the index auto-increments and wraps to 0 after the last position.

**Cache index** is controlled via FX_MULT ($0400002C, DCSEL=2):

| Bit(s) | Field                        | Description                                                       |
|--------|------------------------------|-------------------------------------------------------------------|
| 7      | Reset Accumulator            | Write 1 to reset multiplier accumulator to 0                      |
| 6      | Accumulate                   | Write 1 to trigger accumulate operation                           |
| 5      | Subtract Enable              | Switch accumulation from add to subtract                           |
| 4      | Multiplier Enable            | Enable hardware multiplier                                        |
| 3:2    | Cache Byte Index             | 8-bit mode cache index (0-3)                                      |
| 1      | Cache Nibble Index           | 4-bit mode cache index (uses bits 3:1, range 0-7)                 |
| 0      | Two-byte Cache Incr. Mode    | Cycle between two adjacent bytes (8-bit mode only)                |

The index can also be set explicitly via FX_MULT. In 8-bit mode, bits 3:2 select byte (0-3). In 4-bit mode, bits 3:1 select nibble (0-7).

**Two-byte Cache Increment Mode** (FX_MULT bit 0): Instead of cycling through all 4 bytes, the index alternates between two adjacent bytes: 0↔1 or 2↔3. 8-bit mode only.

#### Setting Cache Data Directly (DCSEL=6) - Write-only

| Address     | Name            | Bits                                                       |
|-------------|-----------------|------------------------------------------------------------|
| $04000029   | FX_CACHE_L      | Cache bits 7:0 / Multiplicand bits 7:0 (signed)            |
| $0400002A   | FX_CACHE_M      | Cache bits 15:8 / Multiplicand bits 15:8 (signed)          |
| $0400002B   | FX_CACHE_H      | Cache bits 23:16 / Multiplier bits 7:0 (signed)            |
| $0400002C   | FX_CACHE_U      | Cache bits 31:24 / Multiplier bits 15:8 (signed)           |

Writing to these registers sets the cache directly without affecting the cache index.

#### Writing Cache to VRAM

When **Cache Write Enable** (FX_CTRL bit 6) is set, writing to DATA0 or DATA1 writes cache contents to VRAM at the current address (4-byte-aligned region).

The value written to DATA0/DATA1 acts as a **nibble mask**:
- 0-bit = write the corresponding cache byte to VRAM
- 1-bit = mask (do not write) that cache byte

Examples: writing `$00` flushes all 32 bits; writing `$0F` (`%00001111`) writes bytes 2 and 3 only.

#### Transparent Writes with Cache

When **Transparent Writes** (FX_CTRL bit 7) is enabled, zero bytes (or zero nibbles in 4-bit mode) in the cache are treated as transparent and not written to VRAM. This applies to cache writes as well.

#### One-byte Cache Cycling

When **One-byte Cache Cycling** (FX_CTRL bit 4) is enabled and DATA0 or DATA1 is written:
- The byte at the current cache index is written to VRAM
- If Cache Write Enable is also set, the byte is duplicated 4 times when written to VRAM

Cache index cycling is normally triggered only by reads with cache fill enabled. However, in polygon mode, reading DATA0 also triggers cycling when one-byte cycling is enabled (even without cache fill enabled).

---

### Multiplier and Accumulator

The 32-bit cache doubles as input to the hardware multiplier when **Multiplier Enable** (FX_MULT bit 4) is set.

Cache layout for multiplication:
- FX_CACHE_L + FX_CACHE_M (DCSEL=6) = Multiplicand (16-bit signed)
- FX_CACHE_H + FX_CACHE_U (DCSEL=6) = Multiplier (16-bit signed)

#### Multiplication Procedure

1. Set DCSEL=2, configure FX_CTRL with Cache Write Enable + Multiplier Enable.
2. Set DCSEL=6. Load multiplicand into FX_CACHE_L/M, multiplier into FX_CACHE_H/U.
3. Reset accumulator: either write 1 to FX_MULT bit 7 (DCSEL=2), or read FX_ACCUM_RESET ($04000029, DCSEL=6).
4. Set ADDR0 to the destination address in VRAM (with appropriate increment).
5. Write to DATA0 - this triggers the multiplication and writes the result to VRAM instead of the cache contents.
6. Read back the result via DATA0 with increment set to step through the 4 result bytes.

**Note:** VERA pre-fetches VRAM contents whenever the address pointer is changed or incremented (even with increment=0). This means stale data can be latched in one data port if VRAM is changed via the other port. Use only one ADDR/DATA port for multiply + readback to avoid this.

#### Accumulation

To accumulate the sum (or difference) of several multiplications:

- **Trigger accumulation:** Write 1 to FX_MULT bit 6 (DCSEL=2), or read FX_ACCUM ($0400002A, DCSEL=6).
- After accumulation, the result is stored back into the accumulator.
- **Default operation:** multiply then add. Set **Subtract Enable** (FX_MULT bit 5) to switch to multiply then subtract.
- If the accumulator is nonzero, subsequent multiplications via VRAM cache write are offset by the accumulator value (added or subtracted), but the accumulator itself is not changed by these subsequent operations.

#### Accumulator Reset

Two methods:
1. Write 1 to FX_MULT bit 7 (DCSEL=2)
2. Read FX_ACCUM_RESET ($04000029, DCSEL=6)

---

### 16-bit Hop

**16-bit Hop** (FX_CTRL bit 3) enables a special ADDR1 increment mode for reading pairs of bytes:

| ADDR1 Increment | Actual Increments (alternating) |
|------------------|---------------------------------|
| +4               | +1, +3, +1, +3, …               |
| +320             | +1, +319, +1, +319, …           |
| Any other value  | No hop; normal increment        |

After setting the 16-bit Hop bit, writing to ADDRx_L resets the hop alignment so the first increment is +1.

Useful for reading a series of 16-bit values after chained multiplication operations.

---

## Composite Video Notes

- **Color bleed:** Adjacent certain colors bleed on composite displays. Choose colors carefully.
- **Overscan:** CRT displays crop edges unpredictably. Use border, scaling, or 240P mode to keep content visible.
- **240P:** Screen modes 3 or 11 auto-enable 240P when NTSC/RGB output is active (320×240 content).
