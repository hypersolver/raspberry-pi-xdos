//
// serial.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014-2024  R. Stange <rsta2@o2online.de>
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
#include <circle/serial.h>
#include <circle/devicenameservice.h>
#include <circle/bcm2835.h>
#include <circle/memio.h>
#include <circle/rp1int.h>
#include <circle/machineinfo.h>
#include <circle/synchronize.h>
#include <assert.h>

#ifndef USE_RPI_STUB_AT

#define GPIOS			2
#define GPIO_TXD		0
#define GPIO_RXD		1

#define VALUES			2
#define VALUE_PIN		0
#define VALUE_ALT		1

// XDOS: on Pi 1-3 (RASPPI <= 3) device 0 is the mini UART (UART1, AUX) on GPIO14/15 (ALT5),
// so the PL011 (UART0) stays free for the on-board Bluetooth controller (GPIO32/33).
// The rest of this file still speaks PL011 registers; MU_READ/MU_WRITE translate them.
// The mini UART baud rate is derived from the VPU core clock (read at Initialize()).
#if RASPPI <= 3
#define XDOS_MINI_UART		1
#else
#define XDOS_MINI_UART		0
#endif

// Registers
#define ARM_UART_DR		(m_nBaseAddress + 0x00)
#define ARM_UART_FR     	(m_nBaseAddress + 0x18)
#define ARM_UART_IBRD   	(m_nBaseAddress + 0x24)
#define ARM_UART_FBRD   	(m_nBaseAddress + 0x28)
#define ARM_UART_LCRH   	(m_nBaseAddress + 0x2C)
#define ARM_UART_CR     	(m_nBaseAddress + 0x30)
#define ARM_UART_IFLS   	(m_nBaseAddress + 0x34)
#define ARM_UART_IMSC   	(m_nBaseAddress + 0x38)
#define ARM_UART_RIS    	(m_nBaseAddress + 0x3C)
#define ARM_UART_MIS    	(m_nBaseAddress + 0x40)
#define ARM_UART_ICR    	(m_nBaseAddress + 0x44)

// Definitions from Raspberry PI Remote Serial Protocol.
//     Copyright 2012 Jamie Iles, jamie@jamieiles.com.
//     Licensed under GPLv2
#define DR_OE_MASK		(1 << 11)
#define DR_BE_MASK		(1 << 10)
#define DR_PE_MASK		(1 << 9)
#define DR_FE_MASK		(1 << 8)

#define FR_TXFE_MASK		(1 << 7)
#define FR_RXFF_MASK		(1 << 6)
#define FR_TXFF_MASK		(1 << 5)
#define FR_RXFE_MASK		(1 << 4)
#define FR_BUSY_MASK		(1 << 3)

#define LCRH_SPS_MASK		(1 << 7)
#define LCRH_WLEN8_MASK		(3 << 5)
#define LCRH_WLEN7_MASK		(2 << 5)
#define LCRH_WLEN6_MASK		(1 << 5)
#define LCRH_WLEN5_MASK		(0 << 5)
#define LCRH_FEN_MASK		(1 << 4)
#define LCRH_STP2_MASK		(1 << 3)
#define LCRH_EPS_MASK		(1 << 2)
#define LCRH_PEN_MASK		(1 << 1)
#define LCRH_BRK_MASK		(1 << 0)

#define CR_CTSEN_MASK		(1 << 15)
#define CR_RTSEN_MASK		(1 << 14)
#define CR_OUT2_MASK		(1 << 13)
#define CR_OUT1_MASK		(1 << 12)
#define CR_RTS_MASK		(1 << 11)
#define CR_DTR_MASK		(1 << 10)
#define CR_RXE_MASK		(1 << 9)
#define CR_TXE_MASK		(1 << 8)
#define CR_LBE_MASK		(1 << 7)
#define CR_UART_EN_MASK		(1 << 0)

#define IFLS_RXIFSEL_SHIFT	3
#define IFLS_RXIFSEL_MASK	(7 << IFLS_RXIFSEL_SHIFT)
#define IFLS_TXIFSEL_SHIFT	0
#define IFLS_TXIFSEL_MASK	(7 << IFLS_TXIFSEL_SHIFT)
	#define IFLS_IFSEL_1_8		0
	#define IFLS_IFSEL_1_4		1
	#define IFLS_IFSEL_1_2		2
	#define IFLS_IFSEL_3_4		3
	#define IFLS_IFSEL_7_8		4

#define INT_OE			(1 << 10)
#define INT_BE			(1 << 9)
#define INT_PE			(1 << 8)
#define INT_FE			(1 << 7)
#define INT_RT			(1 << 6)
#define INT_TX			(1 << 5)
#define INT_RX			(1 << 4)
#define INT_DSRM		(1 << 3)
#define INT_DCDM		(1 << 2)
#define INT_CTSM		(1 << 1)

