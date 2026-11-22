#include "xapp_v2_runtime.h"
#include "xapp_loader_v2.h"
#include "xdos_corejobs.h"
#include "xapp_ipc.h"
#include "xdos_ipc_api.h"
#include "xdos_xapp_memory.h"
#include <circle/memory.h>
#include <circle/new.h>
#include <circle/string.h>
#include <lvgl/lvgl.h>
#include <string.h>

extern "C" void xdos_core0_service(void);

namespace {

struct Instance {
    void (*onClose)(void *);
    void *ctx;
    bool closing;
    bool exitPending;
    lv_obj_t *window;                       /* xdos_v2_window(), owned by the runtime */
    void (*onKey)(void *, unsigned);
    void *keyCtx;
};
Instance instances[XappLoaderV2::Slots];
static_assert(XappIpc::Slots == XappLoaderV2::Slots, "IPC slots must match loader");
static_assert(XappIpc::Payload == XDOS_IPC_PAYLOAD, "IPC payload must match public ABI");
XappIpc ipc;
XappIpc::Endpoint endpoints[XappLoaderV2::Slots];
int focused = -1;                           /* slot whose window gets the keyboard */
bool capture = false;                       /* button went down on a V2 window */

constexpr int kTitleH = 34;
void PlaceWindow(lv_obj_t *window, int32_t x, int32_t y, int32_t width, int32_t height)
{
    auto *display=lv_obj_get_display(window);
    const int32_t maxX=LV_MAX(0,lv_display_get_horizontal_resolution(display)-width);
    const int32_t maxY=LV_MAX(0,lv_display_get_vertical_resolution(display)-height);
    lv_obj_set_pos(window,LV_CLAMP(0,x,maxX),LV_CLAMP(0,y,maxY));
}

bool Held(int slot)
{
    return slot >= 0 && slot < (int)XappLoaderV2::Slots && XappLoaderV2::IsHeld(slot);
}

bool Valid(lv_obj_t *obj) { return obj && lv_obj_is_valid(obj); }

void Focus(int slot)
{
    focused = slot;
    for (unsigned i = 0; i < XappLoaderV2::Slots; ++i) {
        lv_obj_t *w = instances[i].window;
        if (!Valid(w)) continue;
        lv_obj_t *bar = lv_obj_get_child(w, 0);
        if (bar) lv_obj_set_style_bg_color(bar, (int)i == slot ? lv_color_hex(0x1F5FAF) : lv_color_hex(0x5A6470), 0);
    }
    if (slot >= 0 && Valid(instances[slot].window)) lv_obj_move_foreground(instances[slot].window);
}

void TitleDrag(lv_event_t *e)
{
    lv_obj_t *win = (lv_obj_t *)lv_event_get_user_data(e);
    lv_indev_t *indev = lv_indev_active();
    if (!indev || !Valid(win)) return;
    lv_point_t v;
    lv_indev_get_vect(indev, &v);
    if (!v.x && !v.y) return;
    PlaceWindow(win,lv_obj_get_x(win)+v.x,lv_obj_get_y(win)+v.y,
                lv_obj_get_width(win),lv_obj_get_height(win));
}

void CloseClicked(lv_event_t *e)
{
    XappV2Runtime::Close((int)(intptr_t)lv_event_get_user_data(e));
}

void DropWindow(unsigned slot)
{
    if (Valid(instances[slot].window)) lv_obj_delete(instances[slot].window);
    instances[slot].window = nullptr;
    if (focused != (int)slot) return;
    int next=-1;
    for (unsigned i=0;i<XappLoaderV2::Slots;++i) {
        const auto &candidate=instances[i];
        if (!Held(i) || candidate.closing || candidate.exitPending || !Valid(candidate.window))
            continue;
        if (next<0 || lv_obj_get_index(candidate.window)>lv_obj_get_index(instances[next].window))
            next=i;
    }
    Focus(next);
}

// Circle's heap does not recycle arbitrary large allocations. Reserve one
// arena at startup and recycle its blocks independently of the system heap.
constexpr size_t Alignment = alignof(max_align_t);
struct alignas(Alignment) Block {
    size_t capacity;
    Block *next;
    int owner;
};
constexpr int Free = -1;
Block *arena;
size_t slotLimit;
size_t used[XappLoaderV2::Slots];
bool memoryInitialized;

void InitializeMemory()
{
    if (memoryInitialized) return;
    memoryInitialized = true;
    const size_t bytes = CMemorySystem::Get()->GetHeapFreeSpace(HEAP_LOW) / 2
                         & ~(Alignment - 1);
    if (bytes <= sizeof(Block)) return;
    void *memory = CMemorySystem::HeapAllocate(bytes, HEAP_LOW);
    if (!memory) return;
    arena = new (memory) Block{bytes - sizeof(Block), nullptr, Free};
    slotLimit = bytes / XappLoaderV2::Slots;
}

void Coalesce()
{
    for (Block *block = arena; block && block->next;) {
        Block *next = block->next;
        if (block->owner == Free && next->owner == Free) {
            block->capacity += sizeof(Block) + next->capacity;
            block->next = next->next;
        } else {
            block = next;
        }
    }
}

void ReleaseMemory(unsigned slot)
{
    for (Block *block = arena; block; block = block->next)
        if (block->owner == static_cast<int>(slot))
            block->owner = Free;
    used[slot] = 0;
    Coalesce();
}

bool ValidSlot(int slot)
{
    return slot >= 0 && slot < static_cast<int>(XappLoaderV2::Slots)
        && XappLoaderV2::IsHeld(slot);
}

} // namespace

