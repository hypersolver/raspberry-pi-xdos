#pragma once

#include <circle/serial.h>
#include <circle/bcmwatchdog.h>
#include <circle/bcmpropertytags.h>
#include <circle/timer.h>
#include <circle/string.h>
#include <fatfs/ff.h>
#include <string.h>
#include "uart_net_fetch.h"
#include "xdos_file_delete.h"

// Physical debug UART only. Incoming files are confined to the SD card inbox.
struct XdosCommandReply {
    void *context;
    void (*send)(void *,const char *);
    void Write(const char *message) const { send(context,message); }
};
void XdosPrintBenchmarkReport(XdosCommandReply reply);
void ReportMusicAudio(XdosCommandReply reply);

class XdosUartService {
public:
    // Returns false only for unregistered legacy desktop commands.
    bool RunTerminalCommand(const char *line, XdosCommandReply reply)
    {
        const auto *entry = FindCommand(line);
        if (!entry) return false;
        if (entry->command == Command::File) RunFileCommand(line + 5, reply);
        else if (entry->terminal) RunQuery(entry->command,reply,true);
        else reply.Write("This command is available over the debug UART only.\r\n");
        return true;
    }

    static bool SelectStagedImage(const char *input)
    {
        char args[22];
        if (!input) return false;
        const auto len = strlen(input);
        if (len >= sizeof args) return false;
        memcpy(args,input,len+1);
        return SelectBoot(args);
    }

    static bool RestorePreviousBoot() { return RestoreConfig(); }
    const char *PlayPath() const { return playPath_; }
    bool TransferActive() const { return active_; }
    // XDOS_CORE_JOBS: V2 app test commands, executed by the kernel main loop.
    bool TakeXapp2Open(char *name, unsigned size)
    {
        if (!xapp2Name_[0]) return false;
        strncpy(name, xapp2Name_, size - 1);
        name[size - 1] = 0;
        xapp2Name_[0] = 0;
        return true;
    }
    int TakeXapp2Close() { const int s = xapp2Close_; xapp2Close_ = -1; return s; }
    bool TakeXapp2Status() { const bool s = xapp2Status_; xapp2Status_ = false; return s; }
    static void Send(CSerialDevice &serial, const char *text) { Reply(serial, text); }
    bool PowerTransferActive() const { return active_ || fetchState_==FetchState::Working; }
    void SetNetwork(CNetSubSystem *net) { net_ = net; }

    enum class Shortcut { None, Notepad, Calculator, Paint, Files, Browser, Widgets, Benchmark, Close, Terminal, Player, Play, Music };

    Shortcut TakeOpenRequest()
    {
        const auto requested = shortcut_;
        shortcut_ = Shortcut::None;
        return requested;
    }

    void CompleteOpen(CSerialDevice &serial, bool succeeded)
    {
        Reply(serial,succeeded ? "@XDOS OPENED @XDOS OK\r\n" : "@XDOS ERROR\r\n");
    }

    void CompleteClose(CSerialDevice &serial, bool succeeded)
    {
        Reply(serial,succeeded ? "@XDOS CLOSED @XDOS OK\r\n" : "@XDOS ERROR\r\n");
    }

    struct LvglStats {
        unsigned windowMs, averageUs, maximumUs, calls, labels, cpuPercent;
        unsigned dmaFlushes, cpuFlushes, wfiUs;
    };

    void SetLvglStats(const LvglStats &stats) { lvglStats_ = stats; }

    static void RestartSystem()
    {
        static CBcmWatchdog watchdog;
        watchdog.Start(1);
    }

