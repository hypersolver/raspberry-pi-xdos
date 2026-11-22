//
// lvgl.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2019-2024  R. Stange <rsta2@o2online.de>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include <lvgl/lvgl.h>
#include "xdos_flush_fast.h" // XDOS_P0_ASYNC_DMA_V2
#define XDOS_P0_BUFFER_ROWS 64
#define XDOS_P0_MIN_DMA_PIXELS 512
// These totals are intentionally 32-bit: benchmark takes a difference each scene.
extern "C" {
volatile unsigned xdos_p0_dma_submitted = 0;
volatile unsigned xdos_p0_dma_completed = 0;
volatile unsigned xdos_p0_dma_total_us = 0;
volatile unsigned xdos_p0_dma_max_us = 0;
volatile unsigned xdos_p0_dirty_pixels = 0;
volatile unsigned xdos_p0_cpu_flushes = 0;
extern volatile int xdos_cpu_awake;
// XDOS Tune: clock-tick time (us) at which Update() next calls lv_timer_handler.
volatile unsigned xdos_lvgl_due_us = 0;
}
static uint32_t XdosLvglTickMs (void);
static volatile unsigned s_xdos_dma_start_us = 0;
static volatile bool s_xdos_dma_pending = false;
#include <circle/devicenameservice.h>
#include <circle/koptions.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/string.h>
#include <circle/util.h>
#include <circle/new.h>

#define XDOS_SCROLL_VSYNC 1
#define XDOS_SCROLL_OPTIMIZED 1
#define XDOS_SCROLL_RENDER_STATS 1
#define XDOS_SCROLL_LABEL "analyticsregion"
#define XDOS_SCROLL_GPU 1
#include "lvgl_vc4.h"
#include "scroll_present.h"
#define XDOS_SCROLL_UPLOAD_DIFF 0
#include "lvgl_scroll_reuse.h"
#define XDOS_ANALYTICS_CACHE 1
#define XDOS_DIRECTION_TEST 0
#define XDOS_ANALYTICS_UPLOAD_DIFF 0
#include "lvgl_container_reuse.h"
static volatile bool xdos_last_chunk = false;
static bool vc4_redraw_after_failure = false;

CLVGL *CLVGL::s_pThis = 0;

CLVGL::CLVGL (CScreenDevice *pScreen)
:	m_pBuffer1 (0),
	m_pBuffer2 (0),
	m_pScreen (pScreen),
	m_pDisplay (0),
	m_nLastUpdate (0),
	m_pMouseDevice (0),
	m_pTouchScreen (0),
	m_nLastTouchUpdate (0),
	m_pIndev (0),
	m_pCursorDesc (0)
{
	assert (s_pThis == 0);
	s_pThis = this;

	m_PointerData.state = LV_INDEV_STATE_REL;
	m_PointerData.point.x = 0;
	m_PointerData.point.y = 0;
}

CLVGL::CLVGL (CDisplay *pDisplay)
:	m_pBuffer1 (0),
	m_pBuffer2 (0),
	m_pScreen (0),
	m_pDisplay (pDisplay),
	m_nLastUpdate (0),
	m_pMouseDevice (0),
	m_pTouchScreen (0),
	m_nLastTouchUpdate (0),
	m_pIndev (0),
	m_pCursorDesc (0)
{
	assert (s_pThis == 0);
	s_pThis = this;

	m_PointerData.state = LV_INDEV_STATE_REL;
	m_PointerData.point.x = 0;
	m_PointerData.point.y = 0;
}

CLVGL::~CLVGL (void)
{
	s_pThis = 0;

	delete m_pCursorDesc;

	m_pTouchScreen = 0;
	m_pMouseDevice = 0;
	m_pDisplay = 0;
	m_pScreen = 0;

	delete [] m_pBuffer1;
	delete [] m_pBuffer2;
	m_pBuffer1 = 0;
	m_pBuffer2 = 0;
}

