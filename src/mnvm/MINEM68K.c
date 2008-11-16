/*
	MINEM68K.c

	Copyright (C) 2007 Bernd Schmidt, Paul C. Pratt

	You can redistribute this file and/or modify it under the terms
	of version 2 of the GNU General Public License as published by
	the Free Software Foundation.  You should have received a copy
	of the license along with this file; see the file COPYING.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	license for more details.
*/

/*
	MINimum EMulator of 68K cpu

	This code descends from a simple 68000 emulator that I (Paul C. Pratt)
	wrote long ago. That emulator ran on a 680x0, and used the cpu
	it ran on to do some of the work. This descendent fills
	in those holes with code from the Un*x Amiga Emulator
	by Bernd Schmidt, as found being used in vMac.

	This emulator is about 10 times smaller than the UAE,
	at the cost of being 2 to 3 times slower. It also only
	emulates the 68000, not including the emulation of later
	processors or the FPU.
*/

#ifndef AllFiles
#include "SYSDEPNS.h"
#endif

#include "ENDIANAC.h"
#include "ADDRSPAC.h"

#include "MINEM68K.h"

IMPORTFUNC ui5b MM_Access(ui5b Data, blnr WriteMem, blnr ByteSize, CPTR addr);
IMPORTFUNC ui3b *get_real_address(ui5b L, blnr WritableMem, CPTR addr);
IMPORTPROC customreset(void);

typedef unsigned char flagtype;

#ifndef Use68020
#define Use68020 0
#endif

LOCALVAR struct regstruct
{
	ui5b regs[16]; /* Data and Address registers */
	ui5b pc; /* Program Counter */
	CPTR usp; /* User Stack Pointer */
	CPTR isp; /* Interrupt Stack Pointer */
#if Use68020
	CPTR msp; /* Master Stack Pointer */
#endif

	/* Status Register */
	int intmask; /* bits 10-8 : interrupt priority mask */
	flagtype t1; /* bit 15: Trace mode 1 */
#if Use68020
	flagtype t0; /* bit 14: Trace mode 0 */
#endif
	flagtype s; /* bit 13: Supervisor or user privilege level  */
#if Use68020
	flagtype m; /* bit 12: Master or interrupt mode */
#endif
	flagtype x; /* bit 4: eXtend */
	flagtype n; /* bit 3: Negative */
	flagtype z; /* bit 2: Zero */
	flagtype v; /* bit 1: oVerflow */
	flagtype c; /* bit 0: Carry */

#if Use68020
	ui5b sfc; /* Source Function Code register */
	ui5b dfc; /* Destination Function Code register */
	ui5b vbr; /* Vector Base Register */
	ui5b cacr; /* Cache Control Register */
		/*
			bit 0 : Enable Cache
			bit 1 : Freeze Cache
			bit 2 : Clear Entry In Cache (write only)
			bit 3 : Clear Cache (write only)
		*/
	ui5b caar; /* Cache Address Register */
#endif

	flagtype TracePending;
	flagtype ExternalInterruptPending;
	ui3b **fBankReadAddr;
	ui3b **fBankWritAddr;
	ui3b *fIPL;
#if 0
	flagtype ResetPending;
#endif

	ui3b disp_table[256 * 256];
} regs;

#define m68k_dreg(num) (regs.regs[(num)])
#define m68k_areg(num) (regs.regs[(num) + 8])

LOCALFUNC MayNotInline si5b get_word(CPTR addr)
{
	ui3p ba = regs.fBankReadAddr[bankindex(addr)];

	if (ba != nullpr) {
		ui3p m = (addr & MemBankAddrMask) + ba;
		return (si5b)(si4b)do_get_mem_word(m);
	} else {
		return (si5b)(si4b)(ui4b) MM_Access(0, falseblnr, falseblnr, addr);
	}
}

LOCALFUNC MayNotInline si5b get_byte(CPTR addr)
{
	ui3p ba = regs.fBankReadAddr[bankindex(addr)];

	if (ba != nullpr) {
		ui3p m = (addr & MemBankAddrMask) + ba;
		return (si5b)(si3b)*m;
	} else {
		return (si5b)(si3b)(ui3b) MM_Access(0, falseblnr, trueblnr, addr);
	}
}

LOCALFUNC MayNotInline ui5b get_long(CPTR addr)
{
	ui3p ba = regs.fBankReadAddr[bankindex(addr)];

	if (ba != nullpr) {
		ui3p m = (addr & MemBankAddrMask) + ba;
		return do_get_mem_long(m);
	} else {
		si5b hi = get_word(addr);
		si5b lo = get_word(addr + 2);
		return (ui5b) ((((ui5b)hi) << 16) & 0xFFFF0000)
			| (((ui5b)lo) & 0x0000FFFF);
	}
}

LOCALPROC MayNotInline put_word(CPTR addr, ui5b w)
{
	ui3p ba = regs.fBankWritAddr[bankindex(addr)];

	if (ba != nullpr) {
		ui3p m = (addr & MemBankAddrMask) + ba;
		do_put_mem_word(m, w);
	} else {
		(void) MM_Access(w & 0x0000FFFF, trueblnr, falseblnr, addr);
	}
}

LOCALPROC MayNotInline put_byte(CPTR addr, ui5b b)
{
	ui3p ba = regs.fBankWritAddr[bankindex(addr)];

	if (ba != nullpr) {
		ui3p m = (addr & MemBankAddrMask) + ba;
		*m = b;
	} else {
		(void) MM_Access(b & 0x00FF, trueblnr, trueblnr, addr);
	}
}

LOCALPROC MayNotInline put_long(CPTR addr, ui5b l)
{
	ui3p ba = regs.fBankWritAddr[bankindex(addr)];

	if (ba != nullpr) {
		ui3p m = (addr & MemBankAddrMask) + ba;
		do_put_mem_long(m, l);
	} else {
		put_word(addr, l >> 16);
		put_word(addr + 2, l);
	}
}

LOCALFUNC ui3p get_pc_real_address(CPTR addr)
{
	ui3p ba = regs.fBankReadAddr[bankindex(addr)];

	if (ba != nullpr) {
		return (addr & MemBankAddrMask) + ba;
	} else {
		ba = get_real_address(2, falseblnr, addr);
		if (ba != nullpr) {
			return ba;
		} else {
			/* in trouble if get here */
			return regs.fBankReadAddr[0];
		}
	}
}

#define ZFLG regs.z
#define NFLG regs.n
#define CFLG regs.c
#define VFLG regs.v
#define XFLG regs.x

#define LOCALPROCUSEDONCE LOCALFUNC MayInline void

LOCALFUNC ui4b m68k_getSR(void)
{
	return (regs.t1 << 15)
#if Use68020
			| (regs.t0 << 14)
#endif
			| (regs.s << 13)
#if Use68020
			| (regs.m << 12)
#endif
			| (regs.intmask << 8)
			| (XFLG << 4) | (NFLG << 3) | (ZFLG << 2) | (VFLG << 1)
			|  CFLG;
}

FORWARDPROC NeedToGetOut(void);

LOCALFUNC MayInline void m68k_setCR(ui4b newcr)
{
	XFLG = (newcr >> 4) & 1;
	NFLG = (newcr >> 3) & 1;
	ZFLG = (newcr >> 2) & 1;
	VFLG = (newcr >> 1) & 1;
	CFLG = newcr & 1;
}

FORWARDPROC SetExternalInterruptPending(void);

LOCALPROC m68k_setSR(ui4b newsr)
{
	CPTR *pnewstk;
	CPTR *poldstk = regs.s ? (
#if Use68020
		regs.m ? &regs.msp :
#endif
		&regs.isp) : &regs.usp;
	int oldintmask = regs.intmask;

	m68k_setCR(newsr);
	regs.t1 = (newsr >> 15) & 1;
#if Use68020
	regs.t0 = (newsr >> 14) & 1;
	if (regs.t0) {
		ReportAbnormal("t0 flag set in m68k_setSR");
	}
#endif
	regs.s = (newsr >> 13) & 1;
#if Use68020
	regs.m = (newsr >> 12) & 1;
	if (regs.m) {
		ReportAbnormal("m flag set in m68k_setSR");
	}
#endif
	regs.intmask = (newsr >> 8) & 7;

	pnewstk = regs.s ? (
#if Use68020
		regs.m ? &regs.msp :
#endif
		&regs.isp) : &regs.usp;

	if (poldstk != pnewstk) {
		*poldstk = m68k_areg(7);
		m68k_areg(7) = *pnewstk;
	}

	if (regs.intmask != oldintmask) {
		SetExternalInterruptPending();
	}

	if (regs.t1) {
		NeedToGetOut();
	} else {
		/* regs.TracePending = falseblnr; */
	}
}

/*
	This variable was introduced because a program could do a Bcc from
	whithin chip memory to a location whitin expansion memory. With a
	pointer variable the program counter would point to the wrong location.
	With this variable unset the program counter is always correct, but
	programs will run slower (about 4%).
	Usually, you'll want to have this defined.

	vMac REQUIRES this. It allows for fun things like Restart.
*/

#ifndef USE_POINTER
#define USE_POINTER 1
#endif

#if USE_POINTER
LOCALVAR ui3p pc_p;
LOCALVAR ui3p pc_oldp;
#endif


LOCALFUNC MayInline ui5b nextibyte(void)
{
#if USE_POINTER
	ui5b r = do_get_mem_byte(pc_p + 1);
	pc_p += 2;
	return r;
#else
	ui5b r = (ui5b)get_byte(regs.pc + 1);
	regs.pc += 2;
	return r & 0x00FF;
#endif
}

LOCALFUNC MayInline ui5b nextiword(void)
{
#if USE_POINTER
	ui5b r = do_get_mem_word(pc_p);
	pc_p += 2;
	return r;
#else
	ui5b r = (ui5b)get_word(regs.pc);
	regs.pc += 2;
	return r & 0x0000FFFF;
#endif
}

LOCALFUNC MayInline ui5b nextilong(void)
{
#if USE_POINTER
	ui5b r = do_get_mem_long(pc_p);
	pc_p += 4;
#else
	ui5b r = get_long(regs.pc);
	regs.pc += 4;
#endif
	return r;
}

LOCALFUNC MayInline void BackupPC(void)
{
#if USE_POINTER
	pc_p -= 2;
#else
	regs.pc -= 2;
#endif
}

LOCALFUNC MayInline void SkipiWord(void)
{
#if USE_POINTER
	pc_p += 2;
#else
	regs.pc += 2;
#endif
}

#if Use68020
LOCALPROC MayInline void SkipiLong(void)
{
#if USE_POINTER
	pc_p += 4;
#else
	regs.pc += 4;
#endif
}
#endif

#define MakeDumpFile 0

#if MakeDumpFile
IMPORTPROC DumpAJump(CPTR fromaddr, CPTR toaddr);

LOCALPROC DumpAJump2(CPTR toaddr)
{
	CPTR fromaddr = regs.pc + (pc_p - pc_oldp);

	if ((toaddr > fromaddr) || (toaddr < regs.pc)) {
		if ((fromaddr >= 0x00400000) && (fromaddr < 0x00500000)) {
			fromaddr = fromaddr - 0x00400000 + 10000000;
		}
		if ((toaddr >= 0x00400000) && (toaddr < 0x00500000)) {
			toaddr = toaddr - 0x00400000 + 10000000;
		}
		DumpAJump(fromaddr, toaddr);
	}
}

#endif

#if USE_POINTER

LOCALFUNC MayInline void m68k_setpc(CPTR newpc)
{
#if MakeDumpFile && 0
	DumpAJump2(newpc);
#endif
#if 0
	if (newpc == 0xBD50 /* 401AB4 */) {
		/* Debugger(); */
		Exception(5); /* try and get macsbug */
	}
#endif
	pc_p = pc_oldp = get_pc_real_address(newpc);
	regs.pc = newpc;
}

LOCALFUNC MayInline CPTR m68k_getpc(void)
{
	return regs.pc + (pc_p - pc_oldp);
}

#else

LOCALFUNC MayInline void m68k_setpc(CPTR newpc)
{
/* regs.pc = newpc; */
/* bill mod */
	regs.pc = newpc & 0x00FFFFFF;
}

LOCALFUNC MayInline CPTR m68k_getpc(void)
{
	return regs.pc;
}
#endif

#ifndef FastRelativeJump
#define FastRelativeJump (1 && USE_POINTER)
#endif

LOCALPROC ExceptionTo(CPTR newpc
#if Use68020
	, int nr
#endif
	)
{
	ui4b saveSR = m68k_getSR();

	if (! regs.s) {
		regs.usp = m68k_areg(7);
		m68k_areg(7) =
#if Use68020
			regs.m ? regs.msp :
#endif
			regs.isp;
		regs.s = 1;
	}
#if Use68020
	switch (nr) {
		case 5: /* Zero Divide */
		case 6: /* CHK, CHK2 */
		case 7: /* cpTRAPcc, TRAPCcc, TRAPv */
		case 9: /* Trace */
			m68k_areg(7) -= 4;
			put_long(m68k_areg(7), m68k_getpc());
			m68k_areg(7) -= 2;
			put_word(m68k_areg(7), 0x2000 + nr * 4);
			break;
		default:
			m68k_areg(7) -= 2;
			put_word(m68k_areg(7), nr * 4);
			break;
	}
	/* if regs.m should make throw away stack frame */
#endif
	m68k_areg(7) -= 4;
	put_long(m68k_areg(7), m68k_getpc());
	m68k_areg(7) -= 2;
	put_word(m68k_areg(7), saveSR);
	m68k_setpc(newpc);
	regs.t1 = 0;
#if Use68020
	regs.t0 = 0;
	regs.m = 0;
#endif
	regs.TracePending = falseblnr;
}

LOCALPROC Exception(int nr)
{
	ExceptionTo(get_long(4 * nr
#if Use68020
		+ regs.vbr
#endif
		)
#if Use68020
		, nr
#endif
		);
}

GLOBALPROC DiskInsertedPsuedoException(CPTR newpc, ui5b data)
{
	ExceptionTo(newpc
#if Use68020
		, 0
#endif
		);
	m68k_areg(7) -= 4;
	put_long(m68k_areg(7), data);
}

LOCALPROC DoCheckExternalInterruptPending(void)
{
	int level = *regs.fIPL;
	if ((level > regs.intmask) || (level == 7)) {
		Exception(24 + level);
		regs.intmask = level;
	}
}

GLOBALPROC m68k_IPLchangeNtfy(void)
{
	int level = *regs.fIPL;
	if ((level > regs.intmask) || (level == 7)) {
		SetExternalInterruptPending();
	}
}

#if MakeDumpFile
FORWARDPROC InitDumpTable(void);
#endif

GLOBALPROC m68k_reset(void)
{
#if MakeDumpFile
	InitDumpTable();
#endif
#if 0
	regs.ResetPending = trueblnr;
	NeedToGetOut();
#else
/* Sets the MC68000 reset jump vector... */
	m68k_setpc(get_long(0x00000004));

/* Sets the initial stack vector... */
	m68k_areg(7) = get_long(0x00000000);

	regs.s = 1;
#if Use68020
	regs.m = 0;
	regs.t0 = 0;
#endif
	regs.t1 = 0;
	ZFLG = CFLG = NFLG = VFLG = 0;
	regs.ExternalInterruptPending = falseblnr;
	regs.TracePending = falseblnr;
	regs.intmask = 7;

#if Use68020
	regs.sfc = 0;
	regs.dfc = 0;
	regs.vbr = 0;
	regs.cacr = 0;
	regs.caar = 0;
#endif
#endif
}

LOCALPROC SetUp_disp_table(void);

GLOBALPROC MINEM68K_Init(ui3b **BankReadAddr, ui3b **BankWritAddr,
	ui3b *fIPL)
{
	regs.fBankWritAddr = BankWritAddr;
	regs.fBankReadAddr = BankReadAddr;
	regs.fIPL = fIPL;

	SetUp_disp_table();
}

LOCALFUNC MayInline ui5b get_disp_ea(ui5b base)
{
	ui4b dp = nextiword();
	int reg = (dp >> 12) & 0x0F;
	si5b regd = regs.regs[reg];
	if ((dp & 0x0800) == 0) {
		regd = (si5b)(si4b)regd;
	}
#if Use68020
	regd <<= (dp >> 9) & 3;
#if ExtraAbormalReports
	if (((dp >> 9) & 3) != 0) {
		/* ReportAbnormal("Have scale in Extension Word"); */
		/* apparently can happen in Sys 7.5.5 boot on 68000 */
	}
#endif
	if (dp & 0x0100) {
		if ((dp & 0x80) != 0) {
			base = 0;
			/* ReportAbnormal("Extension Word: suppress base"); */
			/* used by Sys 7.5.5 boot */
		}
		if ((dp & 0x40) != 0) {
			regd = 0;
			ReportAbnormal("Extension Word: suppress regd");
		}

		switch ((dp >> 4) & 0x03) {
			case 0:
				/* reserved */
				ReportAbnormal("Extension Word: dp reserved");
				break;
			case 1:
				/* no displacement */
				/* ReportAbnormal("Extension Word: no displacement"); */
				/* used by Sys 7.5.5 boot */
				break;
			case 2:
				base += (si5b)(si4b)nextiword();
				/* ReportAbnormal("Extension Word: word displacement"); */
				/* used by Sys 7.5.5 boot */
				break;
			case 3:
				base += nextilong();
				ReportAbnormal("Extension Word: long displacement");
				break;
		}

		if ((dp & 0x03) == 0) {
			base += regd;
			if ((dp & 0x04) != 0) {
				ReportAbnormal("Extension Word: reserved dp form");
			}
			/* ReportAbnormal("Extension Word: noindex"); */
			/* used by Sys 7.5.5 boot */
		} else {
			if ((dp & 0x04) != 0) {
				base = get_long(base);
				base += regd;
				/* ReportAbnormal("Extension Word: postindex"); */
				/* used by Sys 7.5.5 boot */
			} else {
				base += regd;
				base = get_long(base);
				/* ReportAbnormal("Extension Word: preindex"); */
				/* used by Sys 7.5.5 boot */
			}
			switch (dp & 0x03) {
				case 1:
					/* null outer displacement */
					/* ReportAbnormal("Extension Word: null outer displacement"); */
					/* used by Sys 7.5.5 boot */
					break;
				case 2:
					base += (si5b)(si4b)nextiword();
					ReportAbnormal("Extension Word: word outer displacement");
					break;
				case 3:
					base += nextilong();
					ReportAbnormal("Extension Word: long outer displacement");
					break;
			}
		}

		return base;
	} else
#endif
	{
		return base + (si3b)(dp) + regd;
	}
}