    void Poll(CSerialDevice &serial, bool mounted)
    {
        if (resetting_) return;
        if (active_ && !fetchActive_ && CTimer::GetClockTicks() - lastActivity_ > 15000000U)
            Abort();
        char input[256];
        const int count = serial.Read(input, sizeof input);
        if (count < 0) {
            used_ = 0;
            discard_ = true;
            if (!fetchActive_) Abort();
            return;
        }
        for (int i = 0; i < count; ++i) {
            const char ch = input[i];
            if (ch == '\r') continue;
            if (ch == '\n') {
                if (!discard_) {
                    line_[used_] = 0;
                    Dispatch(serial, mounted);
                    if (resetting_) return;
                }
                used_ = 0;
                discard_ = false;
            } else if (used_ + 1 < sizeof line_) {
                line_[used_++] = ch;
            } else {
                discard_ = true;
            }
        }
    }

private:
    static constexpr const char *Temp = "SD:/XDOS/INBOX/.upload.tmp";
    static constexpr const char *Config = "SD:/config.txt";
    static constexpr const char *ConfigBackup = "SD:/xdosboot.bak";
    static constexpr const char *ConfigTemp = "SD:/xdosboot.tmp";
    FIL file_{};
    bool active_ = false;
    bool discard_ = false;
    bool resetting_ = false;
    unsigned used_ = 0, remaining_ = 0, expected_ = 0, crc_ = 0, lastActivity_ = 0;
    char line_[280]{};
    char path_[72]{};
    LvglStats lvglStats_{};
    Shortcut shortcut_ = Shortcut::None;
    char xapp2Name_[40] = {};
    int xapp2Close_ = -1;
    bool xapp2Status_ = false;
    char playPath_[192]{};
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
        case FetchState::Idle: Reply(serial,"@XDOS FETCH IDLE @XDOS OK\r\n"); break;
        case FetchState::Working: Reply(serial,"@XDOS FETCH WORKING @XDOS OK\r\n"); break;
        case FetchState::Done: Reply(serial,"@XDOS FETCH DONE @XDOS OK\r\n"); break;
        case FetchState::Failed: Reply(serial,"@XDOS FETCH FAILED @XDOS OK\r\n"); break;
        }
    }

    static Shortcut LookupShortcut(const char *name)
    {
        static const struct { const char *name; Shortcut shortcut; } shortcuts[] = {
            {"NOTEPAD",Shortcut::Notepad}, {"CALCULATOR",Shortcut::Calculator},
            {"PAINT",Shortcut::Paint}, {"FILES",Shortcut::Files},
            {"BROWSER",Shortcut::Browser},
            {"PLAYER",Shortcut::Player}, {"MUSIC",Shortcut::Music}, {"TERMINAL",Shortcut::Terminal}, {"WIDGETS",Shortcut::Widgets}, {"BENCHMARK",Shortcut::Benchmark}
        };
        for (const auto &entry : shortcuts)
            if (!strcmp(name,entry.name)) return entry.shortcut;
        return Shortcut::None;
    }

    static void Reply(CSerialDevice &serial, const char *text)
    {
        serial.Write(text, strlen(text));
    }

    static void ReportPower(XdosCommandReply reply)
    {
        CBcmPropertyTags tags;
        TPropertyTagSimple power{};
        if (!tags.GetTag(PROPTAG_GET_THROTTLED, &power, sizeof power, sizeof power.nValue)) {
            reply.Write("@XDOS ERROR\r\n");
            return;
        }
        char status[] = "@XDOS POWER 00000000 @XDOS OK\r\n";
        constexpr char hex[] = "0123456789abcdef";
        for (unsigned i = 0; i < 8; ++i)
            status[12 + i] = hex[(power.nValue >> (28 - i*4)) & 15];
        reply.Write(status);
    }

    static int Digit(char ch)
    {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        return -1;
    }

    static bool Hex32(const char *text, unsigned &value)
    {
        value = 0;
        for (unsigned i = 0; i < 8; ++i) {
            int digit = Digit(text[i]);
            if (digit < 0) return false;
            value = (value << 4) | unsigned(digit);
        }
        return true;
    }

    void Abort()
    {
        if (!active_) return;
        f_close(&file_);
        f_unlink(Temp);
        active_ = false;
    }

    bool Begin(char *args)
    {
        char *space = strchr(args, ' ');
        if (!space || space == args || space - args > 32 || *args == '.') return false;
        for (char *p = args; p != space; ++p) {
            const char ch = *p;
            if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                  (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' || ch == '-'))
                return false;
        }
        const char *numbers = space + 1;
        if (strlen(numbers) != 17 || numbers[8] != ' ' ||
            !Hex32(numbers, remaining_) || !Hex32(numbers + 9, expected_) ||
            !remaining_ || remaining_ > 16U * 1024 * 1024) return false;
        *space = 0;
        strcpy(path_, "SD:/XDOS/INBOX/");
        strcat(path_, args);
        FRESULT result = f_mkdir("SD:/XDOS");
        if (result != FR_OK && result != FR_EXIST) return false;
        result = f_mkdir("SD:/XDOS/INBOX");
        if (result != FR_OK && result != FR_EXIST) return false;
        FILINFO info;
        if (f_stat(path_, &info) != FR_NO_FILE) return false;
        if (f_open(&file_, Temp, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return false;
        crc_ = ~0U;
        active_ = true;
        return true;
    }

    bool Data(const char *hex)
    {
        const unsigned length = strlen(hex);
        if (!length || (length & 1) || length > 256 || length / 2 > remaining_) return false;
        unsigned char data[128];
        for (unsigned i = 0; i < length / 2; ++i) {
            const int high = Digit(hex[2*i]), low = Digit(hex[2*i+1]);
            if (high < 0 || low < 0) return false;
            data[i] = (high << 4) | low;
        }
        return Append(data,length/2);
    }

    bool Append(const unsigned char *data,unsigned length)
    {
        if (length>remaining_) return false;
        UINT written = 0;
        if (f_write(&file_,data,length,&written)!=FR_OK || written!=length) return false;
        crc_ = UpdateCrc(crc_,data,length);
        remaining_ -= written;
        return true;
    }

    bool Commit()
    {
        if (remaining_ || ~crc_ != expected_) return false;
        const FRESULT result = f_close(&file_);
        active_ = false;
        if (result == FR_OK && f_rename(Temp, path_) == FR_OK) return true;
        f_unlink(Temp);
        return false;
    }

    static unsigned UpdateCrc(unsigned crc, const unsigned char *data, unsigned length)
    {
        for (unsigned i = 0; i < length; ++i) {
            crc ^= data[i];
            for (unsigned bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320U : 0U);
        }
        return crc;
    }

    static bool ReadConfig(const char *path, char *data, UINT capacity, UINT &size)
    {
        FIL config;
        if (f_open(&config, path, FA_READ) != FR_OK) return false;
        const auto length = f_size(&config);
        const bool read = length && length < capacity &&
                          f_read(&config, data, length, &size) == FR_OK && size == length;
        if (f_close(&config) != FR_OK || !read) return false;
        data[size] = 0;
        return strlen(data) == size;
    }

    static bool ReplaceConfig(const char *data, UINT size)
    {
        FIL file;
        if (f_open(&file, ConfigTemp, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return false;
        UINT written = 0;
        const bool saved = f_write(&file, data, size, &written) == FR_OK &&
                           written == size && f_sync(&file) == FR_OK;
        if (f_close(&file) != FR_OK || !saved) return false;
        const auto removed = f_unlink(ConfigBackup);
        if (removed != FR_OK && removed != FR_NO_FILE) return false;
        if (f_rename(Config, ConfigBackup) != FR_OK) return false;
        if (f_rename(ConfigTemp, Config) == FR_OK) return true;
        f_rename(ConfigBackup, Config);
        return false;
    }

    static bool SelectHdmiTestMode(const char *mode)
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

    static bool SetKernel(const char *name)
    {
        static char input[4096], output[4120];
        UINT size = 0, used = 0;
        if (!ReadConfig(Config, input, sizeof input, size)) return false;
        bool all = true, replaced = false;
        for (const char *line = input; *line;) {
            const char *next = strchr(line, '\n');
            next = next ? next + 1 : input + size;
            const char *end = next;
            while (end > line && strchr("\r\n \t", end[-1])) --end;
            const char *key = line;
            while (key < end && (*key == ' ' || *key == '\t')) ++key;
            const auto length = end - key;
            if (length && *key == '[') all = length == 5 && !memcmp(key, "[all]", 5);
            constexpr char legacy[] = "include xdboot.txt";
            if (length == sizeof legacy - 1 && !memcmp(key, legacy, sizeof legacy - 1)) {
                line = next;
                continue;
            }
            if (length >= 7 && !memcmp(key, "kernel=", 7)) {
                if (!all || replaced) return false;
                const auto prefix = key - line + 7;
                memcpy(output + used, line, prefix);
                used += prefix;
                const auto nameLength = strlen(name);
                memcpy(output + used, name, nameLength);
                used += nameLength;
                memcpy(output + used, end, next - end);
                used += next - end;
                replaced = true;
            } else {
                memcpy(output + used, line, next - line);
                used += next - line;
            }
            line = next;
        }
        return replaced && ReplaceConfig(output, used);
    }

    static bool RestoreConfig()
    {
        static char backup[4096];
        UINT size = 0;
        return ReadConfig(ConfigBackup, backup, sizeof backup, size) &&
               ReplaceConfig(backup, size);
    }

    static bool CheckImage(const char *path, unsigned expected)
    {
        FIL image;
        if (f_open(&image, path, FA_READ) != FR_OK) return false;
        bool ok = f_size(&image) > 0 && f_size(&image) <= 16U * 1024 * 1024;
        unsigned crc = ~0U;
        unsigned char chunk[1024];
        while (ok) {
            UINT count = 0;
            ok = f_read(&image, chunk, sizeof chunk, &count) == FR_OK;
            if (!ok || !count) break;
            crc = UpdateCrc(crc, chunk, count);
        }
        return f_close(&image) == FR_OK && ok && ~crc == expected;
    }

    static bool SelectBoot(char *args)
    {
        char *space = strchr(args, ' ');
        if (!space || space - args < 5 || space - args > 12 ||
            strlen(space + 1) != 8) return false;
        unsigned expected;
        if (!Hex32(space + 1, expected)) return false;
        *space = 0;
        if (strcmp(space - 4, ".IMG")) return false;
        for (const char *p = args; p != space - 4; ++p) {
            if (!((*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9'))) return false;
        }
        char inbox[40] = "SD:/XDOS/INBOX/";
        char root[20] = "SD:/";
        strcat(inbox, args);
        strcat(root, args);
        FILINFO info;
        const FRESULT existing = f_stat(root, &info);
        if (existing == FR_NO_FILE) {
            if (!CheckImage(inbox, expected) || f_rename(inbox, root) != FR_OK) return false;
        } else if (existing != FR_OK || !CheckImage(root, expected)) {
            return false;
        }
        return SetKernel(args);
    }

    enum class Command { Unknown, Help, Ping, Power, Lvgl, BenchResult, Audio, Open, Play, Close, Fetch, FetchStatus, Reset, Cancel, Boot, BootClear, Put, Data, End, HdmiMode, Xapp2, Xapp2Close, Xapp2Status, File };
    struct CommandEntry { Command command; const char *name; bool arguments; bool terminal; };
    static constexpr CommandEntry commandTable_[] = {
        {Command::Help, "HELP", false, true},
        {Command::Ping, "PING", false, true},
        {Command::Power, "POWER", false, true},
        {Command::Lvgl, "LVGL", false, true},
        {Command::BenchResult, "BENCH-RESULT", false, true},
        {Command::Audio, "AUDIO", false, true},
        {Command::Open, "OPEN", true, false},
        {Command::Play, "PLAY", true, false},
        {Command::Close, "CLOSE", false, false},
        {Command::Fetch, "FETCH", true, false},
        {Command::File, "FILE", true, true},
        {Command::FetchStatus, "FETCH-STATUS", false, false},
        {Command::Reset, "RESET", false, false},
        {Command::Cancel, "CANCEL", false, false},
        {Command::Boot, "BOOT", true, false},
        {Command::BootClear, "BOOT-CLEAR", false, false},
        {Command::Put, "PUT", true, false},
        {Command::Data, "DATA", true, false},
        {Command::End, "END", false, false},
        {Command::HdmiMode, "HDMI-MODE", true, false},
        {Command::Xapp2Status, "XAPP2-STATUS", false, false},
        {Command::Xapp2Close, "XAPP2-CLOSE", true, false},
        {Command::Xapp2, "XAPP2", true, false},
    };

    static const CommandEntry *FindCommand(const char *line)
    {
        for (const auto &entry : commandTable_) {
            const auto length = strlen(entry.name);
            if (!strncmp(line,entry.name,length) &&
                line[length] == (entry.arguments ? ' ' : '\0')) return &entry;
        }
        return nullptr;
    }

    void PrintHelp(XdosCommandReply reply, bool terminal)
    {
        for (const auto &entry : commandTable_) {
            if (terminal && !entry.terminal) continue;
            reply.Write(entry.name);
            reply.Write(entry.arguments ? " <args>\r\n" : "\r\n");
        }
        reply.Write("@XDOS OK\r\n");
    }

    void RunFileCommand(const char *args, XdosCommandReply reply)
    {
        if (strncmp(args, "DELETE ", 7)) {
            reply.Write("Usage: FILE DELETE <SD:/XDOS/path or USB:/path>\r\n@XDOS ERROR\r\n");
            return;
        }
        if (PowerTransferActive()) {
            reply.Write("FILE DELETE: transfer busy\r\n@XDOS ERROR\r\n");
            return;
        }
        const FRESULT result = XdosDeleteFile(args + 7);
        if (result == FR_OK) reply.Write("FILE DELETE: deleted\r\n@XDOS OK\r\n");
        else {
            CString message;
            message.Format("FILE DELETE: FatFs error %u\r\n@XDOS ERROR\r\n", unsigned(result));
            reply.Write(static_cast<const char *>(message));
        }
    }

    bool RunQuery(Command command, XdosCommandReply reply, bool terminal)
    {
        if (command==Command::Audio) { ReportMusicAudio(reply); return true; }
        if (command==Command::Help) {
            PrintHelp(reply,terminal);
            return true;
        }
        if (command==Command::Ping) {
            reply.Write("@XDOS READY 1\r\n");
            return true;
        }
        if (command==Command::Power) {
            ReportPower(reply);
            return true;
        }
        if (command==Command::BenchResult) {
            XdosPrintBenchmarkReport(reply);
            return true;
        }
        if (command==Command::Lvgl) {
            if (!lvglStats_.windowMs) {
                reply.Write("@XDOS LVGL unavailable @XDOS OK\r\n");
                return true;
            }
            CString status;
            status.Format("@XDOS LVGL window_ms=%u avg_us=%u max_us=%u calls=%u labels=%u "
                          "cpu_pct=%u dma_flushes=%u cpu_flushes=%u wfi_us=%u @XDOS OK\r\n",
                          lvglStats_.windowMs,lvglStats_.averageUs,lvglStats_.maximumUs,
                          lvglStats_.calls,lvglStats_.labels,lvglStats_.cpuPercent,
                          lvglStats_.dmaFlushes,lvglStats_.cpuFlushes,lvglStats_.wfiUs);
            reply.Write((const char *)status);
            return true;
        }
        return false;
    }

    void Dispatch(CSerialDevice &serial, bool mounted)
    {
        if (strncmp(line_, "@XDOS ", 6)) return;
        char *cmd = line_ + 6;
        const auto *entry = FindCommand(cmd);
        const auto command = entry ? entry->command : Command::Unknown;
        const XdosCommandReply reply{&serial, [](void *port,const char *message) {
            Reply(*static_cast<CSerialDevice *>(port),message);
        }};
        if (RunQuery(command,reply,false)) return;
        if (command == Command::File) {
            if (mounted) RunFileCommand(cmd + 5, reply);
            else reply.Write("FILE DELETE: SD unavailable\r\n@XDOS ERROR\r\n");
            return;
        }
        if (command==Command::HdmiMode) {
            const bool ok=mounted && !active_ && SelectHdmiTestMode(cmd+10);
            Reply(serial,ok ? "@XDOS HDMI-MODE @XDOS OK\r\n" : "@XDOS ERROR\r\n");
            return;
        }
        if (command==Command::Xapp2 || command==Command::Xapp2Close ||
            command==Command::Xapp2Status) {
            const char *arg = command==Command::Xapp2 ? cmd + 6 :
                              command==Command::Xapp2Close ? cmd + 12 : "";
            bool queued = !active_;
            if (queued && command==Command::Xapp2) {
                queued = mounted && arg[0] && strlen(arg) < sizeof xapp2Name_ &&
                         !strchr(arg, '/') && !strchr(arg, ':') && !xapp2Name_[0];
                if (queued) strcpy(xapp2Name_, arg);
            } else if (queued && command==Command::Xapp2Close) {
                queued = arg[0] >= '0' && arg[0] <= '3' && !arg[1];
                if (queued) xapp2Close_ = arg[0] - '0';
            } else if (queued) {
                xapp2Status_ = true;
            }
            if (!queued) Reply(serial, "@XDOS ERROR\r\n");
            return;
        }
        if (command==Command::Close) {
            if (!active_ && shortcut_ == Shortcut::None) shortcut_ = Shortcut::Close;
            else CompleteClose(serial,false);
            return;
        }
        if (command==Command::Play) {
            const char *path=cmd+5;
            if (mounted && !active_ && shortcut_==Shortcut::None && *path &&
                strlen(path)<sizeof playPath_) {
                strcpy(playPath_,path);
                shortcut_=Shortcut::Play;
            } else Reply(serial,"@XDOS ERROR\r\n");
            return;
        }
        if (command==Command::Open) {
            const auto requested = LookupShortcut(cmd+5);
            if (mounted && !active_ && shortcut_ == Shortcut::None && requested != Shortcut::None)
                shortcut_ = requested;
            else
                CompleteOpen(serial,false);
            return;
        }
        if (command==Command::FetchStatus) {
            ReportFetch(serial);
            return;
        }
        if (command==Command::Fetch) {
            Reply(serial,mounted && StartFetch(cmd+6) ? "@XDOS OK\r\n" : "@XDOS ERROR\r\n");
            return;
        }
        if (fetchActive_) {
            Reply(serial,"@XDOS ERROR\r\n");
            return;
        }
        if (command==Command::Reset && !active_) {
            Reply(serial, "@XDOS RESETTING\r\n");
            resetting_ = true;
            RestartSystem();
            return;
        }
        lastActivity_ = CTimer::GetClockTicks();
        bool ok = false;
        if (mounted) {
            if (command==Command::Cancel) {
                Abort();
                ok = true;
            } else if (!active_ && command==Command::Boot) {
                ok = SelectBoot(cmd + 5);
            } else if (!active_ && command==Command::BootClear) {
                ok = RestoreConfig();
            } else if (!active_ && command==Command::Put) {
                ok = Begin(cmd + 4);
            } else if (active_ && command==Command::Data) {
                ok = Data(cmd + 5);
            } else if (active_ && command==Command::End) {
                ok = Commit();
            }
        }
        if (!ok) Abort();
        Reply(serial, ok ? "@XDOS OK\r\n" : "@XDOS ERROR\r\n");
    }
};
