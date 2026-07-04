# STM32F103 UART Bootloader (Bare Metal)

Project bootloader OTA qua UART cho **STM32F103C8T6**, viết hoàn toàn bare metal (không HAL).  
Mục tiêu: nạp bootloader một lần bằng ST-Link, sau đó cập nhật application qua serial mà không cần debugger.

---

## Yêu cầu phần cứng

| Thành phần | Ghi chú |
|------------|---------|
| MCU | STM32F103C8T6 — 64 KB Flash, 20 KB RAM |
| UART | USART1 — **PA9** (TX), **PA10** (RX) |
| Baud rate | 115200, 8N1 (cần PLL 72 MHz để chính xác) |
| Debugger | ST-Link — nạp bootloader lần đầu |
| USB-UART | CP2102 / CH340 / FT232... |

---

## Sơ đồ bộ nhớ Flash (dual-slot A/B)

```
Địa chỉ        Kích thước   Mục đích
─────────────────────────────────────────
0x08000000     22 KB        Bootloader (project này)
0x08005800     20.5 KB      APP1 slot (header 256 B + payload)
0x08005900                  APP1 payload / vector table (link app1)
0x0800AA00     20.5 KB      APP2 slot (header 256 B + payload)
0x0800AB00                  APP2 payload / vector table (link app2)
0x0800FC00      1 KB        OTA Config (active_slot, cờ OTA)
0x08010000     (end)
```

**Dual-slot:** OTA chỉ ghi slot **inactive** (APP1 active → OTA vào APP2, và ngược lại). Thành công thì đổi `active_slot` và nhảy app mới. Image phải **link đúng địa chỉ payload** của slot đích; sai slot bị từ chối sớm (sau ~264 B DATA).

Linker bootloader: `FLASH LENGTH = 22K` (`STM32F103C8TX_FLASH.ld`).

---

## Luồng hoạt động Bootloader

Mỗi lần **Reset**, CPU luôn bắt đầu từ bootloader tại `0x08000000`.

```
Reset
  │
  ├─ Khởi tạo RCC, GPIO, UART
  ├─ In banner "STM32F103 Bootloader"
  │
  ├─ Cờ OTA (0xDEADBEEF tại 0x0800FC00) được set?
  │     └─ Có → vào OTA mode
  │
  ├─ App hợp lệ (active slot, fallback slot còn lại)?
  │     ├─ Không → vào OTA mode
  │     └─ Có → chờ 3 giây
  │               ├─ Nhận 'U' → vào OTA mode
  │               └─ Hết timeout → nhảy sang slot active
  │
  └─ OTA mode: ghi firmware vào slot inactive → verify → đổi active_slot → nhảy app
```

### Kiểm tra app hợp lệ

Sau khi parse header (`IMG1`), đọc vector table tại **payload** (`slot + 0x100`):

| Word | Offset | Ý nghĩa | Điều kiện |
|------|--------|---------|-----------|
| 0 | `+0` | Initial Stack Pointer | Nằm trong RAM (`0x20000000` – `0x20005000`) |
| 1 | `+4` | Reset Handler | Trong vùng payload slot, bit 0 = 1 (Thumb) |

Ngoài ra: SHA-256 payload khớp header, ECDSA-P256 với `boot_pubkey`.

### Nhảy sang Application

Không gọi hàm thông thường — phải thay đổi context CPU:

1. Tắt interrupt
2. Clear NVIC enable bits
3. `SCB_VTOR = payload_addr` (APP1: `0x08005900`, APP2: `0x0800AB00`)
4. Set MSP từ word[0] của app
5. `bx` Reset Handler của app (word[1], Thumb bit = 1)

---

## Giao thức UART OTA

### Frame format

```
[STX=0x02][CMD 1B][LEN 2B LE][PAYLOAD...][CRC16 2B LE][ETX=0x03]
```

- **CRC16**: CCITT, poly `0x1021`, tính trên `CMD + LEN + PAYLOAD`
- **Chunk size**: tối đa 256 byte mỗi frame DATA

### Commands

