#pragma once

#include <circle/logger.h>
#include <circle/synchronize.h>
#include <circle/xdos_test_debug.h>

namespace {
struct ScrollSamples {
    unsigned count = 0, maximum = 0;
    u64 total = 0;
    unsigned histogram[1001]{};

    void Add(unsigned us, unsigned binUs)
    {
        ++count;
        total += us;
        if (us > maximum) maximum = us;
        const unsigned bin = us/binUs;
        ++histogram[bin<1000 ? bin : 1000];
    }

    unsigned P95(unsigned binUs) const
    {
        const unsigned target = (count*95+99)/100;
        unsigned seen = 0;
        for (unsigned i=0; i<1000; ++i) {
            seen += histogram[i];
            if (seen>=target) return count ? i*binUs+binUs-1 : 0;
        }
        return maximum;
    }

    unsigned Mean() const { return count ? total/count : 0; }
};

volatile bool measuringScroll = false;
ScrollSamples frameTimes, dmaTimes, mirrorTimes, syncTimes;
unsigned lastPresent = 0, shownFrames = 0, flippedFrames = 0, flipErrors = 0;
#if XDOS_SCROLL_GPU
ScrollSamples gpuUpload,gpuDraw,gpuSwap;
unsigned currentUpload=0;
unsigned manualVsyncCalls=0, manualOffsetCalls=0;

void RecordGpuChunk(const XdosVc4Timing &timing,bool last)
{
    if (!measuringScroll) return;
    currentUpload+=timing.upload_us;
    if (last) {
        gpuUpload.Add(currentUpload,250);
        gpuDraw.Add(timing.draw_us,250);
        gpuSwap.Add(timing.swap_us,25);
        currentUpload=0;
    }
}
#endif
#if XDOS_SCROLL_RENDER_STATS
ScrollSamples renderTimes;
bool rendering = false;
unsigned renderStarted = 0, flushStarted = 0, flushInRender = 0;

void RenderEvent(lv_event_t *event)
{
    if (!measuringScroll) return;
    switch (lv_event_get_code(event)) {
    case LV_EVENT_RENDER_START:
        rendering = true;
        renderStarted = CTimer::GetClockTicks();
        flushInRender = 0;
        break;
    case LV_EVENT_FLUSH_START:
    case LV_EVENT_FLUSH_WAIT_START:
        if (rendering) flushStarted = CTimer::GetClockTicks();
        break;
    case LV_EVENT_FLUSH_FINISH:
    case LV_EVENT_FLUSH_WAIT_FINISH:
        if (rendering) flushInRender += CTimer::GetClockTicks()-flushStarted;
        break;
    case LV_EVENT_RENDER_READY:
        if (rendering) {
            const unsigned elapsed = CTimer::GetClockTicks()-renderStarted;
            renderTimes.Add(elapsed>flushInRender ? elapsed-flushInRender : 0,250);
            rendering = false;
        }
        break;
    default:
        break;
    }
}
#endif

void RecordPresentation(bool flipped)
{
    if (!measuringScroll) return;
    const auto now = CTimer::GetClockTicks();
    if (shownFrames) frameTimes.Add(now-lastPresent,250);
    lastPresent = now;
    ++shownFrames;
    if (flipped) ++flippedFrames;
}

#if XDOS_SCROLL_VSYNC
CBcmFrameBuffer *pages = nullptr;
unsigned visibleY = 0, drawY = 0;
unsigned dirtyTop = 0, dirtyBottom = 0;
unsigned mirrorTop = 0, mirrorBottom = 0;
volatile bool waitingFinalDma = false, pendingPresent = false;

#if XDOS_SCROLL_OPTIMIZED
struct DrawnArea { unsigned left,right,top,bottom; };
DrawnArea drawn[512];
unsigned drawnCount = 0;
bool backSynchronized = false;

void SyncUndrawn()
{
    if (backSynchronized) return;
    backSynchronized = true;
    if (mirrorTop==mirrorBottom) return;
    const unsigned started = measuringScroll ? CTimer::GetClockTicks() : 0;
    const unsigned pitch = pages->GetPitch();
    const unsigned width = pages->GetWidth();
    auto *base = reinterpret_cast<u8 *>(uintptr(pages->GetBuffer()));
    for (unsigned y=mirrorTop; y<mirrorBottom; ++y) {
        const auto *source = base+(visibleY+y)*pitch;
        auto *destination = base+(drawY+y)*pitch;
        unsigned x = 0;
        for (unsigned i=0; i<drawnCount && x<width; ++i) {
            const auto &area = drawn[i];
            if (y<area.top || y>=area.bottom || area.right<=x) continue;
            if (x<area.left)
                memcpy(destination+x*2,source+x*2,(area.left-x)*2);
            x=area.right;
        }
        if (x<width)
            memcpy(destination+x*2,source+x*2,(width-x)*2);
    }
    DataSyncBarrier();
    if (measuringScroll) mirrorTimes.Add(CTimer::GetClockTicks()-started,25);
}

void TrackChunk(unsigned left,unsigned right,unsigned top,unsigned bottom)
{
    if (backSynchronized) return;
    if (drawnCount==sizeof drawn/sizeof drawn[0]) {
        // Preserve regions already redrawn before reverting to ordinary updates.
        SyncUndrawn();
        return;
    }
    unsigned i=drawnCount++;
    while (i && drawn[i-1].left>left) {
        drawn[i]=drawn[i-1];
        --i;
    }
    drawn[i]={left,right,top,bottom};
}
#endif

void MirrorRows(unsigned fromY,unsigned toY,unsigned top,unsigned bottom)
{
    if (top==bottom) return;
    const unsigned pitch = pages->GetPitch();
    const auto *source = reinterpret_cast<const u8 *>(uintptr(pages->GetBuffer()))+(fromY+top)*pitch;
    const CDisplay::TArea rows={0,pitch/2-1,toY+top,toY+bottom-1};
    const unsigned start = measuringScroll ? CTimer::GetClockTicks() : 0;
    pages->SetArea(rows,source);
    if (measuringScroll) mirrorTimes.Add(CTimer::GetClockTicks()-start,25);
}

void InitPages(CBcmFrameBuffer *fb)
{
    if (!fb || fb->GetDepth()!=16 || fb->GetSize()<2U*fb->GetPitch()*fb->GetHeight()) {
        CLogger::Get()->Write("SCROLL",LogWarning,"two framebuffer pages unavailable");
        return;
    }
    pages = fb;
    drawY = fb->GetHeight();
    mirrorBottom = drawY;
}

void BeginChunk(unsigned left,unsigned right,unsigned top,unsigned bottom)
{
    if (!pages || drawY==visibleY) return;
#if XDOS_SCROLL_OPTIMIZED
    TrackChunk(left,right,top,bottom);
#else
    (void)left;
    (void)right;
#endif
    if (dirtyTop==dirtyBottom) {
#if !XDOS_SCROLL_OPTIMIZED
        MirrorRows(visibleY,drawY,mirrorTop,mirrorBottom);
        mirrorTop = mirrorBottom = 0;
#endif
        dirtyTop = top;
        dirtyBottom = bottom;
        return;
    }
    if (top<dirtyTop) dirtyTop=top;
    if (bottom>dirtyBottom) dirtyBottom=bottom;
}
#endif
}

