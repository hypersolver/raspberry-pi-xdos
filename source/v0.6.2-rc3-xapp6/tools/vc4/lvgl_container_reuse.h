#pragma once

#include "lvgl/src/display/lv_display_private.h"
#include "lvgl/src/core/lv_obj_draw_private.h"
#include "lvgl/src/misc/lv_area_private.h"

namespace {
lv_obj_t *cachedContainer=nullptr;
lv_area_t cachedBounds{}, cachedOverlays[16];
unsigned cachedOverlayCount=0;
bool cachedFrame=false, cachedGeometry=false, scrollPending=false;
int cachedY=0;
unsigned containerReuseFrames=0;
unsigned contentFallbackFrames=0;

#if XDOS_DIRECTION_TEST
struct DirectionSamples {
    ScrollSamples intervals;
    unsigned frames, reuse, full, lastPresent;
    u64 bytes, previousBytes;
    int direction, startY;
    unsigned distance, contentFallbackStart;
};
DirectionSamples directionSamples{};
bool directionReused=false;
#endif

bool VisibleBounds(lv_obj_t *obj,lv_area_t &bounds)
{
    for (auto *p=obj;p;p=lv_obj_get_parent(p)) {
        if (lv_obj_get_layer_type(p)!=LV_LAYER_TYPE_NONE ||
            lv_obj_has_flag(p,LV_OBJ_FLAG_OVERFLOW_VISIBLE)) return false;
    }
    lv_obj_get_coords(obj,&bounds);
    return lv_obj_area_is_visible(obj,&bounds);
}

void InvalidateContainerRect(int x1,int y1,int x2,int y2)
{
    if (x1>x2 || y1>y2) return;
    const lv_area_t area={x1,y1,x2,y2};
    lv_obj_invalidate_area(cachedContainer,&area);
}

bool AppendOverlay(lv_obj_t *obj,const lv_area_t &bounds,lv_area_t *out,unsigned &count)
{
    lv_area_t area, intersection;
    lv_obj_get_coords(obj,&area);
    const int margin=lv_obj_get_ext_draw_size(obj)+2;
    area.x1-=margin; area.y1-=margin; area.x2+=margin; area.y2+=margin;
    if (!lv_obj_area_is_visible(obj,&area) || !lv_area_intersect(&intersection,&area,&bounds))
        return true;
    if (count==sizeof cachedOverlays/sizeof cachedOverlays[0]) return false;
    out[count++]=intersection;
    return true;
}

bool ContainerOverlays(const lv_area_t &bounds,lv_area_t *out,unsigned &count)
{
    if (!CollectOverlays(out,count)) return false;
    for (unsigned i=0;i<lv_obj_get_child_count(cachedContainer);++i) {
        auto *child=lv_obj_get_child(cachedContainer,i);
        if (lv_obj_has_flag(child,LV_OBJ_FLAG_FLOATING) && !AppendOverlay(child,bounds,out,count))
            return false;
    }
    for (auto *obj=cachedContainer,*parent=lv_obj_get_parent(obj); parent;
         obj=parent,parent=lv_obj_get_parent(parent)) {
        bool above=false;
        for (unsigned i=0;i<lv_obj_get_child_count(parent);++i) {
            auto *sibling=lv_obj_get_child(parent,i);
            if (above && !AppendOverlay(sibling,bounds,out,count)) return false;
            if (sibling==obj) above=true;
        }
    }
    return true;
}

void ContainerBeforeRefresh(lv_display_t *display)
{
    if (!cachedContainer || !scrollPending) return;
    scrollPending=false;
    if (!xdos_vc4_active() || lv_obj_get_screen(cachedContainer)!=lv_screen_active()) return;
    lv_area_t bounds, current[16];
    unsigned currentCount=0;
    const int delta=lv_obj_get_scroll_y(cachedContainer)-cachedY;
    if (!delta) return;
    if (!cachedGeometry || !VisibleBounds(cachedContainer,bounds) ||
        !lv_area_is_equal(&bounds,&cachedBounds) || !ContainerOverlays(bounds,current,currentCount)) {
        lv_obj_invalidate(cachedContainer);
        return;
    }
    const int left=bounds.x1+8, right=bounds.x2-31;
    const int top=bounds.y1+8, bottom=bounds.y2-7;
    if (left>=right || top>=bottom || delta>=bottom-top || delta<=top-bottom) {
        lv_obj_invalidate(cachedContainer);
        return;
    }
    const lv_area_t reused={left,top,right-1,bottom-1};
    // Source damage cannot be reconstructed reliably from the final dirty
    // rectangles. A redraw must include everything composited in the viewport.
    for (unsigned i=0;i<display->inv_p;++i) {
        lv_area_t intersection;
        if (!lv_area_intersect(&intersection,&reused,&display->inv_areas[i])) continue;
        ++contentFallbackFrames;
        xdos_vc4_diff_frame();
        lv_obj_invalidate(cachedContainer);
        return;
    }
    if (!xdos_vc4_shift_viewport(delta,left,top,right,bottom)) {
        lv_obj_invalidate(cachedContainer);
        return;
    }
    ++containerReuseFrames;
#if XDOS_DIRECTION_TEST
    directionReused=true;
#endif
    InvalidateContainerRect(bounds.x1,bounds.y1,bounds.x2,top-1);
    InvalidateContainerRect(bounds.x1,bottom,bounds.x2,bounds.y2);
    InvalidateContainerRect(bounds.x1,top,left-1,bottom-1);
    InvalidateContainerRect(right,top,bounds.x2,bottom-1);
    if (delta>0) InvalidateContainerRect(left,bottom-delta,right-1,bottom-1);
    else InvalidateContainerRect(left,top,right-1,top-delta-1);
    for (unsigned i=0;i<cachedOverlayCount;++i) {
        const auto &a=cachedOverlays[i];
        InvalidateContainerRect(a.x1,a.y1-delta,a.x2,a.y2-delta);
    }
    for (unsigned i=0;i<currentCount;++i) {
        const auto &a=current[i];
        InvalidateContainerRect(a.x1,a.y1,a.x2,a.y2);
    }
}

void ContainerFrameCommitted()
{
    if (!cachedContainer || lv_obj_get_screen(cachedContainer)!=lv_screen_active()) return;
#if XDOS_DIRECTION_TEST
    auto &sample=directionSamples;
    const auto nextY=lv_obj_get_scroll_y(cachedContainer);
    if (sample.direction && cachedFrame && nextY!=cachedY) {
        const unsigned now=CTimer::GetClockTicks();
        if (sample.frames) sample.intervals.Add(now-sample.lastPresent,250);
        sample.lastPresent=now;
        ++sample.frames;
        if (directionReused) ++sample.reuse;
        else ++sample.full;
        sample.bytes+=uploadBytes-sample.previousBytes;
    }
    sample.previousBytes=uploadBytes;
    directionReused=false;
#endif
    cachedY=lv_obj_get_scroll_y(cachedContainer);
    cachedGeometry=VisibleBounds(cachedContainer,cachedBounds) &&
        ContainerOverlays(cachedBounds,cachedOverlays,cachedOverlayCount);
    cachedFrame=true;
}
}

