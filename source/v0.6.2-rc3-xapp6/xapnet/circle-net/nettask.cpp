//
// nettask.h
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2015-2021  R. Stange <rsta2@o2online.de>
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
#include <circle/net/nettask.h>
#include <circle/sched/scheduler.h>
#include <circle/timer.h>
#include <assert.h>

// XDOS adaptive network polling.
//
// Each Process() pass does a blocking LAN7800 bulk-in request (~130 us). With the USB SOF
// interrupt enabled, core 0 wakes 8000x/s, so a fixed 1 ms sleep really gives ~800-1000
// passes/s (~10 % of core 0) even when the network is idle. Poll every 1 ms only while
// frames moved recently, otherwise every XDOS_NET_IDLE_SLEEP_MS.
//   XDOS_NET_POLL_FIXED_MS     define (e.g. 1) to keep the old fixed interval (A/B baseline)
//   XDOS_NET_FAST_WINDOW_US    fast polling lasts this long after the last frame (20000)
//   XDOS_NET_IDLE_SLEEP_MS     interval while idle (10)
#ifndef XDOS_NET_FAST_WINDOW_US
#define XDOS_NET_FAST_WINDOW_US	20000U
#endif
#ifndef XDOS_NET_IDLE_SLEEP_MS
#define XDOS_NET_IDLE_SLEEP_MS	10U
#endif

extern "C"
{
	// Statistics for the XDOS [NET] report (written by the net task on core 0 only).
	volatile unsigned xdos_net_us;		// time spent in Process()
	volatile unsigned xdos_net_n;		// number of Process() passes
	volatile unsigned xdos_net_sleep_ms = 1;	// current polling interval
	extern volatile unsigned xdos_net_fetch_active;
	extern volatile unsigned xdos_net_frames;	// CNetDeviceLayer: frames sent + received
}

CNetTask::CNetTask (CNetSubSystem *pNetSubSystem)
:	m_pNetSubSystem (pNetSubSystem)
{
	SetName ("net");
}

CNetTask::~CNetTask (void)
{
	m_pNetSubSystem = 0;
}

void CNetTask::Run (void)
{
	unsigned nLastFrames = xdos_net_frames;
	unsigned nLastActive = CTimer::GetClockTicks ();

	while (1)
	{
		assert (m_pNetSubSystem != 0);
		const unsigned nStart = CTimer::GetClockTicks ();
		m_pNetSubSystem->Process ();
		const unsigned nEnd = CTimer::GetClockTicks ();
		xdos_net_us += nEnd - nStart;
		xdos_net_n++;

#ifdef XDOS_NET_POLL_FIXED_MS
		(void) nLastFrames;
		(void) nLastActive;
		xdos_net_sleep_ms = XDOS_NET_POLL_FIXED_MS;
#else
		if (xdos_net_frames != nLastFrames)
		{
			nLastFrames = xdos_net_frames;
			nLastActive = nEnd;
		}
		xdos_net_sleep_ms = (xdos_net_fetch_active || nEnd - nLastActive < XDOS_NET_FAST_WINDOW_US) ? 1U : XDOS_NET_IDLE_SLEEP_MS;
#endif

		// This polling task must become non-runnable between passes so
		// Circle can enter WFI when all other tasks are idle.
		CScheduler::Get ()->MsSleep (xdos_net_sleep_ms);
	}
}