#if XDOS_MINI_UART
// AUX / mini UART registers (BCM2835 ARM peripherals, section 2.2)
#define AUX_BASE		(ARM_IO_BASE + 0x215000)
#define AUX_ENABLES		(AUX_BASE + 0x04)
#define AUX_MU_IO		(AUX_BASE + 0x40)
#define AUX_MU_IER		(AUX_BASE + 0x44)
#define AUX_MU_IIR		(AUX_BASE + 0x48)
#define AUX_MU_LCR		(AUX_BASE + 0x4C)
#define AUX_MU_MCR		(AUX_BASE + 0x50)
#define AUX_MU_LSR		(AUX_BASE + 0x54)
#define AUX_MU_CNTL		(AUX_BASE + 0x60)
#define AUX_MU_BAUD		(AUX_BASE + 0x68)
#define MU_LSR_DATA_READY	(1 << 0)
#define MU_LSR_OVERRUN		(1 << 1)
#define MU_LSR_TX_SPACE		(1 << 5)	// TX FIFO can accept at least one byte
#define MU_LSR_TX_IDLE		(1 << 6)
#define MU_IER_RX		(1 << 0)
#define MU_IER_TX		(1 << 1)
#define MU_IER_RX_BITS		(MU_IER_RX | (3 << 2))	// bits 3:2 are needed for RX interrupts (errata)

static volatile u32 s_nMuImsc;		// shadow of the emulated PL011 IMSC
static unsigned s_nMuBaudReg, s_nMuClockRate;

// If the VPU core clock moved since Initialize(), reprogram the divisor (the mini UART
// baud rate follows the core clock). Returns the new clock, or 0 when unchanged.
extern "C" unsigned xdos_miniuart_recheck (void)
{
	if (!s_nMuClockRate) return 0;
	const unsigned nClock = CMachineInfo::Get ()->GetClockRate (CLOCK_ID_CORE);
	if (!nClock || nClock == s_nMuClockRate) return 0;
	const unsigned nBaud = s_nMuClockRate / (8 * (s_nMuBaudReg + 1));
	const unsigned nDiv = (nClock + 4 * nBaud) / (8 * nBaud);
	s_nMuBaudReg = nDiv > 0 ? nDiv - 1 : 0;
	s_nMuClockRate = nClock;
	PeripheralEntry ();
	write32 (AUX_MU_BAUD, s_nMuBaudReg);
	PeripheralExit ();
	return nClock;
}

// For the boot log: VPU core clock used for the baud rate, and the real baud rate.
extern "C" void xdos_miniuart_info (unsigned *pClockRate, unsigned *pBaudrate)
{
	*pClockRate = s_nMuClockRate;
	*pBaudrate = s_nMuClockRate / (8 * (s_nMuBaudReg + 1));
}

// PL011 register read, emulated on the mini UART (only the registers this file uses).
static u32 MuRead (uintptr nReg, uintptr nBase)
{
	const uintptr nOffset = nReg - nBase;
	if (nOffset == 0x18)			// FR
	{
		const u32 nLSR = read32 (AUX_MU_LSR);
		u32 nFR = 0;
		if (!(nLSR & MU_LSR_TX_SPACE))		nFR |= FR_TXFF_MASK;
		if (!(nLSR & MU_LSR_DATA_READY))	nFR |= FR_RXFE_MASK;
		if (!(nLSR & MU_LSR_TX_IDLE))		nFR |= FR_BUSY_MASK;
		return nFR;
	}
	if (nOffset == 0x00)			// DR
	{
		const u32 nLSR = read32 (AUX_MU_LSR);
		u32 nDR = read32 (AUX_MU_IO) & 0xFF;
		if (nLSR & MU_LSR_OVERRUN)	nDR |= DR_OE_MASK;
		return nDR;
	}
	if (nOffset == 0x38)			// IMSC
	{
		return s_nMuImsc;
	}
	if (nOffset == 0x2C)			// LCRH: the mini UART is always 8N1
	{
		return LCRH_FEN_MASK | LCRH_WLEN8_MASK;
	}
	if (nOffset == 0x30)			// CR
	{
		return (read32 (AUX_MU_CNTL) & 3) == 3 ? CR_UART_EN_MASK | CR_TXE_MASK | CR_RXE_MASK : 0;
	}
	return 0;				// MIS/RIS: interrupts clear by reading/writing data
}

