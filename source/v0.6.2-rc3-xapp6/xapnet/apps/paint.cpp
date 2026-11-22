#include <lvgl.h>
#include "xdos_corejobs.h"
#include "xdos_xapp_memory.h"
#include "xapp_workspace.h"

namespace {
typedef struct { int x, y, drawing; } StrokeState;

static void dot(const XdosSurface *surface, int x, int y)
{
    for (int v = y - 1; v <= y + 1; ++v)
        for (int u = x - 1; u <= x + 1; ++u)
            if (u >= 0 && v >= 0 && u < surface->width && v < surface->height)
                surface->pixels[v * surface->stride + u] = 0x1c58;
}

static int32_t stroke(void *opaque, const XdosSurface *surface, int event, int x, int y)
{
    auto *state = static_cast<StrokeState *>(opaque);
    if (event == XDOS_PAINT_CLEAR) {
        for (int row = 0; row < surface->height; ++row)
            for (int col = 0; col < surface->width; ++col)
                surface->pixels[row * surface->stride + col] = 0xffff;
        state->drawing = 0;
        return 1;
    }
    if (event == XDOS_PAINT_UP) {
        state->drawing = 0;
        return 0;
    }
    if (x < 0 || y < 0 || x >= surface->width || y >= surface->height) return 0;
    if (event == XDOS_PAINT_DOWN) {
        state->x = x;
        state->y = y;
        state->drawing = 1;
    }
    if (!state->drawing) return 0;
    int from_x = state->x, from_y = state->y;
    const int dx = x > from_x ? x - from_x : from_x - x;
    const int dy = y > from_y ? from_y - y : y - from_y;
    const int sx = from_x < x ? 1 : -1, sy = from_y < y ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        dot(surface, from_x, from_y);
        if (from_x == x && from_y == y) break;
        const int twice = 2 * error;
        if (twice >= dy) { error += dy; from_x += sx; }
        if (twice <= dx) { error += dx; from_y += sy; }
    }
    state->x = x;
    state->y = y;
    return 1;
}

lv_obj_t *canvas;
XdosSurface surface;
StrokeState state;
int slot;

void Draw(lv_event_t *event)
{
    auto *device=lv_indev_active();
    if (!device) return;
    const auto code=lv_event_get_code(event);
    int action;
    switch (code) {
    case LV_EVENT_PRESSED: action=XDOS_PAINT_DOWN; break;
    case LV_EVENT_PRESSING: action=XDOS_PAINT_MOVE; break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST: action=XDOS_PAINT_UP; break;
    default: return;
    }
    lv_point_t point;
    lv_area_t bounds;
    lv_indev_get_point(device,&point);
    lv_obj_get_coords(canvas,&bounds);
    if (stroke(&state,&surface,action,point.x-bounds.x1,point.y-bounds.y1))
        lv_obj_invalidate(canvas);
}
void Clear(lv_event_t *)
{
    stroke(&state,&surface,XDOS_PAINT_CLEAR,0,0);
    lv_obj_invalidate(canvas);
}
void Close(void *) { xdos_v2_exit(slot); }
}

extern "C" int32_t xapp_entry(const XdosHostApiV1 *api)
{
    if (!api || api->abi_version!=XDOS_ABI_VERSION) return -1;
    slot=xdos_v2_slot_of(reinterpret_cast<const void *>(xapp_entry));
    if (slot<0) return -1;
    auto *display=lv_display_get_default();
    const int width=LV_MIN(608,lv_display_get_horizontal_resolution(display)-38)&~1;
    const int height=LV_MIN(440,lv_display_get_vertical_resolution(display)-112);
    if (width<100 || height<80) return -1;
    auto *pixels=static_cast<uint16_t *>(xdos_v2_alloc(slot,width*height*sizeof(uint16_t)));
    if (!pixels) return -1;
    auto *body=xdos_v2_window(slot,"Paint",width+22,height+64);
    if (!body) return -1;
    surface={pixels,width,height,width};
    auto *button=lv_button_create(body);
    auto *name=lv_label_create(button);
    lv_label_set_text(name,"CLEAR");
    lv_obj_center(name);
    lv_obj_add_event_cb(button,Clear,LV_EVENT_CLICKED,nullptr);
    canvas=lv_canvas_create(body);
    lv_canvas_set_buffer(canvas,pixels,width,height,LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(canvas,2,44);
    lv_obj_add_flag(canvas,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(canvas,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(canvas,Draw,LV_EVENT_ALL,nullptr);
    Clear(nullptr);
    xdos_v2_register_close(slot,Close,nullptr);
    return 0;
}
