#pragma once

enum class XdosAppKind {
    Host, Arduino, Terminal, Notepad, Calculator, Paint, Files, Browser,
    Widgets, Benchmark, Player, Music
};

struct XdosDesktopApp {
    char name[41];
    char path[96];
    XdosAppKind kind;
};

constexpr unsigned XdosDesktopAppLimit = 10;
unsigned XdosReadDesktopApps(XdosDesktopApp (&apps)[XdosDesktopAppLimit]);
