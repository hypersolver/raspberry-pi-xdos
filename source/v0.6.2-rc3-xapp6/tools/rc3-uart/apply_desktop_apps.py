"""Replace the Pixel BBS desktop shortcuts with the first ten FAT .APP entries."""
from pathlib import Path
import re
import shutil
import sys

root = Path(sys.argv[1])
tools = Path(__file__).resolve().parent
if not (root / "pixel_bbs.h").is_file():
    raise SystemExit("Apply the Pixel BBS desktop before adding dynamic shortcuts")

for name in ("desktop_apps.h", "desktop_apps.cpp"):
    shutil.copyfile(tools / name, root / name)


def edit(name, change):
    path = root / name
    text = path.read_text(encoding="utf-8")
    modified = change(text)
    if text == modified:
        raise SystemExit(f"Could not apply desktop apps change to {name}")
    path.write_text(modified, encoding="utf-8")


def desktop_header(text):
    text = '#include "desktop_apps.h"\n' + text
    return text.replace("    void Resize(int w, int h);", """    void Resize(int w, int h);
    void ScanDesktopApps();
    const XdosDesktopApp *TakeDiscoveredApp();""", 1).replace("    Window windows_[NWIN];", """    Window windows_[NWIN];
    XdosDesktopApp discoveredApps_[XdosDesktopAppLimit]{};
    unsigned discoveredCount_ = 0;
    int discoveredPending_ = -1;
    void OpenDiscoveredApp(unsigned index);""", 1)


