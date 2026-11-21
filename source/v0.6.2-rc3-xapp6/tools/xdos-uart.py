"""Windows UART client for XDOS RC3."""

import argparse
import csv
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from ipaddress import IPv4Address
from pathlib import Path
import re
import sys
from threading import Thread
import time
import zlib

import serial


def exchange(port, command, expected, timeout=5):
    port.write(f"\n@XDOS {command}\n".encode("ascii"))
    deadline = time.monotonic() + timeout
    incoming = bytearray()
    while time.monotonic() < deadline:
        incoming.extend(port.read_until(b"\n"))
        if not incoming.endswith(b"\n"):
            continue
        line = incoming.decode("utf-8", errors="replace").strip()
        incoming.clear()
        if line.endswith(expected):
            return line
        if line.endswith("@XDOS ERROR"):
            raise RuntimeError(f"XDOS rejected {command.split()[0]}")
    raise TimeoutError(f"No XDOS response to {command.split()[0]}")


def benchmark_result(port):
    port.write(b"\n@XDOS BENCH-RESULT\n")
    deadline = time.monotonic() + 8
    rows = []
    metadata = None
    incoming = bytearray()
    while time.monotonic() < deadline:
        incoming.extend(port.read_until(b"\n"))
        if not incoming.endswith(b"\n"):
            continue
        line = incoming.decode("utf-8", errors="replace").strip()
        incoming.clear()
        if line.startswith("@XDOS BENCH META,"):
            metadata = tuple(map(int, line.removeprefix("@XDOS BENCH META,").split(",")))
        elif line.startswith("@XDOS BENCH ROW,"):
            rows.append(line.removeprefix("@XDOS BENCH ROW,").rsplit(",", 4))
        elif line == "@XDOS BENCH COMPLETE @XDOS OK":
            if not metadata or len(rows) != metadata[0]:
                raise RuntimeError("Incomplete benchmark report")
            print(f"VC4 at start/end: {metadata[1]}/{metadata[2]}")
            if len(metadata) == 5:
                print(f"Refresh period at start/end (ms): {metadata[3]}/{metadata[4]}")
            print("Scene,Samples,FPS,Render ms,Flush ms")
            csv.writer(sys.stdout, lineterminator="\n").writerows(rows)
            return
        elif line in ("@XDOS BENCH RUNNING @XDOS OK", "@XDOS BENCH UNAVAILABLE @XDOS OK"):
            print(line.split()[2])
            return
        elif line == "@XDOS ERROR":
            raise RuntimeError("XDOS rejected BENCH-RESULT")
    raise TimeoutError("No complete XDOS response to BENCH-RESULT")


def send_file(port, name, data, crc):
    exchange(port, f"PUT {name} {len(data):08x} {crc:08x}", "@XDOS OK")
    try:
        for offset in range(0, len(data), 128):
            exchange(port, "DATA " + data[offset:offset + 128].hex(), "@XDOS OK")
        exchange(port, "END", "@XDOS OK")
    except (Exception, KeyboardInterrupt):
        try:
            exchange(port, "CANCEL", "@XDOS OK", timeout=1)
        except (RuntimeError, TimeoutError, serial.SerialException):
            pass
        raise
    print(f"Saved SD:/XDOS/INBOX/{name} ({len(data)} bytes, CRC32 verified by XDOS).")


