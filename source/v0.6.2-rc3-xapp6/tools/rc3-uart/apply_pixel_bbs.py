"""Install the Pixel BBS wallpaper in the existing clipped desktop renderer."""
from pathlib import Path
import sys

root = Path(sys.argv[1])
desktop = root / "desktop.cpp"
source = desktop.read_text()
(root / "pixel_bbs.h").write_text("""#pragma once

namespace PixelBbs {
constexpr unsigned Grain(int x,int y,bool lower)
{
    constexpr unsigned palette[]={0x101F38,0x142641,0x182B46,0x1C324C,
                                   0x19354A,0x1A3B50,0x1C4354,0x23505C};
    unsigned hash=unsigned(x/8)*0x9E3779B1u ^ unsigned(y/8)*0x85EBCA77u;
    hash=(hash^(hash>>16))*0x7FEB352Du;
    return palette[((hash^(hash>>15))&3)+(lower ? 4 : 0)];
}
constexpr unsigned char Logo[][7]={
    {17,17,10,4,10,17,17}, {30,17,17,17,17,17,30},
    {14,17,17,17,17,17,14}, {15,16,16,14,1,1,30}
};
constexpr unsigned Ink(unsigned row) { return row<3 ? 0x52BBAE : 0x338D8A; }

struct Shortcut {
    int x,y,width,height,icon,gap;
};

inline Shortcut PlaceShortcut(int width,int height,int index,int apps)
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
    const int availableRows=(height-100-top-cellHeight)/pitch;
    const int rows=availableRows<1 ? 1 : availableRows<apps ? availableRows : apps;
    const int columnWidth=(width-2*margin)/3;
    const int labelWidth=7*textHeight+gap;
    const int x=margin+(index<apps ? index/rows : index-apps)*columnWidth;
    const int y=top+(index<apps ? index%rows : rows)*pitch;
    return {x,y,columnWidth-gap<labelWidth ? columnWidth-gap : labelWidth,cellHeight,icon,gap};
}
}
""")
start = source.index("    const int firstRow=", source.index("void Desktop::DrawDesktop()"))
end = source.index("    Fill(0,0,width_,65,BG2);", start)
wallpaper = """    // Draw only the 8×8 cells intersecting the current damage rectangle.
    const int left=clipRender_ ? Clamp(clipX0_,0,width_) : 0;
    const int top=clipRender_ ? Clamp(clipY0_,65,height_-49) : 65;
    const int right=clipRender_ ? Clamp(clipX1_,0,width_) : width_;
    const int bottom=clipRender_ ? Clamp(clipY1_,65,height_-49) : height_-49;
    const int horizon=65+(height_-114)*3/5;
    for (int y=top&~7;y<bottom;y+=8) {
        for (int x=left&~7;x<right && left<right;x+=8) {
            const int y0=y<65 ? 65 : y;
            Fill(x,y0,8,y+8-y0,PixelBbs::Grain(x,y,y>=horizon));
        }
    }

    // Four small glyphs scale to the active screen with no bitmap allocation.
    const int cell=Clamp(width_/128,4,14);
    const int logoX=(width_-23*cell)/2;
    const int logoY=65+(height_-114-7*cell)/2;
    if (right>logoX && left<logoX+23*cell && bottom>logoY && top<logoY+8*cell) {
        for (unsigned letter=0;letter<4;++letter) {
            for (unsigned row=0;row<7;++row) {
                for (unsigned column=0;column<5;++column) {
                    if (!(PixelBbs::Logo[letter][row] & (16u>>column))) continue;
                    const int x=logoX+int(letter*6+column)*cell;
                    const int y=logoY+int(row)*cell;
                    Fill(x,y,cell-1,cell-1,PixelBbs::Ink(row));
                }
            }
        }
    }
"""
source = '#include "pixel_bbs.h"\n' + source[:start] + wallpaper + source[end:]
source = source.replace("void Desktop::DrawIcons()", """void Desktop::PixelText(int x,int y,const char *label,unsigned color)
{
    if (width_<1280 || height_<900) {
        Text(x,y,label,color);
        return;
    }
    if (clipRender_ && (y+32<=clipY0_ || y>=clipY1_)) return;
    for (;*label;++label,x+=16) {
        if (clipRender_ && x>=clipX1_) break;
        if (clipRender_ && x+16<=clipX0_) continue;
        for (unsigned row=0;row<16;++row) {
            const auto bits=font.GetPixelLine(*label,row);
            for (unsigned column=0;column<8;++column)
                if (font.GetPixel(column,bits)) Fill(x+column*2,y+row*2,2,2,color);
        }
    }
}

void Desktop::DrawIcons()""")
player = '"MP4 PLAYER"' in source
start = source.index("    // Desktop shortcuts")
if player:
    start = source.rindex("    if (Contains(x,y,465,",0,start)
