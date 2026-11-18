#include <stdint.h>
#include <stddef.h>

// Raspberry Pi 3B+ (BCM2837B0): peripheral base in ARM physical map.
#define MBOX_BASE   0x3F00B880u
#define MBOX_READ   (*(volatile uint32_t *)(MBOX_BASE + 0x00u))
#define MBOX_READ_STATUS  (*(volatile uint32_t *)(MBOX_BASE + 0x18u))
#define MBOX_WRITE_STATUS (*(volatile uint32_t *)(MBOX_BASE + 0x38u))
#define MBOX_WRITE  (*(volatile uint32_t *)(MBOX_BASE + 0x20u))
#define MBOX_FULL   0x80000000u
#define MBOX_EMPTY  0x40000000u
#define MBOX_CH_PROP 8u

// Mailbox tag buffers must be 16-byte aligned. Firmware requires bus address.
__attribute__((aligned(16))) static volatile uint32_t mbox[36];
static uint32_t *pixels;
static uint32_t pitch;
static uint32_t screen_w = 1024u, screen_h = 768u;

static inline void memory_barrier(void) {
    __asm__ volatile("dmb sy" ::: "memory");
}

static int mbox_call(uint32_t channel) {
    // Property channel 8 takes the ARM physical address of the aligned buffer.
    uint32_t address = ((uint32_t)(uintptr_t)&mbox[0] & ~15u);
    uint32_t message = (address & ~15u) | (channel & 15u);
    uint32_t timeout = 20000000u;
    memory_barrier();
    while ((MBOX_WRITE_STATUS & MBOX_FULL) && --timeout) { }
    if (!timeout) return 0;
    MBOX_WRITE = message;
    timeout = 40000000u;
    while (--timeout) {
        if (MBOX_READ_STATUS & MBOX_EMPTY) continue;
        uint32_t value = MBOX_READ;
        if (value == message) {
            memory_barrier();
            return mbox[1] == 0x80000000u;
        }
    }
    return 0;
}

static int init_framebuffer(void) {
    // Tags: physical size, virtual size, virtual offset, depth, pixel order,
    // allocate frame buffer, and get pitch. Total 35 words incl terminator.
    static const uint32_t msg[] = {
        35u * 4u, 0u,
        0x00048003u, 8u, 8u, 1024u, 768u,
        0x00048004u, 8u, 8u, 1024u, 768u,
        0x00048009u, 8u, 8u, 0u, 0u,
        0x00048005u, 4u, 4u, 32u,
        0x00048006u, 4u, 4u, 1u,
        0x00040001u, 8u, 8u, 16u, 0u,
        0x00040008u, 4u, 4u, 0u,
        0u
    };
    for (unsigned i = 0; i < sizeof(msg)/sizeof(msg[0]); ++i) mbox[i] = msg[i];
    if (!mbox_call(MBOX_CH_PROP)) return 0;
    if ((mbox[28] & 0x3fffffffu) == 0u || mbox[29] == 0u) return 0;
    pixels = (uint32_t *)(uintptr_t)(mbox[28] & 0x3fffffffu);
    pitch = mbox[33];
    if (pitch < 1024u*4u) return 0;
    return 1;
}

static void rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    int x2 = x + w, y2 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > (int)screen_w) x2 = (int)screen_w;
    if (y2 > (int)screen_h) y2 = (int)screen_h;
    for (int yy=y; yy<y2; ++yy) {
        volatile uint32_t *p = (volatile uint32_t *)((uintptr_t)pixels + (uintptr_t)yy * pitch);
        for (int xx=x; xx<x2; ++xx) p[xx] = color;
    }
}
static void border(int x, int y, int w, int h, uint32_t c) {
    rect(x,y,w,2,c); rect(x,y+h-2,w,2,c);
    rect(x,y,2,h,c); rect(x+w-2,y,2,h,c);
}

