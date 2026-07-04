# STM32F103 UART Bootloader (Bare Metal)

UART OTA bootloader for the **STM32F103C8T6**, written fully bare metal (no HAL).

Flash the bootloader once with an ST-Link, then update the application over serial without a debugger.

---

## Hardware requirements

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
0x08000000     22 KB     Bootloader (this project)
0x08005800     20.5 KB   APP1 slot (256 B header + payload)
0x08005900               APP1 payload / vector table (link app1)
0x0800AA00     20.5 KB   APP2 slot (256 B header + payload)
0x0800AB00               APP2 payload / vector table (link app2)
0x0800FC00      1 KB     OTA config (active_slot, OTA flag)
0x08010000     (end)
```

**Dual-slot:** OTA writes only the **inactive** slot (APP1 active → OTA targets APP2, and vice versa). On success it updates `active_slot` and jumps to the new app. The image must be **linked to the correct payload address** for the target slot; a wrong slot is rejected early (after ~264 B of DATA).

Bootloader linker: `FLASH LENGTH = 22K` (`STM32F103C8TX_FLASH.ld`).

---

## Bootloader flow

On every **reset**, the CPU always starts in the bootloader at `0x08000000`.

```
Reset
  │
  ├─ Init RCC, GPIO, UART
  ├─ Print banner "STM32F103 Bootloader"
  │
  ├─ OTA flag set (0xDEADBEEF at 0x0800FC00)?
  │     └─ Yes → enter OTA mode
  │
  ├─ Valid app (active slot, then fallback inactive)?
  │     ├─ No  → enter OTA mode
  │     └─ Yes → wait 3 seconds
  │               ├─ Receive 'U' → enter OTA mode
  │               └─ Timeout     → jump to active slot
  │
  └─ OTA mode: write firmware to inactive slot → verify → update active_slot → jump app
```

### Application validity checks

After parsing the header (`IMG1`), the bootloader reads the vector table at the **payload** (`slot + 0x100`):

| Word | Offset | Meaning | Condition |
|------|--------|---------|-----------|
| 0 | `+0` | Initial Stack Pointer | Within RAM (`0x20000000` – `0x20005000`) |
| 1 | `+4` | Reset Handler | Within slot payload region, bit 0 = 1 (Thumb) |

Also required: SHA-256 of the payload matches the header, and ECDSA-P256 verifies with `boot_pubkey`.

### Jumping to the application

This is not a normal function call — the CPU context must be switched:

1. Disable interrupts
2. Clear NVIC enable bits
3. `SCB_VTOR = payload_addr` (APP1: `0x08005900`, APP2: `0x0800AB00`)
4. Set MSP from word[0] of the app
5. `bx` the app Reset Handler (word[1], Thumb bit = 1)

---

## UART OTA protocol

### Frame format

```
[STX=0x02][CMD 1B][LEN 2B LE][PAYLOAD...][CRC16 2B LE][ETX=0x03]
```

- **CRC16**: CCITT, poly `0x1021`, over `CMD + LEN + PAYLOAD`
- **Chunk size**: up to 256 bytes per DATA frame

### Commands

| CMD | Value | Payload | Description |
|-----|-------|---------|-------------|
| START | `0x01` | `size(4B)` + `crc32(4B)` | Start OTA, erase app region |
| DATA | `0x02` | `seq(2B)` + `data(≤256B)` | Send one firmware chunk |
| END | `0x03` | (empty) | CRC32 + signature verify, ACK, jump app |

### Responses

| RSP | Value | Meaning |
|-----|-------|---------|
| ACK | `0x06` | OK |
| NACK | `0x15` | Error (bad CRC, bad seq, flash error, …) |

### Example transfer

```
PC → MCU:  START {size, crc32}   →  MCU → PC: ACK
PC → MCU:  DATA {seq=0, chunk}   →  MCU → PC: ACK
PC → MCU:  DATA {seq=1, chunk}   →  MCU → PC: ACK
...
PC → MCU:  END                   →  MCU → PC: ACK → jump app
```

### Firmware CRC32

- Polynomial: `0xEDB88320`
- Init: `0xFFFFFFFF`, Final: `~crc`
- Computed over the entire `.bin` file

---

## Application (separate project)

The bootloader occupies the first **22 KB** of flash. Build **two app images** (different link addresses) and sign them with `tools/sign_image.py` before OTA.

### 1. Linker script

```ld
/* APP1 */
FLASH (rx) : ORIGIN = 0x08005900, LENGTH = 0x5100

/* APP2 */
FLASH (rx) : ORIGIN = 0x0800AB00, LENGTH = 0x5100
```

`APP_FLASH_ORIGIN` / `SCB_VTOR` in the app must match `ORIGIN`.

### 2. Trigger OTA from the application

Send `'U'` on UART (the app writes `magic = 0xDEADBEEF` at `0x0800FC00` then system-resets), or press `'U'` during the bootloader’s 3-second window.

### 3. OTA from the PC

```bat
python tools/main.py COM6 tools/app_2.bin --sign --enter-ota
```

`--sign` builds the image `[256 B header | payload]`. The PC retries up to 3 times if ACK is missing.

---

## Source layout

The project splits a thin **app** layer from a reusable **lib**. CubeIDE include paths:

| Path | Role |
|------|------|
| `Inc/` | App headers (`main.h`) |
| `lib/drivers/inc` | MCU drivers |
| `lib/boot/inc` | Bootloader / OTA / image |
| `lib/crypto/inc` | Public crypto (`#include <crypto/...>`) |
| `lib/crypto/src` | Internal uECC headers (`.inc`, `uECC_vli.h`) |
| `lib/crypto/inc/crypto` | For `#include "uECC.h"` inside uECC |