def desktop_source(text):
    start = text.index("    // Taskbar app buttons")
    end = text.index("\n}", start)
    music = "MusicApp" in text
    text = text[:start] + """    for (unsigned i=0; i<discoveredCount_; ++i) {
        const auto slot=PlaceDiscoveredApp(width_,height_,i,discoveredCount_);
        if (Contains(x,y,slot.x,slot.y,slot.width,slot.height)) {
            OpenDiscoveredApp(i);
            return;
        }
    }""" + text[end:]
    start = text.index("void Desktop::DrawIcons()")
    end = text.index("void Desktop::DrawDesktop()", start)
    text = text[:start] + """void Desktop::ScanDesktopApps()
{
    discoveredPending_ = -1;
    discoveredCount_ = XdosReadDesktopApps(discoveredApps_);
    RequestFullRepaint();
}

const XdosDesktopApp *Desktop::TakeDiscoveredApp()
{
    const int index = discoveredPending_;
    discoveredPending_ = -1;
    return index >= 0 ? &discoveredApps_[index] : nullptr;
}

void Desktop::OpenDiscoveredApp(unsigned index)
{
    switch (discoveredApps_[index].kind) {
    case XdosAppKind::Terminal: Open(Terminal); break;
    case XdosAppKind::Notepad: Open(Notepad); break;
    case XdosAppKind::Calculator: Open(Calculator); break;
    case XdosAppKind::Paint: Open(Paint); break;
    case XdosAppKind::Files: Open(Files); break;
    case XdosAppKind::Browser: Open(Browser); break;
    case XdosAppKind::Widgets: appLaunchPending_=true; break;
    case XdosAppKind::Benchmark: officialBenchPending_=true; break;
    case XdosAppKind::Player: workspacePending_=PlayerApp; break;
""" + ("    case XdosAppKind::Music: workspacePending_=MusicApp; break;\n" if music else "") + """    default: discoveredPending_=index; break;
    }
}

void Desktop::DrawIcons()
{
    const int glyphScale=width_>=1280 && height_>=900 ? 2 : 1;
    constexpr unsigned maxLabelChars=16;
    for (unsigned i=0; i<discoveredCount_; ++i) {
        const auto slot=PlaceDiscoveredApp(width_,height_,i,discoveredCount_);
        const auto &app=discoveredApps_[i];
        const char glyph[]={DesktopAppInitial(app.name),0};
        const int centerX=slot.x+slot.width/2;
        const int iconX=centerX-slot.icon/2;
        Fill(iconX,slot.y,slot.icon,slot.icon,DesktopAppColor(glyph[0]));
        Frame(iconX,slot.y,slot.icon,slot.icon,0xC0D4EA);
        PixelText(centerX-4*glyphScale,slot.y+(slot.icon-16*glyphScale)/2,glyph,WHITE);
        const unsigned labelChars=strlen(app.name);
        const int labelWidth=int(labelChars)*8*glyphScale;
        if (labelChars<=maxLabelChars && labelWidth<=slot.width)
            PixelText(centerX-labelWidth/2,slot.y+slot.icon+slot.gap,app.name,WHITE);
    }
}

void Desktop::DrawTaskbar()
{
    Fill(0,height_-49,width_,49,NAV);
    Fill(0,height_-49,width_,1,0x42637A);
    Fill(14,height_-42,123,33,ACCENT);
    Text(28,height_-33,"XD OS",NAV);
    Text(width_-178,height_-30,"Pi 3B  ARM32",CYAN);
}
""" + text[end:]
    return text.replace('#include "pixel_bbs.h"', """#include "pixel_bbs.h"

namespace {
char DesktopAppInitial(const char *name)
{
    for (; *name; ++name) {
        if (*name>='A' && *name<='Z') return *name;
        if (*name>='a' && *name<='z') return char('A'+(*name-'a'));
    }
    return '?';
}

unsigned DesktopAppColor(char initial)
{
    // A-Z. Assign broadly separated hues to common initials (E, T, A, O, I, N, S).
    constexpr unsigned palette[]={
        0x007361,0x676125,0x1F6490,0x5E3979,0xA7303C,0x873851,0x63692C,
        0x226680,0x6F3D9E,0x4D4789,0xAE415A,0x5E6725,0x92442C,0x475F1F,
        0x965324,0x6B4B9A,0x6F5024,0x9D365B,0x225AA3,0x7B4E1E,0x87401F,
        0x126D74,0x963F71,0x33608E,0x006758,0x70376D,
    };
    return initial>='A' && initial<='Z' ? palette[initial-'A'] : 0x4B5566;
}

PixelBbs::Shortcut PlaceDiscoveredApp(int width,int height,unsigned index,unsigned count)
{
    const int reference=width*3/4<height ? width*3/4 : height;
    const int scale=reference<480 ? 480 : reference>1200 ? 1200 : reference;
    const int margin=20*scale/768;
    const int top=80+margin;
    const int icon=34*scale/768;
    const int gap=7*scale/768;
    const int textHeight=width>=1280 && height>=900 ? 32 : 16;
    const int cellHeight=icon+gap+textHeight;
    const int pitch=cellHeight+9*scale/768;
    const int available=1+(height-49-top-cellHeight)/pitch;
    const unsigned rows=available>0 ? unsigned(available) : 1;
    const unsigned columns=(count+rows-1)/rows;
    const int columnWidth=(width-2*margin)/int(columns);
    return {margin+int(index/rows)*columnWidth,top+int(index%rows)*pitch,
            columnWidth-gap,cellHeight,icon,gap};
}
}
""", 1)


def kernel_source(text):
    text = '#include "xapp_loader.h"\n' + text
    text = text.replace('if (storageMounted_) LogBootTiming("fat32_ready");',
                        'if (storageMounted_) { desktop_.ScanDesktopApps(); LogBootTiming("fat32_ready"); }', 1)
    if "desktop_.ScanDesktopApps()" not in text:
        raise SystemExit("FAT32 mount landmark missing")
    return text.replace("        sketch.base.struct_bytes = sizeof sketch;",
                        """        sketch.base.struct_bytes = sizeof sketch;
        if (const auto *app=desktop_.TakeDiscoveredApp()) {
            if (!uartOpen && canOpen && storageMounted_) {
                const bool arduino=app->kind==XdosAppKind::Arduino;
                const auto result=XappLoaderV1::ExecuteFile(
                    app->path, arduino ? &sketch.base : &api, arduino);
                desktop_.ReportAppResult(result==XappLoaderV1::OK ?
                    "XAPP: application started" : "XAPP: invalid or incompatible application");
            }
        }""", 1)


edit("desktop.h", desktop_header)
edit("desktop.cpp", desktop_source)
edit("kernel.cpp", kernel_source)
edit("Makefile", lambda text: re.sub(r"(?m)^include (.*Rules\.mk.*)$",
                                    r"OBJS += desktop_apps.o\ninclude \1", text, count=1))
