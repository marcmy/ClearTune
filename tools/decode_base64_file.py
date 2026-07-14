#!/usr/bin/env python3
import argparse
import base64
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()

    encoded = "".join(args.source.read_text(encoding="ascii").split())
    decoded = base64.b64decode(encoded, validate=True)
    if len(decoded) < 6 or decoded[:4] != b"\x00\x00\x01\x00":
        raise ValueError("decoded data is not a Windows ICO file")

    args.destination.parent.mkdir(parents=True, exist_ok=True)
    args.destination.write_bytes(decoded)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