extern "C" XdosIpcEndpoint xdos_ipc_endpoint(unsigned slot)
{
    if (xdos_this_core() != 0 || slot >= XappLoaderV2::Slots)
        return {slot, 0};
    return {slot, endpoints[slot].generation};
}

extern "C" int xdos_ipc_send(XdosIpcEndpoint source, XdosIpcEndpoint target,
                              const void *data, unsigned size)
{
    if (xdos_this_core() != 0) return XDOS_IPC_INVALID_ENDPOINT;
    return ipc.Send({source.slot, source.generation}, {target.slot, target.generation}, data, size);
}

extern "C" int xdos_ipc_receive(XdosIpcEndpoint receiver, XdosIpcMessage *message)
{
    if (xdos_this_core() != 0) return XDOS_IPC_INVALID_ENDPOINT;
    if (!message) return XDOS_IPC_INVALID_PAYLOAD;
    XappIpc::Message value;
    const auto result = ipc.Receive({receiver.slot, receiver.generation}, value);
    if (result == XappIpc::Ok) {
        message->source = {value.source.slot, value.source.generation};
        message->size = value.size;
        memcpy(message->data, value.data, sizeof message->data);
    }
    return result;
}

extern "C" size_t xdos_v2_memory_limit(void)
{
    return xdos_this_core() == 0 ? slotLimit : 0;
}

extern "C" void *xdos_v2_alloc(int slot, size_t bytes)
{
    if (xdos_this_core() != 0 || !ValidSlot(slot) || instances[slot].closing
        || instances[slot].exitPending || !bytes || bytes > slotLimit)
        return nullptr;

    const size_t aligned = (bytes + Alignment - 1) & ~(Alignment - 1);
    const size_t remaining = slotLimit - used[slot];
    for (Block *block = arena; block; block = block->next) {
        if (block->owner != Free || block->capacity < aligned) continue;
        const size_t spare = block->capacity - aligned;
        const bool split = spare >= sizeof(Block) + Alignment;
        const size_t capacity = split ? aligned : block->capacity;
        if (capacity + sizeof(Block) > remaining) continue;
        if (split) {
            void *tail = reinterpret_cast<unsigned char *>(block + 1) + aligned;
            block->next = new (tail) Block{spare - sizeof(Block), block->next, Free};
            block->capacity = aligned;
        }
        block->owner = slot;
        used[slot] += capacity + sizeof(Block);
        return block + 1;
    }
    return nullptr;
}

extern "C" int xdos_v2_free(int slot, void *pointer)
{
    if (xdos_this_core() != 0 || !ValidSlot(slot) || !pointer) return -1;
    for (Block *block = arena; block; block = block->next) {
        if (pointer != block + 1 || block->owner != slot) continue;
        used[slot] -= block->capacity + sizeof(Block);
        block->owner = Free;
        Coalesce();
        return 0;
    }
    return -1;
}