end = source.index("\n}", start)
hit = """    for (int i=0;i<NWIN+%d;++i) {
        const auto slot=PixelBbs::PlaceShortcut(width_,height_,i,NWIN);
        if (!Contains(x,y,slot.x,slot.y,slot.width,slot.height)) continue;
        if (i<NWIN) Open(i);
        else if (i==NWIN) appLaunchPending_=true;
        else if (i==NWIN+1) officialBenchPending_=true;
""" % (3 if player else 2)
if player:
    hit += "        else workspacePending_=PlayerApp;\n"
source = source[:start] + hit + "        return;\n    }" + source[end:]
start = source.index("void Desktop::DrawSmallIcon(")
end = source.index("void Desktop::PixelText(",start)
source = source[:start] + source[end:]
start = source.index("void Desktop::DrawIcons()")
end = source.index("void Desktop::DrawTaskbar()",start)
source = source[:start] + """void Desktop::DrawIcons()
{
    static const unsigned colors[]={0x1E8E86,0x4669B2,0xE99B45,0xCE6A9A,0x5A83BB,0x76B9D5};
    static const char *extraNames[]={"LVGL DEMO","LVGL BENCHMARK"%s};
    static const char *extraGlyphs[]={"L","B"%s};
    const int glyphScale=width_>=1280 && height_>=900 ? 2 : 1;
    for (int i=0;i<NWIN+int(sizeof extraNames/sizeof *extraNames);++i) {
        const auto slot=PixelBbs::PlaceShortcut(width_,height_,i,NWIN);
        const unsigned color=i<NWIN ? colors[i] : 0x4D7E71;
        Fill(slot.x,slot.y,slot.icon,slot.icon,color);
        Frame(slot.x,slot.y,slot.icon,slot.icon,0xC0D4EA);
        PixelText(slot.x+(slot.icon-8*glyphScale)/2,slot.y+(slot.icon-16*glyphScale)/2,
                  i<NWIN ? icons[i] : extraGlyphs[i-NWIN],WHITE);
        PixelText(slot.x,slot.y+slot.icon+slot.gap,
                  i<NWIN ? names[i] : extraNames[i-NWIN],WHITE);
    }
}

""" % (',"MP4 PLAYER"' if player else "", ',"P"' if player else "") + source[end:]
source = source.replace('Text(23,16,"XD OS",WHITE);', 'PixelText(23,6,"XD OS",WHITE);')
source = source.replace('Text(23,37,"ARM32', 'Text(23,42,"ARM32')
desktop.write_text(source)
header = root / "desktop.h"
header.write_text(header.read_text().replace("    void DrawIcons();",
    "    void PixelText(int x,int y,const char *,unsigned color);\n    void DrawIcons();")
    .replace("    void DrawSmallIcon(int i,int x,int y);\n",""))