// PL011 register write, emulated on the mini UART.
static void MuWrite (uintptr nReg, u32 nValue, uintptr nBase)
{
	const uintptr nOffset = nReg - nBase;
	if (nOffset == 0x00)			// DR
	{
		write32 (AUX_MU_IO, nValue & 0xFF);
	}
	else if (nOffset == 0x38)		// IMSC
	{
		s_nMuImsc = nValue;
		u32 nIER = 0;
		if (nValue & (INT_RX | INT_RT))	nIER |= MU_IER_RX_BITS;
		if (nValue & INT_TX)		nIER |= MU_IER_TX;
		write32 (AUX_MU_IER, nIER);
	}
	else if (nOffset == 0x30)		// CR: receiver + transmitter on/off
	{
		write32 (AUX_MU_CNTL, (nValue & CR_UART_EN_MASK) ? 3 : 0);
	}
	// IBRD/FBRD/LCRH/IFLS/ICR: set up by Initialize() or not needed
}

#define MU_READ(reg)		MuRead (reg, m_nBaseAddress)
#define MU_WRITE(reg, val)	MuWrite (reg, val, m_nBaseAddress)
#define ARM_IRQ_SERIAL		ARM_IRQ_AUX
#else
#define MU_READ(reg)		read32 (reg)
#define MU_WRITE(reg, val)	write32 (reg, val)
#define ARM_IRQ_SERIAL		ARM_IRQ_UART
#endif

#define ALT_FUNC(device, gpio)	((TGPIOMode) (  s_GPIOConfig[device][gpio][VALUE_ALT] \
					      + GPIOModeAlternateFunction0))

#define NONE	{10000, 10000}		// UART does not exist
#define NOALT	{GPIO_PINS, 10}		// UART does not need an ALT setting

#if RASPPI <= 4

static uintptr s_BaseAddress[SERIAL_DEVICES] =
{
#if XDOS_MINI_UART
	ARM_IO_BASE + 0x215000,		// AUX (mini UART)
#else
	ARM_IO_BASE + 0x201000,
#endif
#if RASPPI >= 4
	0,
	ARM_IO_BASE + 0x201400,
	ARM_IO_BASE + 0x201600,
	ARM_IO_BASE + 0x201800,
	ARM_IO_BASE + 0x201A00
#endif
};

static unsigned s_GPIOConfig[SERIAL_DEVICES][GPIOS][VALUES] =
{
	// TXD      RXD
#if SERIAL_GPIO_SELECT == 14 && XDOS_MINI_UART
	{{14,  5}, {15,  5}},		// mini UART TXD1/RXD1
#elif SERIAL_GPIO_SELECT == 14
	{{14,  0}, {15,  0}},
#elif SERIAL_GPIO_SELECT == 32
	{{32,  3}, {33,  3}},
#elif SERIAL_GPIO_SELECT == 36
	{{36,  2}, {37,  2}},
#else
	#error SERIAL_GPIO_SELECT must be 14, 32 or 36!
#endif
#if RASPPI >= 4
	{  NONE,     NONE  }, // unused
	{{ 0,  4}, { 1,  4}},
	{{ 4,  4}, { 5,  4}},
	{{ 8,  4}, { 9,  4}},
	{{12,  4}, {13,  4}}
#endif
};

#else	// #if RASPPI <= 4

static uintptr s_BaseAddress[SERIAL_DEVICES] =
{
	0x1F00030000UL,
	0x1F00034000UL,
	0x1F00038000UL,
	0x1F0003C000UL,
	0x1F00040000UL,
	0x1F00044000UL,
	0,
	0,
	0,
	0,
	ARM_IO_BASE + 0x1001000
};

static unsigned s_GPIOConfig[SERIAL_DEVICES][GPIOS][VALUES] =
{
	// TXD      RXD
	{{14,  4}, {15,  4}},
	{{ 0,  2}, { 1,  2}},
	{{ 4,  2}, { 5,  2}},
	{{ 8,  2}, { 9,  2}},
	{{12,  2}, {13,  2}},
	{{36,  1}, {37,  1}},
	{  NONE,     NONE  }, // unused
	{  NONE,     NONE  }, // unused
	{  NONE,     NONE  }, // unused
	{  NONE,     NONE  }, // unused
	{  NOALT,    NOALT }
};

static unsigned s_IRQ[SERIAL_DEVICES] =
{
	RP1_IRQ_UART0,
	RP1_IRQ_UART1,
	RP1_IRQ_UART2,
	RP1_IRQ_UART3,
	RP1_IRQ_UART4,
	RP1_IRQ_UART5,
	0,
	0,
	0,
	0,
	ARM_IRQ_UART
};

#endif

unsigned CSerialDevice::s_nInterruptUseCount = 0;
CInterruptSystem *CSerialDevice::s_pInterruptSystem = 0;
boolean CSerialDevice::s_bUseFIQ = FALSE;
volatile u32 CSerialDevice::s_nInterruptDeviceMask = 0;
CSerialDevice *CSerialDevice::s_pThis[SERIAL_DEVICES] = {0};

CSerialDevice::CSerialDevice (CInterruptSystem *pInterruptSystem, boolean bUseFIQ, unsigned nDevice)
:	m_pInterruptSystem (pInterruptSystem),
#if RASPPI <= 4
	m_bUseFIQ (bUseFIQ),
