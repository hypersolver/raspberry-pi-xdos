#include "xapp_workspace.h"

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
    StrokeState *state = opaque;
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

int32_t xapp_entry(const XdosHostApiV1 *base)
{
    if (!base || base->abi_version != XDOS_ABI_VERSION ||
        base->struct_bytes < sizeof(XdosWorkspaceApi)) return -1;
    const XdosWorkspaceApi *api = (const XdosWorkspaceApi *)base;
    return api->open_canvas(base->context, "Paint", stroke);
}
