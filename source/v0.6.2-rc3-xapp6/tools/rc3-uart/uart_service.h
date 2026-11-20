#pragma once

#include <circle/serial.h>
#include <circle/bcmwatchdog.h>
#include <circle/bcmpropertytags.h>
#include <circle/timer.h>
#include <fatfs/ff.h>
#include <string.h>

// Physical debug UART only. Incoming files are confined to the SD card inbox.
class XdosUartService {
public:
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
    bool TransferActive() const { return active_; }

    static void RestartSystem()
    {
        static CBcmWatchdog watchdog;
        watchdog.Start(1);
    }

    void Poll(CSerialDevice &serial, bool mounted)
    {
        if (resetting_) return;
        if (active_ && CTimer::GetClockTicks() - lastActivity_ > 15000000U)
            Abort();
        char input[256];
        const int count = serial.Read(input, sizeof input);
        if (count < 0) {
            used_ = 0;
            discard_ = true;
            Abort();
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

    static void Reply(CSerialDevice &serial, const char *text)
    {
        serial.Write(text, strlen(text));
    }

    static void ReportPower(CSerialDevice &serial)
    {
        CBcmPropertyTags tags;
        TPropertyTagSimple power{};
        if (!tags.GetTag(PROPTAG_GET_THROTTLED, &power, sizeof power, sizeof power.nValue)) {
            Reply(serial, "@XDOS ERROR\r\n");
            return;
        }
        char status[] = "@XDOS POWER 00000000 @XDOS OK\r\n";
        constexpr char hex[] = "0123456789abcdef";
        for (unsigned i = 0; i < 8; ++i)
            status[12 + i] = hex[(power.nValue >> (28 - i*4)) & 15];
        Reply(serial, status);
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
        crc_ = UpdateCrc(crc_, data, length / 2);
        UINT written = 0;
        if (f_write(&file_, data, length / 2, &written) != FR_OK || written != length / 2)
            return false;
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

    void Dispatch(CSerialDevice &serial, bool mounted)
    {
        if (strncmp(line_, "@XDOS ", 6)) return;
        char *cmd = line_ + 6;
        if (!strcmp(cmd, "PING")) {
            Reply(serial, "@XDOS READY 1\r\n");
            return;
        }
        if (!strcmp(cmd, "POWER")) {
            ReportPower(serial);
            return;
        }
        if (!strcmp(cmd, "RESET") && !active_) {
            Reply(serial, "@XDOS RESETTING\r\n");
            resetting_ = true;
            RestartSystem();
            return;
        }
        lastActivity_ = CTimer::GetClockTicks();
        bool ok = false;
        if (mounted) {
            if (!strcmp(cmd, "CANCEL")) {
                Abort();
                ok = true;
            } else if (!active_ && !strncmp(cmd, "BOOT ", 5)) {
                ok = SelectBoot(cmd + 5);
            } else if (!active_ && !strcmp(cmd, "BOOT-CLEAR")) {
                ok = RestoreConfig();
            } else if (!active_ && !strncmp(cmd, "PUT ", 4)) {
                ok = Begin(cmd + 4);
            } else if (active_ && !strncmp(cmd, "DATA ", 5)) {
                ok = Data(cmd + 5);
            } else if (active_ && !strcmp(cmd, "END")) {
                ok = Commit();
            }
        }
        if (!ok) Abort();
        Reply(serial, ok ? "@XDOS OK\r\n" : "@XDOS ERROR\r\n");
    }
};
