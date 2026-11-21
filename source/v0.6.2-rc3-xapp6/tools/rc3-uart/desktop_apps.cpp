#include "desktop_apps.h"
#include <fatfs/ff.h>
#include <string.h>
#include <strings.h>

namespace {
constexpr char directory[] = "SD:/XDOS/APPS/";
constexpr char inbox[] = "SD:/XDOS/INBOX/";

bool validModule(const char *path)
{
    const char *file;
    if (!strncasecmp(path, directory, sizeof directory - 1))
        file = path + sizeof directory - 1;
    else if (!strncasecmp(path, inbox, sizeof inbox - 1))
        file = path + sizeof inbox - 1;
    else
        return false;
    const size_t length = strlen(file);
    if (length <= 5 || strcasecmp(file + length - 5, ".XAPP")) return false;
    for (const char *p = file; p < file + length - 5; ++p) {
        const unsigned char ch = *p;
        if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')) return false;
    }
    FILINFO info{};
    return f_stat(path, &info) == FR_OK && !(info.fattrib & AM_DIR);
}

XdosAppKind legacyKind(const char *manifest, const char *path)
{
    struct Legacy { const char *manifest; const char *module; XdosAppKind kind; };
    static const Legacy legacy[] = {
        {"TERMINAL.APP", "SD:/XDOS/APPS/TERMINAL.XAPP", XdosAppKind::Terminal},
        {"NOTE.APP", "SD:/XDOS/APPS/NOTE.XAPP", XdosAppKind::Notepad},
        {"CALC.APP", "SD:/XDOS/APPS/CALC.XAPP", XdosAppKind::Calculator},
        {"PAINT.APP", "SD:/XDOS/APPS/PAINT.XAPP", XdosAppKind::Paint},
        {"FILES.APP", "SD:/XDOS/APPS/FILES.XAPP", XdosAppKind::Files},
        {"BROWSER.APP", "SD:/XDOS/APPS/BROWSER.XAPP", XdosAppKind::Browser},
        {"LVGLDEMO.APP", "SD:/XDOS/APPS/LVGLDEMO.XAPP", XdosAppKind::Widgets},
        {"LVGLBENCH.APP", "SD:/XDOS/APPS/LVGLBENCH.XAPP", XdosAppKind::Benchmark},
        {"PLAYER.APP", "SD:/XDOS/INBOX/PLAYER.XAPP", XdosAppKind::Player},
        {"MUSIC.APP", "SD:/XDOS/INBOX/MUSIC.XAPP", XdosAppKind::Music},
    };
    for (const auto &entry : legacy)
        if (!strcasecmp(manifest, entry.manifest) && !strcasecmp(path, entry.module))
            return entry.kind;
    return XdosAppKind::Host;
}

bool readManifest(const char *manifest, XdosDesktopApp &app)
{
    char path[sizeof directory + 255];
    if (strlen(manifest) >= sizeof path - sizeof directory) return false;
    strcpy(path, directory);
    strcat(path, manifest);
    FIL file;
    if (f_open(&file, path, FA_READ) != FR_OK) return false;
    char text[1025];
    const auto size = f_size(&file);
    UINT got = 0;
    const bool read = size && size < sizeof text &&
        f_read(&file, text, size, &got) == FR_OK && got == size;
    if (f_close(&file) != FR_OK || !read) return false;
    for (UINT i = 0; i < got; ++i)
        if (!text[i]) return false;
    text[got] = 0;
    bool application = false, abi = false, arch = false, arduino = false;
    app = {};
    for (char *line = text; *line;) {
        char *next = strchr(line, '\n');
        if (next) *next++ = 0;
        size_t length = strlen(line);
        if (length && line[length - 1] == '\r') line[--length] = 0;
        if (line[0] == '[') {
            application = !strcmp(line, "[Application]");
        } else if (application) {
            if (!strncmp(line, "Name=", 5)) {
                const char *name = line + 5;
                size_t i = 0;
                for (; name[i] && i + 1 < sizeof app.name; ++i) {
                    const unsigned char ch = name[i];
                    app.name[i] = ch >= 32 && ch <= 126 ? ch : '?';
                }
                app.name[i] = 0;
            } else if (!strncmp(line, "Exec=", 5)) {
                app.path[0] = 0;
                if (length - 5 < sizeof app.path) strcpy(app.path, line + 5);
            } else if (!strncmp(line, "ABI=", 4)) {
                abi = !strcmp(line + 4, "1");
            } else if (!strncmp(line, "Arch=", 5)) {
                arch = !strcmp(line + 5, "armv7-a32");
            } else if (!strncmp(line, "Type=", 5)) {
                arduino = !strcmp(line + 5, "Arduino");
            }
        }
        if (!next) break;
        line = next;
    }
    if (!abi || !arch || !app.name[0] || !validModule(app.path)) return false;
    app.kind = arduino ? XdosAppKind::Arduino : legacyKind(manifest, app.path);
    return true;
}
}

unsigned XdosReadDesktopApps(XdosDesktopApp (&apps)[XdosDesktopAppLimit])
{
    DIR dir;
    if (f_opendir(&dir, "SD:/XDOS/APPS") != FR_OK) return 0;
    unsigned count = 0;
    FILINFO entry{};
    while (count < XdosDesktopAppLimit && f_readdir(&dir, &entry) == FR_OK && entry.fname[0]) {
        const auto length = strlen(entry.fname);
        if (!(entry.fattrib & AM_DIR) && length > 4 &&
            !strcasecmp(entry.fname + length - 4, ".APP") &&
            readManifest(entry.fname, apps[count])) ++count;
    }
    f_closedir(&dir);
    return count;
}