boolean CLVGL::Initialize (void)
{
	if (m_pDisplay == 0)
	{
		assert (m_pScreen != 0);
		m_pDisplay = m_pScreen->GetFrameBuffer ();
	}

	assert (m_pDisplay != 0);
	size_t nWidth = m_pDisplay->GetWidth ();
	size_t nHeight = m_pDisplay->GetHeight ();

	lv_init ();

	lv_log_register_print_cb (LogPrint);

	// Partial refresh: 64 rows default; benchmark can sweep 32/64/96.
	unsigned nBufSizePixels = nWidth * XDOS_P0_BUFFER_ROWS;

	m_pBuffer1 = new (HEAP_DMA30) u16[nBufSizePixels];
	m_pBuffer2 = new (HEAP_DMA30) u16[nBufSizePixels];
	if (   m_pBuffer1 == 0
	    || m_pBuffer2 == 0)
	{
		return FALSE;
	}

	static lv_display_t *display = lv_display_create (nWidth, nHeight);

	if (m_pDisplay->GetDepth () == 1)
	{
		lv_display_set_color_format (display, LV_COLOR_FORMAT_I1);
	}
	else
	{
		assert (m_pDisplay->GetDepth () == LV_COLOR_DEPTH);
	}

	lv_display_set_flush_cb (display, DisplayFlush);
	lv_display_set_buffers (display, m_pBuffer1, m_pBuffer2, nBufSizePixels * sizeof (u16),
				LV_DISP_RENDER_MODE_PARTIAL);
	lv_timer_t *xdos_refresh=lv_display_get_refr_timer (display);
	if (xdos_refresh) lv_timer_set_period (xdos_refresh,16);
	lv_display_set_tile_cnt (display,1);

	m_pMouseDevice = (CMouseDevice *) CDeviceNameService::Get ()->GetDevice ("mouse1", FALSE);
	if (m_pMouseDevice != 0)
	{
		if (m_pMouseDevice->Setup (m_pDisplay, FALSE))
		{
			m_PointerData.point.x = (nWidth+1) / 2;
			m_PointerData.point.y = (nHeight+1) / 2;

			m_pMouseDevice->ShowCursor (TRUE);

			m_pMouseDevice->RegisterEventHandler (MouseEventHandler);

			m_pMouseDevice->RegisterRemovedHandler (MouseRemovedHandler);
		}
		else
		{
			m_pMouseDevice = 0;
		}
	}

	if (m_pMouseDevice == 0)
	{
		m_pTouchScreen = (CTouchScreenDevice *) CDeviceNameService::Get ()->GetDevice ("touch1", FALSE);
		if (m_pTouchScreen != 0)
		{
			m_pTouchScreen->Setup (m_pDisplay);

			const unsigned *pCalibration = CKernelOptions::Get ()->GetTouchScreen ();
			if (pCalibration != 0)
			{
				m_pTouchScreen->SetCalibration (pCalibration);
			}

			m_pTouchScreen->RegisterEventHandler (TouchScreenEventHandler);
		}
	}

	static lv_indev_t *indev = lv_indev_create ();
	lv_indev_set_type (indev, LV_INDEV_TYPE_POINTER);
	lv_indev_set_read_cb (indev, PointerRead);

	assert (m_pIndev == 0);
	m_pIndev = indev;

	if (m_pMouseDevice != 0)
	{
		assert (m_pIndev != 0);
		SetupCursor (m_pIndev);
	}

#if XDOS_SCROLL_VSYNC
	if (m_pScreen) InitPages(m_pScreen->GetFrameBuffer());
#endif
#if XDOS_SCROLL_RENDER_STATS
	lv_display_add_event_cb(display,RenderEvent,LV_EVENT_ALL,nullptr);
	lv_display_add_event_cb(display,ScrollBeforeRefresh,LV_EVENT_REFR_START,nullptr);
#endif
	lv_tick_set_cb (XdosLvglTickMs);

	return TRUE;
}

