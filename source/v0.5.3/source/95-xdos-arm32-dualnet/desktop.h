#ifndef XDOS_DESKTOP_H
#define XDOS_DESKTOP_H

// Hardware independent desktop. No Linux, no libc heap allocations.
// RGB565 and 32-bit XRGB framebuffer supported.
class Desktop
{
public:
    enum MouseAction { Move=0, Down=1, Up=2, Wheel=3 };
    Desktop();
    void Resize(int w, int h);
    void Mouse(MouseAction action, unsigned buttons, int x, int y);
    void Key(char ch);
    void Render(void *framebuffer, unsigned pitchBytes, unsigned depthBits);
    void Print(const char *s);
    bool NeedsRedraw() const { return dirty_; }
    void SetUSB(bool keyboard, bool mouse);
    void SetNetwork(bool running, const char *status);
    bool TakeBrowserRequest(char *out, unsigned size, unsigned &requestID);
    bool BrowserRequestActive(unsigned requestID) const;
    void CancelBrowser();
    void BrowserStatus(const char *status);
    void BrowserError(const char *error);
    void BrowserLoaded(const char *html, unsigned length);
    void GetDamage(int &x, int &y, int &w, int &h) const;
    void ClearDamage();
    int MouseX() const {return mouseX_;}
    int MouseY() const {return mouseY_;}
    int Width() const { return width_; }
    int Height() const { return height_; }

private:
    enum { NWIN=6, NPAINT=1800 };
    enum App { Terminal, Notepad, Calculator, Paint, Files, Browser };
    struct Window { int x, y, w, h; bool open; };
    static_assert(__builtin_offsetof(Window, x)==0 &&
                  __builtin_offsetof(Window, y)==4 &&
                  __builtin_offsetof(Window, w)==8 &&
                  __builtin_offsetof(Window, h)==12 &&
                  __builtin_offsetof(Window, open)==16,
                  "ASM Window layout offsets must match C++ struct");
    struct Mark { unsigned short x, y; };
    Window windows_[NWIN];
    Mark marks_[NPAINT];
    unsigned markCount_;
    char terminal_[2048], note_[2048], cmd_[160], calcDisplay_[32];
    char browserURL_[192], browserContent_[12000], browserStatus_[110];
    char netStatus_[64];
    bool netRunning_, browserPending_, browserLoading_;
    unsigned browserRequestID_;
    bool damagePartial_;
    int damageX0_,damageY0_,damageX1_,damageY1_;
    unsigned termLen_, noteLen_, cmdLen_, calcLen_;
    long calcValue_, calcEntry_;
    char calcOp_;
    bool calcNewEntry_;
    int width_, height_, active_, dragIndex_, dragDX_, dragDY_;
    int mouseX_, mouseY_;
    bool dirty_, keyboard_, mouse_;
    void *framebuffer_;
    unsigned pitchBytes_, depth_;

    static int Clamp(int value, int lo, int hi);
    static bool Contains(int x,int y,int rx,int ry,int rw,int rh);
    void Fill(int x,int y,int w,int h,unsigned color);
    void Frame(int x,int y,int w,int h,unsigned color);
    void Text(int x,int y,const char *str,unsigned color);
    void TextLines(int x,int y,int right,int bottom,const char *str,unsigned color,int maxCols=0);
    void Prompt();
    void Open(int index);
    void TapCalculator(int x,int y);
    void PaintAt(int x,int y);
    void DrawWindow(int index);
    void DrawDesktop();
    void DrawIcons();
    void DrawTerminal(int i);
    void DrawNotepad(int i);
    void DrawCalculator(int i);
    void DrawPaint(int i);
    void DrawPlaceholder(int i);
    void DrawBrowser(int i);
    void SubmitBrowserURL();
    void DragDamage(int oldX,int oldY,int newX,int newY,int w,int h);
    void DrawTaskbar();
    void DrawSmallIcon(int i,int x,int y);
    int HitWindow(int x,int y) const;
};
#endif
