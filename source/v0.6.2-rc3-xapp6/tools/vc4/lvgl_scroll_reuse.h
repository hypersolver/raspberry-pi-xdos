#pragma once

#include <string.h>

namespace {
constexpr int headerRows=64, scrollbarColumns=48, overlayMargin=16;
lv_obj_t *scrollScreen=nullptr;
int committedY=0;
bool committedFrame=false;
lv_area_t previousOverlay[16];
unsigned previousCount=0;
bool previousComplete=false;
unsigned cacheFrames=0;
u64 uploadBytes=0;

bool CollectOverlays(lv_area_t *areas,unsigned &count)
{
    count=0;
    lv_obj_t *layers[]={lv_layer_top(),lv_layer_sys()};
    for (auto *layer : layers) {
        const unsigned children=lv_obj_get_child_count(layer);
        for (unsigned i=0;i<children;++i) {
            lv_obj_t *child=lv_obj_get_child(layer,i);
            if (lv_obj_has_flag(child,LV_OBJ_FLAG_HIDDEN)) continue;
            if (count==sizeof previousOverlay/sizeof previousOverlay[0]) return false;
            lv_obj_get_coords(child,&areas[count]);
            areas[count].x1-=overlayMargin;
            areas[count].x2+=overlayMargin;
            areas[count].y1-=overlayMargin;
            areas[count].y2+=overlayMargin;
            ++count;
        }
    }
    return true;
}

void InvalidateRect(int left,int top,int right,int bottom)
{
    if (left>right || top>bottom) return;
    const lv_area_t area={left,top,right,bottom};
    lv_obj_invalidate_area(scrollScreen,&area);
}

void ScrollBeforeRefresh(lv_event_t *)
{
    if (!scrollScreen || !committedFrame || lv_screen_active()!=scrollScreen ||
        !xdos_vc4_active()) return;
    const int delta=lv_obj_get_scroll_y(scrollScreen)-committedY;
    if (!delta) return;
    const int width=lv_display_get_horizontal_resolution(lv_display_get_default());
    const int height=lv_display_get_vertical_resolution(lv_display_get_default());
    const int right=width-scrollbarColumns;
    lv_area_t currentOverlay[16];
    unsigned currentCount;
    if (!previousComplete || !CollectOverlays(currentOverlay,currentCount) || right<=0 ||
        delta>=height-headerRows || delta<=headerRows-height ||
        !xdos_vc4_shift_scroll(delta,right,headerRows)) {
        lv_obj_invalidate(scrollScreen);
        return;
    }
    ++cacheFrames;
    // Preserve the moving viewport; repaint bands where the old texture has
    // no matching pixels and repair the software-drawn fixed overlays.
    InvalidateRect(0,0,width-1,headerRows-1);
    InvalidateRect(right,headerRows,width-1,height-1);
    if (delta>0) InvalidateRect(0,height-delta,right-1,height-1);
    else InvalidateRect(0,headerRows,right-1,headerRows-delta-1);
    for (unsigned i=0;i<previousCount;++i) {
        const auto &a=previousOverlay[i];
        InvalidateRect(a.x1,a.y1-delta,a.x2,a.y2-delta);
    }
    for (unsigned i=0;i<currentCount;++i) {
        const auto &a=currentOverlay[i];
        InvalidateRect(a.x1,a.y1,a.x2,a.y2);
    }
}

void ScrollFrameCommitted()
{
    if (!scrollScreen || lv_screen_active()!=scrollScreen) return;
    committedY=lv_obj_get_scroll_y(scrollScreen);
    committedFrame=true;
    previousComplete=CollectOverlays(previousOverlay,previousCount);
}
}

extern "C" void xdos_gpu_scroll_begin(lv_obj_t *screen)
{
    scrollScreen=screen;
    committedFrame=false;
    previousComplete=false;
    cacheFrames=0;
    uploadBytes=0;
}

extern "C" int xdos_gpu_scroll_move(lv_obj_t *obj,int value)
{
    if (obj!=scrollScreen || !committedFrame || !xdos_vc4_active()) return 0;
    lv_display_t *display=lv_display_get_default();
    lv_display_enable_invalidation(display,false);
    lv_obj_scroll_to_y(obj,value,LV_ANIM_OFF);
    lv_display_enable_invalidation(display,true);
    return 1;
}

extern "C" void xdos_gpu_scroll_end()
{
    if (!scrollScreen) return;
    CLogger::Get()->Write("SCROLL",LogWarning,
        "cached_scroll_frames=%u upload_bytes=%u",cacheFrames,(unsigned)uploadBytes);
    scrollScreen=nullptr;
    committedFrame=false;
}