#else
	m_bUseFIQ (FALSE),		// silently use the IRQ instead
#endif
	m_nDevice (nDevice),
	m_nBaseAddress (0),
	m_bValid (FALSE),
	m_nRxInPtr (0),
	m_nRxOutPtr (0),
	m_nRxStatus (0),
	m_nTxInPtr (0),
	m_nTxOutPtr (0),
	m_nOptions (SERIAL_OPTION_ONLCR),
	m_pCharReceivedHandler (0),
	m_pMagic (0),
	m_SpinLock (bUseFIQ ? FIQ_LEVEL : IRQ_LEVEL)
#ifdef REALTIME
	, m_LineSpinLock (TASK_LEVEL)
#endif
{
#if RASPPI == 5
	s_IRQ[10] =   CMachineInfo::Get ()->GetSoCStepping () >= SoCSteppingD0
		    ? ARM_IRQ_UART_D0 : ARM_IRQ_UART;
#endif

	if (   m_nDevice >= SERIAL_DEVICES
	    || s_GPIOConfig[nDevice][0][VALUE_PIN] > GPIO_PINS
	   )
	{
		return;
	}

	assert (s_pThis[m_nDevice] == 0);
	s_pThis[m_nDevice] = this;

	m_nBaseAddress = s_BaseAddress[nDevice];
	assert (m_nBaseAddress != 0);

#if SERIAL_GPIO_SELECT == 14 && RASPPI <= 4
	if (nDevice == 0)
	{
		// to be sure there is no collision with the Bluetooth controller
		m_GPIO32.AssignPin (32);
		m_GPIO32.SetMode (GPIOModeInput);

		m_GPIO33.AssignPin (33);
		m_GPIO33.SetMode (GPIOModeInput);
	}
#endif

	TGPIOMode GPIOMode = ALT_FUNC (nDevice, GPIO_TXD);
	if (GPIOMode < GPIOModeUnknown)
	{
		m_TxDPin.AssignPin (s_GPIOConfig[nDevice][GPIO_TXD][VALUE_PIN]);
		m_TxDPin.SetMode (GPIOMode);
	}

	GPIOMode = ALT_FUNC (nDevice, GPIO_RXD);
	if (GPIOMode < GPIOModeUnknown)
	{
		m_RxDPin.AssignPin (s_GPIOConfig[nDevice][GPIO_RXD][VALUE_PIN]);
		m_RxDPin.SetMode (GPIOMode);
		m_RxDPin.SetPullMode (GPIOPullModeUp);
	}

	m_bValid = TRUE;
}

CSerialDevice::~CSerialDevice (void)
{
	if (!m_bValid)
	{
		return;
	}

	CDeviceNameService::Get ()->RemoveDevice ("ttyS", m_nDevice+1, FALSE);

	// remove device from interrupt handling
	s_nInterruptDeviceMask &= ~(1 << m_nDevice);
	DataSyncBarrier ();

	PeripheralEntry ();
	MU_WRITE (ARM_UART_IMSC, 0);
	MU_WRITE (ARM_UART_CR, 0);
	PeripheralExit ();

#if RASPPI <= 4
	// disconnect interrupt, if this is the last device, which uses interrupts
	if (   m_pInterruptSystem != 0
	    && --s_nInterruptUseCount == 0)
	{
		assert (s_pInterruptSystem != 0);
		if (!s_bUseFIQ)
		{
			s_pInterruptSystem->DisconnectIRQ (ARM_IRQ_SERIAL);
		}
		else
		{
			s_pInterruptSystem->DisconnectFIQ ();
		}

		s_pInterruptSystem = 0;
		s_bUseFIQ = FALSE;
	}
#else
	if (s_pInterruptSystem != 0)
	{
		s_pInterruptSystem->DisconnectIRQ (s_IRQ[m_nDevice]);

		if (--s_nInterruptUseCount == 0)
		{
			s_pInterruptSystem = 0;
		}
	}
#endif

	TGPIOMode GPIOMode = ALT_FUNC (m_nDevice, GPIO_TXD);
	if (GPIOMode < GPIOModeUnknown)
	{
		m_TxDPin.SetMode (GPIOModeInput);
	}

	GPIOMode = ALT_FUNC (m_nDevice, GPIO_RXD);
	if (GPIOMode < GPIOModeUnknown)
	{
		m_RxDPin.SetMode (GPIOModeInput);
	}

	s_pThis[m_nDevice] = 0;
	m_bValid = FALSE;
}