extern "C" int xdos_v2_slot_of(const void *address)
{
    const uintptr_t a = reinterpret_cast<uintptr_t>(address);
    for (unsigned i = 0; i < XappLoaderV2::Slots; ++i) {
        const uintptr_t base = XappLoaderV2::SlotBase(i);
        if (a >= base && a < base + XappLoaderV2::SlotBytes(i)) return (int)i;
    }
    return -1;
}

extern "C" void xdos_v2_register_close(int slot, void (*on_close)(void *), void *ctx)
{
    if (slot < 0 || slot >= (int)XappLoaderV2::Slots || !XappLoaderV2::IsHeld(slot)) return;
    instances[slot].onClose = on_close;
    instances[slot].ctx = ctx;
}

/* A movable window on its owning screen with a title bar and close button that
 * calls the app's registered on_close. Returns the content area; the runtime
 * deletes the window when the slot is released. Core 0 only. */
extern "C" lv_obj_t *xdos_v2_window(int slot, const char *title, int width, int height)
{
    if (!Held(slot)) return nullptr;
    Instance &in = instances[slot];
    if (Valid(in.window)) return lv_obj_get_child(in.window, 1);
    const int32_t sw = lv_display_get_horizontal_resolution(lv_display_get_default());
    const int32_t sh = lv_display_get_vertical_resolution(lv_display_get_default());
    if (sw<=0 || sh<=kTitleH) return nullptr;
    if (width < 200) width = 200;
    if (height < 120) height = 120;
    if (width > sw) width = sw;
    if (height > sh - kTitleH) height = sh - kTitleH;
    lv_obj_t *win = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(win);
    lv_obj_set_size(win, width, height + kTitleH);
    const int32_t x = sw - width - 24 - slot * 36, y = 60 + slot * 44;
    PlaceWindow(win,x,y,width,height+kTitleH);
    lv_obj_set_style_bg_color(win, lv_color_hex(0xF4F6F8), 0);
    lv_obj_set_style_bg_opa(win, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(win, lv_color_hex(0x30363D), 0);
    lv_obj_set_style_border_width(win, 1, 0);
    lv_obj_set_style_shadow_width(win, 12, 0);
    lv_obj_set_style_shadow_opa(win, LV_OPA_30, 0);
    lv_obj_remove_flag(win, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bar = lv_obj_create(win);                   /* child 0 */
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, lv_pct(100), kTitleH);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x1F5FAF), 0);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(bar, TitleDrag, LV_EVENT_PRESSING, win);
    lv_obj_t *label = lv_label_create(bar);
    lv_label_set_text(label, title ? title : "XAPP");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_t *close = lv_button_create(bar);
    lv_obj_set_size(close, kTitleH - 6, kTitleH - 6);
    lv_obj_align(close, LV_ALIGN_RIGHT_MID, -3, 0);
    lv_obj_set_style_bg_color(close, lv_color_hex(0xC0392B), 0);
    lv_obj_t *x_label = lv_label_create(close);
    lv_label_set_text(x_label, LV_SYMBOL_CLOSE);
    lv_obj_center(x_label);
    lv_obj_add_event_cb(close, CloseClicked, LV_EVENT_CLICKED, (void *)(intptr_t)slot);

    lv_obj_t *body = lv_obj_create(win);                  /* child 1: content */
    lv_obj_remove_style_all(body);
    lv_obj_set_pos(body, 0, kTitleH);
    lv_obj_set_size(body, width, height);
    lv_obj_set_style_pad_all(body, 8, 0);

    in.window = win;
    Focus(slot);
    return body;
}

/* Keys (ASCII, or the XDOS key codes above 255) go to the focused V2 window. */
extern "C" void xdos_v2_register_key(int slot, void (*on_key)(void *ctx, unsigned key), void *ctx)
{
    if (!Held(slot)) return;
    instances[slot].onKey = on_key;
    instances[slot].keyCtx = ctx;
}

extern "C" void xdos_v2_exit(int slot)
{
    if (slot < 0 || slot >= (int)XappLoaderV2::Slots || !XappLoaderV2::IsHeld(slot)) return;
    instances[slot].exitPending = true;
}

