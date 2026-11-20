"""Prepare separately bootable, UART-measured Pi 3 GLES2 validation images."""

from pathlib import Path
import shutil
import sys


src, uart = map(Path,sys.argv[1:3])
mode = sys.argv[3]
if mode not in {"smoke","scroll"}:
    raise SystemExit("Expected smoke or scroll")
files = {}


def edit(name, old, new):
    path = src / name
    if path not in files:
        files[path] = path.read_text()
    if files[path].count(old) != 1:
        raise RuntimeError(f"{name}: expected exactly one {old!r}")
    files[path] = files[path].replace(old,new)


edit("kernel.h", "#include <circle/types.h>",
     '#include <circle/types.h>\n#include <SDCard/emmc.h>\n#include <fatfs/ff.h>')
edit("kernel.h", "\tCSerialDevice\t\tm_Serial;\n", "")
edit("kernel.h", "\tCInterruptSystem\tm_Interrupt;",
     "\tCInterruptSystem\tm_Interrupt;\n\tCSerialDevice\t\tm_Serial;")
edit("kernel.h", "\tCVCHIQDevice\t\tm_VCHIQ;",
     "\tCEMMCDevice m_SD;\n\tFATFS m_FS;\n\tCVCHIQDevice\t\tm_VCHIQ;")
edit("Makefile", "LIBS\t= ", "LIBS\t= $(CIRCLEHOME)/addon/SDCard/libsdcard.a \\\n"
     "\t  $(CIRCLEHOME)/addon/fatfs/libfatfs.a \\\n\t  ")
edit("kernel.cpp", "#include <circle/memory.h>",
     '#include <circle/memory.h>\n#include "uart_service.h"')
edit("kernel.cpp", "static const char FromKernel[] = \"kernel\";", """static const char FromKernel[] = "kernel";
static CSerialDevice *testSerial;

extern "C" unsigned vc4_clock_us(void) { return CTimer::GetClockTicks(); }
extern "C" void vc4_poll(void)
{
    // The original XDOS selection is already restored before GLES starts.
    if (testSerial) {
        static XdosUartService control;
        control.Poll(*testSerial,false);
    }
    CScheduler::Get()->Yield();
}""")
edit("kernel.cpp", "\tm_Timer (&m_Interrupt),",
     "\tm_Serial (&m_Interrupt),\n\tm_Timer (&m_Interrupt),")
edit("kernel.cpp", "m_Logger (m_Options.GetLogLevel (), &m_Timer)",
     "m_Logger (LogDebug, &m_Timer)")
edit("kernel.cpp", "\tm_VCHIQ (CMemorySystem::Get (), &m_Interrupt)",
     "\tm_SD (&m_Interrupt,&m_Timer,&m_ActLED),\n\tm_VCHIQ (CMemorySystem::Get (), &m_Interrupt)")
edit("kernel.cpp", "boolean bOK = TRUE;", "boolean bOK = m_Interrupt.Initialize ();")
edit("kernel.cpp", "m_Serial.Initialize (115200)", "m_Serial.Initialize (921600)")
start = """		CDevice *pTarget = m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		if (pTarget == 0)
		{
			pTarget = &m_Screen;
		}

		bOK = m_Logger.Initialize (pTarget);"""
edit("kernel.cpp",start,"\t\tbOK = m_Logger.Initialize (&m_Serial);\n\t\ttestSerial = &m_Serial;")
edit("kernel.cpp", """	if (bOK)
	{
		bOK = m_Interrupt.Initialize ();
	}

""","")
edit("kernel.cpp", """	if (bOK)
	{
		bOK = m_USBHCI.Initialize ();
	}""","""	if (!bOK) return FALSE;
	if (!m_SD.Initialize() || f_mount(&m_FS,"SD:",1)!=FR_OK ||
	    !XdosUartService::RestorePreviousBoot()) {
		m_Logger.Write(FromKernel,LogError,"VC4: could not restore XDOS boot configuration");
		while (1) vc4_poll();
	}
	m_Logger.Write(FromKernel,LogNotice,"[VC4] XDOS boot restored");
	bOK = m_USBHCI.Initialize ();""")
edit("kernel.cpp", "\t\tm_Scheduler.Sleep (4);","")
edit("kernel.cpp", "\t\tm_Scheduler.Yield ();","\t\tvc4_poll();")
edit("triangle2.c", "#define check()", """extern unsigned vc4_clock_us(void);
extern void vc4_poll(void);

#define check()""")
edit("triangle2.c", "   init_ogl(state);\n   init_shaders(state);",
     """   init_ogl(state);
   printk("[VC4] renderer=%s GLES=%s\n",glGetString(GL_RENDERER),glGetString(GL_VERSION));
""" + ("   texture_scroll();\n   return 0;" if mode=="scroll" else "   init_shaders(state);"))
edit("triangle2.c", "      draw_triangles(state, cx, cy, 0.003, x, y);",
     """      draw_triangles(state, cx, cy, 0.003, x, y);
      static unsigned frames;
      if (++frames==1) printk("[VC4] hello_triangle2 first frame swapped\n");
      vc4_poll();""")
if mode == "scroll":
    edit("triangle2.c", "   src_rect.width = state->screen_width << 16;",
         "   src_rect.width = 1024 << 16;")
    edit("triangle2.c", "   src_rect.height = state->screen_height << 16;",
         "   src_rect.height = 768 << 16;")
    edit("triangle2.c", "   nativewindow.width = state->screen_width;",
         "   nativewindow.width = 1024;")
    edit("triangle2.c", "   nativewindow.height = state->screen_height;",
         "   nativewindow.height = 768;")
    edit("triangle2.c", "int _main ()", '#include "texture_scroll.inc"\n\nint _main ()')
    shutil.copyfile(Path(__file__).with_name("texture_scroll.inc"),src/"texture_scroll.inc")

for path,text in files.items():
    if path.name == "triangle2.c":
        # C string literals in insertion templates need a literal escaped newline.
        text = text.replace('GLES=%s\n"', 'GLES=%s\\n"')
        text = text.replace('first frame swapped\n"', 'first frame swapped\\n"')
    path.write_text(text,encoding="utf-8")
shutil.copyfile(uart,src/"uart_service.h")
