import sys
src=open(sys.argv[1]).read(); out=sys.argv[2]
s=src
def rep(a,b,n=1):
    global s
    assert s.count(a)==n,(a[:70],s.count(a)); s=s.replace(a,b)
rep('''// Registers
#define ARM_UART_DR		(m_nBaseAddress + 0x00)''','''// XDOS: on Pi 1-3 (RASPPI <= 3) device 0 is the mini UART (UART1, AUX) on GPIO14/15 (ALT5),
// so the PL011 (UART0) stays free for the on-board Bluetooth controller (GPIO32/33).
// The rest of this file still speaks PL011 registers; MU_READ/MU_WRITE translate them.
// The mini UART baud rate is derived from the VPU core clock (read at Initialize()).
#if RASPPI <= 3
#define XDOS_MINI_UART		1
#else
#define XDOS_MINI_UART		0
#endif

// Registers
#define ARM_UART_DR		(m_nBaseAddress + 0x00)''')
s=s.replace('read32 (ARM_UART_','MU_READ (ARM_UART_').replace('write32 (ARM_UART_','MU_WRITE (ARM_UART_')
rep('''#define ALT_FUNC(device, gpio)''','''#if XDOS_MINI_UART
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

#define ALT_FUNC(device, gpio)''')
rep('''#if RASPPI <= 4

static uintptr s_BaseAddress[SERIAL_DEVICES] =
{
	ARM_IO_BASE + 0x201000,''','''#if RASPPI <= 4

static uintptr s_BaseAddress[SERIAL_DEVICES] =
{
#if XDOS_MINI_UART
	ARM_IO_BASE + 0x215000,		// AUX (mini UART)
#else
	ARM_IO_BASE + 0x201000,
#endif''')
rep('''#if SERIAL_GPIO_SELECT == 14
	{{14,  0}, {15,  0}},''','''#if SERIAL_GPIO_SELECT == 14 && XDOS_MINI_UART
	{{14,  5}, {15,  5}},		// mini UART TXD1/RXD1
#elif SERIAL_GPIO_SELECT == 14
	{{14,  0}, {15,  0}},''')
# IRQ connect/disconnect on Pi<=4
s=s.replace('s_pInterruptSystem->DisconnectIRQ (ARM_IRQ_UART);','s_pInterruptSystem->DisconnectIRQ (ARM_IRQ_SERIAL);')
s=s.replace('s_pInterruptSystem->ConnectIRQ (ARM_IRQ_UART, InterruptStub, 0);','s_pInterruptSystem->ConnectIRQ (ARM_IRQ_SERIAL, InterruptStub, 0);')
# clock + divisor
rep('''		nClockRate = CMachineInfo::Get ()->GetClockRate (CLOCK_ID_UART);
	}
	assert (nClockRate > 0);''','''#if XDOS_MINI_UART
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
#endif''')
# PL011 divisor asserts would fail for core clock? nIntDiv computed from clock/(16*baud) fine (<=0xFFFF). keep.
rep('''	PeripheralEntry ();

	MU_WRITE (ARM_UART_IMSC, 0);
	MU_WRITE (ARM_UART_ICR,  0x7FF);''','''	PeripheralEntry ();

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
	MU_WRITE (ARM_UART_ICR,  0x7FF);''')
open(out,'w').write(s)