boolean CSerialDevice::Initialize (unsigned nBaudrate,
				   unsigned nDataBits, unsigned nStopBits, TParity Parity)
{
	if (!m_bValid)
	{
		return FALSE;
	}

	unsigned nClockRate;
#if RASPPI >= 5
	if (m_nDevice < 10)
	{
		nClockRate = 50000000;
	}
	else
#endif
	{
#if XDOS_MINI_UART
		nClockRate = CMachineInfo::Get ()->GetClockRate (CLOCK_ID_CORE);
#else
		nClockRate = CMachineInfo::Get ()->GetClockRate (CLOCK_ID_UART);
#endif
	}
	assert (nClockRate > 0);
#if XDOS_MINI_UART
	// baud = core_clock / (8 * (BAUD + 1)), rounded to the nearest divisor
	const unsigned nMuDiv = (nClockRate + 4 * nBaudrate) / (8 * nBaudrate);
	s_nMuBaudReg = nMuDiv > 0 ? nMuDiv - 1 : 0;
	s_nMuClockRate = nClockRate;
#endif

	assert (300 <= nBaudrate && nBaudrate <= 4000000);
	unsigned nBaud16 = nBaudrate * 16;
	unsigned nIntDiv = nClockRate / nBaud16;
	assert (1 <= nIntDiv && nIntDiv <= 0xFFFF);
	unsigned nFractDiv2 = (nClockRate % nBaud16) * 8 / nBaudrate;
	unsigned nFractDiv = nFractDiv2 / 2 + nFractDiv2 % 2;
	assert (nFractDiv <= 0x3F);

	if (m_pInterruptSystem != 0)		// do we want to use interrupts?
	{
#if RASPPI <= 4
		if (s_nInterruptUseCount > 0)
		{
			// if there is already an interrupt enabled device,
			// interrupt configuration must be the same
			if (   m_pInterruptSystem != s_pInterruptSystem
			    || m_bUseFIQ != s_bUseFIQ)
			{
				s_pThis[m_nDevice] = 0;
				m_bValid = FALSE;

				return FALSE;
			}
		}
		else
		{
			// if we are the first interrupt enabled device,
			// connect interrupt with our configuration
			s_pInterruptSystem = m_pInterruptSystem;
			s_bUseFIQ = m_bUseFIQ;

			if (!s_bUseFIQ)
			{
				s_pInterruptSystem->ConnectIRQ (ARM_IRQ_SERIAL, InterruptStub, 0);
			}
			else
			{
				s_pInterruptSystem->ConnectFIQ (ARM_FIQ_UART, InterruptStub, 0);
			}
		}
#else
		assert (!m_bUseFIQ);

		if (s_nInterruptUseCount > 0)
		{
			if (m_pInterruptSystem != s_pInterruptSystem)
			{
				s_pThis[m_nDevice] = 0;
				m_bValid = FALSE;

				return FALSE;
			}
		}
		else
		{
			s_pInterruptSystem = m_pInterruptSystem;
		}

		s_pInterruptSystem->ConnectIRQ (s_IRQ[m_nDevice], InterruptStub, 0);
#endif

		assert (s_nInterruptUseCount < SERIAL_DEVICES);
		s_nInterruptUseCount++;
	}

	PeripheralEntry ();

#if XDOS_MINI_UART
	write32 (AUX_ENABLES, read32 (AUX_ENABLES) | 1);	// mini UART on (SPI1/2 bits kept)
	write32 (AUX_MU_CNTL, 0);
	write32 (AUX_MU_IER, 0);
	write32 (AUX_MU_LCR, 3);				// 8 data bits (bit 1 needed, errata)
	write32 (AUX_MU_MCR, 0);
	write32 (AUX_MU_IIR, 0xC6);				// clear both FIFOs
	write32 (AUX_MU_BAUD, s_nMuBaudReg);
	assert (nDataBits == 8 && nStopBits == 1 && Parity == ParityNone);
#endif
	MU_WRITE (ARM_UART_IMSC, 0);
	MU_WRITE (ARM_UART_ICR,  0x7FF);
	MU_WRITE (ARM_UART_IBRD, nIntDiv);
	MU_WRITE (ARM_UART_FBRD, nFractDiv);

	// line parameters
	u32 nLCRH = LCRH_FEN_MASK;
	switch (nDataBits)
	{
	case 5:		nLCRH |= LCRH_WLEN5_MASK;	break;
	case 6:		nLCRH |= LCRH_WLEN6_MASK;	break;
	case 7:		nLCRH |= LCRH_WLEN7_MASK;	break;
	case 8:		nLCRH |= LCRH_WLEN8_MASK;	break;

	default:
		assert (0);
		break;
	}

	assert (1 <= nStopBits && nStopBits <= 2);
	if (nStopBits == 2)
	{
		nLCRH |= LCRH_STP2_MASK;
	}

	switch (Parity)
	{
	case ParityNone:
		break;

	case ParityOdd:
		nLCRH |= LCRH_PEN_MASK;
		break;

	case ParityEven:
		nLCRH |= LCRH_PEN_MASK | LCRH_EPS_MASK;
		break;

	case ParitySpace:
		nLCRH |= LCRH_PEN_MASK | LCRH_SPS_MASK | LCRH_EPS_MASK;
		break;

	case ParityMark:
		nLCRH |= LCRH_PEN_MASK | LCRH_SPS_MASK;
		break;

	default:
		assert (0);
		break;
	}

	if (m_pInterruptSystem != 0)
	{
		MU_WRITE (ARM_UART_IFLS,   IFLS_IFSEL_1_4 << IFLS_TXIFSEL_SHIFT
					| IFLS_IFSEL_1_4 << IFLS_RXIFSEL_SHIFT);
		MU_WRITE (ARM_UART_LCRH, nLCRH);
		MU_WRITE (ARM_UART_IMSC, INT_RX | INT_RT | INT_OE);

		// add device to interrupt handling
		s_nInterruptDeviceMask |= 1 << m_nDevice;
		DataSyncBarrier ();
	}
	else
	{
		MU_WRITE (ARM_UART_LCRH, nLCRH);
	}

	MU_WRITE (ARM_UART_CR, CR_UART_EN_MASK | CR_TXE_MASK | CR_RXE_MASK);

	PeripheralExit ();

	CDeviceNameService::Get ()->AddDevice ("ttyS", m_nDevice+1, this, FALSE);

	return TRUE;
}

