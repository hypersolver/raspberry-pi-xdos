#include "desktop_apps.h"
#ifndef XDOS_DESKTOP_H
#define XDOS_DESKTOP_H

// Hardware independent desktop. No Linux, no libc heap allocations.
// RGB565 and 32-bit XRGB framebuffer supported.
class Desktop
{
public:
    static constexpr int CoreStripX = 380;   // taskbar CPU strip start (pixels)

public:
    enum MouseAction { Move=0, Down=1, Up=2, Wheel=3 };
    enum StorageAction { StorageNone=0, StorageList=1, StorageListRoot=2,
                         StorageLoadNote=3, StorageSaveNote=4, StorageCat=5,
                         StorageUSBList=6, StorageUSBCat=7, StorageUSBWrite=8,
                         StorageUSBEject=9, StorageUSBMount=10 };
    struct StorageRequest {
        StorageAction action;
        char filename[64];
        char data[2048];
        unsigned length;
    };
    Desktop();
    void Resize(int w, int h);
    void ScanDesktopApps();
    bool OpenCatalogModule(const char *path);
    enum class PowerRequest { None, Restart, ShutDown };
    PowerRequest TakePowerRequest();
    void ShowShutdownNotice() { shutdownNotice_=true; RequestFullRepaint(); }
    void UpdateDesktopPointer(int x,int y);
    bool DesktopNavigationKey(unsigned key);
    void DrawPowerText(int x,int y,const char *text,int scale);
    bool StorageWorkPending() const { return storagePending_ || storageBusy_; }
    const XdosDesktopApp *TakeDiscoveredApp();
    void Mouse(MouseAction action, unsigned buttons, int x, int y);
    void Key(char ch);
    void Render(void *framebuffer, unsigned pitchBytes, unsigned depthBits);
    void Print(const char *s);
    // LVGL optional UI mode retains the original ASM desktop and app data.
    bool TakeLVGLRequest();
    void RequestLVGL(); // SDK host callback, not called from IRQ
    bool TakeOfficialBenchmarkLaunchRequest();
    bool TakeCalculatorRequest();
    bool TakeNotepadRequest();
    enum Workspace { NoWorkspace, PaintApp, FilesApp, BrowserApp, TerminalApp, PlayerApp, MusicApp };
    Workspace TakeWorkspaceRequest();
    bool RequestList() { return QueueStorage(StorageList); }
    bool RequestRead(const char *name) { return QueueStorage(StorageCat, name); }
    bool RequestHttp(const char *url);
    const char *BrowserText() const { return browserContent_; }
    const char *BrowserMessage() const { return browserStatus_; }
    unsigned BrowserRevision() const { return browserRevision_; }
    const char *FilesText() const { return filesContent_; }
    const char *FilesMessage() const { return filesStatus_; }
    unsigned FilesRevision() const { return filesRevision_; }
    bool TakeAppLaunchRequest(); // desktop shortcut requests manifest on FAT32
    void ReportAppResult(const char *message);
    void RequestFullRepaint();
    void RunLegacyTerminal(const char *text);
    const char *TerminalText() const { return terminal_; }
    unsigned TerminalRevision() const { return terminalRevision_; }
    unsigned NetworkRevision() const { return networkRevision_; }
    const char *NetworkStatus() const { return netStatus_; }
    const char *StorageStatus() const { return storageStatus_; }
    const char *USBStorageStatus() const { return usbStorageStatus_; }
    unsigned USBStorageRevision() const { return usbStorageRevision_; }
    const char *NoteText() const { return note_; }
    unsigned NoteRevision() const { return noteRevision_; }
    unsigned NoteStorageRevision() const { return noteStorageRevision_; }
    const char *NoteStatus() const { return noteStatus_; }
    bool SaveNoteText(const char *text) {
        if (!storageReady_ || storageBusy_ || storagePending_) return false;
        SetNoteFromUI(text);
        return SaveNote();
    }
    void SetNoteFromUI(const char *text);
    bool SaveNote() { return QueueStorage(StorageSaveNote); }
    bool LoadNote() { return QueueStorage(StorageLoadNote); }
    bool ListUSB() { return QueueStorage(StorageUSBList); }
    bool EjectUSB() { return QueueStorage(StorageUSBEject); }
    void SetCPUStatus(const char *status);
    const char *CPUStatus() const { return cpuStatus_; }
    bool NeedsRedraw() const { return dirty_; }
    void SetUSB(bool keyboard, bool mouse);
    void SetNetwork(bool running, const char *status);
    void SetCpuLoad(unsigned cpu0, unsigned cpu1, unsigned cpu2, unsigned cpu3);
    // Taskbar: app names running on CPU0..3 ("" = idle / LVGL draw core).
    void SetCoreApps(const char (*apps)[64]);
    void SetStorage(bool ready, const char *status);
    void SetUSBStorage(bool ready,const char *status);
    bool TakeStorageRequest(StorageRequest &out);
    void StorageCompleted(StorageAction action, bool success, const char *message,
                          const char *data, unsigned length);
    const char *BrowserURL() const { return browserURL_; }
    bool TakeBrowserRequest(char *out, unsigned size, unsigned &requestID);
    bool BrowserRequestActive(unsigned requestID) const;
    void CancelBrowser();
    void BrowserStatus(const char *status);
    void BrowserError(const char *error);
    static constexpr unsigned BrowserTextCapacity() { return sizeof(browserContent_); }
    void BrowserLoaded(const char *text, unsigned length);
    void GetDamage(int &x, int &y, int &w, int &h) const;
    void ClearDamage();
    void RenderDamage(void *framebuffer,unsigned pitchBytes,unsigned depthBits,
                      int x,int y,int w,int h);
    int MouseX() const {return mouseX_;}
    int MouseY() const {return mouseY_;}
    int Width() const { return width_; }
    int Height() const { return height_; }

private:
    enum { NWIN=6 };
    enum App { Terminal, Notepad, Calculator, Paint, Files, Browser };
    struct Window { int x, y, w, h; bool open; };
    static_assert(__builtin_offsetof(Window, x)==0 &&
                  __builtin_offsetof(Window, y)==4 &&
                  __builtin_offsetof(Window, w)==8 &&
                  __builtin_offsetof(Window, h)==12 &&
                  __builtin_offsetof(Window, open)==16,
                  "ASM Window layout offsets must match C++ struct");
    Window windows_[NWIN];
    XdosDesktopApp discoveredApps_[XdosDesktopAppLimit]{};
    unsigned discoveredCount_ = 0;
    bool powerMenuOpen_ = false;
    bool shutdownNotice_ = false;
    int pointerX_ = -1, pointerY_ = -1;
    int iconFocus_ = -1, powerFocus_ = -1;
    void SetIconFocus(int index);
    int IconHighlightSize() const;
    // C6: the single box used to draw, highlight, hover and click an icon.
    void IconBox(unsigned index,int &x,int &y,int &side) const;
    void BlendWhite(int x,int y,int w,int h);
    PowerRequest powerRequest_ = PowerRequest::None;
    int discoveredPending_ = -1;
    void OpenDiscoveredApp(unsigned index);
    Workspace workspacePending_ = NoWorkspace;
    unsigned filesRevision_ = 0, browserRevision_ = 0;
    char filesStatus_[96] = {};
    char terminal_[8192], note_[2048], cmd_[160];
    char browserURL_[192], browserContent_[12000], browserStatus_[110];
    char netStatus_[64];
    unsigned cpuLoad_[4]{101,101,101,101};   // 101 = not sampled yet
    char coreApps_[4][64]{};                  // taskbar: apps per CPU
    char storageStatus_[64], usbStorageStatus_[80], filesContent_[4096], noteStatus_[64];
    bool usbStorageReady_;
    unsigned usbStorageRevision_;
    bool storageReady_, storagePending_, storageBusy_;
    StorageRequest storageRequest_;
    bool netRunning_, browserPending_, browserLoading_;
    unsigned browserRequestID_;
    bool damagePartial_;
    int damageX0_,damageY0_,damageX1_,damageY1_;
    unsigned termLen_, noteLen_, cmdLen_;
    bool calculatorPending_ = false;
    bool notepadPending_ = false;
    unsigned noteStorageRevision_ = 0;
    int width_, height_, active_, dragIndex_, dragDX_, dragDY_;
    int mouseX_, mouseY_;
    bool officialBenchPending_ = false;
    bool dirty_, keyboard_, mouse_, lvglRequested_, appLaunchPending_;
    char cpuStatus_[80];
    unsigned noteRevision_;
    unsigned terminalRevision_, networkRevision_;
    void *framebuffer_;
    unsigned pitchBytes_, depth_;

    static int Clamp(int value, int lo, int hi);
    static bool Contains(int x,int y,int rx,int ry,int rw,int rh);
    void Fill(int x,int y,int w,int h,unsigned color);
    void Frame(int x,int y,int w,int h,unsigned color);
    void Text(int x,int y,const char *str,unsigned color);
    void TextLines(int x,int y,int right,int bottom,const char *str,unsigned color,int maxCols=0);
    void Prompt();
    bool QueueStorage(StorageAction action, const char *name=0,
                      const char *text=0);
    void Open(int index);
    void DrawWindow(int index);
    void DrawDesktop();
    void PixelText(int x,int y,const char *,unsigned color);
    void DrawIcons();
    void DrawTerminal(int i);
    void DrawPlaceholder(int i);
    void SubmitBrowserURL();
    void MarkDamage(int x0,int y0,int x1,int y1);
    bool clipRender_ = false;
    int clipX0_ = 0, clipY0_ = 0, clipX1_ = 0, clipY1_ = 0;
    void DragDamage(int oldX,int oldY,int newX,int newY,int w,int h);
    void DrawTaskbar();
    int HitWindow(int x,int y) const;
};
#endif
