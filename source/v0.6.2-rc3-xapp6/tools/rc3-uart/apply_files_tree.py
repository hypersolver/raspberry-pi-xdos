"""Add a lazy LVGL directory tree and route XAPP files through the existing loader."""
from pathlib import Path
import sys

root = Path(sys.argv[1])


def edit(name, old, new):
    path = root / name
    text = path.read_text(encoding="utf-8")
    if text.count(old) != 1:
        raise SystemExit(f"{name}: expected one occurrence of {old!r}")
    path.write_text(text.replace(old, new), encoding="utf-8")


edit("xdos_lvgl.h", "    bool CloseCurrentApp();", """    bool CloseCurrentApp();
    const char *TakeFileTreeLaunch();
private:
    char fileTreeLaunch_[512] = {};
    bool fileTreeLaunchPending_ = false;
    void OpenFileTree(lv_obj_t *body);
    void NavigateFiles(void *context,const char *path);
    void RenderFiles(void *context);
    static void FileTreeClick(lv_event_t *event);
    static void AddFileTreeNode(lv_obj_t *parent, lv_obj_t *preview, XDOSLVGL *ui,
                                const char *label, const char *path, bool directory);
public:""")

edit("xdos_lvgl.cpp",
     "    workspaceInput_ = MakeWindowTextArea(body);",
     """    if (panel==XDOS_PANEL_FILES) {
        OpenFileTree(body);
        return 0;
    }
    workspaceInput_ = MakeWindowTextArea(body);""")

edit("desktop.h", "    void ScanDesktopApps();",
     "    void ScanDesktopApps();\n    bool OpenCatalogModule(const char *path);")
edit("desktop.cpp", "#include <string.h>", "#include <string.h>\n#include <strings.h>")
with (root / "desktop.cpp").open("a", encoding="utf-8") as out:
    out.write("""
bool Desktop::OpenCatalogModule(const char *path)
{
    for (unsigned i=0;i<discoveredCount_;++i) {
        if (strcasecmp(discoveredApps_[i].path,path)) continue;
        OpenDiscoveredApp(i);
        return true;
    }
    return false;
}
""")

edit("kernel.cpp", "        const bool canOpen=ui_.AtDesktop() && !uart.TransferActive();",
     """        const bool canOpen=ui_.AtDesktop() && !uart.TransferActive();
        const char *fileModule = canOpen && !uartOpen ? ui_.TakeFileTreeLaunch() : nullptr;
        if (fileModule && desktop_.OpenCatalogModule(fileModule)) fileModule=nullptr;""")
edit("kernel.cpp", "        sketch.base.struct_bytes = sizeof sketch;",
     """        sketch.base.struct_bytes = sizeof sketch;
        if (fileModule) {
            const auto result=XappLoaderV1::ExecuteFile(fileModule,&sketch.base);
            desktop_.ReportAppResult(result==XappLoaderV1::OK ?
                "XAPP: application started" : "XAPP: invalid or incompatible application");
        }""")

# The existing APP manifests stay on SD. The new kernel prefers its paired
# samples once present, which also preserves the old paths for rollback.
edit("desktop_apps.cpp", "    if (!abi || !arch || !app.name[0] || !validModule(app.path)) return false;",
     """    if (arduino) {
        const char *paired=nullptr;
        if (!strcasecmp(app.path,"SD:/XDOS/INBOX/LEDBLINK.XAPP")) paired="SD:/XDOS/INBOX/FQBLINK.XAPP";
        if (!strcasecmp(app.path,"SD:/XDOS/INBOX/ARDLVGL.XAPP")) paired="SD:/XDOS/INBOX/FQLVGL.XAPP";
        if (paired && validModule(paired)) strcpy(app.path,paired);
    }
    if (!abi || !arch || !app.name[0] || !validModule(app.path)) return false;""")

with (root / "xdos_lvgl.cpp").open("a", encoding="utf-8") as out:
    tools = Path(__file__).parent
    (root / "xdos_directory.h").write_bytes((tools / "xdos_directory.h").read_bytes())
    out.write((tools / "xdos_directory.inc").read_text(encoding="utf-8-sig"))
    out.write((tools / "files_tree.inc").read_text(encoding="utf-8-sig"))


edit("xdos_lvgl.cpp", "    auto *body = OpenAppWindow(title,width,height);",
     """    if (panel==XDOS_PANEL_FILES) title="Arduino Files";
    if (panel==XDOS_PANEL_MUSIC) title="Arduino MP3 Player";
    auto *body = OpenAppWindow(title,width,height);""")
edit("desktop_apps.cpp", "    return true;\n}\n}", """    if (app.kind==XdosAppKind::Files) strcpy(app.name,"Arduino Files");
    if (app.kind==XdosAppKind::Music) strcpy(app.name,"Arduino MP3");
    return true;
}
}""")
edit("kernel.cpp", "        if (fileModule) {", """        if (fileModule) {
            const char *leaf=strrchr(fileModule,'/');""".replace(
            "const char *leaf=strrchr(fileModule,'/');",
            "const char *leaf=fileModule;\n            for (const char *p=fileModule;*p;++p) if (*p=='/') leaf=p+1;"))
edit("kernel.cpp", "            const auto result=XappLoaderV1::ExecuteFile(fileModule,&sketch.base);",
     """            const auto result=XappLoaderV1::ExecuteFile(fileModule,&sketch.base);
            if (result==XappLoaderV1::OK && !strcasecmp(leaf,"Q1IMAGE.XAPP"))
                ui_.SetAppTaskTitle("Arduino IMAGE VIEW");""")
edit("kernel.cpp", "#include <circle/actled.h>",
     "#include <circle/actled.h>\n#include <strings.h>")

lvgl = root.parents[1] / "addon/lvgl/lvgl.cpp"
text = lvgl.read_text(encoding="utf-8")
read = "\tpData->point.y = s_pThis->m_PointerData.point.y;"
wheel = "\tcase MouseEventMouseMove:\n\tdefault:"
if text.count(read) != 1 or text.count(wheel) != 1:
    raise SystemExit("Unexpected Circle LVGL input driver layout")
text = text.replace(read, read + """
    pData->enc_diff = __atomic_exchange_n(&s_pThis->m_PointerData.enc_diff,0,__ATOMIC_RELAXED);""")
text = text.replace(wheel, """\tcase MouseEventMouseWheel:
        // HID wheel reports positive for up. Scale by approximately one row.
        __atomic_fetch_add(&s_pThis->m_PointerData.enc_diff,nWheelMove*24,__ATOMIC_RELAXED);
        break;

""" + wheel)
lvgl.write_text(text,encoding="utf-8")
