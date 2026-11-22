from pathlib import Path
import shutil
import sys

circle, output = map(Path,sys.argv[1:])
output.mkdir(parents=True,exist_ok=True)
addon = circle / "addon"
lvgl = addon / "lvgl/lvgl"
config = (addon / "lvgl/lv_conf.h").read_text(encoding="utf-8")
config = config.replace("<circle/startup.h>", "<stdint.h>").replace("LV_ASSERT_HANDLER halt();", "LV_ASSERT_HANDLER __builtin_trap();")
(output / "lv_conf.h").write_text(config,encoding="utf-8")
for filename in ("lvgl.h","lv_version.h","LICENCE.txt"):
    shutil.copyfile(lvgl / filename,output / filename)
for header in (lvgl / "src").rglob("*.h"):
    target = output / header.relative_to(lvgl)
    target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(header,target)
fatfs = output / "addon/fatfs"
fatfs.mkdir(parents=True,exist_ok=True)
for filename in ("ff.h","ffconf.h","diskio.h"):
    shutil.copyfile(addon / "fatfs" / filename,fatfs / filename)