extern "C" void xdos_scroll_measure_start()
{
    if (!XDOS_TEST_DEBUG) return;
    frameTimes = {};
    dmaTimes = {};
    mirrorTimes = {};
    syncTimes = {};
#if XDOS_SCROLL_GPU
    gpuUpload={};
    gpuDraw={};
    gpuSwap={};
    currentUpload=0;
    manualVsyncCalls=manualOffsetCalls=0;
#endif
#if XDOS_SCROLL_RENDER_STATS
    renderTimes = {};
    rendering = false;
#endif
    shownFrames = flippedFrames = flipErrors = 0;
    measuringScroll = true;
}

extern "C" void xdos_scroll_measure_end()
{
    if (!XDOS_TEST_DEBUG) return;
    if (!measuringScroll) return;
    measuringScroll = false;
#if XDOS_SCROLL_GPU
    CLogger::Get()->Write("SCROLL",LogWarning,
        "gpu_frames=%u upload_mean_us=%u compose_mean_us=%u swap_mean_us=%u manual_vsync=%u manual_offset=%u",
        gpuDraw.count,gpuUpload.Mean(),gpuDraw.Mean(),gpuSwap.Mean(),
        manualVsyncCalls,manualOffsetCalls);
#endif
#if XDOS_SCROLL_RENDER_STATS
    CLogger::Get()->Write("SCROLL",LogWarning,
        "render_mode=%s render_count=%u render_mean_us=%u render_p95_us=%u render_max_us=%u",
        XDOS_SCROLL_LABEL,renderTimes.count,renderTimes.Mean(),renderTimes.P95(250),renderTimes.maximum);
#endif
    CLogger::Get()->Write("SCROLL",LogWarning,
        "mode=%s frames=%u frame_mean_us=%u frame_p95_us=%u frame_max_us=%u dma_count=%u "
        "dma_mean_us=%u dma_p95_us=%u dma_max_us=%u mirror_mean_us=%u sync_mean_us=%u flips=%u errors=%u",
        XDOS_SCROLL_LABEL,
        shownFrames,frameTimes.Mean(),frameTimes.P95(250),
        frameTimes.maximum,dmaTimes.count,dmaTimes.Mean(),dmaTimes.P95(25),dmaTimes.maximum,
        mirrorTimes.Mean(),syncTimes.Mean(),flippedFrames,flipErrors);
}

