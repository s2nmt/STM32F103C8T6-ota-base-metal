#!/usr/bin/env python3
"""Sign app payload -> signed slot image (256 B header + payload)."""

from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

from keygen import derive_signing_key

IMG_MAGIC = 0x494D4731
IMG_TYPE_APP = 1
IMG_HEADER_SIZE = 0x100
PAYLOAD_OFFSET = IMG_HEADER_SIZE


def sha256(data: bytes) -> bytes:
    return hashlib.sha256(data).digest()


def build_header(payload: bytes, version: int) -> bytes:
    digest = sha256(payload)
    sk = derive_signing_key()
    signature = sk.sign_digest(digest, sigencode=lambda r, s, order: r.to_bytes(32, "big") + s.to_bytes(32, "big"))

    hdr = struct.pack(
        "<6I",
        IMG_MAGIC,
        IMG_TYPE_APP,
        PAYLOAD_OFFSET,
        len(payload),
        version,
        0,
    )
    hdr += digest
    hdr += signature
    hdr += b"\x00" * 32
    if len(hdr) > IMG_HEADER_SIZE:
        raise ValueError("header struct overflow")
    hdr += b"\x00" * (IMG_HEADER_SIZE - len(hdr))
    return hdr


def main() -> int:
    ap = argparse.ArgumentParser(description="Sign STM32F103 app payload")
    ap.add_argument("payload", type=Path, help="Raw app .bin (linked at slot+0x100)")
    ap.add_argument("-o", "--output", type=Path, required=True, help="Signed slot image")
    ap.add_argument("--version", type=int, default=1, help="Image version")
    args = ap.parse_args()

    payload = args.payload.read_bytes()
    if len(payload) == 0:
        print("empty payload", file=sys.stderr)
        return 1

    signed = build_header(payload, args.version) + payload
    args.output.write_bytes(signed)
    print(f"Signed {len(payload)} B payload -> {args.output} ({len(signed)} B total)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