int CSerialDevice::Write (const void *pBuffer, size_t nCount)
{
	assert (m_bValid);

#ifdef REALTIME
	// cannot write from IRQ_LEVEL to prevent deadlock, just ignore it
	if (CurrentExecutionLevel () > TASK_LEVEL)
	{
		return nCount;
	}
#endif

	m_LineSpinLock.Acquire ();

	u8 *pChar = (u8 *) pBuffer;
	assert (pChar != 0);

	int nResult = 0;

	while (nCount--)
	{
		if (!Write (*pChar))
		{
			break;
		}

		if (*pChar++ == '\n')
		{
			if (m_nOptions & SERIAL_OPTION_ONLCR)
			{
				if (!Write ('\r'))
				{
					break;
				}
			}
		}

		nResult++;
	}

	m_LineSpinLock.Release ();

	if (m_pInterruptSystem != 0)
	{
		m_SpinLock.Acquire ();

		if (m_nTxInPtr != m_nTxOutPtr)
		{
			PeripheralEntry ();

			while (m_nTxInPtr != m_nTxOutPtr)
			{
				if (!(MU_READ (ARM_UART_FR) & FR_TXFF_MASK))
				{
					MU_WRITE (ARM_UART_DR, m_TxBuffer[m_nTxOutPtr++]);
					m_nTxOutPtr &= SERIAL_BUF_MASK;
				}
				else
				{
					MU_WRITE (ARM_UART_IMSC, MU_READ (ARM_UART_IMSC) | INT_TX);

					break;
				}
			}

			PeripheralExit ();
		}

		m_SpinLock.Release ();
	}

	return nResult;
}

int CSerialDevice::Read (void *pBuffer, size_t nCount)
{
	assert (m_bValid);

	u8 *pChar = (u8 *) pBuffer;
	assert (pChar != 0);

	int nResult = 0;

	if (m_pInterruptSystem != 0)
	{
		m_SpinLock.Acquire ();

		if (m_nRxStatus < 0)
		{
			nResult = m_nRxStatus;
			m_nRxStatus = 0;
		}
		else
		{
			while (nCount > 0)
			{
				if (m_nRxInPtr == m_nRxOutPtr)
				{
					break;
				}

				*pChar++ = m_RxBuffer[m_nRxOutPtr++];
				m_nRxOutPtr &= SERIAL_BUF_MASK;

				nCount--;
				nResult++;
			}
		}

		m_SpinLock.Release ();
	}
	else
	{
		PeripheralEntry ();

		while (nCount > 0)
		{
			if (MU_READ (ARM_UART_FR) & FR_RXFE_MASK)
			{
				break;
			}

			u32 nDR = MU_READ (ARM_UART_DR);
			if (nDR & DR_BE_MASK)
			{
				nResult = -SERIAL_ERROR_BREAK;

				break;
			}
			else if (nDR & DR_OE_MASK)
			{
				nResult = -SERIAL_ERROR_OVERRUN;

				break;
			}
			else if (nDR & DR_FE_MASK)
			{
				nResult = -SERIAL_ERROR_FRAMING;

				break;
			}
			else if (nDR & DR_PE_MASK)
			{
				nResult = -SERIAL_ERROR_PARITY;

				break;
			}

			*pChar++ = nDR & 0xFF;

			nCount--;
			nResult++;
		}

		PeripheralExit ();
	}

	return nResult;
}

