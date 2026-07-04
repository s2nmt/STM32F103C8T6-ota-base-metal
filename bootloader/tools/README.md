# Bootloader host tools

Python utilities to sign firmware images and upload them to the STM32F103 UART bootloader over serial.

Run all commands from this directory (`bootloader/tools/`).

---

## Setup

```bash
pip install -r requirements.txt
```

| Package | Used by |
|---------|---------|
| `pyserial` | `main.py`, `serial.py` |
| `ecdsa` | `keygen.py`, `sign_image.py`, `main.py --sign` |

Requires **Python 3**.

---

## Tools overview

| Script | Purpose |
|--------|---------|
| `main.py` | OTA client — sign (optional) and upload `.bin` / `.hex` |
| `sign_image.py` | Build a signed slot image (`256 B header + payload`) |
| `keygen.py` | Generate `boot_pubkey.c` for the bootloader |
| `serial.py` | Simple UART monitor / send OTA request |

Sample images: `uartver1.hex`, `uartver2.hex`.

---

## Typical workflow

1. Flash the bootloader once with ST-Link.
2. Build the application linked for the target slot payload address:
   - APP1: `0x08005900`
   - APP2: `0x0800AB00`
3. Upload with signing and OTA entry:

```bash
python main.py COM6 path/to/app.bin --sign --enter-ota
```

On success the bootloader verifies the image (SHA-256 + ECDSA), switches `active_slot`, and jumps to the app.

---

## `main.py` — OTA upload

Uploads a firmware file using the UART OTA protocol (`START` / `DATA` / `END`). Retries up to 3 times per frame if ACK is missing.

### Usage

```bash
python main.py <port> <firmware> [options]
```

| Argument | Description |
|----------|-------------|
| `port` | Serial port (e.g. `COM6`, `/dev/ttyUSB0`) |
| `firmware` | `.bin` or `.hex` file |
| `--baud` | Baud rate (default `115200`) |
| `--enter-ota` | Send `'U'` before upload (bootloader 3 s window or app OTA request) |
| `--sign` | Sign a raw app `.bin` first (calls `sign_image.py`) |
| `--version N` | Image version when using `--sign` (default `1`) |
| `--timeout S` | ACK timeout per frame in seconds (default `10`) |
| `-v`, `--verbose` | Hex-dump TX/RX for debugging |

### Examples

```bash
# Upload an already-signed slot image
python main.py COM6 signed_app.bin --enter-ota

# Sign a raw app .bin and upload
python main.py COM6 app.bin --sign --enter-ota --version 2

# Verbose transfer
python main.py COM6 app.bin --sign --enter-ota -v
```

### Notes

- With `--sign`, the input must be a **raw app payload** (vector table at offset 0 of the `.bin`), linked at `slot + 0x100`.
- Without `--sign`, the file must already be a full slot image: `[256 B IMG1 header | payload]`.
- CRC32 is computed over the bytes sent on the wire (the signed image when `--sign` is used).

---

## `sign_image.py` — Sign payload

Builds a signed slot image:

```
[256 B header (IMG1, SHA-256, ECDSA-P256)][payload]
```

Uses the same deterministic development key as `keygen.py`.

### Usage

```bash
python sign_image.py <payload.bin> -o <signed.bin> [--version N]
```

| Argument | Description |
|----------|-------------|
| `payload` | Raw app `.bin` (linked at slot + `0x100`) |
| `-o`, `--output` | Output signed slot image (required) |
| `--version` | Image version field (default `1`) |

### Example

```bash
python sign_image.py ../../application/Debug/app.bin -o app_signed.bin --version 1
```

Then upload without `--sign`:

```bash
python main.py COM6 app_signed.bin --enter-ota
```

---

## `keygen.py` — Public key for the bootloader

Derives a development ECDSA P-256 key from a fixed seed and writes the public key into:

```
../lib/boot/src/boot_pubkey.c
```

### Usage

```bash
python keygen.py
```

Rebuild and reflash the bootloader after regenerating the key so verify matches images signed by `sign_image.py` / `main.py --sign`.

**Security note:** The seed (`stm32-bare-metal-dev-fixture`) is a **dev fixture only**. Do not use it for production devices; replace with a real key pair and update both signing and `boot_pubkey.c`.

---

## `serial.py` — UART monitor

Opens a fixed port and prints received bytes. Useful to watch bootloader/app logs.

### Configuration

Edit the constants at the top of `serial.py`:

```python
PORT = "COM6"
BAUD = 115200
```

### Usage

```bash
# Read-only monitor
python serial.py

# Send "UPDATE" then monitor (app-side OTA trigger test)
python serial.py --send-update
```

### Note on the filename

`serial.py` shadows the `pyserial` package name if this directory is on `sys.path`. `main.py` and `serial.py` remove the tools directory from `sys.path` before importing `pyserial`. Prefer running scripts as `python main.py ...` from this folder rather than importing them as modules from elsewhere.

---

## Protocol summary

Matches the bootloader OTA stack (`lib/boot`):

```
[STX=0x02][CMD][LEN LE][PAYLOAD...][CRC16 LE][ETX=0x03]
```

| CMD | Value | Role |
|-----|-------|------|
| START | `0x01` | Image size + CRC32, erase inactive slot |
| DATA | `0x02` | Sequence number + up to 256 B chunk |
| END | `0x03` | Final CRC32 + signature verify, then jump |

Responses: `ACK (0x06)`, `NACK (0x15)`.

Default link settings: **115200 8N1**, USART1 (PA9 TX / PA10 RX).

For flash layout and boot flow, see [../README.md](../README.md).
