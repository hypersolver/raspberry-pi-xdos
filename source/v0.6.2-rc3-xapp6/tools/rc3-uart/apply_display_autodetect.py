"""Use the firmware-selected HDMI size and the accepted framebuffer geometry."""

from pathlib import Path
import os
import sys


def edit(path, before, after):
    text = path.read_text(encoding="utf-8")
    if text.count(before) != 1:
        raise RuntimeError(f"{path.name}: expected one display anchor: {before!r}")
    path.write_text(text.replace(before, after), encoding="utf-8")


source, circle = map(Path, sys.argv[1:])
kernel = source / "kernel.cpp"
edit(kernel, "screen_(\n1024, 768\n  )", "screen_(0, 0)")
edit(kernel, """    if(!screen_.Initialize()) {
        Debug("[BOOT ERROR] HDMI SCREEN INIT FAILED\\r\\n");
        return FALSE;
    }
    CBcmFrameBuffer *fb=screen_.GetFrameBuffer();""", """    const auto usable = [](const CBcmFrameBuffer *fb) {
        return fb && fb->GetBuffer() && fb->GetDepth()==16 &&
               fb->GetWidth()>=640 && fb->GetWidth()<=4096 &&
               fb->GetHeight()>=480 && fb->GetHeight()<=2048 &&
               fb->GetPitch()>=fb->GetWidth()*2 && fb->GetPitch()<=16384 &&
               fb->GetSize()/fb->GetPitch()>=fb->GetHeight();
    };
    bool fallback = !screen_.Initialize() || !usable(screen_.GetFrameBuffer());
    if (fallback && (!screen_.Resize(1024,768) || !usable(screen_.GetFrameBuffer()))) {
        Debug("[BOOT ERROR] HDMI SCREEN INIT FAILED\\r\\n");
        return FALSE;
    }
    CBcmFrameBuffer *fb=screen_.GetFrameBuffer();
    CString resolution;
    resolution.Format("[DISPLAY] %ux%u RGB565 (%s)\\r\\n",fb->GetWidth(),fb->GetHeight(),
                      fallback ? "fallback request 1024x768" : "firmware auto");
    Debug(resolution);""")

# Circle normally keeps the requested size even after firmware reports a
# different allocated geometry. Consumers need the firmware response.
edit(circle / "lib/bcmframebuffer.cpp",
     "\tm_nBufferPtr  = m_InitTags.AllocateBuffer.nBufferBaseAddress & 0x3FFFFFFF;",
     """\tm_nWidth         = m_InitTags.SetPhysWidthHeight.nWidth;
\tm_nHeight        = m_InitTags.SetPhysWidthHeight.nHeight;
\tm_nVirtualWidth  = m_InitTags.SetVirtWidthHeight.nWidth;
\tm_nVirtualHeight = m_InitTags.SetVirtWidthHeight.nHeight;
\tif (m_nDepth != m_InitTags.SetDepth.nValue)
\t{
\t\treturn FALSE;
\t}
\tm_nBufferPtr  = m_InitTags.AllocateBuffer.nBufferBaseAddress & 0x3FFFFFFF;""")

if os.environ.get("XDOS_HDMI_MODES_TEST") == "1":
    uart = source / "uart_service.h"
    edit(uart, "    static bool SetKernel(const char *name)", r'''    static bool SelectHdmiTestMode(const char *mode)
    {
        const char *settings;
        if (!strcmp(mode,"720P")) {
            settings="\n[all]\nhdmi_force_hotplug=1\nhdmi_group=1\nhdmi_mode=4\n"
                     "disable_overscan=1\nframebuffer_width=1280\nframebuffer_height=720\n";
        } else if (!strcmp(mode,"1080P")) {
            settings="\n[all]\nhdmi_force_hotplug=1\nhdmi_group=1\nhdmi_mode=16\n"
                     "disable_overscan=1\nframebuffer_width=1920\nframebuffer_height=1080\n";
        } else {
            return false;
        }
        static char input[4096], output[4096];
        UINT size=0, used=0;
        if (!ReadConfig(Config,input,sizeof input,size)) return false;
        for (const char *line=input; *line;) {
            const char *next=strchr(line,'\n');
            next=next ? next+1 : input+size;
            const char *key=line;
            while (*key==' ' || *key=='\t') ++key;
            static const char *replaced[]={
                "hdmi_safe", "hdmi_group", "hdmi_mode", "hdmi_force_hotplug",
                "disable_overscan", "framebuffer_width", "framebuffer_height"
            };
            bool omit=false;
            for (auto name : replaced) {
                const auto n=strlen(name);
                if (!strncmp(key,name,n) && (key[n]=='=' || key[n]==':' || key[n]==' '))
                    omit=true;
            }
            if (!omit) {
                memcpy(output+used,line,next-line);
                used+=next-line;
            }
            line=next;
        }
        const auto length=strlen(settings);
        if (used+length>=sizeof output) return false;
        memcpy(output+used,settings,length);
        return ReplaceConfig(output,used+length);
    }

    static bool SetKernel(const char *name)''')
    edit(uart, '        if (!strcmp(cmd, "PING")) {', r'''        if (!strncmp(cmd,"HDMI-MODE ",10)) {
            const bool ok=mounted && !active_ && SelectHdmiTestMode(cmd+10);
            Reply(serial,ok ? "@XDOS HDMI-MODE @XDOS OK\r\n" : "@XDOS ERROR\r\n");
            return;
        }
        if (!strcmp(cmd, "PING")) {''')
