"""Add physical UART commands that open the normal desktop XAPP shortcuts."""

from pathlib import Path
import sys


source = Path(sys.argv[1])
files = {name: (source / name).read_text()
         for name in ("kernel.cpp", "uart_service.h", "app_shortcut.cpp")}


def edit(name, old, new):
    if files[name].count(old) != 1:
        raise RuntimeError(f"{name}: expected one occurrence of {old!r}")
    files[name] = files[name].replace(old, new)


edit("uart_service.h", "    struct LvglStats {", """    enum class Shortcut { None, Notepad, Calculator, Paint, Files, Browser, Update, Widgets, Benchmark };

    Shortcut TakeOpenRequest()
    {
        const auto requested = shortcut_;
        shortcut_ = Shortcut::None;
        return requested;
    }

    void CompleteOpen(CSerialDevice &serial, bool succeeded)
    {
        Reply(serial,succeeded ? "@XDOS OPENED @XDOS OK\\r\\n" : "@XDOS ERROR\\r\\n");
    }

    struct LvglStats {""")
edit("uart_service.h", "    LvglStats lvglStats_{};", """    LvglStats lvglStats_{};
    Shortcut shortcut_ = Shortcut::None;

    static Shortcut LookupShortcut(const char *name)
    {
        static const struct { const char *name; Shortcut shortcut; } shortcuts[] = {
            {"NOTEPAD",Shortcut::Notepad}, {"CALCULATOR",Shortcut::Calculator},
            {"PAINT",Shortcut::Paint}, {"FILES",Shortcut::Files},
            {"BROWSER",Shortcut::Browser}, {"UPDATE",Shortcut::Update},
            {"WIDGETS",Shortcut::Widgets}, {"BENCHMARK",Shortcut::Benchmark}
        };
        for (const auto &entry : shortcuts)
            if (!strcmp(name,entry.name)) return entry.shortcut;
        return Shortcut::None;
    }""")
edit("uart_service.h", '        if (!strcmp(cmd, "LVGL")) {', """        if (!strncmp(cmd,"OPEN ",5)) {
            const auto requested = LookupShortcut(cmd+5);
            if (mounted && !active_ && shortcut_ == Shortcut::None && requested != Shortcut::None)
                shortcut_ = requested;
            else
                CompleteOpen(serial,false);
            return;
        }
        if (!strcmp(cmd, "LVGL")) {""")
edit("kernel.cpp", "XAPP6-PERF3 / Pi 3 ARM32", "XAPP6-PERF4 / Pi 3 ARM32")
edit("app_shortcut.cpp", "P3WIDGET.XAPP", "P4WIDGET.XAPP")
edit("app_shortcut.cpp", "P3BENCH.XAPP", "P4BENCH.XAPP")
edit("app_shortcut.cpp", "    if (result!=XappLoaderV1::OPEN_FAILED) return result;",
     """    if (result!=XappLoaderV1::OPEN_FAILED && result!=XappLoaderV1::FORMAT_INVALID)
        return result;""")
edit("kernel.cpp", """        ProcessInput();
        XdosHostApiV1 api=""", """        ProcessInput();
        using Shortcut = XdosUartService::Shortcut;
        const auto shortcut=uart.TakeOpenRequest();
        const bool uartOpen=shortcut!=Shortcut::None;
#if XDOS_USE_LVGL
        const bool canOpen=ui_.AtDesktop() && !uart.TransferActive();
#endif
        XdosHostApiV1 api=""")
for getter, shortcut in (("TakeCalculatorRequest", "Calculator"),
                         ("TakeNotepadRequest", "Notepad"),
                         ("TakeOfficialBenchmarkLaunchRequest", "Benchmark")):
    edit("kernel.cpp", f"        if (desktop_.{getter}() && ui_.AtDesktop()) {{",
         f"""        if ((uartOpen ? shortcut==Shortcut::{shortcut} : desktop_.{getter}()) &&
            ui_.AtDesktop() && (!uartOpen || canOpen)) {{""")
edit("kernel.cpp", """        const auto workspace = desktop_.TakeWorkspaceRequest();
        if (workspace != Desktop::NoWorkspace && ui_.AtDesktop()) {""",
     """        auto workspace = desktop_.TakeWorkspaceRequest();
        if (uartOpen) {
            switch (shortcut) {
            case Shortcut::Paint: workspace=Desktop::PaintApp; break;
            case Shortcut::Files: workspace=Desktop::FilesApp; break;
            case Shortcut::Browser: workspace=Desktop::BrowserApp; break;
            case Shortcut::Update: workspace=Desktop::UpdateApp; break;
            default: workspace=Desktop::NoWorkspace; break;
            }
        }
        if (workspace != Desktop::NoWorkspace && ui_.AtDesktop() && (!uartOpen || canOpen)) {""")
edit("kernel.cpp", "        if ((openWidgets || openBuiltinWidgets) && ui_.AtDesktop()) {",
     """        if ((uartOpen ? shortcut==Shortcut::Widgets : openWidgets || openBuiltinWidgets) &&
            ui_.AtDesktop() && (!uartOpen || canOpen)) {""")
edit("kernel.cpp", "        if (openWidgets) {", "        if (!uartOpen && openWidgets) {")
edit("kernel.cpp", """        if (desktop_.TakeBenchmarkLaunchRequest()) {
#if XDOS_USE_LVGL""", """        const bool openXdsBenchmark=desktop_.TakeBenchmarkLaunchRequest();
        if (!uartOpen && openXdsBenchmark) {
#if XDOS_USE_LVGL""")
edit("kernel.cpp", """#if XDOS_USE_LVGL
        ui_.Tick(nowUs);
#endif""", """#if XDOS_USE_LVGL
        if (uartOpen) uart.CompleteOpen(serial_,canOpen && !ui_.AtDesktop());
        ui_.Tick(nowUs);
#else
        if (uartOpen) uart.CompleteOpen(serial_,false);
#endif""")

for name, text in files.items():
    (source / name).write_text(text, encoding="utf-8")