// Simple built-in 5x7 glyphs: only characters used by demo text.
static const char *glyph(char c) {
    switch (c) {
    case ' ': return "00000000000000000000000000000000000";
    case 'A': return "01110100011000111111100011000110001";
    case 'B': return "11110100011000111110100011000111110";
    case 'C': return "01111100001000010000100001000001111";
    case 'D': return "11110100011000110001100011000111110";
    case 'E': return "11111100001000011110100001000011111";
    case 'F': return "11111100001000011110100001000010000";
    case 'G': return "01111100001000010111100011000101111";
    case 'H': return "10001100011000111111100011000110001";
    case 'I': return "11111001000010000100001000010011111";
    case 'K': return "10001100101010011000101001001010001";
    case 'L': return "10000100001000010000100001000011111";
    case 'M': return "10001110111010110101100011000110001";
    case 'N': return "10001110011010110011100011000110001";
    case 'O': return "01110100011000110001100011000101110";
    case 'P': return "11110100011000111110100001000010000";
    case 'R': return "11110100011000111110101001001010001";
    case 'S': return "01111100001000001110000010000111110";
    case 'T': return "11111001000010000100001000010000100";
    case 'U': return "10001100011000110001100011000101110";
    case 'V': return "10001100011000110001100010101001000";
    case 'W': return "10001100011000110101101011101110001";
    case 'X': return "10001100010101000100010101000110001";
    case 'Y': return "10001100010101000100001000010000100";
    case '0': return "01110100011001110101110011000101110";
    case '1': return "00100011000010000100001000010001110";
    case '2': return "01110100010000100010001000100011111";
    case '3': return "11110000010000101110000010000111110";
    case '4': return "00010001100101010010111110001000010";
    case '5': return "11111100001000011110000010000111110";
    case '6': return "01110100001000011110100011000101110";
    case '7': return "11111000010001000100010000100001000";
    case '8': return "01110100011000101110100011000101110";
    case '9': return "01110100011000101111000010000101110";
    case '+': return "00000001000010011111001000010000000";
    case '.': return "00000000000000000000000000110001100";
    case '-': return "00000000000000011111000000000000000";
    case '/': return "00001000010001000100010001000010000";
    case ':': return "00000011000110000000011000110000000";
    default: return "00000000000000000000000000000000000";
    }
}
static void drawtext(int x, int y, const char *s, uint32_t color, int scale) {
    while (*s) {
        const char *g = glyph(*s++);
        for (int yy=0;yy<7;yy++) for (int xx=0;xx<5;xx++)
            if (g[yy*5+xx]=='1') rect(x+xx*scale,y+yy*scale,scale,scale,color);
        x += 6*scale;
    }
}
static void window(int x,int y,int w,int h,const char *title,uint32_t body) {
    rect(x+7,y+7,w,h,0x192936);
    rect(x,y,w,h,0xe8edf4);
    border(x,y,w,h,0x1d364b);
    rect(x+2,y+2,w-4,34,0x173d65);
    drawtext(x+14,y+11,title,0xffffff,2);
    rect(x+w-34,y+10,22,17,0xb83b43);
    drawtext(x+w-28,y+12,"X",0xffffff,2);
    rect(x+4,y+38,w-8,h-42,body);
}
static void desktop(void) {
    rect(0,0,1024,768,0x183948);
    rect(0,0,1024,54,0x0c2837);
    drawtext(20,16,"XD OS ARM32",0xf2f4f8,3);
    drawtext(765,21,"PI 3B+  V0.1",0x82daeb,2);

    // Decorative desktop icons, no input handlers in this build.
    const char *names[] = {"FILES", "PAINT", "CALC", "BROWSER"};
    for (int i=0;i<4;i++) {
        int y=94+i*116;
        rect(38,y,54,54,0x3b9caa);
        rect(46,y+12,38,30,0xa9e0e6);
        drawtext(24,y+66,names[i],0xffffff,2);
    }

    window(160,91,552,395,"WELCOME TO XD OS",0xf6f8fb);
    drawtext(183,149,"XD OS ARM32",0x163d61,4);
    drawtext(183,206,"BARE-METAL BOOT",0x182f3d,2);
    drawtext(183,240,"RASPBERRY PI 3B+",0x182f3d,2);
    drawtext(183,274,"HDMI FRAMEBUFFER OK",0x188064,2);
    drawtext(183,320,"STATIC DISPLAY TEST",0x586a79,2);
    rect(182,380,478,2,0xc2cbd5);
    drawtext(182,400,"NO LINUX - NO QEMU",0x355a6b,2);

    window(609,375,357,233,"SYSTEM INFO",0xd7eaf2);
    drawtext(633,433,"TARGET: ARM32",0x173d65,2);
    drawtext(633,469,"BOARD: PI 3B+",0x173d65,2);
    drawtext(633,505,"SCREEN: 1024X768",0x173d65,2);
    drawtext(633,541,"PHASE: V0.1",0x188064,2);

    rect(0,727,1024,41,0x091d29);
    drawtext(15,740,"XD OS",0xffffff,2);
    drawtext(760,740,"ARM32 DESKTOP",0xb5d6df,2);
}

void kernel_main(void) {
    if (init_framebuffer()) desktop();
    while (1) __asm__ volatile("wfe");
}
