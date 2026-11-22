"""Run the RPiDuino GNU toolchain natively or through WSL."""
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

CIRCLE_REV = "7a81e1b480fe7d19fc1fc695b89bff0a0848f44b"
CIRCLE_URL = "https://github.com/rsta2/circle.git"
WINDOWS = os.name == "nt"


def run(*args, cwd=None):
    subprocess.run(args, cwd=cwd, check=True)


def linux_path(value):
    value = os.fspath(value)
    if value.lower() == "nul" or value == "/dev/null":
        return "/dev/null"
    if not WINDOWS:
        return str(Path(value).resolve())
    return subprocess.check_output(
        ["wsl.exe", "--exec", "wslpath", "-a", str(Path(value).resolve())],
        text=True,
    ).strip()


def compiler_args(args):
    for arg in args:
        if arg.startswith("-I") and len(arg) > 2:
            yield "-I" + linux_path(arg[2:])
        elif not arg.startswith("-") and arg not in ("assembler-with-cpp", "c++"):
            yield linux_path(arg)
        else:
            yield arg


def link(model, bits, prefix, output, archive, objects):
    import fcntl

    cache = Path(os.environ.get("XDG_CACHE_HOME", Path.home() / ".cache")) / "rpiduino"
    cache.mkdir(parents=True, exist_ok=True)
    circle = cache / f"circle-{CIRCLE_REV}-pi{model}-{bits}"
    jobs = str(min(8, os.cpu_count() or 1))

    with (cache / f"{circle.name}.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        if not circle.exists():
            with tempfile.TemporaryDirectory(dir=cache) as staging:
                checkout = Path(staging) / "circle"
                run("git", "init", "-q", checkout)
                source = os.environ.get("RPIDUINO_CIRCLE_SOURCE", CIRCLE_URL)
                run("git", "fetch", "-q", "--depth", "1", source, CIRCLE_REV, cwd=checkout)
                run("git", "checkout", "-q", "--detach", "FETCH_HEAD", cwd=checkout)
                run("bash", "./configure", "-r", model, "-p", prefix, cwd=checkout)
                checkout.rename(circle)
        run("make", "-s", "-C", circle / "lib", "-j", jobs)

    with tempfile.TemporaryDirectory(dir=circle / "sample") as staging:
        work = Path(staging)
        shutil.copyfile(Path(__file__).with_name("circle_runtime.cpp"), work / "runtime.cpp")
        shutil.copyfile(Path(__file__).parent.parent / "cores/rpiduino/Arduino.h", work / "Arduino.h")
        shutil.copyfile(archive, work / "arduino_core.a")
        object_names = []
        for index, source in enumerate(objects):
            name = f"sketch{index}.o"
            shutil.copyfile(source, work / name)
            object_names.append(name)
        (work / "Makefile").write_text(
            "CIRCLEHOME = ../..\n"
            f"OBJS = runtime.o {' '.join(object_names)}\n"
            "LIBS = arduino_core.a $(CIRCLEHOME)/lib/libcircle.a\n"
            "include ../Rules.mk\n",
            encoding="utf-8",
        )
        run("make", "-s", "-j", jobs, cwd=work)
        elf, = work.glob("kernel*.elf")
        shutil.copyfile(elf, output)


def main():
    model, bits, cpu, mode, action, *args = sys.argv[1:]
    prefix = "arm-none-eabi-" if bits == "32" else "aarch64-linux-gnu-"
    launcher = ["wsl.exe", "--exec"] if WINDOWS else []

    if action in ("cc", "cxx"):
        driver = "gcc" if action == "cc" else "g++"
        architecture = ["-marm", "-mfloat-abi=hard", "-mfpu=neon-vfpv4"] if bits == "32" else []
        xapp_flags = []
        if mode == "xapp":
            xapp_flags.extend(("-DRPIDUINO_XDOS_XAPP=1", "-DRPIDUINO_LVGL_HOST_TIMER=1",
                               "-DLV_CONF_INCLUDE_SIMPLE", "-fno-unwind-tables",
                               "-fno-asynchronous-unwind-tables"))
            sdk = os.environ.get("RPIDUINO_XDOS_HEADERS", Path(__file__).with_name("xapp-headers"))
            if sdk:
                base = Path(sdk)
                for child in ("", "lvgl", "addon", "include"):
                    path = base / child
                    if path.is_dir():
                        xapp_flags.append("-I" + linux_path(str(path)))
        run(*launcher, prefix + driver, f"-mcpu={cpu}", *architecture, *xapp_flags, *compiler_args(args))
    elif action == "ar":
        run(*launcher, prefix + "ar", "rcs", *(linux_path(arg) for arg in args))
    elif action == "image":
        run(*launcher, prefix + "objcopy", "-O", "binary", *(linux_path(arg) for arg in args))
    elif action == "link":
        if mode == "xapp":
            host = os.environ.get("RPIDUINO_XDOS_HOST")
            if args[0] == "--host":
                host = args[1]
                args = args[2:]
            if not host:
                host = Path(__file__).with_name("xapp-host.json")
            if not Path(host).is_file():
                raise ValueError("Set RPIDUINO_XDOS_HOST to the paired xapp-host.json")
            if WINDOWS:
                run(*launcher, "python3", linux_path(__file__), model, bits, cpu, mode, action,
                    "--host", linux_path(host), *(linux_path(arg) for arg in args))
            else:
                from xapp_link import link_xapp
                output, _, *objects = args
                link_xapp(output, objects, host)
        elif WINDOWS:
            run(*launcher, "python3", linux_path(__file__), model, bits, cpu, mode, action,
                *(linux_path(arg) for arg in args))
        else:
            output, archive, *objects = args
            link(model, bits, prefix, output, archive, objects)
    else:
        raise SystemExit(f"Unsupported build action: {action}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"RPiDuino build failed: {error}") from error
