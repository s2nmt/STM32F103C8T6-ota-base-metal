# STM32 Bare-metal Tool Series

A hands-on bare-metal project for the **STM32F103C8T6** (Blue Pill): register-level drivers, a UART bootloader, and secure over-the-air (OTA) firmware updates — **no STM32 HAL**.

Flash the bootloader once with an ST-Link, then update the application over serial without a debugger.

---

## What this project is

This repository is a learning-oriented firmware stack that shows how to:

- Drive STM32 peripherals directly through registers (RCC, GPIO, UART, NVIC, Flash, IWDG)
- Build a custom bootloader that verifies and jumps to an application
- Update firmware over UART with a dual-slot A/B layout (safe rollback)
- Sign images with **SHA-256** and **ECDSA P-256**, and verify them before boot

It is aimed at engineers and students who want to understand bootloaders and OTA beyond CubeMX-generated code.

---

## Features

| Feature | Description |
|---------|-------------|
| Bare-metal drivers | Clock, GPIO, UART (IRQ + ring buffer), Flash, SysTick, IWDG |
| Dual-slot OTA | Writes only the inactive slot; swaps `active_slot` on success |
| Signed images | 256-byte `IMG1` header + payload; SHA-256 and ECDSA-P256 verify |
| UART protocol | Framed `START` / `DATA` / `END` with CRC16 and ACK/NACK |
| PC tools | Python client to sign and upload firmware over serial |
| Portable `lib/` | Drivers, boot, and crypto can be reused in other projects |

---

## Repository layout

```
STM32_Bare-metal_Tool_Series/
├── bootloader/          # Bootloader firmware + host tools
│   ├── Src/             # Entry point (verify / jump / OTA)
│   ├── lib/
│   │   ├── drivers/     # Bare-metal MCU drivers
│   │   ├── boot/        # Slot map, OTA, image verify, jump
│   │   └── crypto/      # SHA-256 + micro-ecc (ECDSA P-256)
│   └── tools/           # Python OTA client, signing, keygen
│
└── application/         # Example application (blink + UART)
    ├── Src/             # App entry; press 'U' to request OTA
    └── Inc/
```

Detailed notes live in each subproject:

- [bootloader/README.md](bootloader/README.md) — memory map, protocol, build, and tools
- [application/README.md](application/README.md) — application-side notes

---

## Hardware

| Item | Notes |
|------|--------|
| MCU | STM32F103C8T6 — 64 KB Flash, 20 KB RAM |
| UART | USART1 — **PA9** (TX), **PA10** (RX) |
| Baud rate | 115200, 8N1 (PLL at 72 MHz for accurate baud) |
| Debugger | ST-Link — program the bootloader once |
| USB–UART | CP2102 / CH340 / FT232 (or similar) |

---

## Flash memory map (dual-slot A/B)

```
Address        Size      Purpose
────────────────────────────────────────────
0x08000000     22 KB     Bootloader
0x08005800     20.5 KB   APP1 slot (256 B header + payload)
0x08005900               APP1 payload / vector table
0x0800AA00     20.5 KB   APP2 slot (256 B header + payload)
0x0800AB00               APP2 payload / vector table
0x0800FC00      1 KB     OTA config (active_slot, OTA flag)
0x08010000     (end)
```

OTA always programs the **inactive** slot. On success the bootloader updates `active_slot` and jumps to the new image. A failed update leaves the previous active slot intact.

Applications must be linked to the correct payload address (`0x08005900` for APP1, `0x0800AB00` for APP2) and signed before upload.

---

## Boot flow

```
Reset → bootloader @ 0x08000000
  │
  ├─ OTA flag set (0xDEADBEEF)?  → OTA mode
  ├─ No valid signed app?        → OTA mode
  └─ Valid app
        └─ Wait 3 s for 'U' on UART
              ├─ 'U' received → OTA mode
              └─ Timeout      → verify active (then inactive) slot → jump
```

From the running application, send **`U`** over UART to set the OTA flag and reset into the bootloader.

---

## Quick start

### 1. Build and flash the bootloader

1. Open `bootloader/` in **STM32CubeIDE**
2. **Project → Clean**, then **Build**
3. Program with ST-Link

### 2. Build the application

1. Open `application/` in STM32CubeIDE
2. Link for the target slot (APP1 or APP2 payload origin)
3. Build and produce a `.bin` (or `.hex`)

### 3. Sign and upload over UART

```bash
cd bootloader/tools
pip install -r requirements.txt

# Example (Windows COM port):
python main.py COM6 path/to/app.bin --sign --enter-ota
```

`--sign` builds a signed slot image (`[256 B header | payload]`). `--enter-ota` sends `U` so the device enters OTA mode.

Generate a new key pair and embed the public key with `keygen.py` when you rotate keys (see the bootloader README).

---

## Layering

```
bootloader Src/main.c
    ├── lib/drivers   (uart, flash, clock, gpio, …)
    └── lib/boot      (ota, verify, jump)
            └── lib/crypto   (sha256, ecdsa)
```

- `lib/drivers` does not depend on boot or crypto
- `lib/boot` uses drivers and crypto only
- `lib/crypto` is standalone

Copy the whole `lib/` tree into another project and add the include/source paths described in [bootloader/README.md](bootloader/README.md).

---

## Requirements

- **STM32CubeIDE** (or another GCC ARM toolchain) for firmware builds
- **Python 3** with `pyserial` and `ecdsa` for host tools
- ST-Link for the first bootloader flash

---

## References

- [RM0008 — STM32F103 Reference Manual](https://www.st.com/resource/en/reference_manual/cd00171190.pdf)
- [PM0214 — Cortex-M3 Programming Manual](https://www.st.com/resource/en/programming_manual/cd00443574.pdf)

---

## License

Provided as-is for learning and experimentation. Use and adapt at your own risk.