| CMD | Giá trị | Payload | Mô tả |
|-----|---------|---------|-------|
| START | `0x01` | `size(4B)` + `crc32(4B)` | Bắt đầu OTA, erase vùng app |
| DATA | `0x02` | `seq(2B)` + `data(≤256B)` | Gửi 1 chunk firmware |
| END | `0x03` | (rỗng) | CRC32 + verify chữ ký, ACK, nhảy app |

### Response

| RSP | Giá trị | Ý nghĩa |
|-----|---------|---------|
| ACK | `0x06` | OK |
| NACK | `0x15` | Lỗi (CRC sai, seq sai, flash lỗi...) |

### Ví dụ luồng gửi firmware

```
PC → MCU:  START {size, crc32}   →  MCU → PC: ACK
PC → MCU:  DATA {seq=0, chunk}   →  MCU → PC: ACK
PC → MCU:  DATA {seq=1, chunk}   →  MCU → PC: ACK
...
PC → MCU:  END                   →  MCU → PC: ACK → jump app
```

### CRC32 firmware

- Polynomial: `0xEDB88320`
- Init: `0xFFFFFFFF`, Final: `~crc`
- Tính trên toàn bộ file `.bin`

---

## Application (project riêng)

Bootloader chiếm **22 KB** đầu flash. App build **2 bản** (link khác nhau), ký bằng `tools/sign_image.py` trước khi OTA.

### 1. Linker script

```ld
/* APP1 */
FLASH (rx) : ORIGIN = 0x08005900, LENGTH = 0x5100

/* APP2 */
FLASH (rx) : ORIGIN = 0x0800AB00, LENGTH = 0x5100
```

`APP_FLASH_ORIGIN` / `SCB_VTOR` trong app phải trùng `ORIGIN`.

### 2. Trigger OTA từ Application

Gửi `'U'` trên UART (app ghi `magic = 0xDEADBEEF` tại `0x0800FC00` rồi system reset), hoặc nhấn `'U'` trong cửa sổ 3 s của bootloader.

### 3. OTA từ PC

```bat
python tools/main.py COM6 tools/app_2.bin --sign --enter-ota
```

`--sign` tạo image `[header 256 B | payload]`. PC retry tối đa 3 lần nếu không ACK.

---

## Cấu trúc source code

Project tách **app** (mỏng) và **lib** (copy được sang project khác). Include paths (CubeIDE):

| Path | Vai trò |
|------|---------|
| `Inc/` | Header app (`main.h`) |
| `lib/drivers/inc` | Driver MCU |
| `lib/boot/inc` | Bootloader / OTA / image |
| `lib/crypto/inc` | Public crypto (`#include <crypto/...>`) |
| `lib/crypto/src` | Header nội bộ uECC (`.inc`, `uECC_vli.h`) |
| `lib/crypto/inc/crypto` | Cho `#include "uECC.h"` trong uECC |

