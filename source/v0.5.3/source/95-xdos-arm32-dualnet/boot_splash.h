#ifndef XDOS_BOOT_SPLASH_H
#define XDOS_BOOT_SPLASH_H

#include <circle/types.h>
#include <circle/bcmframebuffer.h>
#include <circle/chargenerator.h>
#include <circle/font.h>

// Boot diagnostics use ASCII because Circle's built-in Font8x16 is Latin-only.
// This module draws directly to the live HDMI framebuffer until XD OS desktop takes over.
// It is deliberately independent of Circle's terminal cursor/escape rendering.
class CBootSplash
{
public:
    enum TStatus { OK, PENDING, WARNING, ERROR };
    CBootSplash();
    boolean Begin(CBcmFrameBuffer *fb);
    void Report(unsigned step, const char *component, const char *detail, TStatus status = OK);
    void Finish(const char *message);
    unsigned GetLineCount() const { return count_; }
    const char *GetLine(unsigned index) const { return index < count_ ? lines_[index] : ""; }

private:
    static const unsigned MAX_LINES = 16;
    static const unsigned MAX_CHARS = 100;
    void FullBackground();
    void RefreshConsole();
    void RefreshProgress();
    void Rect(int x,int y,int w,int h,u32 color);
    void Text(int x,int y,const char *message,u32 color,unsigned scale=1);
    void Pixel(unsigned x,unsigned y,u32 color);
    u32 Color(u32 rgb) const;
    static void Add(char *dst,unsigned cap,const char *s);

    u8 *base_;
    unsigned width_,height_,pitch_,depth_;
    unsigned count_,step_;
    boolean initialized_,done_;
    char lines_[MAX_LINES][MAX_CHARS];
    TStatus statuses_[MAX_LINES];
    char final_[80];
    CCharGenerator font_;
};

#endif
