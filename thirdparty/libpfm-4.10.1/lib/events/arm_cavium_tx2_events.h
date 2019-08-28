/*
 * Copyright (c) 2018 Cavium, Inc
 * Contributed by Steve Walk <swalk.cavium@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Cavium ThunderX2
 *
 * ARM Architecture Reference Manual, ARMv8, for ARMv8-A architecture profile,
 * ARM DDI 0487B.a (ID033117)
 */

static const arm_entry_t arm_thunderx2_pe[]={
	{.name = "SW_INCR",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x00,
	 .desc = "Instruction architecturally executed (condition check pass) software increment"
	},
	{.name = "L1I_CACHE_REFILL",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x01,
	 .desc = "Level 1 instruction cache refill"
	},
	{.name = "L1I_TLB_REFILL",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x02,
	 .desc = "Level 1 instruction TLB refill"
	},
	{.name = "L1D_CACHE_REFILL",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x03,
	 .desc = "Level 1 data cache refill"
	},
	{.name = "L1D_CACHE",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x04,
	 .desc = "Level 1 data cache access"
	},
	{.name = "L1D_TLB_REFILL",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x05,
	 .desc = "Level 1 data TLB refill"
	},
	{.name = "LD_RETIRED",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x06,
	 .desc = "Instruction architecturally executed (condition check pass) - Load"
	},
	{.name = "ST_RETIRED",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x07,
	 .desc = "Instruction architecturally executed (condition check pass) - Store"
	},
	{.name = "INST_RETIRED",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x08,
	 .desc = "Instruction architecturally executed"
	},
	{.name = "EXC_TAKEN",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x09,
	 .desc = "Exception taken"
	},
	{.name = "EXC_RETURN",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x0A,
	 .desc = "Instruction architecturally executed (condition check pass) - Exception return"
	},
	{.name = "CID_WRITE_RETIRED",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x0B,
	 .desc = "Instruction architecturally executed (condition check pass) - Write to CONTEXTIDR"
	},
	{.name = "BR_IMMED_RETIRED",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x0D,
	 .desc = "Instruction architecturally executed, immediate branch"
	},
	{.name = "BR_RETURN_RETIRED",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x0E,
	 .desc = "Instruction architecturally executed (condition check pass) - procedure return"
	},
	{.name = "UNALIGNED_LDST_RETIRED",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x0F,
	 .desc = "Instruction architecturally executed (condition check pass), unaligned load/store"
	},
	{.name = "BR_MIS_PRED",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x10,
	 .desc = "Mispredicted or not predicted branch speculatively executed"
	},
	{.name = "CPU_CYCLES",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x11,
	 .desc = "Cycles"
	},
	{.name = "BR_PRED",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x12,
	 .desc = "Predictable branch speculatively executed"
	},
	{.name = "MEM_ACCESS",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x13,
	 .desc = "Data memory access"
	},
	{.name = "L1I_CACHE",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x14,
	 .desc = "Level 1 instruction cache access"
	},
	{.name = "L1D_CACHE_WB",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x15,
	 .desc = "Level 1 data cache write-back"
	},
	{.name = "L2D_CACHE",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x16,
	 .desc = "Level 2 data cache access"
	},
	{.name = "L2D_CACHE_REFILL",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x17,
	 .desc = "Level 2 data cache refill"
	},
	{.name = "L2D_CACHE_WB",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x18,
	 .desc = "Level 2 data cache write-back"
	},
	{.name = "BUS_ACCESS",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x19,
	 .desc = "Bus access"
	},
	{.name = "INST_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x1B,
	 .desc = "Instruction speculatively executed"
	},
	{.name = "TTBR_WRITE_RETIRED",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x1C,
	 .desc = "Instruction architecturally executed (condition check pass)  Write to translation table base"
	},
	{.name = "L1D_CACHE_ALLOCATE",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x1F,
	 .desc = "Level 1 data cache allocation without refill"
	},
	{.name = "L2D_CACHE_ALLOCATE",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x20,
	 .desc = "Level 2 data/unified cache allocation without refill"
	},
	{.name = "BR_RETIRED",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x21,
	 .desc = "Counts all branches on the architecturally executed path that would incur cost if mispredicted"
	},
	{.name = "BR_MIS_PRED_RETIRED",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x22,
	 .desc = "Instructions executed, mis-predicted branch. All instructions counted by BR_RETIRED that were not correctly predicted"
	},
	{.name = "STALL_FRONTEND",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x23,
	 .desc = "Cycle on which no operation issued because there were no operations to issue"
	},
	{.name = "STALL_BACKEND",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x24,
	 .desc = "Cycle on which no operation issued due to back-end resources being unavailable"
	},
	{.name = "L1D_TLB",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x25,
	 .desc = "Level 1 data TLB access"
	},
	{.name = "L1I_TLB",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x26,
	 .desc = "Instruction TLB access"
	},
	{.name = "L2D_TLB_REFILL",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x2D,
	 .desc = "Attributable memory-read or attributable memory-write operation that causes a TLB refill"
	},
	{.name = "L2I_TLB_REFILL",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x2E,
	 .desc = "Attributable instruction memory access that causes a TLB refill"
	},
	{.name = "L2D_TLB",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x2F,
	 .desc = "Attributable memory read operation or attributable memory write operation that causes a TLB access"
	},
	{.name = "L2I_TLB",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x30,
	 .desc = "Attributable memory read operation or attributable memory write operation that causes a TLB access"
	},
	{.name = "L1D_CACHE_RD",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x40,
	 .desc = "Level 1 data cache access, read"
	},
	{.name = "L1D_CACHE_WR",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x41,
	 .desc = "Level 1 data cache access, write"
	},
	{.name = "L1D_CACHE_REFILL_RD",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x42,
	 .desc = "Level 1 data cache refill, read"
	},
	{.name = "L1D_CACHE_REFILL_WR",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x43,
	 .desc = "Level 1 data cache refill, write"
	},
	{.name = "L1D_CACHE_REFILL_INNER",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x44,
	 .desc = "Level 1 data cache refill, inner"
	},
	{.name = "L1D_CACHE_REFILL_OUTER",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x45,
	 .desc = "Level 1 data cache refill, outer"
	},
	{.name = "L1D_CACHE_WB_VICTIM",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x46,
	 .desc = "Level 1 data cache write-back, victim"
	},
	{.name = "L1D_CACHE_WB_CLEAN",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x47,
	 .desc = "Level 1 data cache write-back, cleaning and coherency"
	},
	{.name = "L1D_CACHE_INVAL",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x48,
	 .desc = "Level 1 data cache invalidate"
	},
	{.name = "L1D_TLB_REFILL_RD",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x4C,
	 .desc = "Level 1 data TLB read refill"
	},
	{.name = "L1D_TLB_REFILL_WR",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x4D,
	 .desc = "Level 1 data TLB write refill"
	},
	{.name = "L1D_TLB_RD",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x4E,
	 .desc = "Level 1 data TLB access, read"
	},
	{.name = "L1D_TLB_WR",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x4F,
	 .desc = "Level 1 data TLB access, write"
	},
	{.name = "L2D_CACHE_RD",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x50,
	 .desc = "Level 2 data cache access, read"
	},
	{.name = "L2D_CACHE_WR",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x51,
	 .desc = "Level 2 data cache access, write"
	},
	{.name = "L2D_CACHE_REFILL_RD",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x52,
	 .desc = "Level 2 data cache refill, read"
	},
	{.name = "L2D_CACHE_REFILL_WR",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x53,
	 .desc = "Level 2 data cache refill, write"
	},
	{.name = "L2D_CACHE_WB_VICTIM",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x56,
	 .desc = "Level 2 data cache write-back, victim"
	},
	{.name = "L2D_CACHE_WB_CLEAN",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x57,
	 .desc = "Level 2 data cache write-back, cleaning and coherency"
	},
	{.name = "L2D_CACHE_INVAL",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x58,
	 .desc = "Level 2 data cache invalidate"
	},
	{.name = "L2D_TLB_REFILL_RD",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x5C,
	 .desc = "Level 2 data/unified TLB refill, read"
	},
	{.name = "L2D_TLB_REFILL_WR",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x5D,
	 .desc = "Level 2 data/unified TLB refill, write"
	},
	{.name = "L2D_TLB_RD",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x5E,
	 .desc = "Level 2 data/unified TLB access, read"
	},
	{.name = "L2D_TLB_WR",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x5F,
	 .desc = "Level 2 data/unified TLB access, write"
	},
	{.name = "BUS_ACCESS_RD",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x60,
	 .desc = "Bus access, read"
	},
	{.name = "BUS_ACCESS_WR",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x61,
	 .desc = "Bus access, write"
	},
	{.name = "BUS_ACCESS_SHARED",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x62,
	 .desc = "Bus access, normal, cacheable, shareable"
	},
	{.name = "BUS_ACCESS_NOT_SHARED",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x63,
	 .desc = "Bus not normal access"
	},
	{.name = "BUS_ACCESS_NORMAL",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x64,
	 .desc = "Bus access, normal"
	},
	{.name = "BUS_ACCESS_PERIPH",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x65,
	 .desc = "Bus access, peripheral"
	},
	{.name = "MEM_ACCESS_RD",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x66,
	 .desc = "Data memory access, read"
	},
	{.name = "MEM_ACCESS_WR",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x67,
	 .desc = "Data memory access, write"
	},
	{.name = "UNALIGNED_LD_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x68,
	 .desc = "Unaligned access, read"
	},
	{.name = "UNALIGNED_ST_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x69,
	 .desc = "Unaligned access, write"
	},
	{.name = "UNALIGNED_LDST_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x6A,
	 .desc = "Unaligned access"
	},
	{.name = "LDREX_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x6C,
	 .desc = "Exclusive operation speculatively executed - LDREX or LDX"
	},
	{.name = "STREX_PASS_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x6D,
	 .desc = "Exclusive operation speculative executed - STREX or STX pass"
	},
	{.name = "STREX_FAIL_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x6E,
	 .desc = "Exclusive operation speculative executed - STREX or STX fail"
	},
	{.name = "STREX_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x6F,
	 .desc = "Exclusive operation speculatively executed - STREX or STX"
	},
	{.name = "LD_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x70,
	 .desc = "Operation speculatively executed, load"
	},
	{.name = "ST_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x71,
	 .desc = "Operation speculatively executed, store"
	},
	{.name = "LDST_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x72,
	 .desc = "Operation speculatively executed, load or store"
	},
	{.name = "DP_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x73,
	 .desc = "Operation speculatively executed, data-processing"
	},
	{.name = "ASE_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x74,
	 .desc = "Operation speculatively executed, Advanced SIMD instruction"
	},
	{.name = "VFP_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x75,
	 .desc = "Operation speculatively executed, floating point instruction"
	},
	{.name = "CRYPTO_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x77,
	 .desc = "Operation speculatively executed, Cryptographic instruction"
	},
	{.name = "BR_IMMED_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x78,
	 .desc = "Branch speculatively executed, immediate branch"
	},
	{.name = "BR_RETURN_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x79,
	 .desc = "Branch speculatively executed, return"
	},
	{.name = "BR_INDIRECT_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x7A,
	 .desc = "Branch speculatively executed, indirect branch"
	},
	{.name = "ISB_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x7C,
	 .desc = "Barrier speculatively executed, ISB"
	},
	{.name = "DSB_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x7D,
	 .desc = "barrier speculatively executed, DSB"
	},
	{.name = "DMB_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x7E,
	 .desc = "Barrier speculatively executed, DMB"
	},
	{.name = "EXC_UNDEF",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x81,
	 .desc = "Exception taken, other synchronous"
	},
	{.name = "EXC_SVC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x82,
	 .desc = "Exception taken, supervisor call"
	},
	{.name = "EXC_PABORT",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x83,
	 .desc = "Exception taken, instruction abort"
	},
	{.name = "EXC_DABORT",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x84,
	 .desc = "Exception taken, data abort or SError"
	},
	{.name = "EXC_IRQ",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x86,
	 .desc = "Exception taken, irq"
	},
	{.name = "EXC_FIQ",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x87,
	 .desc = "Exception taken, fiq"
	},
	{.name = "EXC_SMC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x88,
	 .desc = "Exception taken, smc"
	},
	{.name = "EXC_HVC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x8A,
	 .desc = "Exception taken, hypervisor call"
	},
	{.name = "EXC_TRAP_PABORT",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x8B,
	 .desc = "Exception taken, instruction abort not taken locally"
	},
	{.name = "EXC_TRAP_DABORT",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x8C,
	 .desc = "Exception taken, data abort or SError not taken locally"
	},
	{.name = "EXC_TRAP_OTHER",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x8D,
	 .desc = "Exception taken, other traps not taken locally"
	},
	{.name = "EXC_TRAP_IRQ",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x8E,
	 .desc = "Exception taken, irq not taken locally"
	},
	{.name = "EXC_TRAP_FIQ",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x8F,
	 .desc = "Exception taken, fiq not taken locally"
	},
	{.name = "RC_LD_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x90,
	 .desc = "Release consistency instruction speculatively executed (load-acquire)"
	},
	{.name = "RC_ST_SPEC",
	 .modmsk = ARMV8_ATTRS,
	 .code = 0x91,
	 .desc = "Release consistency instruction speculatively executed (store-release)"
	},

	/* END Cavium ThunderX2 specific events */
};
