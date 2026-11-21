"""Deploy a Pi 3 IMG using the existing XDOS UART update protocol."""
import argparse
from pathlib import Path
import secrets
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
        if line.endswith("@XDOS ERROR"):
            raise RuntimeError(f"XDOS rejected {operation}")
    raise TimeoutError(f"No XDOS reply to {operation}")


def deploy(port, data):
    name = f"A{secrets.token_hex(4)[:7].upper()}.IMG"
    crc = zlib.crc32(data)
    exchange(port, "PING", "@XDOS READY 1")
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
    print("XDOS verified the transfer. Selecting the new kernel...", flush=True)
    exchange(port, f"BOOT {name} {crc:08x}", timeout=25)
    print("Boot selection updated. If the sketch cannot accept UART updates, restore xdosboot.bak as config.txt on the test card.", flush=True)
    exchange(port, "RESET", "@XDOS RESETTING")
    print("XDOS acknowledged RESET. Check the sketch on the Pi.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="Debug UART USB–TTL adapter port")
    parser.add_argument("image", type=Path)
    parser.add_argument("--baud", type=int, choices=(115200, 921600), default=921600)
    args = parser.parse_args()
    try:
        if not 0 < args.image.stat().st_size <= 16 * 1024 * 1024:
            raise ValueError("XDOS requires an IMG between 1 byte and 16 MiB")
        data = args.image.read_bytes()
        with serial.Serial(args.port, args.baud, timeout=0.2, write_timeout=3) as port:
            port.reset_input_buffer()
            deploy(port, data)
    except (OSError, ValueError, RuntimeError) as error:
        raise SystemExit(f"RPiDuino UART upload failed: {error}") from None


if __name__ == "__main__":
    main()
