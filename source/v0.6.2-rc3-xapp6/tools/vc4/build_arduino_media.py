"""Build a paired Arduino media XAPP from a prepared Pi 3B+ audio build tree."""
import argparse
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from PIL import Image

TOOLS = Path(__file__).resolve().parents[1] / "rc3-uart"
SKETCH = TOOLS.parents[1] / "arduino-demos/XappMedia/XappMedia.ino"


def run(*args, env=None):
    subprocess.run([str(arg) for arg in args], env=env, check=True)


def photo_header(source, target):
    with Image.open(source) as image:
        image.thumbnail((320, 180))
        rgb = image.convert("RGB")
    pixels = b"".join(struct.pack("<H", (r >> 3) << 11 | (g >> 2) << 5 | b >> 3)
                      for r, g, b in rgb.getdata())
    lines = [", ".join(f"0x{value:02x}" for value in pixels[i:i + 16])
             for i in range(0, len(pixels), 16)]
    target.write_text(
        f"#define PHOTO_WIDTH {rgb.width}\n#define PHOTO_HEIGHT {rgb.height}\n"
        "alignas(4) static const uint8_t photo_pixels[] = {\n"
        + ",\n".join(lines) + "\n};\n",
        encoding="ascii",
    )


def build(circle, source, photo, output):
    if source.parent != circle / "sample" or not (source / "music_host.inc").is_file():
        raise ValueError("Choose a prepared XDOS sample under Circle/sample with Music enabled")
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="xdos-arduino-media-", dir=circle / "sample") as directory:
        staging = Path(directory)
        # Circle Makefiles use ../.. for CIRCLEHOME, so use a direct sibling sample.
        with tempfile.TemporaryDirectory(prefix="xdos-arduino-media-kernel-", dir=circle / "sample") as kernel_dir:
            sample = Path(kernel_dir)
            shutil.copytree(source, sample, dirs_exist_ok=True)
            shortcut = sample / "app_shortcut.cpp"
            text, replaced = re.subn(r"SD:/XDOS/INBOX/[A-Z0-9]{2}WIDGET\.XAPP", "SD:/XDOS/INBOX/MXWIDGET.XAPP",
                                     shortcut.read_text(encoding="utf-8"))
            if not replaced:
                raise ValueError("Cannot locate the Widgets shortcut in this audio build")
            shortcut.write_text(text, encoding="utf-8")
            with (sample / "kernel.cpp").open("a", encoding="utf-8") as kernel:
                kernel.write("""
extern "C" bool xdos_media_play_video(const char *path)
{
    StopMusic();
    return QueueVideo(path);
}
extern "C" bool xdos_media_play_music(const char *path)
{
    return StartMusic(path, VCHIQSoundDestinationHDMI);
}
extern "C" void xdos_media_stop_music()
{
    StopMusic();
}
""")
            with (sample / "Makefile").open("a", encoding="utf-8") as makefile:
                makefile.write("\nLDFLAGS += -u xdos_media_play_video -u xdos_media_play_music -u xdos_media_stop_music\n")
            run("make", "-C", sample, "-j4", "XDOS_BUFFER_ROWS=128")

            work = staging / "output/arduino-demos"
            work.mkdir(parents=True)
            photo_header(photo, work / "photo_rgb565.h")
            lvgl = circle / "addon/lvgl"
            flags = [
                "-Os", "-mcpu=cortex-a53", "-marm", "-mfloat-abi=hard", "-mfpu=neon-vfpv4",
                "-ffreestanding", "-ffunction-sections", "-fdata-sections", "-fno-unwind-tables",
                "-fno-asynchronous-unwind-tables", "-std=c++17", "-fno-exceptions", "-fno-rtti",
                "-DRASPPI=3", "-DAARCH=32", "-DLV_CONF_INCLUDE_SIMPLE", "-DXDOS_MEDIA_SKETCH",
                "-I", work, "-I", lvgl / "lvgl", "-I", lvgl, "-I", circle / "include",
                "-I", sample, "-I", sample / "xdos_arduino", "-I", TOOLS,
            ]
            run("arm-none-eabi-g++", *flags, "-x", "c++", "-c", SKETCH, "-o", work / "sketch.o")
            run("arm-none-eabi-g++", *flags, "-c", TOOLS / "sketch_xapp.cpp", "-o", work / "entry.o")
            run("arm-none-eabi-ld", "-r", work / "sketch.o", work / "entry.o", "-o", work / "LVGLDEMO.rel.o")
            (work / "LVGLDEMO.inputs").write_text("entry.o\nsketch.o\n", encoding="ascii")
            img = staging / "output/xdos32-v062-rc3-pi3.img"
            elf = sample / "kernel8-32.elf"
            shutil.copyfile(sample / "kernel8-32.img", img)
            run(sys.executable, TOOLS / "build_arduino_demos.py", "finish", staging, circle, elf,
                "--widgets-only", env={**os.environ, "XDOS_WIDGETS_SKETCH": "XappMedia"})
            shutil.copyfile(img, output / "MXMEDIA.IMG")
            shutil.copyfile(staging / "output/LVGLDEMO.XAPP", output / "MXWIDGET.XAPP")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("circle", type=Path)
    parser.add_argument("audio_sample", type=Path)
    parser.add_argument("photo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    build(args.circle.resolve(), args.audio_sample.resolve(), args.photo.resolve(), args.output.resolve())


if __name__ == "__main__":
    main()
