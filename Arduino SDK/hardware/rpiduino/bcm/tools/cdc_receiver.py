"""Raspberry Pi OS USB CDC receiver for a dedicated RPiDuino test card."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import time
import zlib

MAX_IMAGE = 16 * 1024 * 1024
READY = "@XDOS READY 1"
OK = "@XDOS OK"
ERROR = "@XDOS ERROR"
RESETTING = "@XDOS RESETTING"


def atomic_write(path, data):
    temp = path.with_name(path.name + ".tmp")
    with temp.open("wb") as file:
        file.write(data)
        file.flush()
        os.fsync(file.fileno())
    temp.replace(path)
    os.sync()


class Receiver:
    def __init__(self, board, boot):
        self.board = board
        self.boot = boot
        self.selected = False
        self.clear()

    def clear(self):
        self.name = None
        self.image = bytearray()
        self.complete = False

    def command(self, line):
        fields = line.split()
        if len(fields) < 2 or fields[0] != "@XDOS":
            return None
        op, *args = fields[1:]
        try:
            if op == "PING" and not args:
                return READY
            if op == "IDENT" and not args:
                return f"@RPIDUINO BOARD {self.board}"
            if op == "PUT" and len(args) == 3 and not self.selected:
                name, length, crc = args
                if not re.fullmatch(r"A[0-9A-F]{7}\.IMG", name):
                    return ERROR
                if not all(re.fullmatch(r"[0-9a-fA-F]{8}", arg) for arg in (length, crc)):
                    return ERROR
                size = int(length, 16)
                if not 0 < size <= MAX_IMAGE:
                    return ERROR
                self.clear()
                self.name, self.size, self.crc = name, size, int(crc, 16)
                return OK
            if op == "DATA" and len(args) == 1 and self.name and not self.complete:
                chunk = bytes.fromhex(args[0])
                if 0 < len(chunk) <= 128 and len(self.image) + len(chunk) <= self.size:
                    self.image.extend(chunk)
                    return OK
            if op == "END" and not args and self.name and not self.complete:
                self.complete = len(self.image) == self.size and zlib.crc32(self.image) == self.crc
                return OK if self.complete else ERROR
            if op == "CANCEL" and not args and not self.selected:
                self.clear()
                return OK
            if op == "BOOT" and self.complete and args == [self.name, f"{self.crc:08x}"]:
                self.select()
                return OK
            if op == "RESET" and not args and self.selected:
                return RESETTING
        except (OSError, ValueError) as error:
            print(f"RPiDuino update failed: {error}", flush=True)
        return ERROR

    def select(self):
        if self.selected:
            return
        config = self.boot / "config.txt"
        backup = self.boot / "rpiduino-config.bak"
        image = self.boot / self.name
        if not (self.boot / "RPIDUINO.TEST").is_file() or backup.exists() or image.exists():
            raise ValueError("Expected a marked test card with no existing rollback backup or image")
        base = config.read_bytes()
        if not base or b"\0" in base:
            raise ValueError("Invalid config.txt")
        atomic_write(image, self.image)
        atomic_write(backup, base)
        new_config = base.rstrip(b"\r\n") + f"\n\n[all]\narm_64bit=1\nauto_initramfs=0\nkernel={self.name}\n".encode("ascii")
        atomic_write(config, new_config)
        self.selected = True


def main():
    import serial

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", default="/dev/ttyGS0")
    parser.add_argument("--boot", type=Path, default=Path("/boot/firmware"))
    args = parser.parse_args()
    model = Path("/proc/device-tree/model").read_bytes().decode()
    match = re.match(r"Raspberry Pi (4|5) Model B\b", model)
    if not match or not (args.boot / "RPIDUINO.TEST").is_file():
        parser.error("Requires a Raspberry Pi 4B or 5 and a dedicated RPIDUINO.TEST boot card")
    receiver = Receiver(int(match[1]), args.boot)
    pending = bytearray()
    with serial.Serial(args.device, 115200, timeout=0.2, write_timeout=3) as port:
        while True:
            pending.extend(port.read_until(b"\n", size=512))
            if not pending.endswith(b"\n"):
                if len(pending) >= 512:
                    pending.clear()
                continue
            response = receiver.command(pending.decode("ascii", errors="replace").strip())
            pending.clear()
            if response is not None:
                port.write((response + "\r\n").encode("ascii"))
            if response == RESETTING:
                port.flush()
                time.sleep(0.3)
                subprocess.run(["systemctl", "reboot"], check=True)
                return


if __name__ == "__main__":
    main()