LOCALVAR ui5b opsize;

LOCALPROC op_illg(void);

#define AKMemory 0
#define AKRegister 1
#define AKConstant 2

union ArgAddrT {
	ui5b mem;
	ui5b *rga;
};
typedef union ArgAddrT ArgAddrT;

LOCALVAR ui5b ArgKind;
LOCALVAR ArgAddrT ArgAddr;

LOCALPROC SetArgKindReg(ui5b thereg)
{
	ArgKind = AKRegister;
	ArgAddr.rga = &regs.regs[thereg];
}

LOCALPROC MayNotInline DecodeModeRegister(ui5b themode, ui5b thereg)
{
	switch (themode) {
		case 0 :
			SetArgKindReg(thereg);
			break;
		case 1 :
			SetArgKindReg(thereg + 8);
			break;
		case 2 :
			ArgKind = AKMemory;
			ArgAddr.mem = m68k_areg(thereg);
			break;
		case 3 :
			ArgKind = AKMemory;
			ArgAddr.mem = m68k_areg(thereg);
			if ((thereg == 7) && (opsize == 1)) {
				m68k_areg(thereg) += 2;
			} else {
				m68k_areg(thereg) += opsize;
			}
			break;
		case 4 :
			ArgKind = AKMemory;
			if ((thereg == 7) && (opsize == 1)) {
				m68k_areg(thereg) -= 2;
			} else {
				m68k_areg(thereg) -= opsize;
			}
			ArgAddr.mem = m68k_areg(thereg);
			break;
		case 5 :
			ArgKind = AKMemory;
			ArgAddr.mem = m68k_areg(thereg) + (si5b)(si4b)nextiword();
			break;
		case 6 :
			ArgKind = AKMemory;
			ArgAddr.mem = get_disp_ea(m68k_areg(thereg));
			break;
		case 7 :
			switch (thereg) {
				case 0 :
					ArgKind = AKMemory;
					ArgAddr.mem = (si5b)(si4b)nextiword();
					break;
				case 1 :
					ArgKind = AKMemory;
					ArgAddr.mem = nextilong();
					break;
				case 2 :
					ArgKind = AKMemory;
					ArgAddr.mem = m68k_getpc();
					ArgAddr.mem += (si5b)(si4b)nextiword();
					break;
				case 3 :
					ArgKind = AKMemory;
					ArgAddr.mem = get_disp_ea(m68k_getpc());
					break;
				case 4 :
					ArgKind = AKConstant;
					if (opsize == 2) {
						ArgAddr.mem = (si5b)(si4b)nextiword();
					} else if (opsize < 2) {
						ArgAddr.mem = (si5b)(si3b)nextibyte();
					} else {
						ArgAddr.mem = nextilong();
					}
					break;
			}
			break;
		case 8 :
			ArgKind = AKConstant;
			ArgAddr.mem = thereg;
			break;
	}
}

LOCALFUNC si5b GetArgValue(void)
{
	si5b v;

	switch (ArgKind) {
		case AKMemory:
			if (opsize == 2) {
				v = get_word(ArgAddr.mem);
			} else if (opsize < 2) {
				v = get_byte(ArgAddr.mem);
			} else {
				v = get_long(ArgAddr.mem);
			}
			break;
		case AKRegister:
			v = *ArgAddr.rga;
			if (opsize == 2) {
				v = (si5b)(si4b)v;
			} else if (opsize < 2) {
				v = (si5b)(si3b)v;
			}
			break;
		case AKConstant:
		default: /* for compiler. shouldn't be any other cases */
			v = ArgAddr.mem;
			break;
	}
	return v;
}

LOCALPROC SetArgValue(si5b v)
{
	if (ArgKind == AKRegister) {
		if (opsize == 2) {
			*ArgAddr.rga = (*ArgAddr.rga & ~ 0xffff) | ((v) & 0xffff);
		} else if (opsize < 2) {
			*ArgAddr.rga = (*ArgAddr.rga & ~ 0xff) | ((v) & 0xff);
		} else {
			*ArgAddr.rga = v;
		}
	} else {
		/* must be AKMemory */
		/* should not get here for AKConstant */
		if (opsize == 2) {
			put_word(ArgAddr.mem, v);
		} else if (opsize < 2) {
			put_byte(ArgAddr.mem, v);
		} else {
			put_long(ArgAddr.mem, v);
		}
	}
}

#define extendopsizedstvalue() \
	if (opsize == 2) {\
		dstvalue = (si4b)dstvalue;\
	} else if (opsize < 2) {\
		dstvalue = (si3b)dstvalue;\
	}

#define unextendopsizedstvalue() \
	if (opsize == 2) {\
		dstvalue = (ui4b)dstvalue;\
	} else if (opsize < 2) {\
		dstvalue = (ui3b)dstvalue;\
	}

LOCALVAR ui5b opcode;

#define b76 ((opcode >> 6) & 3)
#define b8 ((opcode >> 8) & 1)
#define mode ((opcode >> 3) & 7)
#define reg (opcode & 7)
#define md6 ((opcode >> 6) & 7)
#define rg9 ((opcode >> 9) & 7)

LOCALPROC FindOpSizeFromb76(void)
{
	opsize = 1 << b76;
#if 0
	switch (b76) {
		case 0 :
			opsize = 1;
			break;
		case 1 :
			opsize = 2;
			break;
		case 2 :
			opsize = 4;
			break;
	}
#endif
}

LOCALFUNC ui5b octdat(ui5b x)
{
	if (x == 0) {
		return 8;
	} else {
		return x;
	}
}

LOCALFUNC si5b DecodeDEa_xxxxdddxssmmmrrr(void)
{
	si5b srcvalue;
	FindOpSizeFromb76();
	srcvalue = regs.regs[rg9];
	if (opsize == 2) {
		srcvalue = (si5b)(si4b)srcvalue;
	} else if (opsize < 2) {
		srcvalue = (si5b)(si3b)srcvalue;
	}
	DecodeModeRegister(mode, reg);

	return srcvalue;
}

LOCALFUNC si5b DecodeEaD_xxxxdddxssmmmrrr(void)
{
	si5b srcvalue;
	FindOpSizeFromb76();
	DecodeModeRegister(mode, reg);
	srcvalue = GetArgValue();
	SetArgKindReg(rg9);

	return srcvalue;
}

LOCALFUNC si5b DecodeI_xxxxxxxxssmmmrrr(void)
{
	si5b srcvalue;
	FindOpSizeFromb76();
	if (opsize == 2) {
		srcvalue = (si5b)(si4b)nextiword();
	} else if (opsize < 2) {
		srcvalue = (si5b)(si3b)nextibyte();
	} else {
		srcvalue = nextilong();
	}
	DecodeModeRegister(mode, reg);

	return srcvalue;
}

LOCALFUNC si5b DecodeDD_xxxxdddxssxxxrrr(void)
{
	si5b srcvalue;
	FindOpSizeFromb76();
	SetArgKindReg(reg);
	srcvalue = GetArgValue();
	SetArgKindReg(rg9);

	return srcvalue;
}

LOCALFUNC si5b DecodeAAs_xxxxdddxssxxxrrr(void)
{
	si5b srcvalue;
	FindOpSizeFromb76();
	DecodeModeRegister(4, reg);
	srcvalue = GetArgValue();
	DecodeModeRegister(4, rg9);

	return srcvalue;
}

LOCALFUNC si5b Decode_xxxxnnnxssmmmrrr(void)
{
	FindOpSizeFromb76();
	DecodeModeRegister(mode, reg);
	return octdat(rg9);
}

LOCALFUNC si5b DecodesAA_xxxxdddxssxxxrrr(void)
{
	si5b srcvalue;
	FindOpSizeFromb76();
	DecodeModeRegister(3, reg);
	srcvalue = GetArgValue();
	DecodeModeRegister(3, rg9);

	return srcvalue;
}

LOCALPROCUSEDONCE DoCodeTst(void)
{
	/* Tst  01001010ssmmmrrr */
	si5b srcvalue;

	FindOpSizeFromb76();
	DecodeModeRegister(mode, reg);
	srcvalue = GetArgValue();

	VFLG = CFLG = 0;
	ZFLG = (srcvalue == 0);
	NFLG = (srcvalue < 0);
}

LOCALPROC DoCompare(si5b srcvalue)
{
	si5b dstvalue = GetArgValue();
	int flgs = (srcvalue < 0);
	int flgo = (dstvalue < 0);
	dstvalue -= srcvalue;
	extendopsizedstvalue();
	ZFLG = (dstvalue == 0);
	NFLG = (dstvalue < 0);
	VFLG = (flgs != flgo) && (NFLG != flgo);
	CFLG = (flgs && ! flgo) || (NFLG && ((! flgo) || flgs));
}

LOCALPROCUSEDONCE DoCodeCmp(void)
{
	/* Cmp 1011ddd0ssmmmrrr */
	DoCompare(DecodeEaD_xxxxdddxssmmmrrr());
}

LOCALPROCUSEDONCE DoCodeCmpI(void)
{
	/* CMPI 00001100ssmmmrrr */
	DoCompare(DecodeI_xxxxxxxxssmmmrrr());
}

LOCALPROCUSEDONCE DoCodeCmpM(void)
{
	/* CmpM 1011ddd1ss001rrr */
	DoCompare(DecodesAA_xxxxdddxssxxxrrr());
}

LOCALFUNC MayInline blnr cctrue(void)
{
	switch ((opcode >> 8) & 15) {
		case 0:  return trueblnr;                   /* T */
		case 1:  return falseblnr;                  /* F */
		case 2:  return (! CFLG) && (! ZFLG);       /* HI */
		case 3:  return CFLG || ZFLG;               /* LS */
		case 4:  return ! CFLG;                     /* CC */
		case 5:  return CFLG;                       /* CS */
		case 6:  return ! ZFLG;                     /* NE */
		case 7:  return ZFLG;                       /* EQ */
		case 8:  return ! VFLG;                     /* VC */
		case 9:  return VFLG;                       /* VS */
		case 10: return ! NFLG;                     /* PL */
		case 11: return NFLG;                       /* MI */
		case 12: return NFLG == VFLG;               /* GE */
		case 13: return NFLG != VFLG;               /* LT */
		case 14: return (! ZFLG) && (NFLG == VFLG); /* GT */
		case 15: return ZFLG || (NFLG != VFLG);     /* LE */
		default: return falseblnr; /* shouldn't get here */
	}
}

LOCALPROCUSEDONCE DoCodeBra(void)
{
	ui5b src = ((ui5b)opcode) & 255;
#if FastRelativeJump
	ui3p s = pc_p;
#else
	ui5b s = m68k_getpc();
#endif

	if (src == 0) {
		s += (si5b)(si4b)nextiword();
	} else
#if Use68020
	if (src == 255) {
		s += (si5b)nextilong();
		/* ReportAbnormal("long branch in DoCode6"); */
		/* Used by various Apps */
	} else
#endif
	{
		s += (si5b)(si3b)src;
	}

	/* Bra 0110ccccnnnnnnnn */
#if FastRelativeJump
	pc_p = s;
#else
	m68k_setpc(s);
#endif
}

LOCALPROCUSEDONCE DoCodeBraSkip(void)
{
	ui5b src = ((ui5b)opcode) & 255;

	if (src == 0) {
		SkipiWord();
	} else
#if Use68020
	if (src == 255) {
		SkipiLong();
		/* ReportAbnormal("long branch in DoCode6"); */
		/* Used by various Apps */
	} else
#endif
	{
	}
}

LOCALPROCUSEDONCE DoCodeBcc(void)
{
	/* Bcc 0110ccccnnnnnnnn */
	if (cctrue()) {
		DoCodeBra();
	} else {
		DoCodeBraSkip();
	}
}

LOCALPROCUSEDONCE DoCodeDBcc(void)
{
	/* DBcc 0101cccc11001ddd */

	si5b dstvalue;
#if FastRelativeJump
	ui3p srcvalue = pc_p;
#else
	si5b srcvalue = m68k_getpc();
#endif

	srcvalue += (si5b)(si4b)nextiword();
	if (! cctrue()) {
		dstvalue = (si5b)(si4b)m68k_dreg(reg);
		--dstvalue;
		m68k_dreg(reg) = (m68k_dreg(reg) & ~ 0xffff)
			| ((dstvalue) & 0xffff);
		if (dstvalue != -1) {
#if FastRelativeJump
			pc_p = srcvalue;
#else
			m68k_setpc(srcvalue);
#endif
		}
	}
}

LOCALPROCUSEDONCE DoCodeSwap(void)
{
	/* Swap 0100100001000rrr */
	ui5b srcreg = reg;
	ui5b src = (ui5b)m68k_dreg(srcreg);
	si5b dst = (si5b)(((src >> 16) & 0xFFFF) | ((src&0xFFFF) << 16));
	VFLG = CFLG = 0;
	ZFLG = (dst == 0);
	NFLG = (dst < 0);
	m68k_dreg(srcreg) = dst;
}

LOCALPROC DoMove(void) /* MOVE */
{
	si5b src;

	DecodeModeRegister(mode, reg);
	src = GetArgValue();
	DecodeModeRegister(md6, rg9);
	VFLG = CFLG = 0;
	ZFLG = (src == 0);
	NFLG = (src < 0);
	SetArgValue(src);
}

LOCALPROCUSEDONCE DoCodeMoveL(void)
{
	opsize = 4;
	DoMove();
}

LOCALPROCUSEDONCE DoCodeMoveW(void)
{
	opsize = 2;
	DoMove();
}

LOCALPROCUSEDONCE DoCodeMoveB(void)
{
	opsize = 1;
	DoMove();
}

LOCALPROC DoMoveA(void) /* MOVE */
{
	si5b src;

	DecodeModeRegister(mode, reg);
	src = GetArgValue();
	m68k_areg(rg9) = src;
}

LOCALPROCUSEDONCE DoCodeMoveAL(void)
{
	opsize = 4;
	DoMoveA();
}

LOCALPROCUSEDONCE DoCodeMoveAW(void)
{
	opsize = 2;
	DoMoveA();
}

LOCALPROCUSEDONCE DoCodeMoveQ(void)
{
	/* MoveQ 0111ddd0nnnnnnnn */
	ui5b src = (si5b)(si3b)(opcode & 255);
	ui5b dstreg = rg9;
	VFLG = CFLG = 0;
	ZFLG = ((si5b)(src)) == 0;
	NFLG = ((si5b)(src)) < 0;
	m68k_dreg(dstreg) = (src);
}

LOCALPROC DoBinOpAdd(si5b srcvalue)
{
	si5b dstvalue;

	dstvalue = GetArgValue();
	{
		int flgs = (srcvalue < 0);
		int flgo = (dstvalue < 0);
		dstvalue = (dstvalue + srcvalue);
		extendopsizedstvalue();
		ZFLG = (dstvalue == 0);
		NFLG = (dstvalue < 0);
		VFLG = (flgs && flgo && ! NFLG) || ((! flgs) && (! flgo) && NFLG);
		XFLG = CFLG = (flgs && flgo) || ((! NFLG) && (flgo || flgs));
	}
	SetArgValue(dstvalue);
}

LOCALPROCUSEDONCE DoCodeAddEaR(void)
{
	DoBinOpAdd(DecodeEaD_xxxxdddxssmmmrrr());
}

LOCALPROCUSEDONCE DoCodeAddQ(void)
{
	/* AddQ 0101nnn0ssmmmrrr */
	DoBinOpAdd(Decode_xxxxnnnxssmmmrrr());
}

LOCALPROCUSEDONCE DoCodeAddI(void)
{
	DoBinOpAdd(DecodeI_xxxxxxxxssmmmrrr());
}

LOCALPROCUSEDONCE DoCodeAddREa(void)
{
	DoBinOpAdd(DecodeDEa_xxxxdddxssmmmrrr());
}

LOCALPROC DoBinOpSub(si5b srcvalue)
{
	si5b dstvalue;

	dstvalue = GetArgValue();
	{
		int flgs = (srcvalue < 0);
		int flgo = (dstvalue < 0);
		dstvalue = dstvalue - srcvalue;
		extendopsizedstvalue();
		ZFLG = (dstvalue == 0);
		NFLG = (dstvalue < 0);
		VFLG = (flgs != flgo) && (NFLG != flgo);
		XFLG = CFLG = (flgs && ! flgo) || (NFLG && ((! flgo) || flgs));
	}
	SetArgValue(dstvalue);
}

LOCALPROCUSEDONCE DoCodeSubEaR(void)
{
	DoBinOpSub(DecodeEaD_xxxxdddxssmmmrrr());
}