unsigned CSerialDevice::GetOptions (void) const
{
	return m_nOptions;
}

void CSerialDevice::SetOptions (unsigned nOptions)
{
	m_nOptions = nOptions;
}

void CSerialDevice::SetParity (TParity Parity)
{
	while (IsTransmitting ())
	{
		// just wait
	}

	PeripheralEntry ();

	MU_WRITE (ARM_UART_CR, MU_READ (ARM_UART_CR) & ~CR_UART_EN_MASK);

	while (IsTransmitting ())
	{
		// just wait
	}

	u32 nLCRH = MU_READ (ARM_UART_LCRH);
	nLCRH &= ~(LCRH_PEN_MASK | LCRH_SPS_MASK | LCRH_EPS_MASK);

	switch (Parity)
	{
	case ParityNone:
		break;

	case ParityOdd:
		nLCRH |= LCRH_PEN_MASK;
		break;

	case ParityEven:
		nLCRH |= LCRH_PEN_MASK | LCRH_EPS_MASK;
		break;

	case ParitySpace:
		nLCRH |= LCRH_PEN_MASK | LCRH_SPS_MASK | LCRH_EPS_MASK;
		break;

	case ParityMark:
		nLCRH |= LCRH_PEN_MASK | LCRH_SPS_MASK;
		break;

	default:
		assert (0);
		break;
	}

	MU_WRITE (ARM_UART_LCRH, nLCRH);

	MU_WRITE (ARM_UART_CR, MU_READ (ARM_UART_CR) | CR_UART_EN_MASK);

	PeripheralExit ();
}

boolean CSerialDevice::IsTransmitting (void) const
{
	if (   m_pInterruptSystem != 0
	    && m_nTxInPtr != m_nTxOutPtr)
	{
		return TRUE;
	}

	PeripheralEntry ();

	u32 nFR = MU_READ (ARM_UART_FR);

	PeripheralExit ();

	return !!(nFR & FR_BUSY_MASK);
}

void CSerialDevice::RegisterCharReceivedHandler (TCharReceivedHandler *pHandler, void *pParam)
{
	assert (m_pInterruptSystem != 0);

	m_pParam = pParam;
	m_pCharReceivedHandler = pHandler;
}

void CSerialDevice::RegisterMagicReceivedHandler (const char *pMagic, TMagicReceivedHandler *pHandler)
{
	assert (m_pInterruptSystem != 0);
	assert (m_pMagic == 0);

	assert (pMagic != 0);
	assert (*pMagic != '\0');
	assert (pHandler != 0);

	m_pMagicReceivedHandler = pHandler;

	m_pMagicPtr = pMagic;
	m_pMagic = pMagic;		// enables the scanner
}

unsigned CSerialDevice::AvailableForWrite (void)
{
	assert (m_bValid);
	assert (m_pInterruptSystem != 0);

	m_SpinLock.Acquire ();

	unsigned nResult;
	if (m_nTxOutPtr <= m_nTxInPtr)
	{
		nResult = SERIAL_BUF_SIZE+m_nTxOutPtr-m_nTxInPtr-1;
	}
	else
	{
		nResult = m_nTxOutPtr-m_nTxInPtr-1;
	}

	m_SpinLock.Release ();

	return nResult;
}

unsigned CSerialDevice::AvailableForRead (void)
{
	assert (m_bValid);
	assert (m_pInterruptSystem != 0);

	m_SpinLock.Acquire ();

	unsigned nResult;
	if (m_nRxInPtr < m_nRxOutPtr)
	{
		nResult = SERIAL_BUF_SIZE+m_nRxInPtr-m_nRxOutPtr;
	}
	else
	{
		nResult = m_nRxInPtr-m_nRxOutPtr;
	}

	m_SpinLock.Release ();

	return nResult;
}

int CSerialDevice::Peek (void)
{
	assert (m_bValid);
	assert (m_pInterruptSystem != 0);

	m_SpinLock.Acquire ();

	int nResult = -1;
	if (m_nRxInPtr != m_nRxOutPtr)
	{
		nResult = m_RxBuffer[m_nRxOutPtr];
	}

	m_SpinLock.Release ();

	return nResult;
}

void CSerialDevice::Flush (void)
{
	PeripheralEntry ();

	while (MU_READ (ARM_UART_FR) & FR_BUSY_MASK)
	{
		// just wait
	}

	PeripheralExit ();
}

boolean CSerialDevice::Write (u8 uchChar)
{
	boolean bOK = TRUE;

	if (m_pInterruptSystem != 0)
	{
		m_SpinLock.Acquire ();

		if (((m_nTxInPtr+1) & SERIAL_BUF_MASK) != m_nTxOutPtr)
		{
			m_TxBuffer[m_nTxInPtr++] = uchChar;
			m_nTxInPtr &= SERIAL_BUF_MASK;
		}
		else
		{
			bOK = FALSE;
		}

		m_SpinLock.Release ();
	}
	else
	{
		PeripheralEntry ();

		while (MU_READ (ARM_UART_FR) & FR_TXFF_MASK)
		{
			// do nothing
		}

		MU_WRITE (ARM_UART_DR, uchChar);

		PeripheralExit ();
	}

	return bOK;
}