```
bootloader/
├── Inc/                          # App only
│   └── main.h                    # Aggregated includes for main.c
│
├── Src/                          # App only
│   ├── main.c                    # Entry: init → verify/jump or OTA
│   ├── syscalls.c                # newlib stubs
│   └── sysmem.c                  # _sbrk
│
├── Startup/
│   └── startup_stm32f103c8tx.s   # Bootloader vector table @ 0x08000000
│
├── lib/                          # ★ Copy this folder into other projects
│   │
│   ├── drivers/                  # Bare-metal MCU layer (no OTA dependency)
│   │   ├── inc/
│   │   │   ├── conf.h            # HSE/HSI, SYSCLK 72 MHz, tick priority
│   │   │   ├── types.h           # status_t, …
│   │   │   ├── rcc.h / system.h  # Clock
│   │   │   ├── gpio.h            # GPIO register map
│   │   │   ├── uart.h            # USART1 + ring-buffer RX API
│   │   │   ├── nvic.h            # NVIC / SysTick / IRQn
│   │   │   ├── tick.h            # delay_ms, SysTick
│   │   │   ├── flash.h           # Unlock / erase / write
│   │   │   ├── iwdg.h            # Independent watchdog
│   │   │   ├── init.h / msp.h    # init() board hook
│   │   └── src/
│   │       ├── rcc.c, system.c, gpio.c, uart.c, nvic.c
│   │       ├── tick.c, flash.c, iwdg.c, init.c, msp.c
│   │
│   ├── boot/                     # Bootloader + OTA stack
│   │   ├── inc/
│   │   │   ├── bootloader.h      # Flash map, active/inactive, jump, OTA flag
│   │   │   ├── img.h             # img_header_t (IMG1), 256 B header
│   │   │   ├── verify.h          # boot_verify_slot (SHA-256 + ECDSA)
│   │   │   ├── ota.h             # START/DATA/END frames, ota_run()
│   │   │   └── boot_pubkey.h     # Public key embedded in BL
│   │   └── src/
│   │       ├── bootloader.c      # jump_to_application, A/B slots, OTA flag
│   │       ├── img.c             # Header parse / payload bounds
│   │       ├── verify.c          # Signature verify before jump
│   │       ├── ota.c             # UART OTA, early slot check, ACK
│   │       └── boot_pubkey.c     # Key (generated by tools/keygen.py)
│   │
│   └── crypto/                   # SHA-256 + ECDSA P-256 (micro-ecc)
│       ├── inc/crypto/
│       │   ├── crypto.h          # Wrapper API (sha256, ecdsa verify, memcmp_ct)
│       │   ├── sha256.h
│       │   └── uECC.h
│       └── src/
│           ├── crypto.c, sha256.c, uECC.c
│           └── uECC_*.h, *.inc   # Internal uECC
│
├── tools/                        # PC host (Python)
│   ├── main.py                   # OTA client (ACK retry ×3, --sign, --enter-ota)
│   ├── sign_image.py             # Sign payload → signed slot image
│   ├── keygen.py                 # Generate boot_pubkey.c
│   ├── serial.py                 # Monitor / send 'U'
│   └── requirements.txt          # pyserial, ecdsa
│
├── STM32F103C8TX_FLASH.ld        # BL: FLASH 22K @ 0x08000000
└── README.md
```

### Dependency layers

```
Src/main.c
    │
    ├─► lib/drivers   (uart, flash, clock, gpio, …)
    │
    └─► lib/boot      (ota, verify, jump)
            │
            └─► lib/crypto   (sha256, ecdsa)
```

- `lib/drivers` does **not** include `boot` / `crypto`.
- `lib/boot` uses `drivers` + `crypto`; it does **not** depend on `main.h`.
- `lib/crypto` is standalone.

### Copying `lib/` into another project

1. Copy the entire `lib/` directory.
2. Add the include paths from the table above.
3. Add source path `lib` (or each `lib/*/src`).
4. Keep includes as: `#include <uart.h>`, `#include <bootloader.h>`, `#include <crypto/crypto.h>`.

---

## Implementation status

| Module | Status | Notes |
|--------|--------|-------|
| Drivers (RCC, GPIO, UART IRQ, Flash, Tick) | Done | `lib/drivers` |
| Boot jump + VTOR | Done | `jump_to_application()` |
| Image header + ECDSA verify | Done | `lib/boot` + `lib/crypto` |
| UART OTA A/B | Done | Writes inactive only; early reject wrong slot |
| PC tools | Done | `tools/main.py`, `sign_image.py` |

---

## Build and flash

1. Open the `bootloader` project in **STM32CubeIDE**
2. **Project → Clean**, then **Build** (especially after changing the `lib/` layout)
3. Program the bootloader with ST-Link
4. Serial 115200 8N1 — PLL 72 MHz (`SystemClock_Config`)

### printf over UART

`printf()` → `_write()` in `main.c` → `uart1_write()` (PA9).

---

## Important notes

1. **Flash the bootloader with ST-Link** — after that, update the app over UART without ST-Link.
2. **Do not overwrite the bootloader** — OTA only erases/writes from `APP1_START` onward.
3. **App must be linked to the correct slot** — APP1 `@ 0x08005900`, APP2 `@ 0x0800AB00`; wrong slot yields `OTA: wrong slot img`.
4. **`Debug/`** is in `.gitignore` — do not commit build artifacts.

---

## References

- [RM0008 — STM32F103 Reference Manual](https://www.st.com/resource/en/reference_manual/cd00171190.pdf) — Flash, vector table
- [PM0214 — Cortex-M3 Programming Manual](https://www.st.com/resource/en/programming_manual/cd00443574.pdf) — VTOR, exception model
- Lesson series: UART bare metal → NVIC → Bootloader (this project)