extern "C" void xdos_present_frame()
{
#if XDOS_SCROLL_GPU
    // In GLES2 mode EGL owns presentation. The framebuffer path becomes
    // eligible again only after the GLES2 backend has stopped.
    if (xdos_vc4_active()) return;
#endif
#if XDOS_SCROLL_VSYNC
    if (!pages) return;
    // IRQ completion only marks the frame ready. Mailbox calls stay in task context.
    while (waitingFinalDma) WaitForInterrupt();
    if (!pendingPresent) return;
    pendingPresent = false;
#if XDOS_SCROLL_OPTIMIZED
    SyncUndrawn();
    drawnCount = 0;
    backSynchronized = false;
#endif
    const unsigned start=measuringScroll ? CTimer::GetClockTicks() : 0;
#if XDOS_SCROLL_GPU
    if (measuringScroll) ++manualVsyncCalls;
#endif
    const bool synced=pages->WaitForVerticalSync();
#if XDOS_SCROLL_GPU
    if (synced && measuringScroll) ++manualOffsetCalls;
#endif
    const bool switched=synced && pages->SetVirtualOffset(0,drawY);
    if (measuringScroll) syncTimes.Add(CTimer::GetClockTicks()-start,50);
    if (switched) {
        const unsigned oldVisible = visibleY;
        visibleY = drawY;
        drawY = oldVisible;
        mirrorTop = dirtyTop;
        mirrorBottom = dirtyBottom;
        RecordPresentation(true);
    } else {
        if (measuringScroll) ++flipErrors;
        MirrorRows(drawY,visibleY,0,pages->GetHeight());
#if XDOS_SCROLL_GPU
        if (visibleY && measuringScroll) ++manualOffsetCalls;
#endif
        if (visibleY && pages->SetVirtualOffset(0,0)) {
            MirrorRows(visibleY,0,0,pages->GetHeight());
            visibleY = 0;
        }
        CLogger::Get()->Write("SCROLL",LogWarning,"VSync page flip failed");
        // Continue updating the page confirmed visible by the last successful flip.
        drawY = visibleY;
        mirrorTop = mirrorBottom = 0;
        RecordPresentation(false);
    }
    dirtyTop = dirtyBottom = 0;
#endif
}

namespace {
void FinishChunk(bool last)
{
    if (!last) return;
#if XDOS_SCROLL_VSYNC
    if (pages && drawY!=visibleY) {
        pendingPresent = true;
        waitingFinalDma = false;
        return;
    }
#endif
    RecordPresentation(false);
}
}