void CLVGL::Update (boolean bPlugAndPlayUpdated)
{
	if (   bPlugAndPlayUpdated
	    && m_pMouseDevice == 0)
	{
		m_pMouseDevice =
			(CMouseDevice *) CDeviceNameService::Get ()->GetDevice ("mouse1", FALSE);
		if (m_pMouseDevice != 0)
		{
			assert (m_pDisplay != 0);
			if (m_pMouseDevice->Setup (m_pDisplay, FALSE))
			{
				m_PointerData.point.x = (m_pDisplay->GetWidth ()+1) / 2;
				m_PointerData.point.y = (m_pDisplay->GetHeight ()+1) / 2;

				m_pMouseDevice->ShowCursor (TRUE);

				m_pMouseDevice->RegisterEventHandler (MouseEventHandler);

				m_pMouseDevice->RegisterRemovedHandler (MouseRemovedHandler);

				assert (m_pIndev != 0);
				SetupCursor (m_pIndev);
			}
		}
	}

	unsigned nTicks = CTimer::Get ()->GetClockTicks ();
	// Adaptive LVGL timer: do not call lv_timer_handler at loop speed.
	static unsigned s_next_wait_ms = 5;
	if (nTicks - m_nLastUpdate >= s_next_wait_ms * (CLOCKHZ/1000))
	{
		unsigned wait_ms = lv_timer_handler ();
		xdos_present_frame();
		if (vc4_redraw_after_failure) {
			vc4_redraw_after_failure = false;
			lv_obj_invalidate(lv_screen_active());
		}
		// XAPP open: CPU stays awake, so service LVGL close to its deadline.
		const unsigned min_wait_ms = xdos_cpu_awake ? 1U : 5U;
		if (wait_ms < min_wait_ms) wait_ms = min_wait_ms;
		if (wait_ms > 20) wait_ms = 20;
		s_next_wait_ms = wait_ms;
		xdos_lvgl_due_us = nTicks + wait_ms * (CLOCKHZ/1000);

		m_nLastUpdate = nTicks;
	}

	if (m_pMouseDevice != 0)
	{
		m_pMouseDevice->UpdateCursor ();
	}
	else if (m_pTouchScreen != 0)
	{
		if (nTicks - m_nLastTouchUpdate >= CLOCKHZ/60)
		{
			m_pTouchScreen->Update ();

			m_nLastTouchUpdate = nTicks;
		}
	}
}

void CLVGL::DisplayFlush (lv_display_t *pDisplay, const lv_area_t *pArea, u8 *pBuffer)
{
	assert (s_pThis != 0);

	assert (pArea != 0);
	CDisplay::TArea Area;
	Area.x1 = pArea->x1;
	Area.x2 = pArea->x2;
	Area.y1 = pArea->y1;
	Area.y2 = pArea->y2;

	assert (Area.x1 <= Area.x2);
	assert (Area.y1 <= Area.y2);
	assert (pBuffer != 0);

	assert (s_pThis->m_pDisplay != 0);
	if (s_pThis->m_pDisplay->GetDepth () == 1)
	{
		pBuffer += 8;		// ignore palette
	}

	assert (pDisplay != 0);
	// LVGL's area is already dirty-clipped; do not repaint the full screen.
	const unsigned w = (unsigned) (pArea->x2 - pArea->x1 + 1);
	const unsigned h = (unsigned) (pArea->y2 - pArea->y1 + 1);
	const unsigned pixels = w * h;
	xdos_p0_dirty_pixels += pixels;
	const bool last = lv_display_flush_is_last(pDisplay);
	if (s_pThis->m_pScreen && s_pThis->m_pDisplay->GetDepth()==16) {
		XdosVc4Timing timing;
		const int result=xdos_vc4_upload(Area.x1,Area.y1,w,h,pBuffer,last,&timing);
		if (result>0) {
			RecordGpuChunk(timing,last);
			if (scrollScreen || cachedContainer) uploadBytes+=timing.uploaded_bytes;
			if (last) {
				ScrollFrameCommitted();
				ContainerFrameCommitted();
				RecordPresentation(true);
			}
			lv_display_flush_ready(pDisplay);
			return;
		}
		if (result<0) {
			xdos_vc4_stop();
			vc4_redraw_after_failure = true;
			CLogger::Get()->Write("VC4 LVGL",LogWarning,"GPU flush failed; switching to VSyncOpt");
		}
	}
#if XDOS_SCROLL_VSYNC
	BeginChunk(Area.x1,Area.x2+1,Area.y1,Area.y2+1);
	Area.y1 += drawY;
	Area.y2 += drawY;
#endif
	// For very small updates DMA setup/IRQ costs more than short CPU copies.
	if (s_pThis->m_pScreen && s_pThis->m_pDisplay->GetDepth () == 16 &&
	    pixels < XDOS_P0_MIN_DMA_PIXELS)
	{
		CBcmFrameBuffer *fb=s_pThis->m_pScreen->GetFrameBuffer ();
		if (fb && fb->GetBuffer ())
		{
			xdos_flush_rgb565 ((void *)(uintptr)fb->GetBuffer (), fb->GetPitch (),
			    16, fb->GetWidth (), fb->GetSize()/fb->GetPitch(), Area.x1, Area.y1,
			    Area.x2, Area.y2, (const u16 *)pBuffer);
			++xdos_p0_cpu_flushes;
			FinishChunk(last);
			lv_display_flush_ready (pDisplay);
			return;
		}
	}
	// With SCREEN_DMA_BURST_LENGTH enabled, Circle's SetArea() starts a
	// 2D DMA and only invokes DisplayFlushComplete after the IRQ completes.
	// LVGL retains this draw buffer until lv_display_flush_ready is called.
	if (s_pThis->m_pScreen && s_pThis->m_pDisplay->GetDepth () == 16)
	{
		s_xdos_dma_start_us = CTimer::GetClockTicks ();
		s_xdos_dma_pending = true;
		++xdos_p0_dma_submitted;
	}
	xdos_last_chunk = last;
#if XDOS_SCROLL_VSYNC
	if (last && pages && drawY!=visibleY) waitingFinalDma = true;
#endif
	s_pThis->m_pDisplay->SetArea (Area, pBuffer, DisplayFlushComplete, pDisplay);
}