void CSerialDevice::InterruptHandler (void)
{
	boolean bMagicReceived = FALSE;

	boolean bCharReceived = FALSE;
	u8 uchChar;
	int nStatus;

	m_SpinLock.Acquire ();

	PeripheralEntry ();

	// acknowledge pending interrupts
	MU_WRITE (ARM_UART_ICR, MU_READ (ARM_UART_MIS));

	while (!(MU_READ (ARM_UART_FR) & FR_RXFE_MASK))
	{
		u32 nDR = MU_READ (ARM_UART_DR);
		if (nDR & DR_BE_MASK)
		{
			if (m_nRxStatus == 0)
			{
				m_nRxStatus = -SERIAL_ERROR_BREAK;
			}
		}
		else if (nDR & DR_OE_MASK)
		{
			if (m_nRxStatus == 0)
			{
				m_nRxStatus = -SERIAL_ERROR_OVERRUN;
			}
		}
		else if (nDR & DR_FE_MASK)
		{
			if (m_nRxStatus == 0)
			{
				m_nRxStatus = -SERIAL_ERROR_FRAMING;
			}
		}
		else if (nDR & DR_PE_MASK)
		{
			if (m_nRxStatus == 0)
			{
				m_nRxStatus = -SERIAL_ERROR_PARITY;
			}
		}

		if (m_pMagic != 0)
		{
			if ((char) (nDR & 0xFF) == *m_pMagicPtr)
			{
				if (*++m_pMagicPtr == '\0')
				{
					bMagicReceived = TRUE;
				}
			}
			else
			{
				m_pMagicPtr = m_pMagic;
			}
		}

		if (m_pCharReceivedHandler != 0)
		{
			bCharReceived = TRUE;
			uchChar = nDR & 0xFF;

			nStatus = m_nRxStatus;
			m_nRxStatus = 0;

			break;
		}
		else if (((m_nRxInPtr+1) & SERIAL_BUF_MASK) != m_nRxOutPtr)
		{
			m_RxBuffer[m_nRxInPtr++] = nDR & 0xFF;
			m_nRxInPtr &= SERIAL_BUF_MASK;
		}
		else
		{
			if (m_nRxStatus == 0)
			{
				m_nRxStatus = -SERIAL_ERROR_OVERRUN;
			}
		}
	}

	while (!(MU_READ (ARM_UART_FR) & FR_TXFF_MASK))
	{
		if (m_nTxInPtr != m_nTxOutPtr)
		{
			MU_WRITE (ARM_UART_DR, m_TxBuffer[m_nTxOutPtr++]);
			m_nTxOutPtr &= SERIAL_BUF_MASK;
		}
		else
		{
			MU_WRITE (ARM_UART_IMSC, MU_READ (ARM_UART_IMSC) & ~INT_TX);

			break;
		}
	}

	PeripheralExit ();

	m_SpinLock.Release ();

	if (bMagicReceived)
	{
		(*m_pMagicReceivedHandler) ();
	}

	if (bCharReceived)
	{
		(*m_pCharReceivedHandler) (uchChar, nStatus, m_pParam);
	}
}

void CSerialDevice::InterruptStub (void *pParam)
{
	DataMemBarrier ();
	u32 nDeviceMask = s_nInterruptDeviceMask;

#if RASPPI >= 4
	for (unsigned i = 0; nDeviceMask != 0 && i < SERIAL_DEVICES; nDeviceMask &= ~(1 << i), i++)
	{
		if (nDeviceMask & (1 << i))
		{
			assert (s_pThis[i] != 0);
			s_pThis[i]->InterruptHandler ();
		}
	}
#else
	// only device 0 is supported here
	if (nDeviceMask & 1)
	{
		assert (s_pThis[0] != 0);
		s_pThis[0]->InterruptHandler ();
	}
#endif
}

#else	// #ifndef USE_RPI_STUB_AT

boolean CSerialDevice::Initialize (unsigned nBaudrate)
{
	CDeviceNameService::Get ()->AddDevice ("ttyS1", this, FALSE);

	return TRUE;
}

int CSerialDevice::Write (const void *pBuffer, unsigned nCount)
{
	asm volatile
	(
		"push {r0-r2}\n"
		"mov r0, %0\n"
		"mov r1, %1\n"
		"bkpt #0x7FFB\n"	// send message to GDB client
		"pop {r0-r2}\n"

		: : "r" (pBuffer), "r" (nCount)
	);

	return nCount;
}

#endif
