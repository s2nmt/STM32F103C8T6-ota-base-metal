#!/usr/bin/env python3
"""
STM32F103 UART OTA client — gui firmware .bin / .hex qua serial.

Can: pip install pyserial

Vi du:
  python main.py COM3 firmware.bin
  python main.py COM3 firmware.bin --baud 115200 --enter-ota
"""

from __future__ import annotations

import argparse
import os
import struct
import subprocess
import sys
import threading
import time
from collections import deque
from datetime import datetime
from pathlib import Path

# tools/serial.py trung ten package pyserial — bo thu muc script khoi path
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if _SCRIPT_DIR in sys.path:
    sys.path.remove(_SCRIPT_DIR)
# Neu van con module 'serial' local da load nham, go bo
if "serial" in sys.modules and not hasattr(sys.modules["serial"], "Serial"):
    del sys.modules["serial"]

try:
    import serial
except ImportError:
    print("Thieu pyserial. Chay: pip install pyserial", file=sys.stderr)
    sys.exit(1)

if not hasattr(serial, "Serial"):
    print(
        "Import nham module 'serial' (khong phai pyserial). "
        "Doi ten tools/serial.py hoac: pip install pyserial",
        file=sys.stderr,
    )
    sys.exit(1)

# --- Protocol constants (ota.h) ---
STX = 0x02
ETX = 0x03
CMD_START = 0x01
CMD_DATA = 0x02
CMD_END = 0x03
RSP_ACK = 0x06
RSP_NACK = 0x15
CHUNK_SIZE = 256
OTA_UPDATE_KEY = b"U"

CMD_NAMES = {CMD_START: "START", CMD_DATA: "DATA", CMD_END: "END"}

_LOG_T0 = time.monotonic()


def log_ts() -> str:
    now = datetime.now()
    return f"{now.strftime('%H:%M:%S')}.{now.microsecond // 1000:03d}"


def log_print(tag: str, message: str) -> None:
    print(f"{log_ts()} {tag}{message}", flush=True)


def hex_dump(data: bytes, prefix: str = "") -> str:
    return prefix + " ".join(f"{b:02X}" for b in data)


