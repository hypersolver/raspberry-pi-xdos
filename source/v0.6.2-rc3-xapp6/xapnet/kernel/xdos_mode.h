// XDOS work mode (R19): DESKTOP / KIOSK / IOT, chosen at boot from SD:/XDOS/XDOS.CFG
// (or SD:/XDOS/INBOX/XDOS.CFG, so it can be uploaded over UART or the network).
//
//   MODE=AUTO                  ; AUTO | DESKTOP | KIOSK | IOT
//   KIOSK=PANEL.XAPP           ; full-screen app for KIOSK (file in SD:/XDOS/INBOX or a full SD: path)
//   KIOSK_EXIT=RESTART         ; RESTART | DESKTOP | REBOOT when the kiosk app ends
//   AUTORUN=MQTT.XAPP,IO.XAPP  ; background apps (Screen=None), started in every mode
//
// Lines are KEY=VALUE; ';' or '#' start a comment; keys and keywords ignore case.
// AUTO: KIOSK when KIOSK= is set, otherwise DESKTOP (display detection comes later:
// no display -> IOT).
#pragma once
#include <fatfs/ff.h>
#include <string.h>
#include <strings.h>

enum class XdosMode { Desktop, Kiosk, Iot };
enum class XdosKioskExit { Restart, Desktop, Reboot };

struct XdosModeConfig {
    static constexpr unsigned MaxAutorun = 4;
    static constexpr unsigned NameBytes = 48;
    bool found = false;                 // XDOS.CFG present
    bool autoMode = true;               // MODE=AUTO (or missing)
    XdosMode mode = XdosMode::Desktop;  // resolved mode
    XdosKioskExit kioskExit = XdosKioskExit::Restart;
    char kiosk[NameBytes] = {};
    char autorun[MaxAutorun][NameBytes] = {};
    unsigned autorunCount = 0;
    unsigned badLines = 0;
};

inline const char *XdosModeName(XdosMode mode)
{
    return mode == XdosMode::Kiosk ? "KIOSK" : mode == XdosMode::Iot ? "IOT" : "DESKTOP";
}

inline bool XdosParseMode(const char *text, XdosMode &mode)
{
    if (!strcasecmp(text, "DESKTOP")) mode = XdosMode::Desktop;
    else if (!strcasecmp(text, "KIOSK")) mode = XdosMode::Kiosk;
    else if (!strcasecmp(text, "IOT")) mode = XdosMode::Iot;
    else return false;
    return true;
}

// "NAME.XAPP" -> "SD:/XDOS/INBOX/NAME.XAPP"; full "SD:/..." paths are kept.
inline void XdosAppPath(const char *name, char *path, unsigned size)
{
    if (!strncasecmp(name, "SD:", 3)) { strncpy(path, name, size - 1); path[size - 1] = 0; return; }
    strncpy(path, "SD:/XDOS/INBOX/", size - 1);
    path[size - 1] = 0;
    strncat(path, name, size - 1 - strlen(path));
}

inline char *XdosTrim(char *s)
{
    while (*s == ' ' || *s == '\t') ++s;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) --end;
    *end = 0;
    return s;
}

inline void XdosParseModeLine(XdosModeConfig &cfg, char *line)
{
    for (char *p = line; *p; ++p) if (*p == ';' || *p == '#') { *p = 0; break; }
    line = XdosTrim(line);
    if (!*line) return;
    char *eq = strchr(line, '=');
    if (!eq) { ++cfg.badLines; return; }
    *eq = 0;
    char *key = XdosTrim(line), *value = XdosTrim(eq + 1);
    if (!strcasecmp(key, "MODE")) {
        if (!strcasecmp(value, "AUTO")) cfg.autoMode = true;
        else if (XdosParseMode(value, cfg.mode)) cfg.autoMode = false;
        else ++cfg.badLines;
    } else if (!strcasecmp(key, "KIOSK")) {
        if (strlen(value) < sizeof cfg.kiosk) strcpy(cfg.kiosk, value);
        else ++cfg.badLines;
    } else if (!strcasecmp(key, "KIOSK_EXIT")) {
        if (!strcasecmp(value, "RESTART")) cfg.kioskExit = XdosKioskExit::Restart;
        else if (!strcasecmp(value, "DESKTOP")) cfg.kioskExit = XdosKioskExit::Desktop;
        else if (!strcasecmp(value, "REBOOT")) cfg.kioskExit = XdosKioskExit::Reboot;
        else ++cfg.badLines;
    } else if (!strcasecmp(key, "AUTORUN")) {
        for (char *item = value; item && *item;) {
            char *comma = strchr(item, ',');
            if (comma) *comma = 0;
            char *name = XdosTrim(item);
            if (*name && cfg.autorunCount < XdosModeConfig::MaxAutorun &&
                strlen(name) < XdosModeConfig::NameBytes)
                strcpy(cfg.autorun[cfg.autorunCount++], name);
            else if (*name) ++cfg.badLines;
            item = comma ? comma + 1 : nullptr;
        }
    } else {
        ++cfg.badLines;
    }
}

// Reads SD:/XDOS/XDOS.CFG, else SD:/XDOS/INBOX/XDOS.CFG (where UART/network uploads land).
// SD must be mounted. Missing file = AUTO with no kiosk app.
inline XdosModeConfig XdosReadModeConfig(const char *path = "SD:/XDOS/XDOS.CFG",
                                         const char *fallback = "SD:/XDOS/INBOX/XDOS.CFG")
{
    XdosModeConfig cfg;
    FIL file;
    if (f_open(&file, path, FA_READ) == FR_OK || (fallback && f_open(&file, fallback, FA_READ) == FR_OK)) {
        cfg.found = true;
        char text[1024];
        UINT got = 0;
        f_read(&file, text, sizeof text - 1, &got);
        f_close(&file);
        text[got] = 0;
        for (char *line = text; line && *line;) {
            char *next = strchr(line, '\n');
            if (next) *next++ = 0;
            XdosParseModeLine(cfg, line);
            line = next;
        }
    }
    if (cfg.autoMode) cfg.mode = cfg.kiosk[0] ? XdosMode::Kiosk : XdosMode::Desktop;
    if (cfg.mode == XdosMode::Kiosk && !cfg.kiosk[0]) cfg.mode = XdosMode::Desktop;  // nothing to run
    return cfg;
}