def send_file_over_network(port, name, data, crc, bind, http_port):
    class FileHandler(BaseHTTPRequestHandler):
        def log_message(self, *_args):
            pass

        def do_GET(self):
            if self.path != "/" + name:
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            try:
                self.wfile.write(data)
            except OSError:
                pass

    with ThreadingHTTPServer((str(bind), http_port), FileHandler) as server:
        worker = Thread(target=server.serve_forever, daemon=True)
        worker.start()
        try:
            exchange(port, f"FETCH {bind} {server.server_port} {name} {len(data):08x} {crc:08x}", "@XDOS OK")
            deadline = time.monotonic() + 140
            while time.monotonic() < deadline:
                line = exchange(port, "FETCH-STATUS", "@XDOS OK")
                if line.endswith("@XDOS FETCH DONE @XDOS OK"):
                    print(f"Saved SD:/XDOS/INBOX/{name} over the network ({len(data)} bytes, CRC32 verified by XDOS).")
                    return
                if not line.endswith("@XDOS FETCH WORKING @XDOS OK"):
                    raise RuntimeError("XDOS network download failed")
                time.sleep(0.5)
            raise TimeoutError("XDOS network download timed out")
        finally:
            server.shutdown()
            worker.join()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM3")
    parser.add_argument("--baud", type=int, choices=(115200, 921600), default=115200)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("ping")
    commands.add_parser("reset")
    commands.add_parser("power", help="Read the Raspberry Pi firmware power status")
    commands.add_parser("lvgl", help="Read the latest LVGL performance sample")
    commands.add_parser("bench-result", help="Read the completed official LVGL Benchmark results")
    commands.add_parser("close", help="Close the current XDOS XAPP")
    open_app = commands.add_parser("open", help="Launch an XDOS desktop XAPP")
    open_app.add_argument("shortcut", type=str.upper, choices=(
        "NOTEPAD", "CALCULATOR", "PAINT", "FILES", "BROWSER", "TERMINAL", "UPDATE", "WIDGETS", "BENCHMARK", "PLAYER",
    ))
    play = commands.add_parser("play", help="Play an H.264 MP4 from the Pi's SD card")
    play.add_argument("path", help="SD:/ path or filename in SD:/XDOS/INBOX")
    commands.add_parser("boot-clear", help="Restore the previous boot configuration")
    upload = commands.add_parser("scp", help="Copy a file into SD:/XDOS/INBOX/")
    upload.add_argument("file", type=Path)
    upload.add_argument("--name")
    network = commands.add_parser("net-scp", help="Copy one file into the XDOS inbox over LAN HTTP")
    network.add_argument("file", type=Path)
    network.add_argument("--name")
    network.add_argument("--bind", type=IPv4Address, required=True, help="A reachable IPv4 address on this PC")
    network.add_argument("--http-port", default=8765, type=int)
    for command, help_text in (
        ("boot", "Select an already uploaded image for the next reset"),
        ("deploy", "Upload, select and reboot into an image"),
    ):
        boot = commands.add_parser(command, help=help_text)
        boot.add_argument("file", type=Path, help="Local image used for CRC32 verification")
        boot.add_argument("--name", required=True, help="Target filename, e.g. UART5.IMG")
    args = parser.parse_args()
    if args.command == "play" and (
        not 0 < len(args.path) <= 191 or not args.path.isascii() or
        any(ord(char) < 32 or ord(char) == 127 for char in args.path)
    ):
        parser.error("Use an ASCII SD path up to 191 characters.")
    if args.command in ("scp", "net-scp", "boot", "deploy"):
        name = args.name or args.file.name
        if not re.fullmatch(r"[A-Za-z0-9_-][A-Za-z0-9_.-]{0,31}", name):
            parser.error("Use a destination name of up to 32 ASCII letters, digits, dots, underscores or hyphens.")
        if not 0 < args.file.stat().st_size <= 16 * 1024 * 1024:
            parser.error("File must be between 1 byte and 16 MiB.")
        data = args.file.read_bytes()
        crc = zlib.crc32(data)
        if args.command in ("boot", "deploy") and not re.fullmatch(r"[A-Z0-9]{1,8}\.IMG", name):
            parser.error("The boot filename must use 1–8 uppercase letters or digits followed by .IMG.")
        if args.command == "net-scp" and (
            not 1 <= args.http_port <= 65535 or args.bind.is_unspecified
            or args.bind.is_multicast or args.bind.is_loopback
        ):
            parser.error("Use a reachable LAN IPv4 address and a port between 1 and 65535.")

    with serial.Serial(args.port, args.baud, timeout=0.25, write_timeout=3) as port:
        exchange(port, "PING", "@XDOS READY 1")
        if args.command == "power":
            line = exchange(port, "POWER", "@XDOS OK")
            match = re.search(r"@XDOS POWER ([0-9a-f]{8}) @XDOS OK$", line)
            if not match:
                raise RuntimeError("Invalid power response")
            state = int(match[1], 16)
            print(f"Firmware power status: 0x{state:08x}")
            print(f"Undervoltage now: {'yes' if state & 1 else 'no'}")
            print(f"Undervoltage since boot: {'yes' if state & (1 << 16) else 'no'}")
        elif args.command == "lvgl":
            line = exchange(port, "LVGL", "@XDOS OK")
            match = re.search(r"@XDOS LVGL (.+) @XDOS OK$", line)
            if not match:
                raise RuntimeError("Invalid LVGL response")
            print("LVGL sample unavailable." if match[1] == "unavailable" else match[1])
        elif args.command == "bench-result":
            benchmark_result(port)
        elif args.command == "close":
            exchange(port, "CLOSE", "@XDOS CLOSED @XDOS OK")
            print("Closed current XAPP.")
        elif args.command == "open":
            exchange(port, f"OPEN {args.shortcut}", "@XDOS OPENED @XDOS OK", timeout=15)
            print(f"Opened {args.shortcut}.")
        elif args.command == "play":
            exchange(port, f"PLAY {args.path}", "@XDOS PLAYING")
            print(f"Playback accepted for {args.path}.")
        elif args.command == "reset":
            exchange(port, "RESET", "@XDOS RESETTING")
            print("Reset acknowledged.")
        elif args.command == "boot-clear":
            exchange(port, "BOOT-CLEAR", "@XDOS OK")
            print("Previous boot configuration restored for the next reset.")
        elif args.command == "net-scp":
            send_file_over_network(port, name, data, crc, args.bind, args.http_port)
        elif args.command in ("scp", "boot", "deploy"):
            if args.command != "boot":
                send_file(port, name, data, crc)
            if args.command != "scp":
                exchange(port, f"BOOT {name} {crc:08x}", "@XDOS OK", timeout=25)
                print(f"Selected {name} for the next reset.")
            if args.command == "deploy":
                exchange(port, "RESET", "@XDOS RESETTING")
                print("Reset acknowledged. Check the UART boot log.")
        else:
            print("XDOS UART ready.")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