LOCALPROCUSEDONCE DoCodeSubQ(void)
{
	/* SubQ 0101nnn1ssmmmrrr */
	DoBinOpSub(Decode_xxxxnnnxssmmmrrr());
}

LOCALPROCUSEDONCE DoCodeSubI(void)
{
	DoBinOpSub(DecodeI_xxxxxxxxssmmmrrr());
}

LOCALPROCUSEDONCE DoCodeSubREa(void)
{
	DoBinOpSub(DecodeDEa_xxxxdddxssmmmrrr());
}

LOCALPROCUSEDONCE DoCodeLea(void)
{
	/* Lea 0100aaa111mmmrrr */
	DecodeModeRegister(mode, reg);
	m68k_areg(rg9) = ArgAddr.mem;
}

LOCALPROCUSEDONCE DoCodePEA(void)
{
	/* PEA 0100100001mmmrrr */
	DecodeModeRegister(mode, reg);
	m68k_areg(7) -= 4;
	put_long(m68k_areg(7), ArgAddr.mem);
}

LOCALPROCUSEDONCE DoCodeA(void)
{
	BackupPC();
	Exception(0xA);
}

LOCALPROCUSEDONCE DoCodeBsr(void)
{
	ui5b src = ((ui5b)opcode) & 255;
#if FastRelativeJump
	ui3p s = pc_p;
#else
	ui5b s = m68k_getpc();
#endif

	if (src == 0) {
		s += (si5b)(si4b)nextiword();
	} else
#if Use68020
	if (src == 255) {
		s += (si5b)nextilong();
		/* ReportAbnormal("long branch in DoCode6"); */
		/* Used by various Apps */
	} else
#endif
	{
		s += (si5b)(si3b)src;
	}

	/* Bsr 01100001nnnnnnnn */
	m68k_areg(7) -= 4;
	put_long(m68k_areg(7), m68k_getpc());
#if FastRelativeJump
	pc_p = s;
#else
	m68k_setpc(s);
#endif
}

LOCALPROCUSEDONCE DoCodeJsr(void)
{
	/* Jsr 0100111010mmmrrr */
	DecodeModeRegister(mode, reg);
	m68k_areg(7) -= 4;
	put_long(m68k_areg(7), m68k_getpc());
	m68k_setpc(ArgAddr.mem);
}

LOCALPROCUSEDONCE DoCodeLinkA6(void)
{
	CPTR stackp = m68k_areg(7);
	stackp -= 4;
	put_long(stackp, m68k_areg(6));
	m68k_areg(6) = stackp;
	m68k_areg(7) = stackp + (si5b)(si4b)nextiword();
}

LOCALPROCUSEDONCE DoCodeMOVEMRmML(void)
{
	/* MOVEM reg to mem 01001000111100rrr */
	si4b z;
	ui5b regmask = nextiword();
	si5b p = m68k_areg(reg);

#if Use68020
	{
		int n = 0;

		for (z = 0; z < 16; ++z) {
			if ((regmask & (1 << z)) != 0) {
				n++;
			}
		}
		m68k_areg(reg) = p - n * 4;
	}
#endif
	for (z = 16; --z >= 0; ) {
		if ((regmask & (1 << (15 - z))) != 0) {
			p -= 4;
			put_long(p, regs.regs[z]);
		}
	}
#if ! Use68020
	m68k_areg(reg) = p;
#endif
}

LOCALPROCUSEDONCE DoCodeMOVEMApRL(void)
{
	/* MOVEM mem to reg 01001100111011rrr */
	si4b z;
	ui5b regmask = nextiword();
	si5b p = m68k_areg(reg);

	for (z = 0; z < 16; ++z) {
		if ((regmask & (1 << (z))) != 0) {
			regs.regs[z] = get_long(p);
			p += 4;
		}
	}
	m68k_areg(reg) = p;
}

LOCALPROCUSEDONCE DoCodeUnlkA6(void)
{
	si5b src = m68k_areg(6);
	m68k_areg(6) = get_long(src);
	m68k_areg(7) =  src + 4;
}

LOCALPROCUSEDONCE DoCodeRts(void)
{
	/* Rts 0100111001110101 */
	ui5b NewPC = get_long(m68k_areg(7));
	m68k_areg(7) += 4;
	m68k_setpc(NewPC);
}

LOCALPROCUSEDONCE DoCodeJmp(void)
{
	/* JMP 0100111011mmmrrr */
	DecodeModeRegister(mode, reg);
	m68k_setpc(ArgAddr.mem);
}

LOCALPROCUSEDONCE DoCodeClr(void)
{
	/* Clr 01000010ssmmmrrr */

	FindOpSizeFromb76();
	DecodeModeRegister(mode, reg);
	VFLG = CFLG = 0;
	ZFLG = 1;
	NFLG = 0;
	SetArgValue(0);
}

LOCALPROCUSEDONCE DoCodeAddA(void)
{
	/* ADDA 1101dddm11mmmrrr */
	si5b srcvalue;

	opsize = b8 * 2 + 2;
	DecodeModeRegister(mode, reg);
	srcvalue = GetArgValue();
	m68k_areg(rg9) += srcvalue;
}

LOCALPROCUSEDONCE DoCodeAddQA(void)
{
	/* 0101nnn0ss001rrr */
	m68k_areg(reg) += octdat(rg9);
}

LOCALPROCUSEDONCE DoCodeSubA(void)
{
	si5b srcvalue;

	/* SUBA 1001dddm11mmmrrr */
	opsize = b8 * 2 + 2;
	DecodeModeRegister(mode, reg);
	srcvalue = GetArgValue();
	m68k_areg(rg9) -= srcvalue;
}

LOCALPROCUSEDONCE DoCodeSubQA(void)
{
	/* 0101nnn1ss001rrr */
	m68k_areg(reg) -= octdat(rg9);
}

LOCALPROCUSEDONCE DoCodeCmpA(void)
{
	si5b srcvalue;
	si5b dstvalue;

	opsize = b8 * 2 + 2;
	DecodeModeRegister(mode, reg);
	srcvalue = GetArgValue();

	dstvalue = m68k_areg(rg9);
	{
		int flgs = (srcvalue < 0);
		int flgo = (dstvalue < 0);
		dstvalue -= srcvalue;
		ZFLG = (dstvalue == 0);
		NFLG = (dstvalue < 0);
		VFLG = (flgs != flgo) && (NFLG != flgo);
		CFLG = (flgs && ! flgo) || (NFLG && ((! flgo) || flgs));
	}
}

LOCALPROC DoBinOpAddX(si5b srcvalue)
{
	si5b dstvalue;

	dstvalue = GetArgValue();
	{
		int flgs = (srcvalue < 0);
		int flgo = (dstvalue < 0);
		dstvalue += srcvalue + (XFLG ? 1 : 0);
		extendopsizedstvalue();
		if (dstvalue != 0) {
			ZFLG = 0;
		}
		NFLG = (dstvalue < 0);
		XFLG = CFLG = (flgs && flgo) || ((! NFLG) && (flgo || flgs));
		VFLG = (flgs && flgo && ! NFLG) || ((! flgs) && (! flgo) && NFLG);
	}
	SetArgValue(dstvalue);
}

LOCALPROCUSEDONCE DoCodeAddXd(void)
{
	DoBinOpAddX(DecodeDD_xxxxdddxssxxxrrr());
}

LOCALPROCUSEDONCE DoCodeAddXm(void)
{
	DoBinOpAddX(DecodeAAs_xxxxdddxssxxxrrr());
}

LOCALPROC DoBinOpSubX(si5b srcvalue)
{
	si5b dstvalue;

	dstvalue = GetArgValue();
	{
		int flgs = (srcvalue < 0);
		int flgo = (dstvalue < 0);
		dstvalue = dstvalue - srcvalue - (XFLG ? 1 : 0);
		extendopsizedstvalue();
		if (dstvalue != 0) {
			ZFLG = 0;
		}
		NFLG = (dstvalue < 0);
		VFLG = ((! flgs) && flgo && (! NFLG)) || (flgs && (! flgo) && NFLG);
		XFLG = CFLG = (flgs && (! flgo)) || (NFLG && ((! flgo) || flgs));
	}
	SetArgValue(dstvalue);
}

LOCALPROCUSEDONCE DoCodeSubXd(void)
{
	DoBinOpSubX(DecodeDD_xxxxdddxssxxxrrr());
}

LOCALPROCUSEDONCE DoCodeSubXm(void)
{
	DoBinOpSubX(DecodeAAs_xxxxdddxssxxxrrr());
}

#define BinOpASL 0
#define BinOpASR 1
#define BinOpLSL 2
#define BinOpLSR 3
#define BinOpRXL 4
#define BinOpRXR 5
#define BinOpROL 6
#define BinOpROR 7

LOCALPROC DoBinOp1(si5b srcvalue, ui5b binop)
{
	si5b dstvalue;

	dstvalue = GetArgValue();
	switch (binop) {
		case BinOpASL:
			{
				ui5b cnt = ((ui5b)srcvalue) & 63;
				si5b dstvalue0 = dstvalue;
				si5b comparevalue;
				if (! cnt) {
					VFLG = 0;
					CFLG = 0;
				} else {
					if (cnt > 32) {
						dstvalue = 0;
					} else {
						dstvalue = (si5b)(((ui5b)dstvalue) << (cnt - 1));
					}
					extendopsizedstvalue();
					CFLG = XFLG = (dstvalue < 0);
					dstvalue = (si5b)(((ui5b)dstvalue) << 1);
					extendopsizedstvalue();
				}
				if (dstvalue < 0) {
					comparevalue = - (si5b)(((ui5b) - dstvalue) >> (cnt));
				} else {
					comparevalue = (si5b)(((ui5b)dstvalue) >> (cnt));
				}
				VFLG = (comparevalue != dstvalue0);
				ZFLG = (dstvalue == 0);
				NFLG = (dstvalue < 0);
			}
			break;
		case BinOpASR:
			{
				ui5b cnt = ((ui5b)srcvalue) & 63;
				NFLG = (dstvalue < 0);
				VFLG = 0;
				if (! cnt) {
					CFLG = 0;
				} else {
					if (NFLG) {
						dstvalue = (~ dstvalue);
					}
					unextendopsizedstvalue();
					if (cnt > 32) {
						dstvalue = 0;
					} else {
						dstvalue = (si5b)(((ui5b)dstvalue) >> (cnt - 1));
					}
					CFLG = ((ui5b)dstvalue & 1);
					dstvalue = (si5b)(((ui5b)dstvalue) >> 1);
					if (NFLG) {
						CFLG = ! CFLG;
						dstvalue = (~ dstvalue);
					}
					XFLG = CFLG;
				}
				ZFLG = (dstvalue == 0);
			}
			break;
		case BinOpLSL:
			{
				ui5b cnt = ((ui5b)srcvalue) & 63;
				if (! cnt) {
					CFLG = 0;
				} else {
					if (cnt > 32) {
						dstvalue = 0;
					} else {
						dstvalue = (si5b)(((ui5b)dstvalue) << (cnt - 1));
					}
					extendopsizedstvalue();
					CFLG = XFLG = (dstvalue < 0);
					dstvalue = (si5b)(((ui5b)dstvalue) << 1);
					extendopsizedstvalue();
				}
				ZFLG = (dstvalue == 0);
				NFLG = (dstvalue < 0);
				VFLG = 0;
			}
			break;
		case BinOpLSR:
			{
				ui5b cnt = ((ui5b)srcvalue) & 63;
				if (! cnt) {
					CFLG = 0;
				} else {
					unextendopsizedstvalue();
					if (cnt > 32) {
						dstvalue = 0;
					} else {
						dstvalue = (si5b)(((ui5b)dstvalue) >> (cnt - 1));
					}
					CFLG = XFLG = ((ui5b)dstvalue & 1);
					dstvalue = (si5b)(((ui5b)dstvalue) >> 1);
				}
				ZFLG = (dstvalue == 0);
				NFLG = (dstvalue < 0);
				VFLG = 0;
			}
			break;
		case BinOpROL:
			{
				ui5b cnt = ((ui5b)srcvalue) & 63;
				if (! cnt) {
					CFLG = 0;
				} else {
					for (; cnt; --cnt) {
						CFLG = (dstvalue < 0);
						dstvalue = (si5b)(((ui5b)dstvalue) << 1);
						if (CFLG) {
							dstvalue = (si5b)(((ui5b)dstvalue) | 1);
						}
						extendopsizedstvalue();
					}
				}
				ZFLG = (dstvalue == 0);
				NFLG = (dstvalue < 0);
				VFLG = 0;
			}
			break;
		case BinOpRXL:
			{
				ui5b cnt = ((ui5b)srcvalue) & 63;
				if (! cnt) {
					CFLG = XFLG;
				} else {
					for (; cnt; --cnt) {
						CFLG = (dstvalue < 0);
						dstvalue = (si5b)(((ui5b)dstvalue) << 1);
						if (XFLG) {
							dstvalue = (si5b)(((ui5b)dstvalue) | 1);
						}
						extendopsizedstvalue();
						XFLG = CFLG;
					}
				}
				ZFLG = (dstvalue == 0);
				NFLG = (dstvalue < 0);
				VFLG = 0;
			}
			break;
		case BinOpROR:
			{
				ui5b cnt = ((ui5b)srcvalue) & 63;
				ui5b cmask = (ui5b)1 << (opsize * 8 - 1);
				if (! cnt) {
					CFLG = 0;
				} else {
					unextendopsizedstvalue();
					for (; cnt; --cnt) {
						CFLG = ((((ui5b)dstvalue) & 1) != 0);
						dstvalue = (si5b)(((ui5b)dstvalue) >> 1);
						if (CFLG) {
							dstvalue = (si5b)(((ui5b)dstvalue) | cmask);
						}
					}
					extendopsizedstvalue();
				}
				ZFLG = (dstvalue == 0);
				NFLG = (dstvalue < 0);
				VFLG = 0;
			}
			break;
		case BinOpRXR:
			{
				ui5b cnt = ((ui5b)srcvalue) & 63;
				ui5b cmask = (ui5b)1 << (opsize * 8 - 1);
				if (! cnt) {
					CFLG = XFLG;
				} else {
					unextendopsizedstvalue();
					for (; cnt; --cnt) {
						CFLG = ((((ui5b)dstvalue) & 1) != 0);
						dstvalue = (si5b)(((ui5b)dstvalue) >> 1);
						if (XFLG) {
							dstvalue = (si5b)(((ui5b)dstvalue) | cmask);
						}
						XFLG = CFLG;
					}
					extendopsizedstvalue();
				}
				ZFLG = (dstvalue == 0);
				NFLG = (dstvalue < 0);
				VFLG = 0;
			}
			break;
		default:
			/* should not get here */;
			break;
	}
	SetArgValue(dstvalue);
}

LOCALFUNC ui5b rolops(ui5b x)
{
	ui5b binop;

	binop = (x << 1);
	if (! b8) {
		binop++; /* 'R' */
	} /* else 'L' */
	return binop;
}

LOCALPROCUSEDONCE DoCodeRolopNM(void)
{
	opsize = 2;
	DecodeModeRegister(mode, reg);
	DoBinOp1(1, rolops(rg9));
}

LOCALPROCUSEDONCE DoCodeRolopND(void)
{
	/* 1110cccdss0ttddd */
	FindOpSizeFromb76();
	SetArgKindReg(reg);
	DoBinOp1(octdat(rg9), rolops(mode & 3));
}

LOCALPROCUSEDONCE DoCodeRolopDD(void)
{
	/* 1110rrrdss1ttddd */
	si5b srcvalue;
	FindOpSizeFromb76();
	SetArgKindReg(rg9);
	srcvalue = GetArgValue();
	SetArgKindReg(reg);
	DoBinOp1(srcvalue, rolops(mode & 3));
}

#define BinOpBTst 0
#define BinOpBChg 1
#define BinOpBClr 2
#define BinOpBSet 3

LOCALPROC DoBinBitOp1(si5b srcvalue)
{
	si5b dstvalue;
	ui5b binop;

	dstvalue = GetArgValue();

	ZFLG = (((ui5b)dstvalue & ((ui5b)1 << srcvalue)) == 0);
	binop = b76;
	if (binop != BinOpBTst) {
		switch (binop) {
			case BinOpBChg:
				dstvalue ^= (1 << srcvalue);
				break;
			case BinOpBClr:
				dstvalue &= ~ (1 << srcvalue);
				break;
			case BinOpBSet:
				dstvalue |= (1 << srcvalue);
				break;
			default:
				/* should not get here */;
				break;
		}
		SetArgValue(dstvalue);
	}
}

LOCALPROCUSEDONCE DoCodeBitOpDD(void)
{
	/* dynamic bit, Opcode = 0000ddd1tt000rrr */
	si5b srcvalue = ((si5b)(si3b)m68k_dreg(rg9)) & 31;
	opsize = 4;
	SetArgKindReg(reg);
	DoBinBitOp1(srcvalue);
}

LOCALPROCUSEDONCE DoCodeBitOpDM(void)
{
	/* dynamic bit, Opcode = 0000ddd1ttmmmrrr */
	si5b srcvalue = ((si5b)(si3b)m68k_dreg(rg9)) & 7;
	opsize = 1;
	DecodeModeRegister(mode, reg);
	DoBinBitOp1(srcvalue);
}

LOCALPROCUSEDONCE DoCodeBitOpND(void)
{
	/* static bit 00001010tt000rrr */
	si5b srcvalue = ((si5b)(si3b)nextibyte()) & 31;
	opsize = 4;
	SetArgKindReg(reg);
	DoBinBitOp1(srcvalue);
}

