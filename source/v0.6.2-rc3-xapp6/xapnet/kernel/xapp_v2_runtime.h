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
/* Slot whose app was just throttled for overrunning its core-0 slices, or -1 (once). */
int TakeThrottled();
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
