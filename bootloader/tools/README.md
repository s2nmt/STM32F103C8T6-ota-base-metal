# Bootloader host tools

Python utilities on the PC to **read**, **inspect**, **sign**, and **transfer** firmware to the STM32F103 over UART OTA.

Run all commands from this directory (`bootloader/tools/`).

---

## Table of contents

1. [Setup](#setup)
2. [Files and tools overview](#files-and-tools-overview)
3. [Firmware file types](#firmware-file-types)
4. [Signed image layout (file analysis)](#signed-image-layout-file-analysis)
5. [End-to-end OTA flow](#end-to-end-ota-flow)
6. [UART protocol (how files are transferred)](#uart-protocol-how-files-are-transferred)
7. [main.py — OTA upload](#mainpy--ota-upload)
8. [sign_image.py — Sign firmware](#sign_imagepy--sign-firmware)
9. [keygen.py — Public key](#keygenpy--public-key)
10. [serial.py — UART monitor](#serialpy--uart-monitor)
11. [Practical examples](#practical-examples)
12. [Troubleshooting](#troubleshooting)

---

## Setup

```bash
cd bootloader/tools
pip install -r requirements.txt
```

| Package | Used by |
|---------|---------|
| `pyserial` | `main.py`, `serial.py` |
| `ecdsa` | `keygen.py`, `sign_image.py`, `main.py --sign` |

Requires **Python 3**.

---

## Files and tools overview

```
bootloader/tools/
├── main.py           # OTA client — read file, sign (optional), send over UART
├── sign_image.py     # Sign raw payload → full slot image (256 B header + payload)
├── keygen.py         # Generate boot_pubkey.c for the bootloader
├── serial.py         # Simple UART monitor
├── requirements.txt
├── app_1.bin         # Sample payload / image (if present)
├── app_2.bin
├── uartver1.hex      # Sample Intel HEX firmware
└── uartver2.hex
```

| Script | Purpose |
|--------|---------|
| `main.py` | Read `.bin`/`.hex` → compute CRC32 → send START/DATA/END frames over serial |
| `sign_image.py` | Read raw payload → SHA-256 + ECDSA-P256 → write 256 B header |
| `keygen.py` | Write fixed dev public key to `../lib/boot/src/boot_pubkey.c` |
| `serial.py` | UART log viewer only; does not transfer firmware |

---

## Firmware file types

The tools accept **two file formats** and **two content types**:

### File format

| Extension | How `main.py` reads it | Notes |
|-----------|------------------------|-------|
| `.bin` | `path.read_bytes()` — raw bytes | Most common; linker/GCC output |
| `.hex` | Parse Intel HEX into a contiguous buffer | Supports record `0x04` (extended address) |

`load_firmware()` in `main.py`:

```python
def load_firmware(path):
    if path.suffix.lower() == ".hex":
        return load_intel_hex(path)   # merge regions, pad gaps with 0xFF
    return path.read_bytes()
```

For `.hex` files, the tool uses `min(addr)` → `max(addr)`; missing bytes are filled with `0xFF`.

### Content type (important)

| Type | Typical size | Produced by | When uploading |
|------|--------------|-------------|----------------|
| **Raw payload** | few KB (app code only) | Linker, `ORIGIN = slot + 0x100` | Use `--sign` or sign first with `sign_image.py` |
| **Signed slot image** | payload + 256 B header | `sign_image.py` or `main.py --sign` | Upload directly; **no** `--sign` needed |

**Quick rules:**

- A `.bin` from the application `Debug/` folder (linked at `0x08005900` or `0x0800AB00`) is a **raw payload**.
- A file named `*_signed.bin` is a **slot image** (first 256 B are the `IMG1` header).

### Link address must match the inactive slot

The bootloader only writes to the **inactive** A/B slot:

| Slot | Slot base | Payload (link ORIGIN) | OTA target when |
|------|-----------|----------------------|-----------------|
| APP1 | `0x08005800` | `0x08005900` | Active = APP2 |
| APP2 | `0x0800AA00` | `0x0800AB00` | Active = APP1 |

An image linked for the wrong slot (e.g. built for APP1 while inactive is APP2) gets **NACK** after the first ~264 bytes of DATA.

---

## Signed image layout (file analysis)

The slot image sent over the wire looks like:

```
┌─────────────────────────────────────────────────────────────┐
│  Offset 0x000        │  256 B header (IMG1)                 │
├──────────────────────┼──────────────────────────────────────┤
│  Offset 0x100        │  Payload (vector table + code)       │
└─────────────────────────────────────────────────────────────┘
```

### 256 B header (`img_header_t`)

| Offset | Field | Type | Value / meaning |
|--------|-------|------|-----------------|
| `0x00` | `magic` | `uint32` LE | `0x494D4731` (`"IMG1"`) |
| `0x04` | `image_type` | `uint32` LE | `1` = app |
| `0x08` | `payload_offset` | `uint32` LE | `0x100` (256) |
| `0x0C` | `payload_size` | `uint32` LE | Payload byte count |
| `0x10` | `version` | `uint32` LE | Image version |
| `0x14` | `reserved0` | `uint32` LE | `0` |
| `0x18` | `sha256` | 32 B | SHA-256 of the **payload** |
| `0x38` | `signature` | 64 B | ECDSA-P256 (r‖s, 32+32) |
| `0x78` | `reserved1` | 32 B | `0` |
| `0x98`–`0xFF` | padding | — | `0x00` to fill 256 B |

### Payload (from byte 256 onward)

The first 8 bytes of the payload are the Cortex-M3 **vector table**:

| Payload offset | Content | Bootloader check |
|----------------|---------|------------------|
| `+0` | Initial SP | Within RAM `0x20000000`–`0x20005000` |
| `+4` | Reset Handler | Inside slot payload region, bit 0 = 1 (Thumb) |

### Inspecting a `.bin` file on the PC

**Step 1 — Check the first 4 bytes (magic):**

```bash
# PowerShell
Format-Hex -Path app_signed.bin -Count 16
```

- `31 47 4D 49` (LE) = `IMG1` → signed slot image.
- `00 59 00 08` or similar → usually a **raw payload** (word 0 = SP, word 1 = reset); sign first.

**Step 2 — Read header fields:**

```python
import struct
data = open("app_signed.bin", "rb").read()
magic, img_type, off, size, ver, _ = struct.unpack("<6I", data[:24])
print(f"magic=0x{magic:08X} type={img_type} offset={off} size={size} ver={ver}")
payload = data[off:off+size]
```

**Step 3 — Check link address (reset vector):**

```python
sp, reset = struct.unpack("<2I", payload[:8])
print(f"SP=0x{sp:08X}  Reset=0x{reset:08X}")
# APP1: Reset usually 0x08005xxx | APP2: 0x0800Axxx or 0x0800Bxxx
```

**Step 4 — CRC32 (same as bootloader at END):**

`main.py` uses IEEE CRC32 (reflected poly `0xEDB88320`), init `0xFFFFFFFF`, final xor `~crc`:

```python
# crc32_firmware() in main.py — computed over ALL bytes sent on the wire
# (header + payload for a signed image)
```

### What `sign_image.py` does

```
payload.bin  ──►  SHA-256(payload)
              ──►  ECDSA-P256 sign(digest)
              ──►  build 256 B header
              ──►  signed.bin = header || payload
```

---

## End-to-end OTA flow

```
┌──────────┐    build      ┌─────────────┐    --sign      ┌──────────────┐
│ Linker   │ ────────────► │ payload.bin │ ─────────────► │ *_signed.bin │
│ (GCC)    │               │ (raw)       │  sign_image  │ slot image   │
└──────────┘               └─────────────┘                └──────┬───────┘
                                                                  │
┌──────────┐   'U' + reset   ┌─────────────┐   UART OTA          │
│ Running  │ ──────────────► │ Bootloader  │ ◄───────────────────┘
│ app      │  ota_flag      │ OTA mode    │   START/DATA/END
└──────────┘                 └──────┬──────┘
                                    │ verify SHA-256 + ECDSA
                                    │ write inactive slot
                                    │ update active_slot
                                    ▼
                              Jump to new app
```

### On the MCU (bootloader `main.c`)

1. Reset → banner `STM32F103 Bootloader`
2. OTA flag (`0xDEADBEEF` at `0x0800FC00`)? → `OTA RX ready` → wait for frames
3. No flag → verify app → **jump immediately** (no 3 s wait)
4. No valid app → enter OTA mode

### On the PC (`main.py --enter-ota`)

1. Wait for app to print `Hello world` / `app X started`
2. Send `'U'` → app writes flash flag → reset
3. Wait for `OTA flag set` → `OTA RX ready` (do **not** send more `'U'` after reset)
4. Send START → DATA (256 B chunks) → END
5. MCU prints `OTA OK` then jumps to the app

---

## UART protocol (how files are transferred)

Link settings: **115200 8N1**, USART1 (PA9 TX / PA10 RX).

### Frame layout

```
 STX   CMD   LEN_L  LEN_H   PAYLOAD...   CRC_L  CRC_H   ETX
 02    1B     1B     1B      0–258 B       1B     1B     03
```

- `LEN` = payload length, **little-endian** 16-bit.
- **CRC16-CCITT**: poly `0x1021`, init `0xFFFF`, over `CMD + LEN + PAYLOAD`.
- Responses: `ACK = 0x06`, `NACK = 0x15` (single byte, no STX/ETX).

### Three commands

#### 1. START (`CMD = 0x01`)

**Payload (8 bytes):**

| Offset | Field | Type |
|--------|-------|------|
| 0 | `fw_size` | `uint32` LE — total bytes to write (including header if signed) |
| 4 | `fw_crc32` | `uint32` LE — CRC32 of the full file |

MCU: pick inactive slot → erase required flash pages → ACK.

#### 2. DATA (`CMD = 0x02`)

**Payload:** `seq (uint16 LE)` + `chunk (≤ 256 B)`

| Field | Meaning |
|-------|---------|
| `seq` | Starts at `0`, increments; must match in order |
| `chunk` | Next consecutive slice of the file |

MCU: write flash → ACK. After ~264 B, checks that the vector table matches the inactive slot.

#### 3. END (`CMD = 0x03`)

**Payload:** empty.

MCU:

1. Compare bytes written vs `fw_size`
2. CRC32 full flash region vs `fw_crc32`
3. Parse `IMG1` header, verify SHA-256 + ECDSA signature
4. Write `active_slot` to `0x0800FC00`
5. Print `OTA OK <jump_addr>` → ACK → jump to app

### Example START frame (hex)

For a 8492-byte file with CRC32 `0x12345678`:

```
START payload:  [size LE 4B][crc32 LE 4B]
CRC16 body:     CMD + LEN + payload
Full frame:     02 01 08 00 [8 bytes payload] [CRC16 LE] 03
```

Use `-v` / `--verbose` on `main.py` to print a hex dump of each frame.

### Retries

`send_frame()` retries up to **3 times** if no ACK is received. START timeout = `3 × --timeout` (default 30 s because flash erase is slow).

---

## `main.py` — OTA upload

### How the file is read and prepared before transfer

```python
upload_path = firmware_path
if sign:
    upload_path = sign_firmware_file(...)   # calls sign_image.py

fw = load_firmware(upload_path)           # read .bin or .hex
fw_crc = crc32_firmware(fw)               # CRC32 over full image
fw_size = len(fw)
```

Then `fw` is split into 256 B chunks and wrapped in DATA frames.

### Usage

```bash
python main.py <port> <firmware> [options]
```

| Argument | Description |
|----------|-------------|
| `port` | Serial port (`COM3`, `/dev/ttyUSB0`, …) |
| `firmware` | `.bin` or `.hex` file |
| `--baud` | Baud rate (default `115200`) |
| `--enter-ota` | Send `'U'` to the app, wait for bootloader OTA mode |
| `--sign` | Sign raw payload before upload |
| `--version N` | Image version when using `--sign` (default `1`) |
| `--timeout S` | ACK timeout per frame in seconds (default `10`) |
| `-v`, `--verbose` | Hex-dump TX/RX |

### When to use `--sign` / `--enter-ota`

| Scenario | Suggested command |
|----------|-------------------|
| App is running; start OTA from scratch | `python main.py COM3 app.bin --sign --enter-ota` |
| Already have `*_signed.bin` | `python main.py COM3 app_signed.bin --enter-ota` |
| Bootloader already in OTA mode (`OTA RX ready`) | `python main.py COM3 app_signed.bin` |

### How to read the log

```
Firmware: app_2_signed.bin
  Size   : 8492 bytes          ← total bytes transferred (header + payload)
  CRC32  : 0xA1B2C3D4          ← MCU checks this at END
  Port   : COM3 @ 115200
--- UART log ---
[MCU] OTA requested, resetting...
[MCU] STM32F103 Bootloader
[MCU] OTA flag set             ← flash flag OK
[MCU] OTA RX ready             ← ready for frames
[PC]  TX START try 1/3 ...
[PC]  DATA chunk 1/34 (3%)
...
[MCU] OTA OK 08005900
[PC]  OTA SUCCESS
```

---

## `sign_image.py` — Sign firmware

Separate signing from upload (useful for CI or pre-checking images).

```bash
python sign_image.py <payload.bin> -o <signed.bin> [--version N]
```

| Argument | Description |
|----------|-------------|
| `payload` | Raw app `.bin` (linked at `slot + 0x100`) |
| `-o`, `--output` | Output signed slot image (required) |
| `--version` | Version field in header (default `1`) |

Example:

```bash
python sign_image.py ../../application/Debug/app_2.bin -o app_2_signed.bin --version 2
python main.py COM3 app_2_signed.bin --enter-ota
```

---

## `keygen.py` — Public key

Writes `../lib/boot/src/boot_pubkey.c` from a fixed dev seed (`stm32-bare-metal-dev-fixture`).

```bash
python keygen.py
```

Then **rebuild and reflash the bootloader**. The MCU public key must match the private key used by `sign_image.py`.

> **Security note:** The dev seed is for learning/prototyping only. Use a real key pair in production.

---

## `serial.py` — UART monitor

Log viewer only; does not perform OTA transfer.

Edit constants at the top of the file:

```python
PORT = "COM6"
BAUD = 115200
```

```bash
python serial.py                 # read-only
python serial.py --send-update   # send a test string (not full OTA protocol)
```

### Note on the filename `serial.py`

This file shadows the `pyserial` package name. `main.py` removes the `tools/` directory from `sys.path` before `import serial`. Always run:

```bash
python main.py COM3 firmware.bin
```

Do not import `main` as a module from elsewhere.

---

## Practical examples

### First-time setup

```bash
# 1. Flash bootloader with ST-Link (once)
# 2. Install dependencies
pip install -r requirements.txt

# 3. Build app in IDE, get .bin from Debug/
# 4. OTA app into inactive slot
python main.py COM3 ../../application/Debug/app_2.bin --sign --enter-ota --version 1
```

### Next OTA (swap app 1 ↔ app 2)

Build the app linked to the **inactive slot payload** address, then:

```bash
python main.py COM3 app_new_signed.bin --enter-ota
```

### Sign first, upload later

```bash
python sign_image.py raw.bin -o signed.bin --version 3
python main.py COM3 signed.bin --enter-ota -v
```

---

## Troubleshooting

| UART symptom | Common cause |
|--------------|--------------|
| No `OTA flag set` | App failed to write flash flag → rebuild app (flash driver) |
| `OTA requested` then immediate `app X started` | OTA flag not persisted; bootloader jumped straight to app |
| START timeout / no ACK | MCU not at `OTA RX ready`; frame sent too early; try `--enter-ota` |
| NACK on first DATA | Bad CRC16 frame; wrong sequence number |
| NACK after ~1 chunk | Image linked for **wrong slot** (wrong ORIGIN) |
| `OTA: verify fail` / NACK at END | Not signed; wrong key; corrupt file |
| `OTA: CRC fail` | PC file differs from what MCU received; transfer error |

### Quick checklist

1. Does the file start with `IMG1` magic? (if not → add `--sign`)
2. Is the reset vector inside the inactive slot payload region?
3. Does the UART log show `OTA RX ready` before the PC sends START?
4. Use `-v` to inspect frame hex and ACK/NACK

---

## Further reading

- Flash map and MCU boot flow: [../README.md](../README.md)
- C header layout: `../lib/boot/inc/img.h`
- MCU protocol implementation: `../lib/boot/src/ota.c`, `../lib/boot/inc/ota.h`