LOCALPROCUSEDONCE DoCodeBitOpNM(void)
{
	/* static bit 00001010ttmmmrrr */
	si5b srcvalue = ((si5b)(si3b)nextibyte()) & 7;
	opsize = 1;
	DecodeModeRegister(mode, reg);
	DoBinBitOp1(srcvalue);
}

LOCALPROC DoBinOpAnd(si5b srcvalue)
{
	si5b dstvalue;

	dstvalue = GetArgValue();
	dstvalue &= srcvalue;
	VFLG = CFLG = 0;
	ZFLG = (dstvalue == 0);
	NFLG = (dstvalue < 0);
	SetArgValue(dstvalue);
}

LOCALPROCUSEDONCE DoCodeAndI(void)
{
	DoBinOpAnd(DecodeI_xxxxxxxxssmmmrrr());
}

LOCALPROCUSEDONCE DoCodeAndDEa(void)
{
	/* And 1100ddd1ssmmmrrr */
	DoBinOpAnd(DecodeDEa_xxxxdddxssmmmrrr());
}

LOCALPROCUSEDONCE DoCodeAndEaD(void)
{
	/* And 1100ddd0ssmmmrrr */
	DoBinOpAnd(DecodeEaD_xxxxdddxssmmmrrr());
}

LOCALPROC DoBinOr(si5b srcvalue)
{
	si5b dstvalue;

	dstvalue = GetArgValue();
	dstvalue |= srcvalue;
	VFLG = CFLG = 0;
	ZFLG = (dstvalue == 0);
	NFLG = (dstvalue < 0);
	SetArgValue(dstvalue);
}

LOCALPROCUSEDONCE DoCodeOrI(void)
{
	DoBinOr(DecodeI_xxxxxxxxssmmmrrr());
}

LOCALPROCUSEDONCE DoCodeOrDEa(void)
{
	/* OR 1000ddd1ssmmmrrr */
	DoBinOr(DecodeDEa_xxxxdddxssmmmrrr());
}

LOCALPROCUSEDONCE DoCodeOrEaD(void)
{
	/* OR 1000ddd0ssmmmrrr */
	DoBinOr(DecodeEaD_xxxxdddxssmmmrrr());
}

LOCALPROC DoBinOpEor(si5b srcvalue)
{
	si5b dstvalue;

	dstvalue = GetArgValue();
	dstvalue ^= srcvalue;
	VFLG = CFLG = 0;
	ZFLG = (dstvalue == 0);
	NFLG = (dstvalue < 0);
	SetArgValue(dstvalue);
}

LOCALPROCUSEDONCE DoCodeEorI(void)
{
	DoBinOpEor(DecodeI_xxxxxxxxssmmmrrr());
}

LOCALPROCUSEDONCE DoCodeEor(void)
{
	/* Eor 1011ddd1ssmmmrrr */
	DoBinOpEor(DecodeDEa_xxxxdddxssmmmrrr());
}

LOCALPROCUSEDONCE DoCodeNot(void)
{
	/* Not 01000110ssmmmrrr */
	si5b dstvalue;

	FindOpSizeFromb76();
	DecodeModeRegister(mode, reg);
	dstvalue = GetArgValue();
	{
		dstvalue = ~ dstvalue;
		extendopsizedstvalue();
		ZFLG = (dstvalue == 0);
		NFLG = (dstvalue < 0);
		VFLG = CFLG = 0;
	}
	SetArgValue(dstvalue);
}

LOCALPROCUSEDONCE DoCodeScc(void)
{
	/* Scc 0101cccc11mmmrrr */
	opsize = 1;
	DecodeModeRegister(mode, reg);
	SetArgValue(cctrue() ? 0xff : 0);
}

LOCALPROCUSEDONCE DoCodeEXTL(void)
{
	/* EXT.L */
	ui5b srcreg = reg;
	si5b src = m68k_dreg(srcreg);
	ui5b dst = (si5b)(si4b)src;
	VFLG = CFLG = 0;
	ZFLG = ((si5b)(dst)) == 0;
	NFLG = ((si5b)(dst)) < 0;
	m68k_dreg(srcreg) = (dst);
}

LOCALPROCUSEDONCE DoCodeEXTW(void)
{
	/* EXT.W */
	ui5b srcreg = reg;
	si5b src = m68k_dreg(srcreg);
	ui4b dst = (si4b)(si3b)src;
	VFLG = CFLG = 0;
	ZFLG = ((si4b)(dst)) == 0;
	NFLG = ((si4b)(dst)) < 0;
	m68k_dreg(srcreg) = (m68k_dreg(srcreg) & ~ 0xffff) | ((dst) & 0xffff);
}

LOCALPROCUSEDONCE DoCodeNeg(void)
{
	/* Neg 01000100ssmmmrrr */
	si5b dstvalue;

	FindOpSizeFromb76();
	DecodeModeRegister(mode, reg);
	dstvalue = GetArgValue();
	{
		int flgs = (dstvalue < 0);
		dstvalue = 0 - dstvalue;
		extendopsizedstvalue();
		ZFLG = (dstvalue == 0);
		NFLG = (dstvalue < 0);
		VFLG = (flgs && NFLG);
		XFLG = CFLG = (flgs || NFLG);
	}
	SetArgValue(dstvalue);
}

LOCALPROCUSEDONCE DoCodeNegX(void)
{
	/* NegX 01000000ssmmmrrr */
	si5b dstvalue;

	FindOpSizeFromb76();
	DecodeModeRegister(mode, reg);
	dstvalue = GetArgValue();
	{
		int flgs = (dstvalue < 0);
		dstvalue = 0 - dstvalue - (XFLG ? 1 : 0);
		extendopsizedstvalue();
		if (dstvalue != 0) {
			ZFLG = 0;
		}
		NFLG = (dstvalue < 0);
		VFLG = (flgs && NFLG);
		XFLG = CFLG = (flgs || NFLG);
	}
	SetArgValue(dstvalue);
}

LOCALPROCUSEDONCE DoCodeMulU(void)
{
	/* MulU 1100ddd011mmmrrr */
	si5b srcvalue;
	si5b dstvalue;

	opsize = 2;
	DecodeModeRegister(mode, reg);
	srcvalue = GetArgValue();
	dstvalue = (ui5b)(ui4b)regs.regs[rg9] * (ui5b)(ui4b)srcvalue;
	VFLG = CFLG = 0;
	ZFLG = (dstvalue == 0);
	NFLG = (dstvalue < 0);
	regs.regs[rg9] = dstvalue;
}

LOCALPROCUSEDONCE DoCodeMulS(void)
{
	/* MulS 1100ddd111mmmrrr */
	si5b srcvalue;
	si5b dstvalue;

	opsize = 2;
	DecodeModeRegister(mode, reg);
	srcvalue = GetArgValue();
	dstvalue = (si5b)(si4b)regs.regs[rg9] * (si5b)(si4b)srcvalue;
	VFLG = CFLG = 0;
	ZFLG = (dstvalue == 0);
	NFLG = (dstvalue < 0);
	regs.regs[rg9] = dstvalue;
}

LOCALPROCUSEDONCE DoCodeDivU(void)
{
	/* DivU 1000ddd011mmmrrr */
	si5b srcvalue;
	si5b dstvalue;

	opsize = 2;
	DecodeModeRegister(mode, reg);
	srcvalue = GetArgValue();
	dstvalue = regs.regs[rg9];
	if (srcvalue == 0) {
		Exception(5);
	} else {
		ui5b newv = (ui5b)dstvalue / (ui5b)(ui4b)srcvalue;
		ui5b rem = (ui5b)dstvalue % (ui5b)(ui4b)srcvalue;
		if (newv > 0xffff) {
			VFLG = NFLG = 1;
			CFLG = 0;
		} else {
			VFLG = CFLG = 0;
			ZFLG = ((si4b)(newv)) == 0;
			NFLG = ((si4b)(newv)) < 0;
			newv = (newv & 0xffff) | ((ui5b)rem << 16);
			dstvalue = newv;
		}
	}
	regs.regs[rg9] = dstvalue;
}

LOCALPROCUSEDONCE DoCodeDivS(void)
{
	/* DivS 1000ddd111mmmrrr */
	si5b srcvalue;
	si5b dstvalue;

	opsize = 2;
	DecodeModeRegister(mode, reg);
	srcvalue = GetArgValue();
	dstvalue = regs.regs[rg9];
	if (srcvalue == 0) {
		Exception(5);
	} else {
		si5b newv = (si5b)dstvalue / (si5b)(si4b)srcvalue;
		ui4b rem = (si5b)dstvalue % (si5b)(si4b)srcvalue;
		if ((newv & 0xffff8000) != 0 && (newv & 0xffff8000) != 0xffff8000) {
			VFLG = NFLG = 1; CFLG = 0;
		} else {
			if (((si4b)rem < 0) != ((si5b)dstvalue < 0)) rem = - rem;
			VFLG = CFLG = 0;
			ZFLG = ((si4b)(newv)) == 0;
			NFLG = ((si4b)(newv)) < 0;
			newv = (newv & 0xffff) | ((ui5b)rem << 16);
			dstvalue = newv;
		}
	}
	regs.regs[rg9] = dstvalue;
}

LOCALPROCUSEDONCE DoCodeExgdd(void)
{
	/* Exg 1100ddd101000rrr, opsize = 4 */
	ui5b srcreg = rg9;
	ui5b dstreg = reg;
	si5b src = m68k_dreg(srcreg);
	si5b dst = m68k_dreg(dstreg);
	m68k_dreg(srcreg) = dst;
	m68k_dreg(dstreg) = src;
}

LOCALPROCUSEDONCE DoCodeExgaa(void)
{
	/* Exg 1100ddd101001rrr, opsize = 4 */
	ui5b srcreg = rg9;
	ui5b dstreg = reg;
	si5b src = m68k_areg(srcreg);
	si5b dst = m68k_areg(dstreg);
	m68k_areg(srcreg) = dst;
	m68k_areg(dstreg) = src;
}

LOCALPROCUSEDONCE DoCodeExgda(void)
{
	/* Exg 1100ddd110001rrr, opsize = 4 */
	ui5b srcreg = rg9;
	ui5b dstreg = reg;
	si5b src = m68k_dreg(srcreg);
	si5b dst = m68k_areg(dstreg);
	m68k_dreg(srcreg) = dst;
	m68k_areg(dstreg) = src;
}

LOCALPROCUSEDONCE DoCodeMoveCCREa(void)
{
	/* Move from CCR 0100001011mmmrrr */
	ReportAbnormal("Move from CCR");
	opsize = 2;
	DecodeModeRegister(mode, reg);
	SetArgValue(m68k_getSR() & 0xFF);
}

LOCALPROCUSEDONCE DoCodeMoveEaCR(void)
{
	/* 0100010011mmmrrr */
	opsize = 2;
	DecodeModeRegister(mode, reg);
	m68k_setCR(GetArgValue());
}

LOCALPROCUSEDONCE DoCodeMoveSREa(void)
{
	/* Move from SR 0100000011mmmrrr */
	opsize = 2;
	DecodeModeRegister(mode, reg);
	SetArgValue(m68k_getSR());
}

LOCALPROCUSEDONCE DoCodeMoveEaSR(void)
{
	/* 0100011011mmmrrr */
	opsize = 2;
	DecodeModeRegister(mode, reg);
	m68k_setSR(GetArgValue());
}

LOCALPROC DoBinOpStatusCCR(void)
{
	blnr IsStatus = (b76 != 0);
	si5b srcvalue;
	si5b dstvalue;

	FindOpSizeFromb76();
	if (IsStatus && (! regs.s)) {
		BackupPC();
		Exception(8);
	} else {
		srcvalue = (si5b)(si4b)nextiword();
		dstvalue = m68k_getSR();
		switch (rg9) {
			case 0 :
				dstvalue |= srcvalue;
				break;
			case 1 :
				dstvalue &= srcvalue;
				break;
			case 5 :
				dstvalue ^= srcvalue;
				break;
			default: /* should not happen */
				break;
		}
		if (IsStatus) {
			m68k_setSR(dstvalue);
		} else {
			m68k_setCR(dstvalue);
		}
	}
}

LOCALPROCUSEDONCE DoCodeMOVEMApRW(void)
{
	/* MOVEM mem to reg 01001100110011rrr */
	si4b z;
	ui5b regmask = nextiword();
	si5b p = m68k_areg(reg);

	for (z = 0; z < 16; ++z) {
		if ((regmask & (1 << (z))) != 0) {
			regs.regs[z] = get_word(p);
			p += 2;
		}
	}
	m68k_areg(reg) = p;
}

LOCALPROCUSEDONCE DoCodeMOVEMRmMW(void)
{
	/* MOVEM reg to mem 01001000110100rrr */
	si4b z;
	ui5b regmask = nextiword();
	si5b p = m68k_areg(reg);

#if Use68020
	{
		int n = 0;

		for (z = 0; z < 16; ++z) {
			if ((regmask & (1 << z)) != 0) {
				n++;
			}
		}
		m68k_areg(reg) = p - n * 2;
	}
#endif
	for (z = 16; --z >= 0; ) {
		if ((regmask & (1 << (15 - z))) != 0) {
			p -= 2;
			put_word(p, regs.regs[z]);
		}
	}
#if ! Use68020
	m68k_areg(reg) = p;
#endif
}

LOCALPROC reglist(si4b direction, ui5b m1, ui5b r1)
{
	si4b z;
	si5b p;
	ui5b regmask;

	regmask = nextiword();
	opsize = 2 * b76 - 2;
	DecodeModeRegister(m1, r1);
	p = ArgAddr.mem;
	if (direction == 0) {
		if (opsize == 2) {
			for (z = 0; z < 16; ++z) {
				if ((regmask & (1 << (z))) != 0) {
					put_word(p, regs.regs[z]);
					p += 2;
				}
			}
		} else {
			for (z = 0; z < 16; ++z) {
				if ((regmask & (1 << (z))) != 0) {
					put_long(p, regs.regs[z]);
					p += 4;
				}
			}
		}
	} else {
		if (opsize == 2) {
			for (z = 0; z < 16; ++z) {
				if ((regmask & (1 << (z))) != 0) {
					regs.regs[z] = get_word(p);
					p += 2;
				}
			}
		} else {
			for (z = 0; z < 16; ++z) {
				if ((regmask & (1 << (z))) != 0) {
					regs.regs[z] = get_long(p);
					p += 4;
				}
			}
		}
	}
}

LOCALPROCUSEDONCE DoCodeMOVEMrm(void)
{
	/* MOVEM reg to mem 010010001ssmmmrrr */
	reglist(0, mode, reg);
}

LOCALPROCUSEDONCE DoCodeMOVEMmr(void)
{
	/* MOVEM mem to reg 0100110011smmmrrr */
	reglist(1, mode, reg);
}

LOCALPROC DoBinOpAbcd(ui5b m1, ui5b r1, ui5b m2, ui5b r2)
{
	si5b srcvalue;
	si5b dstvalue;

	opsize = 1;
	DecodeModeRegister(m1, r1);
	srcvalue = GetArgValue();
	DecodeModeRegister(m2, r2);
	dstvalue = GetArgValue();
	{
		/* if (opsize != 1) a bug */
		int flgs = (srcvalue < 0);
		int flgo = (dstvalue < 0);
		ui4b newv_lo = (srcvalue & 0xF) + (dstvalue & 0xF) + (XFLG ? 1 : 0);
		ui4b newv_hi = (srcvalue & 0xF0) + (dstvalue & 0xF0);
		ui4b newv;

		if (newv_lo > 9) {
			newv_lo += 6;
		}
		newv = newv_hi + newv_lo;
		CFLG = XFLG = (newv & 0x1F0) > 0x90;
		if (CFLG) {
			newv += 0x60;
		}
		dstvalue = (si3b)newv;
		if (dstvalue != 0) {
			ZFLG = 0;
		}
		NFLG = (dstvalue < 0);
		VFLG = (flgs != flgo) && (NFLG != flgo);
		/* but according to my reference book, VFLG is Undefined for ABCD */
	}
	SetArgValue(dstvalue);
}

LOCALPROCUSEDONCE DoCodeAbcdr(void)
{
	/* ABCD 1100ddd100000rrr */
	DoBinOpAbcd(0, reg, 0, rg9);
}

LOCALPROCUSEDONCE DoCodeAbcdm(void)
{
	/* ABCD 1100ddd100001rrr */
	DoBinOpAbcd(4, reg, 4, rg9);
}

LOCALPROC DoBinOpSbcd(ui5b m1, ui5b r1, ui5b m2, ui5b r2)
{
	si5b srcvalue;
	si5b dstvalue;

	opsize = 1;
	DecodeModeRegister(m1, r1);
	srcvalue = GetArgValue();
	DecodeModeRegister(m2, r2);
	dstvalue = GetArgValue();
	{
		int flgs = (srcvalue < 0);
		int flgo = (dstvalue < 0);
		ui4b newv_lo = (dstvalue & 0xF) - (srcvalue & 0xF) - (XFLG ? 1 : 0);
		ui4b newv_hi = (dstvalue & 0xF0) - (srcvalue & 0xF0);
		ui4b newv;

		if (newv_lo > 9) {
			newv_lo -= 6;
			newv_hi -= 0x10;
		}
		newv = newv_hi + (newv_lo & 0xF);
		CFLG = XFLG = (newv_hi & 0x1F0) > 0x90;
		if (CFLG) {
			newv -= 0x60;
		}
		dstvalue = (si3b)newv;
		if (dstvalue != 0) {
			ZFLG = 0;
		}
		NFLG = (dstvalue < 0);
		VFLG = (flgs != flgo) && (NFLG != flgo);
		/* but according to my reference book, VFLG is Undefined for SBCD */
	}
	SetArgValue(dstvalue);
}