void CLVGL::DisplayFlushComplete (void *pParam)
{
	if (s_xdos_dma_pending)
	{
		unsigned elapsed=CTimer::GetClockTicks () - s_xdos_dma_start_us;
		s_xdos_dma_pending=false;
		xdos_p0_dma_total_us+=elapsed;
		if (elapsed>xdos_p0_dma_max_us) xdos_p0_dma_max_us=elapsed;
		++xdos_p0_dma_completed;
		if (measuringScroll) dmaTimes.Add(elapsed,25);
	}
	lv_display_t *pDisplay = (lv_display_t *) pParam;
	assert (pDisplay != 0);
	FinishChunk(xdos_last_chunk);

	lv_display_flush_ready (pDisplay);
}

extern "C" bool xdos_files_wheel(int x, int y, int delta) __attribute__((weak));

void CLVGL::PointerRead (lv_indev_t *pIndev, lv_indev_data_t *pData)
{
	assert (s_pThis != 0);

	pData->state = s_pThis->m_PointerData.state;
	pData->point.x = s_pThis->m_PointerData.point.x;
	pData->point.y = s_pThis->m_PointerData.point.y;
    pData->enc_diff = __atomic_exchange_n(&s_pThis->m_PointerData.enc_diff,0,__ATOMIC_RELAXED);
    if (pData->enc_diff && xdos_files_wheel
        && xdos_files_wheel(pData->point.x, pData->point.y, pData->enc_diff))
        pData->enc_diff = 0;
}

void CLVGL::MouseEventHandler (TMouseEvent Event, unsigned nButtons,
			       unsigned nPosX, unsigned nPosY, int nWheelMove)
{
	assert (s_pThis != 0);

	s_pThis->m_PointerData.point.x = nPosX;
	s_pThis->m_PointerData.point.y = nPosY;

	switch (Event)
	{
	case MouseEventMouseDown:
		if (nButtons & MOUSE_BUTTON_LEFT)
		{
			s_pThis->m_PointerData.state = LV_INDEV_STATE_PRESSED;
		}
		break;

	case MouseEventMouseUp:
		if (nButtons & MOUSE_BUTTON_LEFT)
		{
			s_pThis->m_PointerData.state = LV_INDEV_STATE_RELEASED;
		}
		break;

	case MouseEventMouseWheel:
        // HID wheel reports positive for up. Scale by approximately one row.
        __atomic_fetch_add(&s_pThis->m_PointerData.enc_diff,nWheelMove*24,__ATOMIC_RELAXED);
        break;

	case MouseEventMouseMove:
	default:
		break;
	}
}

void CLVGL::TouchScreenEventHandler (TTouchScreenEvent Event, unsigned nID,
				     unsigned nPosX, unsigned nPosY)
{
	assert (s_pThis != 0);
	assert (s_pThis->m_pDisplay != 0);

	switch (Event)
	{
	case TouchScreenEventFingerDown:
	case TouchScreenEventFingerMove:
		if (   nID == 0
		    && nPosX < s_pThis->m_pDisplay->GetWidth ()
		    && nPosY < s_pThis->m_pDisplay->GetHeight ())
		{
			s_pThis->m_PointerData.state = LV_INDEV_STATE_PRESSED;
			s_pThis->m_PointerData.point.x = nPosX;
			s_pThis->m_PointerData.point.y = nPosY;
		}
		break;

	case TouchScreenEventFingerUp:
		if (nID == 0)
		{
			s_pThis->m_PointerData.state = LV_INDEV_STATE_RELEASED;
		}
		break;

	default:
		break;
	}
}

