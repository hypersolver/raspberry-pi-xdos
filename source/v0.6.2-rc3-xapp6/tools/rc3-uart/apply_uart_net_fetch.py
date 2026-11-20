"""Add LAN HTTP download to the physical UART control service."""

from pathlib import Path
import shutil
import sys


source = Path(sys.argv[1])
files = {name: (source / name).read_text() for name in ("kernel.cpp", "uart_service.h", "app_shortcut.cpp")}


def edit(name, old, new):
    if files[name].count(old) != 1:
        raise RuntimeError(f"{name}: expected one occurrence of {old!r}")
    files[name] = files[name].replace(old, new)


edit("uart_service.h", "#include <string.h>", '#include <string.h>\n#include "uart_net_fetch.h"')
edit("uart_service.h", "    bool TransferActive() const { return active_; }",
     """    bool TransferActive() const { return active_; }
    void SetNetwork(CNetSubSystem *net) { net_ = net; }""")
edit("uart_service.h", "        if (active_ && CTimer::GetClockTicks()",
     "        if (active_ && !fetchActive_ && CTimer::GetClockTicks()")
edit("uart_service.h", "            discard_ = true;\n            Abort();",
     "            discard_ = true;\n            if (!fetchActive_) Abort();")
edit("uart_service.h", "    Shortcut shortcut_ = Shortcut::None;", """    Shortcut shortcut_ = Shortcut::None;
    enum class FetchState { Idle, Working, Done, Failed };
    FetchState fetchState_ = FetchState::Idle;
    bool fetchActive_ = false;
    CNetSubSystem *net_ = nullptr;

    class FetchTask : public CTask {
    public:
        FetchTask(XdosUartService &service,const CIPAddress &host,unsigned port)
        : service_(service),host_(host),port_(port) {}
        void Run() override { service_.RunFetch(host_,port_); }
    private:
        XdosUartService &service_;
        CIPAddress host_;
        unsigned port_;
    };

    bool StartFetch(char *args)
    {
        if (active_ || !net_ || !net_->IsRunning()) return false;
        u8 bytes[4];
        unsigned port;
        if (!XdosNetFetch::ParseEndpoint(args,bytes,port) || !Begin(args))
            return false;
        fetchState_ = FetchState::Working;
        fetchActive_ = true;
        if (new FetchTask(*this,CIPAddress(bytes),port)) return true;
        Abort();
        fetchActive_ = false;
        fetchState_ = FetchState::Failed;
        return false;
    }

    void RunFetch(const CIPAddress &host,unsigned port)
    {
        const char *name = path_+sizeof("SD:/XDOS/INBOX/")-1;
        const bool received = XdosNetFetch::Download(net_,host,port,name,remaining_,
            [this](const u8 *bytes,unsigned count) { return Append(bytes,count); });
        const bool saved = received && Commit();
        if (!saved) Abort();
        fetchActive_ = false;
        fetchState_ = saved ? FetchState::Done : FetchState::Failed;
    }

    void ReportFetch(CSerialDevice &serial)
    {
        switch (fetchState_) {
        case FetchState::Idle: Reply(serial,"@XDOS FETCH IDLE @XDOS OK\\r\\n"); break;
        case FetchState::Working: Reply(serial,"@XDOS FETCH WORKING @XDOS OK\\r\\n"); break;
        case FetchState::Done: Reply(serial,"@XDOS FETCH DONE @XDOS OK\\r\\n"); break;
        case FetchState::Failed: Reply(serial,"@XDOS FETCH FAILED @XDOS OK\\r\\n"); break;
        }
    }""")
edit("uart_service.h", """        crc_ = UpdateCrc(crc_, data, length / 2);
        UINT written = 0;
        if (f_write(&file_, data, length / 2, &written) != FR_OK || written != length / 2)
            return false;
        remaining_ -= written;
        return true;
    }""", """        return Append(data,length/2);
    }

    bool Append(const unsigned char *data,unsigned length)
    {
        if (length>remaining_) return false;
        UINT written = 0;
        if (f_write(&file_,data,length,&written)!=FR_OK || written!=length) return false;
        crc_ = UpdateCrc(crc_,data,length);
        remaining_ -= written;
        return true;
    }""")
edit("uart_service.h", '        if (!strcmp(cmd, "RESET") && !active_) {', """        if (!strcmp(cmd,"FETCH-STATUS")) {
            ReportFetch(serial);
            return;
        }
        if (!strncmp(cmd,"FETCH ",6)) {
            Reply(serial,mounted && StartFetch(cmd+6) ? "@XDOS OK\\r\\n" : "@XDOS ERROR\\r\\n");
            return;
        }
        if (fetchActive_) {
            Reply(serial,"@XDOS ERROR\\r\\n");
            return;
        }
        if (!strcmp(cmd, "RESET") && !active_) {""")
edit("kernel.cpp", "XAPP6-PERF4 / Pi 3 ARM32", "XAPP6-PERF5 / Pi 3 ARM32")
edit("kernel.cpp", "        if (debugSerialReady_) uart.Poll(serial_, storageMounted_);",
     """        if (debugSerialReady_) {
            uart.SetNetwork(networkInit_ ? net_ : nullptr);
            uart.Poll(serial_,storageMounted_);
        }""")
edit("app_shortcut.cpp", "P4WIDGET.XAPP", "P5WIDGET.XAPP")
edit("app_shortcut.cpp", "P4BENCH.XAPP", "P5BENCH.XAPP")

shutil.copyfile(Path(__file__).with_name("uart_net_fetch.h"),source / "uart_net_fetch.h")
for name, text in files.items():
    (source / name).write_text(text,encoding="utf-8")