LOCALPROCUSEDONCE DoCodeSbcdr(void)
{
	/* SBCD 1000xxx100000xxx */
	DoBinOpSbcd(0, reg, 0, rg9);
}

LOCALPROCUSEDONCE DoCodeSbcdm(void)
{
	/* SBCD 1000xxx100001xxx */
	DoBinOpSbcd(4, reg, 4, rg9);
}

LOCALPROCUSEDONCE DoCodeNbcd(void)
{
	/* Nbcd 0100100000mmmrrr */
	si5b dstvalue;

	opsize = 1;
	DecodeModeRegister(mode, reg);
	dstvalue = GetArgValue();
	{
		ui4b newv_lo = - (dstvalue & 0xF) - (XFLG ? 1 : 0);
		ui4b newv_hi = - (dstvalue & 0xF0);
		ui4b newv;

		if (newv_lo > 9) {
			newv_lo -= 6;
			newv_hi -= 0x10;
		}
		newv = newv_hi + (newv_lo & 0xF);
		CFLG = XFLG = (newv_hi & 0x1F0) > 0x90;
		if (CFLG) {
			newv -= 0x60;
		}

		dstvalue = newv;
		extendopsizedstvalue();
		NFLG = (dstvalue < 0);
		if (dstvalue != 0) {
			ZFLG = 0;
		}
	}
	SetArgValue(dstvalue);
}

LOCALPROCUSEDONCE DoCodeRte(void)
{
	/* Rte 0100111001110011 */
	if (! regs.s) {
		BackupPC();
		Exception(8);
	} else {
		ui5b NewPC;
		CPTR stackp = m68k_areg(7);
		ui5b NewSR = (ui5b) get_word(stackp);
		stackp += 2;
		NewPC = get_long(stackp);
		stackp += 4;

#if Use68020
		{
			ui4b format = get_word(stackp);
			stackp += 2;

			switch ((format >> 12) & 0x0F) {
				case 0:
					/* ReportAbnormal("rte stack frame format 0"); */
					break;
				case 1:
					ReportAbnormal("rte stack frame format 1");
					NewPC = m68k_getpc() - 2;
						/* rerun instruction */
					break;
				case 2:
					ReportAbnormal("rte stack frame format 2");
					stackp += 4;
					break;
				case 9:
					ReportAbnormal("rte stack frame format 9");
					stackp += 12;
					break;
				case 10:
					ReportAbnormal("rte stack frame format 10");
					stackp += 24;
					break;
				case 11:
					ReportAbnormal("rte stack frame format 11");
					stackp += 84;
					break;
				default:
					ReportAbnormal("unknown rte stack frame format");
					Exception(14);
					return;
					break;
			}
		}
#endif
		m68k_areg(7) = stackp;
		m68k_setSR(NewSR);
		m68k_setpc(NewPC);
	}
}

LOCALPROCUSEDONCE DoCodeNop(void)
{
	/* Nop Opcode = 0100111001110001 */
}

LOCALPROCUSEDONCE DoCodeMoveP(void)
{
	/* MoveP 0000ddd1mm001aaa */
	ui5b TheReg = reg;
	ui5b TheRg9 = rg9;
	ui5b Displacement = nextiword();
		/* shouldn't this sign extend ? */
	CPTR memp = m68k_areg(TheReg) + Displacement;
#if 0
	if ((Displacement & 0x00008000) != 0) {
		/***** for testing only *****/
		BackupPC();
		op_illg();
	}
#endif

	switch (b76) {
		case 0:
			{
				ui4b val = ((get_byte(memp) & 0x00FF) << 8)
					| (get_byte(memp + 2) & 0x00FF);
				m68k_dreg(TheRg9) = (m68k_dreg(TheRg9) & ~ 0xffff) | ((val) & 0xffff);
			}
			break;
		case 1:
			{
				ui5b val = ((get_byte(memp) << 24) & 0x00FF)
					| ((get_byte(memp + 2) << 16) & 0x00FF)
					| ((get_byte(memp + 4) << 8) & 0x00FF)
					| (get_byte(memp + 6) & 0x00FF);
				m68k_dreg(TheRg9) = (val);
			}
			break;
		case 2:
			{
				si4b src = m68k_dreg(TheRg9);
				put_byte(memp, src >> 8); put_byte(memp + 2, src);
			}
			break;
		case 3:
			{
				si5b src = m68k_dreg(TheRg9);
				put_byte(memp, src >> 24); put_byte(memp + 2, src >> 16);
				put_byte(memp + 4, src >> 8); put_byte(memp + 6, src);
			}
			break;
	}
}

LOCALPROC op_illg(void)
{
	BackupPC();
	Exception(4);
}

LOCALPROC DoCheck(void)
{
	si5b srcvalue;
	si5b dstvalue;

	DecodeModeRegister(mode, reg);
	srcvalue = GetArgValue();
	DecodeModeRegister(0, rg9);
	dstvalue = GetArgValue();
	if (dstvalue < 0) {
		NFLG = 1;
		Exception(6);
	} else if (dstvalue > srcvalue) {
		NFLG = 0;
		Exception(6);
	}
}

LOCALPROCUSEDONCE DoCodeChkW(void)
{
	/* Chk.W 0100ddd110mmmrrr */
	opsize = 2;
	DoCheck();
}

LOCALPROCUSEDONCE DoCodeTrap(void)
{
	/* Trap 010011100100vvvv */
	Exception((opcode & 15) + 32);
}

LOCALPROCUSEDONCE DoCodeTrapV(void)
{
	/* TrapV 0100111001110110 */
	if (VFLG) {
		Exception(7);
	}
}

LOCALPROCUSEDONCE DoCodeRtr(void)
{
	/* Rtr 0100111001110111 */
	ui5b NewPC;
	CPTR stackp = m68k_areg(7);
	ui5b NewCR = get_word(stackp);
	stackp += 2;
	NewPC = get_long(stackp);
	stackp += 4;
	m68k_areg(7) = stackp;
	m68k_setCR(NewCR);
	m68k_setpc(NewPC);
}

LOCALPROCUSEDONCE DoCodeLink(void)
{
	ui5b srcreg = reg;
	CPTR stackp = m68k_areg(7);
	stackp -= 4;
	m68k_areg(7) = stackp; /* only matters if srcreg == 7 */
	put_long(stackp, m68k_areg(srcreg));
	m68k_areg(srcreg) = stackp;
	m68k_areg(7) += (si5b)(si4b)nextiword();
}

LOCALPROCUSEDONCE DoCodeUnlk(void)
{
	ui5b srcreg = reg;
	if (srcreg != 7) {
		si5b src = m68k_areg(srcreg);
		m68k_areg(srcreg) = get_long(src);
		m68k_areg(7) =  src + 4;
	} else {
		/* wouldn't expect this to happen */
		m68k_areg(7) = get_long(m68k_areg(7)) + 4;
	}
}

LOCALPROCUSEDONCE DoCodeMoveRUSP(void)
{
	/* MOVE USP 0100111001100aaa */
	if (! regs.s) {
		BackupPC();
		Exception(8);
	} else {
		regs.usp = m68k_areg(reg);
	}
}

LOCALPROCUSEDONCE DoCodeMoveUSPR(void)
{
	/* MOVE USP 0100111001101aaa */
	if (! regs.s) {
		BackupPC();
		Exception(8);
	} else {
		m68k_areg(reg) = regs.usp;
	}
}

LOCALPROCUSEDONCE DoCodeTas(void)
{
	/* Tas 0100101011mmmrrr */
	si5b dstvalue;

	opsize = 1;
	DecodeModeRegister(mode, reg);
	dstvalue = GetArgValue();

	{
		ZFLG = (dstvalue == 0);
		NFLG = (dstvalue < 0);
		VFLG = CFLG = 0;
		dstvalue |= 0x80;
	}
	SetArgValue(dstvalue);
}

LOCALPROCUSEDONCE DoCodeF(void)
{
	/* ReportAbnormal("DoCodeF"); */
	BackupPC();
	Exception(0xB);
}

LOCALPROCUSEDONCE DoCodeCallMorRtm(void)
{
	/* CALLM or RTM 0000011011mmmrrr */
	ReportAbnormal("CALLM or RTM instruction");
}

FORWARDPROC m68k_setstopped(void);

LOCALPROCUSEDONCE DoCodeStop(void)
{
	/* Stop 0100111001110010 */
	if (! regs.s) {
		BackupPC();
		Exception(8);
	} else {
		m68k_setSR((si4b)nextiword());
		m68k_setstopped();
	}
}

LOCALPROCUSEDONCE DoCodeReset(void)
{
	/* Reset 0100111001100000 */
	if (! regs.s) {
		BackupPC();
		Exception(8);
	} else {
		customreset();
	}
}

#if Use68020
LOCALPROC DoCHK2orCMP2(void)
{
	/* CHK2 or CMP2 00000ss011mmmrrr */
	si5b regv;
	si5b lower;
	si5b upper;
	ui4b extra = nextiword();

	/* ReportAbnormal("CHK2 or CMP2 instruction"); */
	switch ((opcode >> 9) & 3) {
		case 0:
			opsize = 1;
			break;
		case 1:
			opsize = 2;
			break;
		case 2:
			opsize = 4;
			break;
		default:
			ReportAbnormal("illegal opsize in CHK2 or CMP2");
			break;
	}
	if ((extra & 0x8000) == 0) {
		DecodeModeRegister(0, (extra >> 12) & 0x07);
		regv = GetArgValue();
	} else {
		regv = m68k_areg((extra >> 12) & 0x07);
	}
	DecodeModeRegister(mode, reg);
	if (ArgKind != AKMemory) {
		ReportAbnormal("illegal CHK2 or CMP2 mode");
	} else {
		/* CPTR oldpc = m68k_getpc(); */
		lower = GetArgValue();
		ArgAddr.mem += opsize;
		upper = GetArgValue();

		ZFLG = (upper == regv) || (lower == regv);
		CFLG = (lower <= upper)
				? (regv < lower || regv > upper)
				: (regv > upper || regv < lower);
		if ((extra & 0x800) && CFLG) {
			Exception(6);
		}
	}
}
#endif

#if Use68020
LOCALPROC DoCAS(void)
{
	/* CAS  00001ss011mmmrrr */
	si5b srcvalue;
	si5b dstvalue;

	ui4b src = nextiword();
	int ru = (src >> 6) & 7;
	int rc = src & 7;

	ReportAbnormal("CAS instruction");
	switch ((opcode >> 9) & 3) {
		case 1 :
			opsize = 1;
			break;
		case 2 :
			opsize = 2;
			break;
		case 3 :
			opsize = 4;
			break;
	}

	DecodeModeRegister(0, rc);
	srcvalue = GetArgValue();
	DecodeModeRegister(mode, reg);
	dstvalue = GetArgValue();
	{
		int flgs = srcvalue < 0;
		int flgo = dstvalue < 0;
		si5b newv = dstvalue - srcvalue;
		if (opsize == 1) {
			newv = (ui3b)newv;
		} else if (opsize == 2) {
			newv = (ui4b)newv;
		}
		ZFLG = (newv == 0);
		NFLG = (newv < 0);
		VFLG = (flgs != flgo) && (NFLG != flgo);
		CFLG = (flgs && ! flgo) || (NFLG && ((! flgo) || flgs));
		if (ZFLG) {
			SetArgValue(m68k_dreg(ru));
		} else {
			DecodeModeRegister(0, rc);
			SetArgValue(dstvalue);
		}
	}
}
#endif

#if Use68020
LOCALPROC DoCAS2(void)
{
	/* CAS2 00001ss011111100 */
	ui5b extra = nextilong();
	int dc2 = extra & 7;
	int du2 = (extra >> 6) & 7;
	int dc1 = (extra >> 16) & 7;
	int du1 = (extra >> 22) & 7;
	CPTR rn1 = regs.regs[(extra >> 28) & 0x0F];
	CPTR rn2 = regs.regs[(extra >> 12) & 0x0F];
	si5b src = m68k_dreg(dc1);
	si5b dst1;
	si5b dst2;

	ReportAbnormal("DoCAS2 instruction");
	switch ((opcode >> 9) & 3) {
		case 1 :
			op_illg();
			return;
			break;
		case 2 :
			opsize = 2;
			break;
		case 3 :
			opsize = 4;
			break;
	}
	if (opsize == 2) {
		dst1 = get_word(rn1);
		dst2 = get_word(rn2);
		src = (si5b)(si4b)src;
	} else {
		dst1 = get_long(rn1);
		dst2 = get_long(rn2);
	}
	{
		int flgs = src < 0;
		int flgo = dst1 < 0;
		si5b newv = dst1 - src;
		if (opsize == 2) {
			newv = (ui4b)newv;
		}
		ZFLG = (newv == 0);
		NFLG = (newv < 0);
		VFLG = (flgs != flgo) && (NFLG != flgo);
		CFLG = (flgs && ! flgo) || (NFLG && ((! flgo) || flgs));
		if (ZFLG) {
			src = m68k_dreg(dc2);
			if (opsize == 2) {
				src = (si5b)(si4b)src;
			}
			flgs = src < 0;
			flgo = dst2 < 0;
			newv = dst2 - src;
			if (opsize == 2) {
				newv = (ui4b)newv;
			}
			ZFLG = (newv == 0);
			NFLG = (newv < 0);
			VFLG = (flgs != flgo) && (NFLG != flgo);
			CFLG = (flgs && ! flgo) || (NFLG && ((! flgo) || flgs));
			if (ZFLG) {
				if (opsize == 2) {
					put_word(rn1, m68k_dreg(du1));
					put_word(rn2, m68k_dreg(du2));
				} else {
					put_word(rn1, m68k_dreg(du1));
					put_word(rn2, m68k_dreg(du2));
				}
			}
		}
	}
	if (! ZFLG) {
		if (opsize == 2) {
			m68k_dreg(du1) = (m68k_dreg(du1) & ~ 0xffff) | ((ui5b)dst1 & 0xffff);
			m68k_dreg(du2) = (m68k_dreg(du2) & ~ 0xffff) | ((ui5b)dst2 & 0xffff);
		} else {
			m68k_dreg(du1) = dst1;
			m68k_dreg(du2) = dst2;
		}
	}
}
#endif

#if Use68020
LOCALPROC DoMOVES(void)
{
	/* MoveS 00001110ssmmmrrr */
	ReportAbnormal("MoveS instruction");
	if (! regs.s) {
		BackupPC();
		Exception(8);
	} else {
		ui4b extra = nextiword();
		FindOpSizeFromb76();
		DecodeModeRegister(mode, reg);
		if (extra & 0x0800) {
			ui5b src = regs.regs[(extra >> 12) & 0x0F];
			SetArgValue(src);
		} else {
			ui5b rr = (extra >> 12) & 7;
			si5b srcvalue = GetArgValue();
			if (extra & 0x8000) {
				m68k_areg(rr) = srcvalue;
			} else {
				DecodeModeRegister(0, rr);
				SetArgValue(srcvalue);
			}
		}
	}
}
#endif

#define ui5b_lo(x) ((x) & 0x0000FFFF)
#define ui5b_hi(x) (((x) >> 16) & 0x0000FFFF)

#if Use68020
struct ui6r {
	ui5b hi;
	ui5b lo;
};
typedef struct ui6r ui6r;
#endif

#if Use68020
LOCALPROC Ui6r_Negate(ui6r *v)
{
	v->hi = ~ v->hi;
	v->lo = - v->lo;
	if (v->lo == 0) {
		v->hi++;
	}
}
#endif

#if Use68020
LOCALFUNC blnr Ui6r_IsZero(ui6r *v)
{
	return (v->hi == 0) && (v->lo == 0);
}
#endif

#if Use68020
LOCALFUNC blnr Ui6r_IsNeg(ui6r *v)
{
	return ((si5b)v->hi) < 0;
}
#endif

#if Use68020
LOCALPROC mul_unsigned(ui5b src1, ui5b src2, ui6r *dst)
{
	ui5b src1_lo = ui5b_lo(src1);
	ui5b src2_lo = ui5b_lo(src2);
	ui5b src1_hi = ui5b_hi(src1);
	ui5b src2_hi = ui5b_hi(src2);

	ui5b r0 = src1_lo * src2_lo;
	ui5b r1 = src1_hi * src2_lo;
	ui5b r2 = src1_lo * src2_hi;
	ui5b r3 = src1_hi * src2_hi;

	ui5b ra1 = ui5b_hi(r0) + ui5b_lo(r1) + ui5b_lo(r2);

	dst->lo = (ui5b_lo(ra1) << 16) | ui5b_lo(r0);
	dst->hi = ui5b_hi(ra1) + ui5b_hi(r1) + ui5b_hi(r2) + r3;
}
#endif

#if Use68020
LOCALFUNC blnr div_unsigned(ui6r *src, ui5b div,
	ui5b *quot, ui5b *rem)
{
	int i;
	ui5b q = 0;
	ui5b cbit = 0;
	ui5b src_hi = src->hi;
	ui5b src_lo = src->lo;

	if (div <= src_hi) {
		return trueblnr;
	}
	for (i = 0 ; i < 32 ; i++) {
		cbit = src_hi & 0x80000000ul;
		src_hi <<= 1;
		if (src_lo & 0x80000000ul) {
			src_hi++;
		}
		src_lo <<= 1;
		q = q << 1;
		if (cbit || div <= src_hi) {
			q |= 1;
			src_hi -= div;
		}
	}
	*quot = q;
	*rem = src_hi;
	return falseblnr;
}
#endif

