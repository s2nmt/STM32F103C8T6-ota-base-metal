import os
import sys
import time

# File ten serial.py trung package pyserial — bo thu muc script khoi sys.path
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if _SCRIPT_DIR in sys.path:
    sys.path.remove(_SCRIPT_DIR)

import serial  # noqa: E402

# --- Hardcode (sua cho dung voi may ban) ---
PORT = "COM6"
BAUD = 115200


def main() -> None:
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1)
    except serial.SerialException as exc:
        print(f"Khong mo duoc {PORT}: {exc}", file=sys.stderr)
        sys.exit(1)

    print(f"Da mo {PORT} @ {BAUD}")
    print("Chi doc UART (khong gui UPDATE). Dung --send-update de test OTA tu app.")

    send_update = "--send-update" in sys.argv

    if send_update:
        tx = b"UPDATE\r\n"
        ser.write(tx)
        print(f"Gui ({len(tx)} byte): {tx.decode().strip()!r}")

    # --- Nhan lien tuc, chi break khi loi serial ---
    try:
        while True:
            time.sleep(0.2)
            rx = ser.read(ser.in_waiting or 1)
            if rx:
                print(f"Nhan ({len(rx)} byte): {rx!r}")
    except serial.SerialException as exc:
        print(f"Loi serial: {exc}", file=sys.stderr)
    finally:
        ser.close()
        print("Da dong port")


if __name__ == "__main__":
    main()
