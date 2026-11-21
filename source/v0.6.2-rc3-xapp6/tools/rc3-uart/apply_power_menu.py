"""Add the desktop XDOS button's Restart and Shut Down menu."""
from pathlib import Path
import sys

root = Path(sys.argv[1])

def patch(name, *changes):
    path = root / name
    text = path.read_text(encoding="utf-8")
    for old, new in changes:
        if text.count(old) != 1:
            raise SystemExit(f"{name}: missing or non-unique landmark: {old}")
        text = text.replace(old, new, 1)
    path.write_text(text, encoding="utf-8")

patch("desktop.h",
    ("    void ScanDesktopApps();", """    void ScanDesktopApps();
    enum class PowerRequest { None, Restart, ShutDown };
    PowerRequest TakePowerRequest();
    bool StorageWorkPending() const { return storagePending_ || storageBusy_; }"""),
    ("    unsigned discoveredCount_ = 0;", """    unsigned discoveredCount_ = 0;
    bool powerMenuOpen_ = false;
    PowerRequest powerRequest_ = PowerRequest::None;"""))

patch("desktop.cpp",
    ("void Desktop::Mouse(MouseAction action,unsigned buttons,int x,int y)", """namespace {
constexpr int PowerMenuX = 14;
constexpr int PowerMenuWidth = 142;
constexpr int PowerMenuRowHeight = 32;
constexpr int PowerMenuGap = 5;

int PowerMenuY(int height)
{
    return height-49-PowerMenuGap-2*PowerMenuRowHeight;
}
}

Desktop::PowerRequest Desktop::TakePowerRequest()
{
    const auto request=powerRequest_;
    powerRequest_=PowerRequest::None;
    return request;
}

void Desktop::Mouse(MouseAction action,unsigned buttons,int x,int y)"""),
    ("    if(action!=Down||!(buttons&1)) return;", """    if(action!=Down||!(buttons&1)) return;
    if (Contains(x,y,14,height_-42,123,33)) {
        powerMenuOpen_=!powerMenuOpen_;
        RequestFullRepaint();
        return;
    }
    if (powerMenuOpen_) {
        const int top=PowerMenuY(height_);
        if (Contains(x,y,PowerMenuX,top,PowerMenuWidth,2*PowerMenuRowHeight))
            powerRequest_=y<top+PowerMenuRowHeight ? PowerRequest::Restart : PowerRequest::ShutDown;
        powerMenuOpen_=false;
        RequestFullRepaint();
        return;
    }"""),
    ("void Desktop::Key(char ch)\n{", """void Desktop::Key(char ch)
{
    if (powerMenuOpen_) {
        if (ch=='r' || ch=='R') powerRequest_=PowerRequest::Restart;
        else if (ch=='s' || ch=='S') powerRequest_=PowerRequest::ShutDown;
        else if (ch!=27) return;
        powerMenuOpen_=false;
        RequestFullRepaint();
        return;
    }"""),
    ('    Text(28,height_-33,"XD OS",NAV);', """    Text(28,height_-33,"XDOS",NAV);
    if (powerMenuOpen_) {
        const int top=PowerMenuY(height_);
        const int labelX=PowerMenuX+12;
        Fill(PowerMenuX,top,PowerMenuWidth,2*PowerMenuRowHeight,BG2);
        Frame(PowerMenuX,top,PowerMenuWidth,2*PowerMenuRowHeight,ACCENT);
        Fill(PowerMenuX+1,top+PowerMenuRowHeight,PowerMenuWidth-2,1,0x42637A);
        Text(labelX,top+8,"Restart",WHITE);
        Fill(labelX,top+24,8,1,WHITE);
        Text(labelX,top+PowerMenuRowHeight+8,"Shut Down",WHITE);
        Fill(labelX,top+PowerMenuRowHeight+24,8,1,WHITE);
    }"""))

patch("uart_service.h", ("    bool TransferActive() const { return active_; }",
    """    bool TransferActive() const { return active_; }
    bool PowerTransferActive() const { return active_ || fetchState_==FetchState::Working; }"""))

patch("kernel.cpp", ("        const bool canOpen=ui_.AtDesktop() && !uart.TransferActive();",
    """        const bool canOpen=ui_.AtDesktop() && !uart.TransferActive();
        const auto power=desktop_.TakePowerRequest();
        if (power!=Desktop::PowerRequest::None && ui_.AtDesktop()) {
            if (uart.PowerTransferActive() || desktop_.StorageWorkPending()) {
                desktop_.ReportAppResult("Power action unavailable while storage is busy");
            } else {
                const bool restart=power==Desktop::PowerRequest::Restart;
                if (storageMounted_) {
                    f_mount(nullptr,"SD:",0);
                    storageMounted_=false;
                }
                desktop_.ReportAppResult(restart ? "Restarting XDOS..." : "Shut down. Safe to disconnect power.");
                ui_.Refresh(CTimer::GetClockTicks());
                lv_refr_now(lv_display_get_default());
                scheduler_.MsSleep(40);
                xdos_present_frame();
                Debug(restart ? "[POWER] Desktop restart\\r\\n" : "[POWER] Desktop shutdown\\r\\n");
                if (restart) reboot();
                halt();
            }
        }"""))
