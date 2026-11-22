"""Deploy a Pi 3 IMG using the existing XDOS UART update protocol."""
import argparse
from pathlib import Path
import secrets
import struct
import time
import zlib

try:
    import serial
except ImportError:
    raise SystemExit("Install pyserial with: python -m pip install pyserial") from None


def exchange(port, command, expected="@XDOS OK", timeout=5):
    operation = command.split()[0]
    message = f"\n@XDOS {command}\n".encode("ascii")
    if port.write(message) != len(message):
        raise OSError(f"Incomplete UART write during {operation}")
    deadline = time.monotonic() + timeout
    pending = bytearray()
    while time.monotonic() < deadline:
        pending.extend(port.read_until(b"\n"))
        if not pending.endswith(b"\n"):
            continue
        line = pending.decode("ascii", errors="replace").strip()
        pending.clear()
        if line.endswith(expected):
            return
        if operation == "IDENT" and line.startswith("@RPIDUINO BOARD "):
            raise RuntimeError(f"Board mismatch: expected {expected}, received {line}")
        if line.endswith("@XDOS ERROR"):
            raise RuntimeError(f"XDOS rejected {operation}")
    raise TimeoutError(f"No XDOS reply to {operation}")


def deploy(port, data, cdc_board=None, xapp=False):
    suffix = "XAPP" if xapp else "IMG"
    name = f"A{secrets.token_hex(4)[:7].upper()}.{suffix}"
    crc = zlib.crc32(data)
    exchange(port, "PING", "@XDOS READY 1")
    if cdc_board is not None:
        exchange(port, "IDENT", f"@RPIDUINO BOARD {cdc_board}")
    print(f"Transferring {len(data)} bytes as {name}, CRC32 {crc:08x}...", flush=True)
    exchange(port, f"PUT {name} {len(data):08x} {crc:08x}")
    try:
        for offset in range(0, len(data), 128):
            exchange(port, "DATA " + data[offset:offset + 128].hex())
        exchange(port, "END")
    except (OSError, RuntimeError, KeyboardInterrupt):
        try:
            exchange(port, "CANCEL", timeout=1)
        except (OSError, RuntimeError):
            pass
        raise
    if xapp:
        print(f"Uploaded and verified SD:/XDOS/INBOX/{name}. Open it from XDOS Files.")
        return
    print("Receiver verified the transfer. Selecting the new kernel...", flush=True)
    exchange(port, f"BOOT {name} {crc:08x}", timeout=25)
    backup = "rpiduino-config.bak" if cdc_board else "xdosboot.bak"
    print(f"Boot selection updated. To return to the updater, copy {backup} over config.txt on the test card.", flush=True)
    exchange(port, "RESET", "@XDOS RESETTING")
    print("Receiver acknowledged RESET. Check the sketch on the Pi.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="Debug UART USB–TTL adapter port")
    parser.add_argument("image", type=Path)
    parser.add_argument("--baud", type=int, choices=(115200, 921600), default=921600)
    parser.add_argument("--cdc-board", type=int, choices=(4, 5))
    parser.add_argument("--xapp", action="store_true")
    args = parser.parse_args()
    try:
        if not 0 < args.image.stat().st_size <= 16 * 1024 * 1024:
            raise ValueError("XDOS requires an IMG between 1 byte and 16 MiB")
        data = args.image.read_bytes()
        if args.xapp:
            if args.cdc_board or not 32 < len(data) <= 320 * 1024:
                raise ValueError("Invalid XDOS XAPP size or transport")
            magic, abi, header, length, entry, flags, _, _ = struct.unpack("<4s7I",data[:32])
            if (magic, abi, header, length, flags) != (b"XDAP", 1, 32, len(data), 1) or \
                    entry & 3 or not header <= entry < length:
                raise ValueError("Invalid XDOS XAPP header")
        with serial.Serial(args.port, args.baud, timeout=0.2, write_timeout=3) as port:
            port.reset_input_buffer()
            deploy(port, data, args.cdc_board, args.xapp)
    except (OSError, ValueError, RuntimeError) as error:
        raise SystemExit(f"RPiDuino UART upload failed: {error}") from None


if __name__ == "__main__":
    main()
