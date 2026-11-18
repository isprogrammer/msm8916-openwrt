#!/usr/bin/env python3
"""Convert the stock Qualcomm delay/length/cmd byte list to MIPI DBI v1."""
import pathlib
import re
import sys

if len(sys.argv) != 3:
    raise SystemExit(f"usage: {sys.argv[0]} STOCK_DTS_TEXT OUTPUT.bin")
source = pathlib.Path(sys.argv[1]).read_text()
match = re.search(r"qcom,mdss-spi-on-command\s*=\s*\[([^]]+)\]", source)
if not match:
    raise SystemExit("stock on-command property not found")
raw = bytes.fromhex(match.group(1))
records = bytearray()
i = 0
count = 0
while i < len(raw):
    if i + 2 > len(raw):
        raise SystemExit("truncated delay/length header")
    delay_ms, payload_len = raw[i], raw[i + 1]
    i += 2
    if payload_len < 1 or i + payload_len > len(raw):
        raise SystemExit("invalid or truncated command payload")
    command = raw[i]
    params = raw[i + 1:i + payload_len]
    records.extend((command, len(params)))
    records.extend(params)
    i += payload_len
    if delay_ms:
        records.extend((0x00, 0x01, delay_ms))
    count += 1
if count != 19:
    raise SystemExit(f"expected 19 stock commands, got {count}")
pathlib.Path(sys.argv[2]).write_bytes(b"MIPI DBI" + b"\0" * 7 + b"\x01" + records)
print(f"converted {count} stock commands to {len(records)} command bytes")