#if XDOS_DIRECTION_TEST
extern "C" void xdos_analytics_direction_begin(int direction,unsigned distance)
{
    auto &sample=directionSamples;
    sample={};
    sample.direction=direction;
    sample.distance=distance;
    sample.previousBytes=uploadBytes;
    sample.startY=lv_obj_get_scroll_y(cachedContainer);
    sample.contentFallbackStart=contentFallbackFrames;
}

extern "C" void xdos_analytics_direction_end()
{
    auto &sample=directionSamples;
    if (!sample.direction || !cachedContainer) return;
    CLogger::Get()->Write("SCROLL",LogWarning,
        "DIR %s target_px=%u start_y=%d end_y=%d frames=%u reuse=%u full=%u content_full=%u bytes_per_frame=%u p95_us=%u vc4=%u",
        sample.direction>0?"UP":"DOWN",sample.distance,sample.startY,
        lv_obj_get_scroll_y(cachedContainer),sample.frames,sample.reuse,sample.full,
        contentFallbackFrames-sample.contentFallbackStart,
        sample.frames?(unsigned)(sample.bytes/sample.frames):0,
        sample.intervals.P95(250),(unsigned)xdos_vc4_active());
    sample.direction=0;
}
#endif

extern "C" void xdos_container_prepare_refresh(lv_display_t *display)
{
    ContainerBeforeRefresh(display);
}

extern "C" bool xdos_container_scroll_damage(lv_obj_t *obj,int dx,int dy)
{
    if (!XDOS_ANALYTICS_CACHE || !xdos_vc4_active() || obj!=cachedContainer ||
        !cachedFrame || !cachedGeometry || dx || !dy ||
        lv_obj_get_screen(obj)!=lv_screen_active()) return false;
    auto *display=lv_display_get_default();
    lv_area_t dirty[LV_INV_BUF_SIZE];
    const auto n=display->inv_p;
    for (unsigned i=0;i<n;++i) dirty[i]=display->inv_areas[i];
    for (unsigned i=0;i<n;++i) {
        lv_area_t area;
        if (!lv_area_intersect(&area,&dirty[i],&cachedBounds)) continue;
        InvalidateContainerRect(area.x1,area.y1+dy,area.x2,area.y2+dy);
    }
    scrollPending=true;
    // Keep the scrollbar dirty and ensure the refresh timer is resumed even
    // when scrolling is the only source of damage.
    InvalidateContainerRect(cachedBounds.x2-15,cachedBounds.y1,
                            cachedBounds.x2,cachedBounds.y2);
    return true;
}

extern "C" void xdos_analytics_start(lv_obj_t *container)
{
    cachedContainer=container;
    cachedFrame=cachedGeometry=scrollPending=false;
    containerReuseFrames=0;
    contentFallbackFrames=0;
    uploadBytes=0;
    xdos_vc4_track_uploads(1);
    lv_obj_invalidate(container);
    xdos_scroll_measure_start();
}

extern "C" void xdos_analytics_end()
{
    if (!cachedContainer) return;
    if (XDOS_TEST_DEBUG)
        CLogger::Get()->Write("SCROLL",LogWarning,
            "analytics_reuse_frames=%u upload_bytes=%u content_fallback_frames=%u",
            containerReuseFrames,(unsigned)uploadBytes,contentFallbackFrames);
    cachedContainer=nullptr;
    scrollPending=false;
    xdos_vc4_track_uploads(0);
    xdos_scroll_measure_end();
}
