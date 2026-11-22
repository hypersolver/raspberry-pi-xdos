#pragma once

enum class XdosAppKind {
    Host, Arduino, Terminal, Notepad, Calculator, Paint, Files, Browser,
    Widgets, Benchmark, Player, Music, CoreApp
};

// How an app uses the screen (.APP "Screen=Full|Window|None", R16-B). Only one Full
// app may run at a time, and while one runs a draw core is kept from lending.
// Default: V1 XAPPs are Full (they take over the screen), ABI=2 apps are Window.
enum class XdosScreen : unsigned char { Full, Window, None };

struct XdosDesktopApp {
    char name[41];
    char path[96];
    XdosAppKind kind;
    XdosScreen screen;
};

constexpr unsigned XdosDesktopColumns = 8;
constexpr unsigned XdosDesktopAppLimit = XdosDesktopColumns * 2;
unsigned XdosReadDesktopApps(XdosDesktopApp (&apps)[XdosDesktopAppLimit]);