int XappV2Runtime::Open(const char *path, const XdosHostApiV1 *api, bool large)
{
    InitializeMemory();
    const int slot = XappLoaderV2::AcquireSlot(large);
    if (slot < 0) return XappLoaderV2::NO_SLOT;
    instances[slot] = Instance{};
    endpoints[slot] = ipc.Activate(slot);
    const XappLoaderV2::Result r = XappLoaderV2::ExecuteFile(path, api, (unsigned)slot, true);
    if (r != XappLoaderV2::OK) {
        /* A failing entry must not leave LVGL objects behind; release now. */
        DropWindow((unsigned)slot);
        instances[slot] = Instance{};
        ReleaseMemory(static_cast<unsigned>(slot));
        ipc.Deactivate(endpoints[slot]);
        endpoints[slot] = {};
        XappLoaderV2::ReleaseSlot((unsigned)slot);
        return r;
    }
    return slot;
}

bool XappV2Runtime::Close(int slot)
{
    if (slot < 0 || slot >= (int)XappLoaderV2::Slots || !XappLoaderV2::IsHeld(slot)) return false;
    Instance &in = instances[slot];
    if (!in.onClose) return false;
    if (in.closing) return true;
    in.closing = true;
    ipc.Deactivate(endpoints[slot]);
    endpoints[slot] = {};
    in.onClose(in.ctx);
    return true;
}

void XappV2Runtime::Service()
{
    InitializeMemory();
    xdos_core0_service();
    for (unsigned i = 0; i < XappLoaderV2::Slots; ++i) {
        if (!instances[i].exitPending) continue;
        DropWindow(i);
        instances[i] = Instance{};
        ReleaseMemory(i);
        ipc.Deactivate(endpoints[i]);
        endpoints[i] = {};
        XappLoaderV2::ReleaseSlot(i);
    }
}

void XappV2Runtime::Status(char *buffer, unsigned size)
{
    CString s;
    s.Format("slots");
    for (unsigned i = 0; i < XappLoaderV2::Slots; ++i) {
        CString part;
        part.Format(" %u=%s", i, !XappLoaderV2::IsHeld(i) ? "free" :
                    instances[i].closing ? "closing" : "app");
        s.Append(part);
    }
    s.Append(" cores");
    for (unsigned core = 1; core < 4; ++core) {
        int32_t result = 0;
        const int st = xdos_core_job_state(core, &result);
        CString part;
        if (st == -2) part.Format(" %u=lvgl", core);
        else if (st == 1) part.Format(" %u=running", core);
        else if (st == 0) part.Format(" %u=done(%d)", core, (int)result);
        else part.Format(" %u=?", core);
        s.Append(part);
    }
    strncpy(buffer, (const char *)s, size - 1);
    buffer[size - 1] = 0;
}

static int WindowAt(int x, int y)
{
    lv_point_t point{x,y};
    if (lv_indev_search_obj(lv_layer_top(),&point)) return -1;
    auto *screen=lv_screen_active();
    auto *hit=lv_indev_search_obj(screen,&point);
    while (hit && lv_obj_get_parent(hit)!=screen) hit=lv_obj_get_parent(hit);
    for (unsigned i=0;hit && i<XappLoaderV2::Slots;++i)
        if (Held(i) && instances[i].window==hit) return static_cast<int>(i);
    return -1;
}

bool XappV2Runtime::BlocksDesktopHover(int x, int y)
{
    return capture || WindowAt(x, y) >= 0;
}

bool XappV2Runtime::Pointer(int action, int x, int y)
{
    if (action == 1) {                       /* button down: pick the window under it */
        const int hit = WindowAt(x, y);
        if (hit<0 || (!instances[hit].closing && !instances[hit].exitPending)) Focus(hit);
        capture = hit >= 0;
        return capture;
    }
    if (action == 2) {                       /* button up */
        const bool was = capture;
        capture = false;
        return was;
    }
    if (capture) return true;                /* dragging inside a window */
    return false;                            /* hover over the desktop stays with the desktop */
}

bool XappV2Runtime::DeliverKey(unsigned key)
{
    if (!Held(focused)) { focused = -1; return false; }
    auto *window=instances[focused].window;
    if (!Valid(window) || lv_obj_get_screen(window)!=lv_screen_active()
        || lv_obj_has_flag(window,LV_OBJ_FLAG_HIDDEN)) return false;
    if (key == 264) return false;            /* XdosKeyWindows: desktop menu */
    if (key == 265) return Close(focused);   /* XdosKeyAltF4: close the focused V2 app */
    Instance &in = instances[focused];
    if (!in.onKey || in.closing || in.exitPending) return false;
    in.onKey(in.keyCtx, key);
    return true;
}