def rx_byte_label(b: int) -> str:
    if b == RSP_ACK:
        return "ACK(06)"
    if b == RSP_NACK:
        return "NACK(15)"
    if b == STX:
        return "STX(02)"
    if b == ETX:
        return "ETX(03)"
    if 32 <= b < 127:
        return f"'{chr(b)}'({b:02X})"
    return f"{b:02X}"


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def crc32_firmware(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc >>= 1
    return (~crc) & 0xFFFFFFFF


def build_frame(cmd: int, payload: bytes) -> bytes:
    length = len(payload)
    body = bytes([cmd]) + struct.pack("<H", length) + payload
    crc = crc16_ccitt(body)
    return bytes([STX]) + body + struct.pack("<H", crc) + bytes([ETX])


def load_intel_hex(path: Path) -> bytes:
    mem: dict[int, int] = {}
    ext_addr = 0

    for line in path.read_text(encoding="ascii", errors="ignore").splitlines():
        line = line.strip()
        if not line.startswith(":"):
            continue
        count = int(line[1:3], 16)
        addr = int(line[3:7], 16)
        rtype = int(line[7:9], 16)
        data = bytes(int(line[9 + i * 2 : 11 + i * 2], 16) for i in range(count))

        if rtype == 0x00:
            base = ext_addr + addr
            for i, b in enumerate(data):
                mem[base + i] = b
        elif rtype == 0x04 and len(data) >= 2:
            ext_addr = struct.unpack(">H", data[:2])[0] << 16
        elif rtype == 0x01:
            break

    if not mem:
        raise ValueError(f"Khong co data trong file HEX: {path}")

    start = min(mem)
    end = max(mem) + 1
    return bytes(mem.get(a, 0xFF) for a in range(start, end))


def load_firmware(path: Path) -> bytes:
    if path.suffix.lower() == ".hex":
        return load_intel_hex(path)
    return path.read_bytes()


def sign_firmware_file(path: Path, version: int = 1) -> Path:
    """Ky payload raw -> signed slot image (header 256 B + payload)."""
    tools_dir = Path(__file__).resolve().parent
    sign_script = tools_dir / "sign_image.py"
    if not sign_script.is_file():
        raise FileNotFoundError(f"Khong tim thay {sign_script}")

    # Absolute paths: subprocess cwd=tools/ nen path tuong doi (tools/app.bin) se sai
    src = path.resolve()
    if not src.is_file():
        raise FileNotFoundError(f"Khong tim thay firmware: {src}")

    out = src.with_name(f"{src.stem}_signed{src.suffix}")
    cmd = [
        sys.executable,
        str(sign_script),
        str(src),
        "-o",
        str(out),
        "--version",
        str(version),
    ]
    log_print("[PC] ", f"Ky firmware: {' '.join(cmd)}")
    subprocess.run(cmd, check=True, cwd=str(tools_dir))
    return out


class UartLogReader:
    """Doc UART nen, in log MCU, tach ACK/NACK khoi text printf."""

    def __init__(self, ser: serial.Serial, prefix: str = "[MCU] ", verbose: bool = False):
        self.ser = ser
        self.prefix = prefix
        self.verbose = verbose
        self._queue: deque[int] = deque()
        self._lock = threading.Lock()
        self._stop = threading.Event()
        self._text_buf = bytearray()
        self._thread = threading.Thread(target=self._reader_loop, daemon=True)
        self.saw_ota_ok = False

    def start(self) -> None:
        self._thread.start()

    def stop(self) -> None:
        self._stop.set()
        self._thread.join(timeout=1.0)
        self._flush_text()

    def _reader_loop(self) -> None:
        while not self._stop.is_set():
            waiting = self.ser.in_waiting
            chunk = self.ser.read(waiting if waiting else 1)
            if chunk:
                with self._lock:
                    self._queue.extend(chunk)
            else:
                time.sleep(0.01)

    def _flush_text(self) -> None:
        if not self._text_buf:
            return
        line = self._text_buf.decode("utf-8", errors="replace")
        self._text_buf.clear()
        if line:
            if (
                "OTA OK" in line
                or "app 1 started" in line
                or "app 2 started" in line
            ):
                self.saw_ota_ok = True
            log_print(self.prefix, line)

    def _handle_text_byte(self, byte: int) -> None:
        if byte in (RSP_ACK, RSP_NACK, STX, ETX):
            return
        if byte in (0x0A, 0x0D):
            self._flush_text()
            return
        self._text_buf.append(byte)

    def drain(self, duration_s: float = 0.3) -> None:
        deadline = time.monotonic() + duration_s
        while time.monotonic() < deadline:
            with self._lock:
                while self._queue:
                    b = self._queue.popleft()
                    if b not in (RSP_ACK, RSP_NACK):
                        self._handle_text_byte(b)
            time.sleep(0.01)
        self._flush_text()

    def wait_for_text(self, needle: str, timeout_s: float = 30.0) -> None:
        """Doi chuoi trong log MCU (printf), bo qua ACK/NACK."""
        accum = ""
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            with self._lock:
                while self._queue:
                    b = self._queue.popleft()
                    if b in (RSP_ACK, RSP_NACK, STX, ETX):
                        continue
                    if b in (0x0A, 0x0D):
                        self._flush_text()
                        if needle in accum:
                            return
                        accum = ""
                        continue
                    if 32 <= b < 127:
                        accum += chr(b)
                    self._handle_text_byte(b)
                    if needle in accum:
                        self._flush_text()
                        return
            time.sleep(0.01)
        self._flush_text()
        raise TimeoutError(f"Khong thay '{needle}' tu MCU")

    def wait_for_any_text(self, needles: list[str], timeout_s: float = 30.0) -> str:
        accum = ""
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            with self._lock:
                while self._queue:
                    b = self._queue.popleft()
                    if b in (RSP_ACK, RSP_NACK, STX, ETX):
                        continue
                    if b in (0x0A, 0x0D):
                        self._flush_text()
                        for needle in needles:
                            if needle in accum:
                                return needle
                        accum = ""
                        continue
                    if 32 <= b < 127:
                        accum += chr(b)
                    self._handle_text_byte(b)
                    for needle in needles:
                        if needle in accum:
                            self._flush_text()
                            return needle
            time.sleep(0.01)
        self._flush_text()
        raise TimeoutError(f"Khong thay {needles} tu MCU")

    def discard_bytes(self, count: int, timeout_s: float = 0.05) -> None:
        """Bo qua echo UART (IRQ echo tung byte da gui)."""
        deadline = time.monotonic() + timeout_s
        dropped = 0
        while dropped < count and time.monotonic() < deadline:
            with self._lock:
                while self._queue and dropped < count:
                    self._queue.popleft()
                    dropped += 1
            time.sleep(0.001)

    def _log_raw_rx(self, b: int) -> None:
        if not self.verbose:
            return
        if b in (RSP_ACK, RSP_NACK):
            log_print("[RAW RX] ", rx_byte_label(b))
        elif b not in (STX, ETX) and b not in range(32, 127):
            log_print("[RAW RX] ", rx_byte_label(b))

    def wait_response(self, timeout_s: float) -> int:
        deadline = time.monotonic() + timeout_s
        raw_seen: list[int] = []
        while time.monotonic() < deadline:
            with self._lock:
                while self._queue:
                    b = self._queue.popleft()
                    if b in (RSP_ACK, RSP_NACK):
                        if self.verbose and raw_seen:
                            log_print("[RAW RX] ", f"truoc ACK/NACK: {hex_dump(bytes(raw_seen))}")
                        self._log_raw_rx(b)
                        self._flush_text()
                        return b
                    if self.verbose and b not in (0x0A, 0x0D):
                        raw_seen.append(b)
                        if len(raw_seen) <= 32:
                            self._log_raw_rx(b)
                    self._handle_text_byte(b)
            time.sleep(0.01)
        if self.verbose and raw_seen:
            log_print("[RAW RX] ", f"timeout, da nhan: {hex_dump(bytes(raw_seen))}")
        self._flush_text()
        raise TimeoutError("Het thoi gian cho ACK/NACK tu bootloader")


def send_frame(
    log: UartLogReader,
    ser: serial.Serial,
    cmd: int,
    payload: bytes,
    timeout_s: float,
    verbose: bool = False,
    retries: int = 3,
) -> None:
    """Gui frame, cho ACK. Khong co phan hoi thi gui lai, toi da `retries` lan."""
    frame = build_frame(cmd, payload)
    cmd_name = CMD_NAMES.get(cmd, f"0x{cmd:02X}")
    ack_timeout = timeout_s * 3 if cmd == CMD_START else timeout_s
    last_err: Exception | None = None

    for attempt in range(1, retries + 1):
        if verbose:
            log_print(
                "[PC TX] ",
                f"{cmd_name} try {attempt}/{retries} ({len(frame)} bytes): {hex_dump(frame)}",
            )
            if payload:
                log_print("[PC TX] ", f"payload: {hex_dump(payload)}")
        else:
            log_print("[PC] ", f"TX {cmd_name} try {attempt}/{retries} ({len(frame)} bytes)")

        ser.write(frame)
        ser.flush()

        try:
            rsp = log.wait_response(ack_timeout)
        except TimeoutError as exc:
            last_err = exc
            log_print("[PC] ", f"{cmd_name}: khong ACK (try {attempt}/{retries})")
            continue

        if rsp == RSP_ACK:
            return

        last_err = RuntimeError(f"MCU tra NACK (0x{rsp:02X}) cho CMD 0x{cmd:02X}")
        log_print("[PC] ", f"{cmd_name}: NACK (try {attempt}/{retries})")

    if last_err is not None:
        raise last_err
    raise TimeoutError(f"{cmd_name}: het {retries} lan, khong ACK")


def open_serial_port(port: str, baud: int) -> serial.Serial:
    """Mo COM khong toggle DTR (tranh reset MCU luc mo port)."""
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = baud
    ser.timeout = 0.1
    ser.dsrdtr = False
    ser.rtscts = False
    ser.dtr = False
    ser.rts = False
    ser.open()
    return ser


def _send_ota_key(ser: serial.Serial, verbose: bool = False, times: int = 5) -> None:
    for i in range(times):
        if verbose:
            log_print("[PC TX] ", f"'U' (55) lan {i + 1}/{times}")
        ser.write(OTA_UPDATE_KEY)
        ser.flush()
        time.sleep(0.05)


def enter_ota_mode(log: UartLogReader, ser: serial.Serial, verbose: bool = False) -> None:
    """Gui 'U' cho app de set co OTA, roi doi bootloader in 'OTA RX ready'.

    Khong gui 'U' sau khi app da reset — bootloader khong con cua so 'Press U'.
    """
    app_needles = (
        "app 1 started",
        "app 2 started",
        "Hello world",
    )

    log_print("[PC] ", "Doi app, gui 'U' de yeu cau OTA...")
    deadline = time.monotonic() + 45.0
    ota_requested = False

    while time.monotonic() < deadline:
        try:
            if ota_requested:
                needles = ["OTA RX ready", "OTA flag set"]
            else:
                needles = [
                    *app_needles,
                    "OTA requested, resetting...",
                    "OTA RX ready",
                ]
            hit = log.wait_for_any_text(
                needles,
                timeout_s=min(2.0, max(0.1, deadline - time.monotonic())),
            )
        except TimeoutError:
            continue

        if hit == "OTA RX ready":
            log_print("[PC] ", "Bootloader da o san OTA mode.")
            time.sleep(0.15)
            return

        if hit in ("OTA flag set", "OTA requested, resetting..."):
            ota_requested = True
            log_print("[PC] ", "App da yeu cau OTA, doi bootloader...")
            continue

        if not ota_requested:
            log_print("[PC] ", f"Thay '{hit}' — gui 'U'...")
            _send_ota_key(ser, verbose=verbose, times=3)

    raise TimeoutError("Khong thay 'OTA RX ready' tu MCU")


def ota_upload(
    port: str,
    firmware_path: Path,
    baud: int = 115200,
    enter_ota: bool = False,
    sign: bool = False,
    image_version: int = 1,
    timeout_s: float = 5.0,
    verbose: bool = False,
) -> None:
    global _LOG_T0
    _LOG_T0 = time.monotonic()

    upload_path = firmware_path
    if sign:
        upload_path = sign_firmware_file(firmware_path, version=image_version)

    fw = load_firmware(upload_path)
    fw_crc = crc32_firmware(fw)
    fw_size = len(fw)

    print(f"Firmware: {upload_path.name}")
    print(f"  Size   : {fw_size} bytes")
    print(f"  CRC32  : 0x{fw_crc:08X}")
    print(f"  Port   : {port} @ {baud}")
    log_print("", "--- UART log ---")

    with open_serial_port(port, baud) as ser:
        log = UartLogReader(ser, verbose=verbose)
        log.start()

        try:
            time.sleep(0.5)
            log.drain(0.3)

            if enter_ota:
                enter_ota_mode(log, ser, verbose=verbose)
            else:
                log.wait_for_text("OTA RX ready", timeout_s=15.0)
                time.sleep(0.15)

            # Cho MCU vao ota_run; tranh START mat neu gui qua som / byte rac
            log.drain(0.1)
            log_print("[PC] ", "START...")
            start_payload = struct.pack("<II", fw_size, fw_crc)
            send_frame(log, ser, CMD_START, start_payload, timeout_s, verbose=verbose)

            seq = 0
            offset = 0
            total_chunks = (fw_size + CHUNK_SIZE - 1) // CHUNK_SIZE

            while offset < fw_size:
                chunk = fw[offset : offset + CHUNK_SIZE]
                payload = struct.pack("<H", seq) + chunk
                send_frame(log, ser, CMD_DATA, payload, timeout_s, verbose=verbose)

                offset += len(chunk)
                seq += 1
                pct = offset * 100 // fw_size
                log_print("[PC] ", f"DATA chunk {seq}/{total_chunks} ({pct}%)")
                log.drain(0.05)

            log_print("[PC] ", "END...")
            try:
                send_frame(log, ser, CMD_END, b"", timeout_s, verbose=verbose)
            except (TimeoutError, RuntimeError) as exc:
                log.drain(0.5)
                if log.saw_ota_ok:
                    log_print(
                        "[PC] ",
                        f"END ACK loi ({exc}) nhung MCU da boot app — OK.",
                    )
                else:
                    raise

            log.drain(0.3)
            log_print("[PC] ", "=" * 40)
            log_print("[PC] ", "OTA THANH CONG")
            log_print(
                "[PC] ",
                f"Da nap {fw_size} bytes ({upload_path.name}), CRC32=0x{fw_crc:08X}",
            )
            log_print("[PC] ", "=" * 40)
        finally:
            log.stop()


def main() -> None:
    parser = argparse.ArgumentParser(description="STM32F103 UART OTA upload tool")
    parser.add_argument("port", help="Serial port, e.g. COM3")
    parser.add_argument("firmware", type=Path, help=".bin or .hex firmware file")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default 115200)")
    parser.add_argument(
        "--enter-ota",
        action="store_true",
        help="Send 'U' before upload (bootloader 3s window)",
    )
    parser.add_argument(
        "--sign",
        action="store_true",
        help="Sign raw app .bin before upload (calls tools/sign_image.py)",
    )
    parser.add_argument(
        "--version",
        type=int,
        default=1,
        help="Image version when using --sign (default 1)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=10.0,
        help="ACK timeout per frame in seconds",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="In hex dump TX/RX de debug",
    )
    args = parser.parse_args()

    if not args.firmware.is_file():
        print(f"Khong tim thay file: {args.firmware}", file=sys.stderr)
        sys.exit(1)

    try:
        ota_upload(
            args.port,
            args.firmware,
            baud=args.baud,
            enter_ota=args.enter_ota,
            sign=args.sign,
            image_version=args.version,
            timeout_s=args.timeout,
            verbose=args.verbose,
        )
    except serial.SerialException as exc:
        print(f"Loi serial: {exc}", file=sys.stderr)
        sys.exit(1)
    except (TimeoutError, RuntimeError, ValueError) as exc:
        print(f"Loi OTA: {exc}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