#if Use68020
LOCALPROC DoMulL(void)
{
	ui6r dst;
	ui5b srcvalue;
	ui4b extra = nextiword();
	ui5b r2 = (extra >> 12) & 7;
	ui5b dstvalue = m68k_dreg(r2);

	DecodeModeRegister(mode, reg);
	srcvalue = (ui5b)GetArgValue();

	if (extra & 0x800) {
		/* MULS.L - signed */

		si5b src1 = (si5b)srcvalue;
		si5b src2 = (si5b)dstvalue;
		flagtype s1 = src1 < 0;
		flagtype s2 = src2 < 0;
		flagtype sr = s1 != s2;

		/* ReportAbnormal("MULS.L"); */
		/* used by Sys 7.5.5 boot extensions */
		if (s1) {
			src1 = - src1;
		}
		if (s2) {
			src2 = - src2;
		}
		mul_unsigned((ui5b)src1, (ui5b)src2, &dst);
		if (sr) {
			Ui6r_Negate(&dst);
		}
		VFLG = CFLG = 0;
		ZFLG = Ui6r_IsZero(&dst);
		NFLG = Ui6r_IsNeg(&dst);
		if (extra & 0x400) {
			m68k_dreg(extra & 7) = dst.hi;
		} else {
			if ((dst.lo & 0x80000000) != 0) {
				if ((dst.hi & 0xffffffff) != 0xffffffff) {
					VFLG = 1;
				}
			} else {
				if (dst.hi != 0) {
					VFLG = 1;
				}
			}
		}
	} else {
		/* MULU.L - unsigned */

		/* ReportAbnormal("MULU.U"); */
		/* Used by various Apps */

		mul_unsigned(srcvalue, dstvalue, &dst);

		VFLG = CFLG = 0;
		ZFLG = Ui6r_IsZero(&dst);
		NFLG = Ui6r_IsNeg(&dst);
		if (extra & 0x400) {
			m68k_dreg(extra & 7) = dst.hi;
		} else {
			if (dst.hi != 0) {
				VFLG = 1;
			}
		}
	}
	m68k_dreg(r2) = dst.lo;
}
#endif

#if Use68020
LOCALPROC DoDivL(void)
{
	ui6r v2;
	ui5b src;
	ui5b quot;
	ui5b rem;
	ui4b extra = nextiword();
	ui5b rDr = extra & 7;
	ui5b rDq = (extra >> 12) & 7;

	DecodeModeRegister(mode, reg);
	src = (ui5b)GetArgValue();

	if (src == 0) {
		Exception(5);
		return;
	}
	if (extra & 0x0800) {
		/* signed variant */
		flagtype sr;
		flagtype s2;
		flagtype s1 = ((si5b)src < 0);

		v2.lo = (si5b)m68k_dreg(rDq);
		if (extra & 0x0400) {
			v2.hi = (si5b)m68k_dreg(rDr);
		} else {
			v2.hi = ((si5b)v2.lo) < 0 ? -1 : 0;
		}
		s2 = Ui6r_IsNeg(&v2);
		sr = (s1 != s2);
		if (s2) {
			Ui6r_Negate(&v2);
		}
		if (s1) {
			src = - src;
		}
		if (div_unsigned(&v2, src, &quot, &rem) ||
			sr ? quot > 0x80000000 : quot > 0x7fffffff) {
			VFLG = NFLG = 1;
			CFLG = 0;
		} else {
			if (sr) {
				quot = - quot;
			}
			if (((si5b)rem < 0) != s2) {
				rem = - rem;
			}
			VFLG = CFLG = 0;
			ZFLG = ((si5b)quot) == 0;
			NFLG = ((si5b)quot) < 0;
			m68k_dreg(rDr) = rem;
			m68k_dreg(rDq) = quot;
		}
	} else {
		/* unsigned */

		v2.lo = (ui5b)m68k_dreg(rDq);
		if (extra & 0x400) {
			v2.hi = (ui5b)m68k_dreg(rDr);
		} else {
			v2.hi = 0;
		}
		if (div_unsigned(&v2, src, &quot, &rem)) {
			VFLG = NFLG = 1;
			CFLG = 0;
		} else {
			VFLG = CFLG = 0;
			ZFLG = ((si5b)quot) == 0;
			NFLG = ((si5b)quot) < 0;
			m68k_dreg(rDr) = rem;
			m68k_dreg(rDq) = quot;
		}
	}
}
#endif

#if Use68020
LOCALPROC DoMoveToControl(void)
{
	if (! regs.s) {
		BackupPC();
		Exception(8);
	} else {
		ui4b src = nextiword();
		int regno = (src >> 12) & 0x0F;
		ui5b v = regs.regs[regno];

		switch (src & 0x0FFF) {
			case 0x0000:
				regs.sfc = v & 7;
				/* ReportAbnormal("DoMoveToControl: sfc"); */
				/* happens on entering macsbug */
				break;
			case 0x0001:
				regs.dfc = v & 7;
				/* ReportAbnormal("DoMoveToControl: dfc"); */
				break;
			case 0x0002:
				regs.cacr = v & 0x3;
				/* ReportAbnormal("DoMoveToControl: cacr"); */
				/* used by Sys 7.5.5 boot */
				break;
			case 0x0800:
				regs.usp = v;
				ReportAbnormal("DoMoveToControl: usp");
				break;
			case 0x0801:
				regs.vbr = v;
				/* ReportAbnormal("DoMoveToControl: vbr"); */
				/* happens on entering macsbug */
				break;
			case 0x0802:
				regs.caar = v &0xfc;
				/* ReportAbnormal("DoMoveToControl: caar"); */
				/* happens on entering macsbug */
				break;
			case 0x0803:
				regs.msp = v;
				if (regs.m == 1) {
					m68k_areg(7) = regs.msp;
				}
				/* ReportAbnormal("DoMoveToControl: msp"); */
				/* happens on entering macsbug */
				break;
			case 0x0804:
				regs.isp = v;
				if (regs.m == 0) {
					m68k_areg(7) = regs.isp;
				}
				ReportAbnormal("DoMoveToControl: isp");
				break;
			default:
				op_illg();
				ReportAbnormal("DoMoveToControl: unknown reg");
				break;
		}
	}
}
#endif

#if Use68020
LOCALPROC DoMoveFromControl(void)
{
	if (! regs.s) {
		BackupPC();
		Exception(8);
	} else {
		ui5b v;
		ui4b src = nextiword();
		int regno = (src >> 12) & 0x0F;

		switch (src & 0x0FFF) {
			case 0x0000:
				v = regs.sfc;
				/* ReportAbnormal("DoMoveFromControl: sfc"); */
				/* happens on entering macsbug */
				break;
			case 0x0001:
				v = regs.dfc;
				/* ReportAbnormal("DoMoveFromControl: dfc"); */
				/* happens on entering macsbug */
				break;
			case 0x0002:
				v = regs.cacr;
				/* ReportAbnormal("DoMoveFromControl: cacr"); */
				/* used by Sys 7.5.5 boot */
				break;
			case 0x0800:
				v = regs.usp;
				ReportAbnormal("DoMoveFromControl: usp");
				break;
			case 0x0801:
				v = regs.vbr;
				/* ReportAbnormal("DoMoveFromControl: vbr"); */
				/* happens on entering macsbug */
				break;
			case 0x0802:
				v = regs.caar;
				/* ReportAbnormal("DoMoveFromControl: caar"); */
				/* happens on entering macsbug */
				break;
			case 0x0803:
				v = (regs.m == 1)
					? m68k_areg(7)
					: regs.msp;
				/* ReportAbnormal("DoMoveFromControl: msp"); */
				/* happens on entering macsbug */
				break;
			case 0x0804:
				v = (regs.m == 0)
					? m68k_areg(7)
					: regs.isp;
				ReportAbnormal("DoMoveFromControl: isp");
				break;
			default:
				ReportAbnormal("DoMoveFromControl: unknown reg");
				op_illg();
				break;
		}
		regs.regs[regno] = v;
	}
}
#endif

#if Use68020
LOCALPROCUSEDONCE DoCodeMoveC(void)
{
	/* MOVEC 010011100111101m */
	/* ReportAbnormal("MOVEC"); */
	switch (reg) {
		case 2:
			DoMoveFromControl();
			break;
		case 3:
			DoMoveToControl();
			break;
		default:
			op_illg();
			break;
	}
}
#endif

#if Use68020
LOCALPROCUSEDONCE DoCodeChkL(void)
{
	/* Chk.L 0100ddd100mmmrrr */
	ReportAbnormal("CHK.L instruction");
	opsize = 4;
	DoCheck();
}
#endif

#if Use68020
LOCALPROCUSEDONCE DoCodeBkpt(void)
{
	/* BKPT 0100100001001rrr */
	ReportAbnormal("BKPT instruction");
	op_illg();
}
#endif

#if Use68020
LOCALPROCUSEDONCE DoCodeDivL(void)
{
	opsize = 4;
	/* DIVU 0100110001mmmrrr 0rrr0s0000000rrr */
	/* DIVS 0100110001mmmrrr 0rrr1s0000000rrr */
	ReportAbnormal("DIVS/DIVU long");
	DoDivL();
}
#endif

#if Use68020
LOCALPROCUSEDONCE DoCodeMulL(void)
{
	opsize = 4;
	/* MULU 0100110000mmmrrr 0rrr0s0000000rrr */
	/* MULS 0100110000mmmrrr 0rrr1s0000000rrr */
	DoMulL();
}
#endif

#if Use68020
LOCALPROCUSEDONCE DoCodeRtd(void)
{
	/* Rtd 0100111001110100 */
	ui5b NewPC = get_long(m68k_areg(7));
	si5b offs = (si5b)(si4b)nextiword();
	/* ReportAbnormal("RTD"); */
	/* used by Sys 7.5.5 boot */
	m68k_areg(7) += (4 + offs);
	m68k_setpc(NewPC);
}
#endif

#if Use68020
LOCALPROCUSEDONCE DoCodeLinkL(void)
{
	/* Link.L 0100100000001rrr */

	ui5b srcreg = reg;
	CPTR stackp = m68k_areg(7);

	ReportAbnormal("Link.L");

	stackp -= 4;
	m68k_areg(7) = stackp; /* only matters if srcreg == 7 */
	put_long(stackp, m68k_areg(srcreg));
	m68k_areg(srcreg) = stackp;
	m68k_areg(7) += (si5b)nextilong();
}
#endif

#if Use68020
LOCALPROCUSEDONCE DoCodeTRAPcc(void)
{
	/* TRAPcc 0101cccc11111sss */
	/* ReportAbnormal("TRAPcc"); */
	switch (reg) {
		case 2:
			ReportAbnormal("TRAPcc word data");
			(void) nextiword();
			break;
		case 3:
			ReportAbnormal("TRAPcc long data");
			(void) nextilong();
			break;
		case 4:
			/* no optional data */
			break;
		default:
			ReportAbnormal("TRAPcc illegal format");
			op_illg();
			break;
	}
	if (cctrue()) {
		ReportAbnormal("TRAPcc trapping");
		Exception(7);
		/* pc pushed onto stack wrong */
	}
}
#endif

#if Use68020
LOCALPROC DoUNPK(void)
{
	si5b val;
	ui5b m1 = ((opcode >> 3) & 1) << 2;
	ui5b srcreg = reg;
	ui5b dstreg = rg9;
	si5b offs = (si5b)(si4b)nextiword();

	opsize = 1;
	DecodeModeRegister(m1, srcreg);
	val = GetArgValue();

	val = (((val & 0xF0) << 4) | (val & 0x0F)) + offs;

	opsize = 2;
	DecodeModeRegister(m1, dstreg);
	SetArgValue(val);
}
#endif

#if Use68020
LOCALPROC DoPACK(void)
{
	si5b val;
	ui5b m1 = ((opcode >> 3) & 1) << 2;
	ui5b srcreg = reg;
	ui5b dstreg = rg9;
	si5b offs = (si5b)(si4b)nextiword();

	opsize = 2;
	DecodeModeRegister(m1, srcreg);
	val = GetArgValue();

	val += offs;
	val = ((val >> 4) & 0xf0) | (val & 0xf);

	opsize = 1;
	DecodeModeRegister(m1, dstreg);
	SetArgValue(val);
}
#endif

#if Use68020
LOCALPROCUSEDONCE DoCodePack(void)
{
	ReportAbnormal("PACK");
	DoPACK();
}
#endif

#if Use68020
LOCALPROCUSEDONCE DoCodeUnpk(void)
{
	ReportAbnormal("UNPK");
	DoUNPK();
}
#endif

#if Use68020
LOCALPROC DoBitField(void)
{
	ui5b tmp;
	ui5b newtmp;
	si5b dsta;
	ui5b bf0;
	ui5b bf1;
	ui5b dstreg = opcode & 7;
	ui4b extra = nextiword();
	si5b offset = ((extra & 0x0800) != 0)
		? m68k_dreg((extra >> 6) & 7)
		: ((extra >> 6) & 0x1f);
	ui5b width = ((extra & 0x0020) != 0)
		? m68k_dreg(extra & 7)
		: extra;

	ReportAbnormal("Bit Field operator");
	width = ((width - 1) & 0x1f) + 1; /* 0 -> 32 */
	if (mode == 0) {
		bf0 = m68k_dreg(dstreg);
		offset &= 0x1f;
		tmp = bf0 << offset;
	} else {
		DecodeModeRegister(mode, reg);
		if (ArgKind == AKMemory) { /* should check addr modes beforehand */
			dsta = ArgAddr.mem;
			dsta += (offset >> 3) | (offset & 0x80000000 ? ~ 0x1fffffff : 0);
			offset &= 7;
			{
				bf0 = get_long(dsta);
				bf1 = get_byte(dsta + 4) & 0xff;
				tmp = (bf0 << offset) | (bf1 >> (8 - offset));
			}
		}
	}

	NFLG = ((si5b)tmp) < 0;
	tmp >>= (32 - width);
	ZFLG = tmp == 0;
	VFLG = 0;
	CFLG = 0;

	newtmp = tmp;

	switch ((opcode >> 8) & 7) {
		case 0: /* BFTST */
			/* do nothing */
			break;
		case 1: /* BFEXTU */
			m68k_dreg((extra >> 12) & 7) = tmp;
			break;
		case 2: /* BFCHG */
			newtmp = (~ newtmp) & ((1 << width) - 1);
			break;
		case 3: /* BFEXTS */
			if (NFLG) {
				m68k_dreg((extra >> 12) & 7) = tmp
					| ((width == 32) ? 0 : (-1 << width));
			} else {
				m68k_dreg((extra >> 12) & 7) = tmp;
			}
			break;
		case 4: /* BFCLR */
			newtmp = 0;
			break;
		case 5: /* BFFFO */
			{
				ui5b mask = 1 << (width - 1);
				si5b i = offset;
				while (mask && ((tmp & mask) != 0)) {
					mask >>= 1;
					i++;
				}
				m68k_dreg((extra >> 12) & 7) = i;
			}
			break;
		case 6: /* BFSET */
			newtmp = (1 << width) - 1;
			break;
		case 7: /* BFINS */
			newtmp = m68k_dreg((extra >> 12) & 7) & ((1 << width) - 1);
			break;
	}

	if (newtmp != tmp) {
		ui5b mask = ~ ((((1 << width) - 1) << (32 - width)) >> offset);

		newtmp <<= (32 - width);
		bf0 = (bf0 & mask) | (newtmp >> offset);
		if (mode == 0) {
			m68k_dreg(dstreg) = bf0;
		} else {
			put_long(dsta, bf0);
			if ((offset + width) > 32) {
				bf1 = (bf1 & (0xff >> (width - 32 + offset)))
					| (newtmp << (8 - offset));
				put_byte(dsta + 4, bf1);
			}
		}
	}
}
#endif

enum {
	kIKindTst,
	kIKindCmp,
	kIKindCmpI,
	kIKindCmpM,
	kIKindBcc,
	kIKindBra,
	kIKindDBcc,
	kIKindSwap,
	kIKindMoveL,
	kIKindMoveW,
	kIKindMoveB,
	kIKindMoveAL,
	kIKindMoveAW,
	kIKindMoveQ,
	kIKindAddEaR,
	kIKindAddQ,
	kIKindAddI,
	kIKindAddREa,
	kIKindSubEaR,
	kIKindSubQ,
	kIKindSubI,
	kIKindSubREa,
	kIKindLea,
	kIKindPEA,
	kIKindA,
	kIKindBsr,
	kIKindJsr,
	kIKindLinkA6,
	kIKindMOVEMRmML,
	kIKindMOVEMApRL,
	kIKindUnlkA6,
	kIKindRts,
	kIKindJmp,
	kIKindClr,
	kIKindAddA,
	kIKindAddQA,
	kIKindSubA,
	kIKindSubQA,
	kIKindCmpA,
	kIKindAddXd,
	kIKindAddXm,
	kIKindSubXd,
	kIKindSubXm,
	kIKindRolopNM,
	kIKindRolopND,
	kIKindRolopDD,
	kIKindBitOpDD,
	kIKindBitOpDM,
	kIKindBitOpND,
	kIKindBitOpNM,
	kIKindAndI,
	kIKindAndEaD,
	kIKindAndDEa,
	kIKindOrI,
	kIKindOrDEa,
	kIKindOrEaD,
	kIKindEor,
	kIKindEorI,
	kIKindNot,
	kIKindScc,
	kIKindNegX,
	kIKindNeg,
	kIKindEXTW,
	kIKindEXTL,
	kIKindMulU,
	kIKindMulS,
	kIKindDivU,
	kIKindDivS,
	kIKindExgdd,
	kIKindExgaa,
	kIKindExgda,
	kIKindMoveCCREa,
	kIKindMoveEaCCR,
	kIKindMoveSREa,
	kIKindMoveEaSR,
	kIKindBinOpStatusCCR,
	kIKindMOVEMApRW,
	kIKindMOVEMRmMW,
	kIKindMOVEMrm,
	kIKindMOVEMmr,
	kIKindAbcdr,
	kIKindAbcdm,
	kIKindSbcdr,
	kIKindSbcdm,
	kIKindNbcd,
	kIKindRte,
	kIKindNop,
	kIKindMoveP,
	kIKindIllegal,
	kIKindChkW,
	kIKindTrap,
	kIKindTrapV,
	kIKindRtr,
	kIKindLink,
	kIKindUnlk,
	kIKindMoveRUSP,
	kIKindMoveUSPR,
	kIKindTas,
	kIKindF,
	kIKindCallMorRtm,
	kIKindStop,
	kIKindReset,

#if Use68020
	kIKindTRAPcc,
	kIKindChkL,
	kIKindBkpt,
	kIKindDivL,
	kIKindMulL,
	kIKindRtd,
	kIKindMoveC,
	kIKindLinkL,
	kIKindPack,
	kIKindUnpk,
	kIKindCHK2orCMP2,
	kIKindCAS2,
	kIKindCAS,
	kIKindMoveS,
	kIKindBitField,
#endif

