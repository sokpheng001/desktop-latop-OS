/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * AArch64 exception frame layout shared by vectors.S and the C handler.
 */

#ifndef OS001_ARCH_AARCH64_EXCEPTION_H
#define OS001_ARCH_AARCH64_EXCEPTION_H

/* Byte offsets into the frame vectors.S builds on the stack. */
#define EXC_FRAME_X0   0 /* x0..x30, 8 bytes each */
#define EXC_FRAME_X30  240
#define EXC_FRAME_ELR  248
#define EXC_FRAME_SPSR 256
#define EXC_FRAME_ESR  264
#define EXC_FRAME_FAR  272
#define EXC_FRAME_SP0  280 /* SP_EL0: the user stack pointer of the interrupted thread */
#define EXC_FRAME_SIZE 288

/*
 * Vector table slots (Arm ARM DDI 0487, D1 "Exception vectors"): four groups of four,
 * by where the exception came from and its type. kind = group * 4 + type.
 */
#define EXC_GROUP_CUR_SP0   0 /* current EL, using SP_EL0 */
#define EXC_GROUP_CUR_SPX   1 /* current EL, using SP_ELx */
#define EXC_GROUP_LOWER_A64 2 /* lower EL running AArch64 */
#define EXC_GROUP_LOWER_A32 3 /* lower EL running AArch32 */
#define EXC_TYPE_SYNC	    0
#define EXC_TYPE_IRQ	    1
#define EXC_TYPE_FIQ	    2
#define EXC_TYPE_SERROR	    3
#define EXC_TYPES_PER_GROUP 4

#ifndef __ASSEMBLY__

#include "compiler.h"
#include "types.h"

struct exc_frame {
	u64 x[31];
	u64 elr;
	u64 spsr;
	u64 esr;
	u64 far;
	u64 sp_el0;
};

STATIC_ASSERT(OFFSET_OF(struct exc_frame, x[30]) == EXC_FRAME_X30, "frame x30 offset");
STATIC_ASSERT(OFFSET_OF(struct exc_frame, elr) == EXC_FRAME_ELR, "frame elr offset");
STATIC_ASSERT(OFFSET_OF(struct exc_frame, spsr) == EXC_FRAME_SPSR, "frame spsr offset");
STATIC_ASSERT(OFFSET_OF(struct exc_frame, esr) == EXC_FRAME_ESR, "frame esr offset");
STATIC_ASSERT(OFFSET_OF(struct exc_frame, far) == EXC_FRAME_FAR, "frame far offset");
STATIC_ASSERT(OFFSET_OF(struct exc_frame, sp_el0) == EXC_FRAME_SP0, "frame sp_el0 offset");
STATIC_ASSERT(sizeof(struct exc_frame) == EXC_FRAME_SIZE, "frame size");

/* Called from vectors.S with the saved frame and the vector slot number. */
void arch_exception_handler(struct exc_frame *frame, u64 kind);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_ARCH_AARCH64_EXCEPTION_H */
