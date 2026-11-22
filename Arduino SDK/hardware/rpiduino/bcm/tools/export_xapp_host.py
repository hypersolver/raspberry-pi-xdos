"""Export the V1 XAPP link descriptor from a matched ARM32 ELF and IMG."""
import argparse
import json
from pathlib import Path
import subprocess
import zlib

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("elf", type=Path)
parser.add_argument("image", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
symbols = {}
for line in subprocess.check_output(
        ["arm-none-eabi-nm", "-g", "--defined-only", args.elf], text=True).splitlines():
    addr, _, name = line.split()
    symbols[name] = int(addr, 16)
raw = args.image.read_bytes()
text_size = symbols["_etext"] - symbols["_start"]
if not 0 < text_size <= len(raw):
    parser.error("ELF text range is outside the IMG")
payload = {
    "format": 1,
    "kernel_crc32": zlib.crc32(raw[:text_size]),
    "slot_start": symbols["xdos_xapp_exec_slot"],
    "slot_end": symbols["xdos_xapp_exec_slot_end"],
    "symbols": symbols,
}
args.output.write_text(json.dumps(payload, separators=(",", ":")) + "\n", encoding="utf-8")