	kNumIKinds
};

#if MakeDumpFile
LOCALVAR ui5b DumpTable[kNumIKinds];

LOCALPROC InitDumpTable(void)
{
	si5b i;

	for (i = 0; i < kNumIKinds; ++i) {
		DumpTable[i] = 0;
	}
}

IMPORTPROC DumpATable(ui5b *p, ui5b n);

EXPORTPROC DoDumpTable(void);
GLOBALPROC DoDumpTable(void)
{
	DumpATable(DumpTable, kNumIKinds);
}
#endif

LOCALVAR ui5b MaxInstructionsToGo;

LOCALPROC m68k_go_MaxInstructions(void)
{
	/* MaxInstructionsToGo must be >= 1 on entry */
	do {
		opcode = nextiword();

#if MakeDumpFile
		DumpTable[regs.disp_table[opcode]] ++;
#endif

		switch (regs.disp_table[opcode] /* opcode >> 12 */) {
			case kIKindTst :
				DoCodeTst();
				break;
			case kIKindCmp :
				DoCodeCmp();
				break;
			case kIKindCmpI :
				DoCodeCmpI();
				break;
			case kIKindCmpM :
				DoCodeCmpM();
				break;
			case kIKindBcc :
				DoCodeBcc();
				break;
			case kIKindBra :
				DoCodeBra();
				break;
			case kIKindDBcc :
				DoCodeDBcc();
				break;
			case kIKindSwap :
				DoCodeSwap();
				break;
			case kIKindMoveL :
				DoCodeMoveL();
				break;
			case kIKindMoveW :
				DoCodeMoveW();
				break;
			case kIKindMoveB :
				DoCodeMoveB();
				break;
			case kIKindMoveAL :
				DoCodeMoveAL();
				break;
			case kIKindMoveAW :
				DoCodeMoveAW();
				break;
			case kIKindMoveQ:
				DoCodeMoveQ();
				break;
			case kIKindAddEaR :
				DoCodeAddEaR();
				break;
			case kIKindAddQ :
				DoCodeAddQ();
				break;
			case kIKindAddI :
				DoCodeAddI();
				break;
			case kIKindAddREa :
				DoCodeAddREa();
				break;
			case kIKindSubEaR :
				DoCodeSubEaR();
				break;
			case kIKindSubQ :
				DoCodeSubQ();
				break;
			case kIKindSubI :
				DoCodeSubI();
				break;
			case kIKindSubREa :
				DoCodeSubREa();
				break;
			case kIKindLea :
				DoCodeLea();
				break;
			case kIKindPEA :
				DoCodePEA();
				break;
			case kIKindA :
				DoCodeA();
				break;
			case kIKindBsr :
				DoCodeBsr();
				break;
			case kIKindJsr :
				DoCodeJsr();
				break;
			case kIKindLinkA6 :
				DoCodeLinkA6();
				break;
			case kIKindMOVEMRmML :
				DoCodeMOVEMRmML();
				break;
			case kIKindMOVEMApRL :
				DoCodeMOVEMApRL();
				break;
			case kIKindUnlkA6 :
				DoCodeUnlkA6();
				break;
			case kIKindRts :
				DoCodeRts();
				break;
			case kIKindJmp :
				DoCodeJmp();
				break;
			case kIKindClr :
				DoCodeClr();
				break;
			case kIKindAddA :
				DoCodeAddA();
				break;
			case kIKindAddQA :
				DoCodeAddQA();
				break;
			case kIKindSubA :
				DoCodeSubA();
				break;
			case kIKindSubQA :
				DoCodeSubQA();
				break;
			case kIKindCmpA :
				DoCodeCmpA();
				break;
			case kIKindAddXd :
				DoCodeAddXd();
				break;
			case kIKindAddXm :
				DoCodeAddXm();
				break;
			case kIKindSubXd :
				DoCodeSubXd();
				break;
			case kIKindSubXm :
				DoCodeSubXm();
				break;

			case kIKindRolopNM :
				DoCodeRolopNM();
				break;
			case kIKindRolopND :
				DoCodeRolopND();
				break;
			case kIKindRolopDD :
				DoCodeRolopDD();
				break;
			case kIKindBitOpDD :
				DoCodeBitOpDD();
				break;
			case kIKindBitOpDM :
				DoCodeBitOpDM();
				break;
			case kIKindBitOpND :
				DoCodeBitOpND();
				break;
			case kIKindBitOpNM :
				DoCodeBitOpNM();
				break;

			case kIKindAndI :
				DoCodeAndI();
				break;
			case kIKindAndEaD :
				DoCodeAndEaD();
				break;
			case kIKindAndDEa :
				DoCodeAndDEa();
				break;
			case kIKindOrI :
				DoCodeOrI();
				break;
			case kIKindOrEaD :
				DoCodeOrEaD();
				break;
			case kIKindOrDEa :
				DoCodeOrDEa();
				break;
			case kIKindEor :
				DoCodeEor();
				break;
			case kIKindEorI :
				DoCodeEorI();
				break;
			case kIKindNot :
				DoCodeNot();
				break;

			case kIKindScc :
				DoCodeScc();
				break;
			case kIKindEXTL :
				DoCodeEXTL();
				break;
			case kIKindEXTW :
				DoCodeEXTW();
				break;
			case kIKindNeg :
				DoCodeNeg();
				break;
			case kIKindNegX :
				DoCodeNegX();
				break;

			case kIKindMulU :
				DoCodeMulU();
				break;
			case kIKindMulS :
				DoCodeMulS();
				break;
			case kIKindDivU :
				DoCodeDivU();
				break;
			case kIKindDivS :
				DoCodeDivS();
				break;
			case kIKindExgdd :
				DoCodeExgdd();
				break;
			case kIKindExgaa :
				DoCodeExgaa();
				break;
			case kIKindExgda :
				DoCodeExgda();
				break;

			case kIKindMoveCCREa :
				DoCodeMoveCCREa();
				break;
			case kIKindMoveEaCCR :
				DoCodeMoveEaCR();
				break;
			case kIKindMoveSREa :
				DoCodeMoveSREa();
				break;
			case kIKindMoveEaSR :
				DoCodeMoveEaSR();
				break;
			case kIKindBinOpStatusCCR :
				DoBinOpStatusCCR();
				break;

			case kIKindMOVEMApRW :
				DoCodeMOVEMApRW();
				break;
			case kIKindMOVEMRmMW :
				DoCodeMOVEMRmMW();
				break;
			case kIKindMOVEMrm :
				DoCodeMOVEMrm();
				break;
			case kIKindMOVEMmr :
				DoCodeMOVEMmr();
				break;

			case kIKindAbcdr :
				DoCodeAbcdr();
				break;
			case kIKindAbcdm :
				DoCodeAbcdm();
				break;
			case kIKindSbcdr :
				DoCodeSbcdr();
				break;
			case kIKindSbcdm :
				DoCodeSbcdm();
				break;
			case kIKindNbcd :
				DoCodeNbcd();
				break;

			case kIKindRte :
				DoCodeRte();
				break;
			case kIKindNop :
				DoCodeNop();
				break;
			case kIKindMoveP :
				DoCodeMoveP();
				break;
			case kIKindIllegal :
				op_illg();
				break;

			case kIKindChkW :
				DoCodeChkW();
				break;
			case kIKindTrap :
				DoCodeTrap();
				break;
			case kIKindTrapV :
				DoCodeTrapV();
				break;
			case kIKindRtr :
				DoCodeRtr();
				break;
			case kIKindLink :
				DoCodeLink();
				break;
			case kIKindUnlk :
				DoCodeUnlk();
				break;
			case kIKindMoveRUSP :
				DoCodeMoveRUSP();
				break;
			case kIKindMoveUSPR :
				DoCodeMoveUSPR();
				break;
			case kIKindTas :
				DoCodeTas();
				break;
			case kIKindF :
				DoCodeF();
				break;
			case kIKindCallMorRtm :
				DoCodeCallMorRtm();
				break;
			case kIKindStop :
				DoCodeStop();
				break;
			case kIKindReset :
				DoCodeReset();
				break;
#if Use68020
			case kIKindTRAPcc :
				DoCodeTRAPcc();
				break;
			case kIKindChkL :
				DoCodeChkL();
				break;
			case kIKindBkpt :
				DoCodeBkpt();
				break;
			case kIKindDivL :
				DoCodeDivL();
				break;
			case kIKindMulL :
				DoCodeMulL();
				break;
			case DoCodeRtd :
				DoCodeRtd();
				break;
			case kIKindMoveC :
				DoCodeMoveC();
				break;
			case kIKindLinkL :
				DoCodeLinkL();
				break;
			case kIKindPack :
				DoCodePack();
				break;
			case kIKindUnpk :
				DoCodeUnpk();
				break;
			case kIKindCHK2orCMP2 :
				DoCHK2orCMP2();
				break;
			case kIKindCAS2 :
				DoCAS2();
				break;
			case kIKindCAS :
				DoCAS();
				break;
			case kIKindMoveS :
				DoMOVES();
				break;
			case kIKindBitField :
				DoBitField();
				break;
#endif
		}

	} while (--MaxInstructionsToGo != 0);
}

LOCALVAR ui5b MoreInstructionsToGo;

LOCALPROC NeedToGetOut(void)
{
	if (MaxInstructionsToGo == 0) {
		/*
			already have gotten out, and exception processing has
			caused another exception, such as because a bad
			stack pointer pointing to a memory mapped device.
		*/
	} else {
		MoreInstructionsToGo += (MaxInstructionsToGo - 1);
			/* not counting the current instruction */
		MaxInstructionsToGo = 1;
	}
}

GLOBALFUNC ui5b GetInstructionsRemaining(void)
{
	return MoreInstructionsToGo + MaxInstructionsToGo;
}

GLOBALPROC SetInstructionsRemaining(ui5b n)
{
	if (MaxInstructionsToGo >= n) {
		MoreInstructionsToGo = 0;
		MaxInstructionsToGo = n;
	} else {
		MoreInstructionsToGo = n - MaxInstructionsToGo;
	}
}

LOCALPROC do_trace(void)
{
	regs.TracePending = trueblnr;
	NeedToGetOut();
}

LOCALPROC SetExternalInterruptPending(void)
{
	regs.ExternalInterruptPending = trueblnr;
	NeedToGetOut();
}

LOCALPROC m68k_setstopped(void)
{
	/* not implemented. doesn't seemed to be used on Mac Plus */
	Exception(4); /* fake an illegal instruction */
}

GLOBALPROC m68k_go_nInstructions(ui5b n)
{
	MaxInstructionsToGo = n;
	MoreInstructionsToGo = 0;
	do {

#if 0
		if (regs.ResetPending) {
			m68k_DoReset();
		}
#endif
		if (regs.TracePending) {
			Exception(9);
		}
		if (regs.ExternalInterruptPending) {
			regs.ExternalInterruptPending = falseblnr;
			DoCheckExternalInterruptPending();
		}
		if (regs.t1) {
			do_trace();
		}
		m68k_go_MaxInstructions();
		MaxInstructionsToGo = MoreInstructionsToGo;
		MoreInstructionsToGo = 0;
	} while (MaxInstructionsToGo != 0);
}

LOCALFUNC blnr IsValidAddrMode(void)
{
	return (mode != 7) || (reg < 5);
}

LOCALFUNC blnr IsValidDstAddrMode(void)
{
	return (md6 != 7) || (rg9 < 2);
}

LOCALFUNC MayNotInline ui3b CheckDataAltAddrMode(ui3b v)
{
	blnr IsOk;

	switch (mode) {
		case 1:
			IsOk = falseblnr;
			break;
		case 0:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
			IsOk = trueblnr;
			break;
		case 7:
			IsOk = reg < 2;
			break;
		default:
			IsOk = falseblnr; /* for compiler */
			break;
	}

	if (IsOk) {
		return v;
	} else {
		return kIKindIllegal;
	}
}

LOCALFUNC MayNotInline ui3b CheckDataAddrMode(ui3b v)
{
	blnr IsOk;

	switch (mode) {
		case 1:
			IsOk = falseblnr;
			break;
		case 0:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
			IsOk = trueblnr;
			break;
		case 7:
			IsOk = reg < 5;
			break;
		default:
			IsOk = falseblnr; /* for compiler */
			break;
	}

	if (IsOk) {
		return v;
	} else {
		return kIKindIllegal;
	}
}

LOCALFUNC MayNotInline ui3b CheckControlAddrMode(ui3b v)
{
	blnr IsOk;

	switch (mode) {
		case 0:
		case 1:
		case 3:
		case 4:
			IsOk = falseblnr;
			break;
		case 2:
		case 5:
		case 6:
			IsOk = trueblnr;
			break;
		case 7:
			IsOk = reg < 4;
			break;
		default:
			IsOk = falseblnr; /* for compiler */
			break;
	}

	if (IsOk) {
		return v;
	} else {
		return kIKindIllegal;
	}
}

LOCALFUNC MayNotInline ui3b CheckControlAltAddrMode(ui3b v)
{
	blnr IsOk;

	switch (mode) {
		case 0:
		case 1:
		case 3:
		case 4:
			IsOk = falseblnr;
			break;
		case 2:
		case 5:
		case 6:
			IsOk = trueblnr;
			break;
		case 7:
			IsOk = reg < 2;
			break;
		default:
			IsOk = falseblnr; /* for compiler */
			break;
	}

	if (IsOk) {
		return v;
	} else {
		return kIKindIllegal;
	}
}

LOCALFUNC MayNotInline ui3b CheckAltMemAddrMode(ui3b v)
{
	blnr IsOk;

	switch (mode) {
		case 0:
		case 1:
			IsOk = falseblnr;
			break;
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
			IsOk = trueblnr;
			break;
		case 7:
			IsOk = reg < 2;
			break;
		default:
			IsOk = falseblnr; /* for compiler */
			break;
	}

	if (IsOk) {
		return v;
	} else {
		return kIKindIllegal;
	}
}

LOCALFUNC MayNotInline ui3b CheckValidAddrMode(ui3b v)
{
	if (IsValidAddrMode()) {
		return v;
	} else {
		return kIKindIllegal;
	}
}

LOCALFUNC MayInline ui3b DeCode0(void)
{
	ui3b v;

	if (b8 == 1) {
		if (mode == 1) {
			/* MoveP 0000ddd1mm001aaa */
			v = kIKindMoveP;
		} else {
			/* dynamic bit, Opcode = 0000ddd1ttmmmrrr */
			if (mode == 0) {
				v = kIKindBitOpDD;
			} else {
				if (b76 == 0) {
					v = CheckDataAddrMode(kIKindBitOpDM);
				} else {
					v = CheckDataAltAddrMode(kIKindBitOpDM);
				}
			}
		}
	} else {
		if (rg9 == 4) {
			/* static bit 00001010ssmmmrrr */
			if (mode == 0) {
				v = kIKindBitOpND;
			} else {
				if (b76 == 0) {
					if ((mode == 7) && (reg == 4)) {
						v = kIKindIllegal;
					} else {
						v = CheckDataAddrMode(kIKindBitOpNM);
					}
				} else {
					v = CheckDataAltAddrMode(kIKindBitOpNM);
				}
			}
		} else
		if (b76 == 3) {
#if Use68020
			if (rg9 < 3) {
				/* CHK2 or CMP2 00000ss011mmmrrr */
				v = CheckControlAddrMode(kIKindCHK2orCMP2);
			} else
			if (rg9 >= 5) {
				if ((mode == 7) && (reg == 4)) {
					/* CAS2 00001ss011111100 */
					v = kIKindCAS2;
				} else {
					/* CAS  00001ss011mmmrrr */
					v = kIKindCAS;
				}
			} else
			if (rg9 == 3) {
				/* CALLM or RTM 0000011011mmmrrr */
				v = kIKindCallMorRtm;
			} else
#endif
			{
				v = kIKindIllegal;
			}
		} else
		if (rg9 == 6) {
			/* CMPI 00001100ssmmmrrr */
			v = CheckDataAltAddrMode(kIKindCmpI);
		} else if (rg9 == 7) {
#if Use68020
			/* MoveS 00001110ssmmmrrr */
			v = CheckAltMemAddrMode(kIKindMoveS);
#else
			v = kIKindIllegal;
#endif
		} else {
			if ((mode == 7) && (reg == 4)) {
				switch (rg9) {
					case 0 :
					case 1 :
					case 5 :
						v = kIKindBinOpStatusCCR;
						break;
					default:
						v = kIKindIllegal;
						break;
				}
			} else {
				switch (rg9) {
					case 0 :
						v = CheckDataAltAddrMode(kIKindOrI);
						break;
					case 1 :
						v = CheckDataAltAddrMode(kIKindAndI);
						break;
					case 2 :
						v = CheckDataAltAddrMode(kIKindSubI);
						break;
					case 3 :
						v = CheckDataAltAddrMode(kIKindAddI);
						break;
					case 5 :
						v = CheckDataAltAddrMode(kIKindEorI);
						break;
					default: /* for compiler. should be 0, 1, 2, 3, or 5 */
						v = kIKindIllegal;
						break;
				}
			}
		}
	}

	return v;
}

