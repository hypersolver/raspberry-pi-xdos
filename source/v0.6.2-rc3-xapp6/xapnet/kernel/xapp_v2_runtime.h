#ifndef XDOS_XAPP_V2_RUNTIME_H
#define XDOS_XAPP_V2_RUNTIME_H
/* Core 0 side of the V2 app runtime: open/close/status and the per-frame
 * service (core 0 call queue, deferred slot release). */
#include "xdos_sdk_v1.h"
#include "desktop_apps.h"

namespace XappV2Runtime {
/* Load `path` into the lowest free V2 slot and run its entry. Returns the
 * slot (0..3) or a negative XappLoaderV2::Result. */
int Open(const char *path, const XdosHostApiV1 *api, bool large = false,
         XdosScreen screen = XdosScreen::Window);
/* Open() result when a Full app is requested while another Full app runs (R16-B). */
constexpr int SCREEN_BUSY = -11;
/* True while a V2 app opened with XdosScreen::Full is running. */
bool FullActive();
/* Open() result when an app with a window or full screen is requested in IOT mode (R19). */
constexpr int NO_SCREEN = -12;
/* R19 IOT mode: no LVGL; only Screen=None apps may open. */
void SetHeadless(bool headless);
bool Headless();
/* R19 KIOSK mode: the Full app's window covers the whole screen, no title bar. */
void SetKiosk(bool kiosk);
/* True while `slot` holds a running app (not closing). */
bool Running(int slot);
/* Slot whose app was just throttled for overrunning its core-0 slices, or -1 (once). */
int TakeThrottled();
/* Display name of the app in `slot` (default: its file name without extension). */
void SetName(int slot, const char *name);
/* For the taskbar: names of the apps on each CPU. cpu[0] lists the apps without a
 * borrowed core (their UI and work run on CPU0); cpu[1..3] name the app that
 * borrowed that core, or "" when the core draws for LVGL. */
void CoreApps(char (*cpu)[64]);
/* Ask the app in `slot` to close (its registered on_close). False when the
 * slot is free or the app registered no close handler. */
bool Close(int slot);
/* Call once per main-loop pass on core 0, outside lv_timer_handler. */
void Service();
/* Mouse from the kernel input loop (0 move, 1 down, 2 up), screen pixels.
 * True when the event belongs to a V2 window and must not reach the desktop. */
bool BlocksDesktopHover(int x, int y);
bool Pointer(int action, int x, int y);
/* Keyboard: true when the focused V2 window took the key. */
bool DeliverKey(unsigned key);
/* One-line status for UART: slots and lent cores. */
void Status(char *buffer, unsigned size);
}
#endif
