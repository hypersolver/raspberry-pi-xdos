"""Create the versioned Boards Manager archive and update the package index."""
import hashlib
import json
import zipfile
from pathlib import Path

SDK = Path(__file__).resolve().parent
PLATFORM = SDK / "hardware/rpiduino/bcm"
REPO = "https://github.com/hypersolver/raspberry-pi-xdos"
RAW_MAIN = "https://raw.githubusercontent.com/hypersolver/raspberry-pi-xdos/main"


def properties(path):
    return dict(
        line.split("=", 1) for line in path.read_text(encoding="utf-8").splitlines()
        if line and not line.lstrip().startswith("#")
    )


def main():
    version = properties(PLATFORM / "platform.txt")["version"]
    archive_name = f"rpiduino-bcm-{version}.zip"
    archive = SDK / "packages" / archive_name
    archive.parent.mkdir(exist_ok=True)
    with zipfile.ZipFile(archive, "w") as package:
        for path in sorted(PLATFORM.rglob("*")):
            if path.is_file() and "__pycache__" not in path.parts:
                entry = zipfile.ZipInfo(
                    f"rpiduino-bcm-{version}/{path.relative_to(PLATFORM).as_posix()}"
                )
                package.writestr(entry, path.read_bytes(), zipfile.ZIP_DEFLATED)

    contents = archive.read_bytes()
    platform = {
        "name": "RPiDuino",
        "architecture": "bcm",
        "version": version,
        "category": "Contributed",
        "url": f"{RAW_MAIN}/Arduino%20SDK/packages/{archive_name}",
        "archiveFileName": archive_name,
        "checksum": "SHA-256:" + hashlib.sha256(contents).hexdigest(),
        "size": str(len(contents)),
        "boards": [{"name": name} for key, name in properties(PLATFORM / "boards.txt").items()
                   if key.endswith(".name")],
        "toolsDependencies": [],
    }
    index = SDK.parent / "package_rpiduino_index.json"
    previous = json.loads(index.read_text(encoding="utf-8"))["packages"][0]["platforms"] if index.exists() else []
    data = {"packages": [{
        "name": "rpiduino",
        "maintainer": "RPiDuino contributors",
        "websiteURL": REPO,
        "help": {"online": f"{REPO}/tree/main/Arduino%20SDK"},
        "platforms": [platform] + [item for item in previous if item["version"] != version],
        "tools": [],
    }]}
    index.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