LOCALFUNC MayInline ui3b DeCode1(void)
{
	ui3b v;

	if ((mode == 1) || ! IsValidAddrMode()) {
		v = kIKindIllegal;
	} else if (md6 == 1) { /* MOVEA */
		v = kIKindIllegal;
	} else if (! IsValidDstAddrMode()) {
		v = kIKindIllegal;
	} else {
		v = kIKindMoveB;
	}

	return v;
}

LOCALFUNC MayInline ui3b DeCode2(void)
{
	ui3b v;

	if (md6 == 1) { /* MOVEA */
		v = CheckValidAddrMode(kIKindMoveAL);
	} else if (! IsValidAddrMode()) {
		v = kIKindIllegal;
	} else if (! IsValidDstAddrMode()) {
		v = kIKindIllegal;
	} else {
		v = kIKindMoveL;
	}

	return v;
}

LOCALFUNC MayInline ui3b DeCode3(void)
{
	ui3b v;

	if (md6 == 1) { /* MOVEA */
		v = CheckValidAddrMode(kIKindMoveAW);
	} else if (! IsValidAddrMode()) {
		v = kIKindIllegal;
	} else if (! IsValidDstAddrMode()) {
		v = kIKindIllegal;
	} else {
		v = kIKindMoveW;
	}

	return v;
}

LOCALFUNC MayInline ui3b DeCode4(void)
{
	ui3b v;

	if (b8 != 0) {
		switch (b76) {
			case 0 :
#if Use68020
				/* Chk.L 0100ddd100mmmrrr */
				v = CheckDataAddrMode(kIKindChkL);
#else
				v = kIKindIllegal;
#endif
				break;
			case 1 :
				v = kIKindIllegal;
				break;
			case 2 :
				/* Chk.W 0100ddd110mmmrrr */
				v = CheckDataAddrMode(kIKindChkW);
				break;
			case 3 :
				/* Lea 0100aaa111mmmrrr */
				v = CheckControlAddrMode(kIKindLea);
				break;
		}
	} else {
		switch (rg9) {
			case 0 :
				if (b76 != 3) {
					/* NegX 01000000ssmmmrrr */
					v = CheckDataAltAddrMode(kIKindNegX);
				} else {
#if Use68020
/* reference seems incorrect to say not for 68000 */
#endif
					/* Move from SR 0100000011mmmrrr */
					v = CheckDataAltAddrMode(kIKindMoveSREa);
				}
				break;
			case 1 :
				if (b76 != 3) {
					/* Clr 01000010ssmmmrrr */
					v = CheckDataAltAddrMode(kIKindClr);
				} else {
#if Use68020
					/* Move from CCR 0100001011mmmrrr */
					v = CheckDataAltAddrMode(kIKindMoveCCREa);
#else
					v = kIKindIllegal;
#endif
				}
				break;
			case 2 :
				if (b76 != 3) {
					/* Neg 01000100ssmmmrrr */
					v = CheckDataAltAddrMode(kIKindNeg);
				} else {
					/* Move to CCR 0100010011mmmrrr */
					v = CheckDataAddrMode(kIKindMoveEaCCR);
				}
				break;
			case 3 :
				if (b76 != 3) {
					/* Not 01000110ssmmmrrr */
					v = CheckDataAltAddrMode(kIKindNot);
				} else {
					/* Move from SR 0100011011mmmrrr */
					v = CheckDataAddrMode(kIKindMoveEaSR);
				}
				break;
			case 4 :
				switch (b76) {
					case 0 :
#if Use68020
						if (mode == 1) {
							/* Link.L 0100100000001rrr */
							v = kIKindLinkL;
						} else
#endif
						{
							/* Nbcd 0100100000mmmrrr */
							v = CheckDataAltAddrMode(kIKindNbcd);
						}
						break;
					case 1 :
						if (mode == 0) {
							/* Swap 0100100001000rrr */
							v = kIKindSwap;
						} else
#if Use68020
						if (mode == 1) {
							v = kIKindBkpt;
						} else
#endif
						{
							/* PEA 0100100001mmmrrr */
							v = CheckControlAddrMode(kIKindPEA);
						}
						break;
					case 2 :
						if (mode == 0) {
							/* EXT.W */
							v = kIKindEXTW;
						} else {
							/* MOVEM reg to mem 01001d001ssmmmrrr */
							if (mode == 4) {
								v = kIKindMOVEMRmMW;
							} else {
								v = CheckControlAltAddrMode(kIKindMOVEMrm);
							}
						}
						break;
					case 3 :
						if (mode == 0) {
							/* EXT.L */
							v = kIKindEXTL;
						} else {
							/* MOVEM reg to mem 01001d001ssmmmrrr */
							if (mode == 4) {
								v = kIKindMOVEMRmML;
							} else {
								v = CheckControlAltAddrMode(kIKindMOVEMrm);
							}
						}
						break;
				}
				break;
			case 5 :
				if (b76 == 3) {
					if ((mode == 7) && (reg == 4)) {
						/* the ILLEGAL instruction */
						v = kIKindIllegal;
					} else {
						/* Tas 0100101011mmmrrr */
						v = CheckDataAltAddrMode(kIKindTas);
					}
				} else {
					/* Tst  01001010ssmmmrrr */
					if (b76 == 0) {
						v = CheckDataAltAddrMode(kIKindTst);
					} else {
						v = CheckValidAddrMode(kIKindTst);
					}
				}
				break;
			case 6 :
				if (((opcode >> 7) & 1) == 1) {
					/* MOVEM mem to reg 0100110011smmmrrr */
					if (mode == 3) {
						if (b76 == 2) {
							v = kIKindMOVEMApRW;
						} else {
							v = kIKindMOVEMApRL;
						}
					} else {
						v = CheckControlAddrMode(kIKindMOVEMmr);
					}
				} else {
#if Use68020
					if (((opcode >> 7) & 1) == 1) {
						/* DIVU 0100110001mmmrrr 0rrr0s0000000rrr */
						/* DIVS 0100110001mmmrrr 0rrr1s0000000rrr */
						v = kIKindDivL;
					} else {
						/* MULU 0100110000mmmrrr 0rrr0s0000000rrr */
						/* MULS 0100110000mmmrrr 0rrr1s0000000rrr */
						v = kIKindMulL;
					}
#else
					v = kIKindIllegal;
#endif
				}
				break;
			case 7 :
				switch (b76) {
					case 0 :
						v = kIKindIllegal;
						break;
					case 1 :
						switch (mode) {
							case 0 :
							case 1 :
								/* Trap 010011100100vvvv */
								v = kIKindTrap;
								break;
							case 2 :
								/* Link */
								if (reg == 6) {
									v = kIKindLinkA6;
								} else {
									v = kIKindLink;
								}
								break;
							case 3 :
								/* Unlk */
								if (reg == 6) {
									v = kIKindUnlkA6;
								} else {
									v = kIKindUnlk;
								}
								break;
							case 4 :
								/* MOVE USP 0100111001100aaa */
								v = kIKindMoveRUSP;
								break;
							case 5 :
								/* MOVE USP 0100111001101aaa */
								v = kIKindMoveUSPR;
								break;
							case 6 :
								switch (reg) {
									case 0 :
										/* Reset 0100111001100000 */
										v = kIKindReset;
										break;
									case 1 :
										/* Nop Opcode = 0100111001110001 */
										v = kIKindNop;
										break;
									case 2 :
										/* Stop 0100111001110010 */
										v = kIKindStop;
										break;
									case 3 :
										/* Rte 0100111001110011 */
										v = kIKindRte;
										break;
									case 4 :
										/* Rtd 0100111001110100 */
#if Use68020
										v = kIKindRtd;
#else
										v = kIKindIllegal;
#endif
										break;
									case 5 :
										/* Rts 0100111001110101 */
										v = kIKindRts;
										break;
									case 6 :
										/* TrapV 0100111001110110 */
										v = kIKindTrapV;
										break;
									case 7 :
										/* Rtr 0100111001110111 */
										v = kIKindRtr;
										break;
								}
								break;
							case 7 :
#if Use68020
								/* MOVEC 010011100111101m */
								v = kIKindMoveC;
#else
								v = kIKindIllegal;
#endif
								break;
						}
						break;
					case 2 :
						/* Jsr 0100111010mmmrrr */
						v = CheckControlAddrMode(kIKindJsr);
						break;
					case 3 :
						/* JMP 0100111011mmmrrr */
						v = CheckControlAddrMode(kIKindJmp);
						break;
				}
				break;
		}
	}

	return v;
}

LOCALFUNC MayInline ui3b DeCode5(void)
{
	ui3b v;

	if (b76 == 3) {
		if (mode == 1) {
			/* DBcc 0101cccc11001ddd */
			v = kIKindDBcc;
		} else {
#if Use68020
			if ((mode == 7) && (reg >= 2)) {
				/* TRAPcc 0101cccc11111sss */
				v = kIKindTRAPcc;
			} else
#endif
			{
				/* Scc 0101cccc11mmmrrr */
				v = CheckDataAltAddrMode(kIKindScc);
			}
		}
	} else {
		if (mode == 1) {
			if (b8 == 0) {
				v = kIKindAddQA; /* AddQA 0101nnn0ss001rrr */
			} else {
				v = kIKindSubQA; /* SubQA 0101nnn1ss001rrr */
			}
		} else {
			if (b8 == 0) {
				/* AddQ 0101nnn0ssmmmrrr */
				v = CheckDataAltAddrMode(kIKindAddQ);
			} else {
				/* SubQ 0101nnn1ssmmmrrr */
				v = CheckDataAltAddrMode(kIKindSubQ);
			}
		}
	}

	return v;
}

LOCALFUNC MayInline ui3b DeCode6(void)
{
	ui3b v;
	ui5b cond = (opcode >> 8) & 15;

	if (cond == 1) {
		/* Bsr 01100001nnnnnnnn */
		v = kIKindBsr;
	} else if (cond == 0) {
		/* Bra 01100000nnnnnnnn */
		v = kIKindBra;
	} else {
		/* Bcc 0110ccccnnnnnnnn */
		v = kIKindBcc;
	}

	return v;
}

LOCALFUNC MayInline ui3b DeCode7(void)
{
	ui3b v;

	if (b8 == 0) {
		v = kIKindMoveQ;
	} else {
		v = kIKindIllegal;
	}

	return v;
}

LOCALFUNC MayInline ui3b DeCode8(void)
{
	ui3b v;

	if (b76 == 3) {
		if (b8 == 0) {
			/* DivU 1000ddd011mmmrrr */
			v = CheckDataAddrMode(kIKindDivU);
		} else {
			/* DivS 1000ddd111mmmrrr */
			v = CheckDataAddrMode(kIKindDivS);
		}
	} else {
		if (b8 == 0) {
			/* OR 1000ddd0ssmmmrrr */
			v = CheckDataAddrMode(kIKindOrEaD);
		} else {
			if (mode < 2) {
				switch (b76) {
					case 0:
						/* SBCD 1000xxx10000mxxx */
						if (mode == 0) {
							v = kIKindSbcdr;
						} else {
							v = kIKindSbcdm;
						}
						break;
#if Use68020
					case 1:
						/* PACK 1000rrr10100mrrr */
						v = kIKindPack;
						break;
					case 2:
						/* UNPK 1000rrr11000mrrr */
						v = kIKindUnpk;
						break;
#endif
					default:
						v = kIKindIllegal;
						break;
				}
			} else {
				/* OR 1000ddd1ssmmmrrr */
				v = CheckDataAltAddrMode(kIKindOrDEa);
			}
		}
	}

	return v;
}

LOCALFUNC MayInline ui3b DeCode9(void)
{
	ui3b v;

	if (b76 == 3) {
		/* SUBA 1001dddm11mmmrrr */
		v = CheckValidAddrMode(kIKindSubA);
	} else {
		if (b8 == 0) {
			/* SUB 1001ddd0ssmmmrrr */
			v = CheckValidAddrMode(kIKindSubEaR);
		} else {
			if (mode == 0) {
				/* SUBX 1001ddd1ss000rrr */
				v = kIKindSubXd;
			} else if (mode == 1) {
				/* SUBX 1001ddd1ss001rrr */
				v = kIKindSubXm;
			} else {
				/* SUB 1001ddd1ssmmmrrr */
				v = CheckAltMemAddrMode(kIKindSubREa);
			}
		}
	}

	return v;
}

LOCALFUNC MayInline ui3b DeCodeB(void)
{
	ui3b v;

	if (b76 == 3) {
		/* CMPA 1011ddds11mmmrrr */
		v = CheckValidAddrMode(kIKindCmpA);
	} else if (b8 == 1) {
		if (mode == 1) {
			/* CmpM 1011ddd1ss001rrr */
			v = kIKindCmpM;
		} else {
			/* Eor 1011ddd1ssmmmrrr */
			v = CheckDataAltAddrMode(kIKindEor);
		}
	} else {
		/* Cmp 1011ddd0ssmmmrrr */
		v = CheckValidAddrMode(kIKindCmp);
	}

	return v;
}

LOCALFUNC MayInline ui3b DeCodeC(void)
{
	ui3b v;

	if (b76 == 3) {
		if (b8 == 0) {
			/* MulU 1100ddd011mmmrrr */
			v = CheckDataAddrMode(kIKindMulU);
		} else {
			/* MulS 1100ddd111mmmrrr */
			v = CheckDataAddrMode(kIKindMulS);
		}
	} else {
		if (b8 == 0) {
			/* And 1100ddd0ssmmmrrr */
			v = CheckDataAddrMode(kIKindAndEaD);
		} else {
			if (mode < 2) {
				switch (b76) {
					case 0 :
						/* ABCD 1100ddd10000mrrr */
						if (mode == 0) {
							v = kIKindAbcdr;
						} else {
							v = kIKindAbcdm;
						}
						break;
					case 1 :
						/* Exg 1100ddd10100trrr */
						if (mode == 0) {
							v = kIKindExgdd;
						} else {
							v = kIKindExgaa;
						}
						break;
					case 2 :
						if (mode == 0) {
							v = kIKindIllegal;
						} else {
							/* Exg 1100ddd110001rrr */
							v = kIKindExgda;
						}
						break;
				}
			} else {
				/* And 1100ddd1ssmmmrrr */
				v = CheckAltMemAddrMode(kIKindAndDEa);
			}
		}
	}

	return v;
}

LOCALFUNC MayInline ui3b DeCodeD(void)
{
	ui3b v;

	if (b76 == 3) {
		/* ADDA 1101dddm11mmmrrr */
		v = CheckValidAddrMode(kIKindAddA);
	} else {
		if (b8 == 0) {
			/* ADD 1101ddd0ssmmmrrr */
			v = CheckValidAddrMode(kIKindAddEaR);
		} else {
			if (mode == 0) {
				v = kIKindAddXd;
			} else if (mode == 1) {
				v = kIKindAddXm;
			} else {
				/* ADD 1101ddd1ssmmmrrr */
				v = CheckAltMemAddrMode(kIKindAddREa);
			}
		}
	}

	return v;
}

LOCALFUNC MayInline ui3b DeCodeE(void)
{
	ui3b v;

	if (b76 == 3) {
		if ((opcode & 0x0800) != 0) {
#if Use68020
			/* 11101???11mmmrrr */
			v = kIKindBitField;
#else
			v = kIKindIllegal;
#endif
		} else {
			/* 11100ttd11mmmddd */
			v = CheckAltMemAddrMode(kIKindRolopNM);
		}
	} else {
		if (mode < 4) {
			/* 1110cccdss0ttddd */
			v = kIKindRolopND;
		} else {
			/* 1110rrrdss1ttddd */
			v = kIKindRolopDD;
		}
	}

	return v;
}

LOCALPROC SetUp_disp_table(void)
{
	ui5b i;
	ui3b v;

	for (i = 0; i < (ui5b)256 * 256; ++i) {
		opcode = i;
		switch (opcode >> 12) {
			case 0x0 :
				v = DeCode0();
				break;
			case 0x1 :
				v = DeCode1();
				break;
			case 0x2 :
				v = DeCode2();
				break;
			case 0x3 :
				v = DeCode3();
				break;
			case 0x4 :
				v = DeCode4();
				break;
			case 0x5 :
				v = DeCode5();
				break;
			case 0x6 :
				v = DeCode6();
				break;
			case 0x7:
				v = DeCode7();
				break;
			case 0x8:
				v = DeCode8();
				break;
			case 0x9:
				v = DeCode9();
				break;
			case 0xA :
				v = kIKindA;
				break;
			case 0xB :
				v = DeCodeB();
				break;
			case 0xC :
				v = DeCodeC();
				break;
			case 0xD:
				v = DeCodeD();
				break;
			case 0xE :
				v = DeCodeE();
				break;
			case 0xF :
				v = kIKindF;
				break;
		}

		regs.disp_table[opcode] = v;
	}
}