```
10_bootloader/
├── Inc/                          # App only
│   └── main.h                    # Gom include cho main.c
│
├── Src/                          # App only
│   ├── main.c                    # Entry: init → verify/jump hoặc OTA
│   ├── syscalls.c                # newlib stubs
│   └── sysmem.c                  # _sbrk
│
├── Startup/
│   └── startup_stm32f103c8tx.s   # Vector table bootloader @ 0x08000000
│
├── lib/                          # ★ Copy nguyên folder này sang project khác
│   │
│   ├── drivers/                  # Lớp MCU bare-metal (không phụ thuộc OTA)
│   │   ├── inc/
│   │   │   ├── conf.h            # HSE/HSI, SYSCLK 72 MHz, tick priority
│   │   │   ├── types.h           # status_t, …
│   │   │   ├── rcc.h / system.h  # Clock
│   │   │   ├── gpio.h            # GPIO register map
│   │   │   ├── uart.h            # USART1 + ring buffer RX API
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
│   │   │   ├── img.h             # img_header_t (IMG1), header 256 B
│   │   │   ├── verify.h          # boot_verify_slot (SHA-256 + ECDSA)
│   │   │   ├── ota.h             # Frame START/DATA/END, ota_run()
│   │   │   └── boot_pubkey.h     # Public key nhúng trong BL
│   │   └── src/
│   │       ├── bootloader.c      # jump_to_application, slot A/B, OTA flag
│   │       ├── img.c             # Parse header / payload bounds
│   │       ├── verify.c          # Verify chữ ký trước jump
│   │       ├── ota.c             # UART OTA, early slot check, ACK
│   │       └── boot_pubkey.c     # Key (tools/keygen.py generate)
│   │
│   └── crypto/                   # SHA-256 + ECDSA P-256 (micro-ecc)
│       ├── inc/crypto/
│       │   ├── crypto.h          # API bọc (sha256, ecdsa verify, memcmp_ct)
│       │   ├── sha256.h
│       │   └── uECC.h
│       └── src/
│           ├── crypto.c, sha256.c, uECC.c
│           └── uECC_*.h, *.inc   # Nội bộ uECC
│
├── tools/                        # PC host (Python)
│   ├── main.py                   # OTA client (ACK retry ×3, --sign, --enter-ota)
│   ├── sign_image.py             # Ký payload → signed slot image
│   ├── keygen.py                 # Sinh boot_pubkey.c
│   ├── serial.py                 # Monitor / gửi 'U'
│   └── requirements.txt          # pyserial, ecdsa
│
├── STM32F103C8TX_FLASH.ld        # BL: FLASH 22K @ 0x08000000
└── README.md
```

### Phân tầng phụ thuộc

```
Src/main.c
    │
    ├─► lib/drivers   (uart, flash, clock, gpio, …)
    │
    └─► lib/boot      (ota, verify, jump)
            │
            └─► lib/crypto   (sha256, ecdsa)
```

- `lib/drivers` **không** include `boot` / `crypto`.
- `lib/boot` dùng `drivers` + `crypto`; **không** phụ thuộc `main.h`.
- `lib/crypto` độc lập.

### Copy `lib/` sang project khác

1. Copy nguyên thư mục `lib/`.
2. Thêm include paths như bảng trên.
3. Thêm source path `lib` (hoặc từng `lib/*/src`).
4. Include giữ nguyên kiểu: `#include <uart.h>`, `#include <bootloader.h>`, `#include <crypto/crypto.h>`.

---

## Trạng thái implement

| Module | Trạng thái | Ghi chú |
|--------|------------|---------|
| Drivers (RCC, GPIO, UART IRQ, Flash, Tick) | Done | `lib/drivers` |
| Boot jump + VTOR | Done | `jump_to_application()` |
| Image header + ECDSA verify | Done | `lib/boot` + `lib/crypto` |
| OTA UART A/B | Done | Chỉ ghi inactive; early reject sai slot |
| Tool PC | Done | `tools/main.py`, `sign_image.py` |

---

## Build và nạp

1. Mở project `10_bootloader` trong **STM32CubeIDE**
2. **Project → Clean** rồi **Build** (sau khi đổi cấu trúc `lib/`)
3. Nạp bootloader bằng ST-Link
4. Serial 115200 8N1 — PLL 72 MHz (`SystemClock_Config`)

### printf qua UART

`printf()` → `_write()` trong `main.c` → `uart1_write()` (PA9).

---

## Lưu ý quan trọng

1. **Nạp bootloader bằng ST-Link** — sau đó update app qua UART, không cần ST-Link.
2. **Không ghi đè bootloader** — OTA chỉ erase/ghi từ `APP1_START` trở đi.
3. **App phải link đúng slot** — APP1 `@ 0x08005900`, APP2 `@ 0x0800AB00`; nạp nhầm bị `OTA: wrong slot img`.
4. **`Debug/`** trong `.gitignore` — không commit artifact build.

---

## Tham chiếu

- [RM0008 — STM32F103 Reference Manual](https://www.st.com/resource/en/reference_manual/cd00171190.pdf) — Flash, vector table
- [PM0214 — Cortex-M3 Programming Manual](https://www.st.com/resource/en/programming_manual/cd00443574.pdf) — VTOR, exception model
- Lesson series: UART bare metal → NVIC → Bootloader (project này)