void CLVGL::LogPrint (lv_log_level_t LogLevel, const char *pMessage)
{
	assert (pMessage != 0);
	size_t nLen = strlen (pMessage);
	assert (nLen > 0);

	char Buffer[nLen+1];
	strcpy (Buffer, pMessage);

	if (Buffer[nLen-1] == '\n')
	{
		Buffer[nLen-1] = '\0';
	}

	TLogSeverity Severity;
	switch (LogLevel)
	{
	case LV_LOG_LEVEL_ERROR:
		Severity = LogError;
		break;

	case LV_LOG_LEVEL_WARN:
		Severity = LogWarning;
		break;

	case LV_LOG_LEVEL_INFO:
	case LV_LOG_LEVEL_USER:
		Severity = LogNotice;
		break;

	default:
		Severity = LogDebug;
		break;
	}

	CLogger::Get ()->Write ("lvgl", Severity, Buffer);
}

void CLVGL::MouseRemovedHandler (CDevice *pDevice, void *pContext)
{
	assert (s_pThis != 0);
	s_pThis->m_pMouseDevice = 0;
}

// XDOS Tune: 1 ms LVGL clock; the 100 Hz periodic tick only gave 10 ms steps.
static uint32_t XdosLvglTickMs (void)
{
	return (uint32_t) (CTimer::GetClockTicks64 () / 1000U);
}

void CLVGL::PeriodicTickHandler (void)
{
	lv_tick_inc (1000 / HZ);
}

#define CURSOR_WIDTH	11
#define CURSOR_HEIGHT	16

static const u8 CursorSymbol[CURSOR_HEIGHT * CURSOR_WIDTH * 4] =
{
#define B	0x00, 0x00, 0x00, 0x00
#define G	0x80, 0x80, 0x80, 0xFF
#define W	0xFF, 0xFF, 0xFF, 0xFF
	G,G,B,B,B,B,B,B,B,B,B,
	G,W,G,B,B,B,B,B,B,B,B,
	G,W,W,G,B,B,B,B,B,B,B,
	G,W,W,W,G,B,B,B,B,B,B,
	G,W,W,W,W,G,B,B,B,B,B,
	G,W,W,W,W,W,G,B,B,B,B,
	G,W,W,W,W,W,W,G,B,B,B,
	G,W,W,W,W,W,W,W,G,B,B,
	G,W,W,W,W,W,W,W,W,G,B,
	G,G,G,G,W,W,G,G,G,G,G,
	B,B,B,G,W,W,W,G,B,B,B,
	B,B,B,B,G,W,W,G,B,B,B,
	B,B,B,B,G,W,W,W,G,B,B,
	B,B,B,B,B,G,W,W,G,B,B,
	B,B,B,B,B,G,W,W,W,G,B,
	B,B,B,B,B,B,G,G,G,G,B,
};

void CLVGL::SetupCursor (lv_indev_t *pIndev)
{
	if (m_pCursorDesc == 0)
	{
		m_pCursorDesc = new lv_image_dsc_t;
		assert (m_pCursorDesc);
		memset (m_pCursorDesc, 0, sizeof *m_pCursorDesc);

		m_pCursorDesc->header.magic = LV_IMAGE_HEADER_MAGIC;
		m_pCursorDesc->header.h = CURSOR_HEIGHT;
		m_pCursorDesc->header.w = CURSOR_WIDTH;
		m_pCursorDesc->header.cf = LV_COLOR_FORMAT_ARGB8888;
		m_pCursorDesc->header.stride = CURSOR_WIDTH * 4;
		m_pCursorDesc->data_size = sizeof CursorSymbol;
		m_pCursorDesc->data = CursorSymbol;

		lv_obj_t *pCursorImage = lv_image_create (lv_screen_active ());
		assert (pCursorImage != 0);
		lv_image_set_src (pCursorImage, m_pCursorDesc);

		assert (pIndev != 0);
		lv_indev_set_cursor (pIndev, pCursorImage);
	}
}
