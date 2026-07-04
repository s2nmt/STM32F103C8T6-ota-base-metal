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
0x08000000     16 KB        Bootloader (project này)
0x08004000     23 KB        APP1 — slot A
0x08009C00     24 KB        APP2 — slot B
0x0800FC00      1 KB        OTA Config (active slot, cờ update)
0x08010000     (end)
```

**Dual-slot:** OTA ghi vào slot **inactive**; thành công thì đổi `active_slot` và nhảy app mới. Thất bại thì slot active cũ vẫn còn — rollback tự nhiên.

Linker script của bootloader giới hạn flash ở **16 KB** (`STM32F103C8TX_FLASH.ld`).

Application build **2 bản** (hoặc cùng binary) link tại `0x08004000` (APP1) hoặc `0x08009C00` (APP2) tùy slot đang test.

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

Bootloader đọc vector table tại địa chỉ slot (APP1 hoặc APP2):

| Word | Offset | Ý nghĩa | Điều kiện |
|------|--------|---------|-----------|
| 0 | `+0` | Initial Stack Pointer | Nằm trong RAM (`0x20000000` – `0x20005000`) |
| 1 | `+4` | Reset Handler | Trong vùng app flash, bit 0 = 1 (Thumb mode) |

### Nhảy sang Application

Không gọi hàm thông thường — phải thay đổi context CPU:

1. Tắt interrupt
2. Clear NVIC enable bits
3. `SCB_VTOR = 0x08004000`
4. Set MSP từ word[0] của app
5. Gọi Reset Handler của app (word[1])

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
| END | `0x03` | (rỗng) | Kết thúc, verify CRC32, nhảy app |

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

Bootloader chỉ chiếm 16 KB đầu flash. Application cần project riêng:

### 1. Linker script (APP1 — slot mặc định)

```ld
FLASH (rx) : ORIGIN = 0x08004000, LENGTH = 23K
RAM  (xrw): ORIGIN = 0x20000000, LENGTH = 20K
```

APP2 dùng `ORIGIN = 0x08009C00, LENGTH = 24K` khi build bản thứ hai.

### 2. Relocate vector table

Thêm vào đầu `main()`:

```c
#define SCB_VTOR (*(volatile uint32_t *)0xE000ED08UL)
SCB_VTOR = 0x08004000UL;
```

### 3. Trigger OTA từ Application

Ghi struct config vào `0x0800FC00`, rồi system reset:

```c
typedef struct {
    uint32_t magic;         // 0xDEADBEEF → yêu cầu OTA
    uint32_t active_slot;   // OTA_SLOT_APP1 (1) hoặc OTA_SLOT_APP2 (2)
    uint32_t app_size;
    uint32_t crc32;
    uint32_t version;
} ota_config_t;

#define AIRCR (*(volatile uint32_t *)0xE000ED0CUL)
AIRCR = 0x05FA0004UL;  // System reset
```

---

## Cấu trúc source code

### Hiện tại (đã có)

```
10_bootloader/
├── Inc/
│   ├── gpio.h          # GPIO register map
│   ├── rcc.h           # Clock enable
│   ├── uart.h          # USART1 register map + baud defines
│   ├── nvic.h          # NVIC/SysTick + IRQn_Type
│   └── main.h
├── Src/
│   ├── main.c          # Entry (đang test UART + printf)
│   ├── uart.c          # uart1_init(), uart_brr_calc()
│   ├── nvic.c          # nvic_irq_enable/disable
│   ├── gpio.c
│   ├── rcc.c
│   └── syscalls.c      # newlib stubs (_write weak)
├── Startup/
│   └── startup_stm32f103c8tx.s
├── STM32F103C8TX_FLASH.ld
└── .gitignore          # ignore Debug/, Release/
```

### Kế hoạch (chưa implement)

```
├── Inc/
│   ├── bootloader.h    # Memory map, jump app, OTA flag
│   ├── flash.h         # Flash unlock/erase/write
│   └── ota.h           # Giao thức OTA UART
├── Src/
│   ├── bootloader.c    # app_is_valid(), jump_to_application()
│   ├── flash.c         # Flash driver bare metal
│   └── ota.c           # Parser frame START/DATA/END
```

---

## Trạng thái implement

| Module | Trạng thái | Ghi chú |
|--------|------------|---------|
| RCC / GPIO | Done | Clock + pin config |
| UART1 + printf | Done | `_write()` redirect qua PA9 |
| NVIC + IRQ | Done | `USART1_IRQn = 37`, RX interrupt |
| Linker 16 KB | TODO | Hiện dùng full 64 KB |
| Flash driver | TODO | unlock / erase / write |
| Boot jump | TODO | `app_is_valid()`, `jump_to_application()` |
| OTA protocol | TODO | START / DATA / END |
| Tool PC | TODO | Script Python gửi `.bin` |

---

## Build và nạp

1. Mở project `10_bootloader` trong **STM32CubeIDE**
2. **Project → Build Project**
3. Nạp bằng ST-Link: **Run → Debug**
4. Mở serial monitor — baud khớp với `uart1_init()`

### Lưu ý baud rate

Mặc định `UART1_PCLK_HZ = 8 MHz` (HSI, chưa bật PLL):

| Constant | Thực tế @ 8 MHz |
|----------|-----------------|
| `UART_BAUD_9600` | ~9600 — khớp tốt |
| `UART_BAUD_115200` | ~111111 — lệch |

Để dùng `UART_BAUD_115200` chính xác: bật PLL (72 MHz), cập nhật `UART1_PCLK_HZ` trong `uart.h`.

### printf qua UART

`printf()` gọi `_write()` (newlib). Project override `_write()` trong `main.c` để gửi qua `uart1_send_string()`.

---

## Flash driver (kế hoạch)

Driver thao tác trực tiếp register `FLASH` (`0x40022000`):

| Hàm | Chức năng |
|-----|-----------|
| `flash_unlock()` / `flash_lock()` | Mở/khóa flash |
| `flash_erase_page(addr)` | Xóa 1 page (1 KB) |
| `flash_erase_region(start, size)` | Xóa nhiều page |
| `flash_write_buffer(addr, data, len)` | Ghi half-word vào flash |

Bootloader **chỉ erase/ghi** vùng `0x08004000` – `0x0800FBFF`, không đụng vùng bootloader.

---

## Lưu ý quan trọng

1. **Nạp bootloader bằng ST-Link** — sau khi có bootloader, update app qua UART không cần ST-Link.
2. **Không ghi đè bootloader** — nạp nhầm app tại `0x08000000` sẽ mất OTA; cần ST-Link nạp lại bootloader.
3. **Thư mục `Debug/`** đã được `.gitignore` — chỉ commit source, không commit build artifacts.
4. Tham khảo implementation đầy đủ tại project anh em: `10_OTA_bootloader/`.

---

## Tham chiếu

- [RM0008 — STM32F103 Reference Manual](https://www.st.com/resource/en/reference_manual/cd00171190.pdf) — Flash memory interface, interrupt vector table (Table 63)
- [PM0214 — Cortex-M3 Programming Manual](https://www.st.com/resource/en/programming_manual/cd00443574.pdf) — VTOR, exception model
- Lesson series: UART bare metal → NVIC → Bootloader (project này)