splash = root / "boot_splash.cpp"
source = '#include "pixel_bbs.h"\n' + splash.read_text()
start = source.index("    // Minimal XD OS identity")
end = source.index("    Text(left+111,131", start)
source = source[:start] + """    const int W=(int)width_,H=(int)height_;
    for (int y=0;y<H;y+=8)
        for (int x=0;x<W;x+=8)
            Rect(x,y,8,8,PixelBbs::Grain(x,y,y>=H*3/5));
    Rect(0,0,W,6,MINT);
    Rect(0,H-42,W,42,DEEP);
    Rect(0,H-42,W,1,EDGE);

    const int left=(W-1120)/2>22?(W-1120)/2:22;
    const int contentW=W-2*left;
    const int cell=W>=1280 ? 9 : 6;
    for (unsigned letter=0;letter<4;++letter)
        for (unsigned row=0;row<7;++row)
            for (unsigned column=0;column<5;++column)
                if (PixelBbs::Logo[letter][row] & (16u>>column))
                    Rect(left+(letter*6+column)*cell,54+row*cell,
                         cell-1,cell-1,PixelBbs::Ink(row));
""" + source[end:]
source = source.replace("Text(left+111,131", "Text(left,131")
source = source.replace('(W-880)/2','(W-1120)/2')
source = source.replace('    Text(left,230,"Loading hardware drivers and desktop components",SECOND,1);\n','')
source = source.replace('    Rect(left,273,contentW,1,EDGE);','')
start = source.index("    Rect(left,256,contentW,10,EDGE);")
end = source.index("\n}", start)
source = source[:start] + """    const unsigned percent=done_ ? 100 : step_*100/7;
    const int barHeight=W>=1280 ? 46 : 36;
    Rect(left,239,contentW,barHeight,EDGE);
    Rect(left+3,242,contentW-6,barHeight-6,PANEL);
    Rect(left+3,242,(contentW-6)*int(percent)/100,barHeight-6,0x288F83);
    char label[5];
    unsigned n=0;
    if (percent>=100) label[n++]='1';
    if (percent>=10) label[n++]=char('0'+percent/10%10);
    label[n++]=char('0'+percent%10);
    label[n++]='%';
    label[n]=0;
    const int scale=W>=1280 ? 2 : 1;
    Text(left+(contentW-int(n)*8*scale)/2,239+(barHeight-16*scale)/2,label,WHITE,scale);""" + source[end:]
source = source.replace("    if(step>step_)step_=step;",
"""    // Only synchronous milestones count. Deferred work is logged separately.
    if (!done_ && (status==OK || (step==2 && status==WARNING)) &&
        ((step>=1 && step<=5) || step==7)) {
        const unsigned flag=1u<<step;
        if (!(completed_&flag)) {
            completed_|=flag;
            ++step_;
        }
    }""")
source = source.replace("count_=step_=0;", "count_=step_=completed_=0;").replace("    step_=12;\n", "")
splash.write_text(source)
splash_h = root / "boot_splash.h"
splash_h.write_text(splash_h.read_text().replace("unsigned count_,step_;",
                                               "unsigned count_,step_,completed_=0;"))

kernel = root / "kernel.cpp"
source = kernel.read_text()
source = source.replace('Boot(12,"DESKTOP","No auto-open windows; GUI ready");',
                        'Boot(12,"DESKTOP","Initializing desktop display",CBootSplash::PENDING);')
finish = '    boot_.Finish("BOOT READY - opening XD OS desktop");\n'
if source.count(finish) != 1:
    raise RuntimeError("Expected one boot splash finish call")
source = source.replace(finish + '    scheduler_.MsSleep(150);\n', '')
source = source.replace(finish, '')
marker = '#else\n    desktop_.Render(back_,pitch_,depth_);'
if source.count(marker) != 1:
    raise RuntimeError("Expected one desktop display initialization fallback")
source = source.replace(marker,
    finish + '    scheduler_.MsSleep(150);\n#else\n' + finish +
    '    scheduler_.MsSleep(150);\n    desktop_.Render(back_,pitch_,depth_);')
kernel.write_text(source)
