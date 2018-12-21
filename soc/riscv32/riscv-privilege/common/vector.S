/*
 * Copyright (c) 2017 Jean-Paul Etienne <fractalclone@gmail.com>
 * Contributors: 2018 Antmicro <www.antmicro.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <toolchain.h>
#ifdef CONFIG_BOARD_LITEX_PICORV32
#include <custom_ops.S>
#endif

/* exports */
GTEXT(__start)

/* imports */
GTEXT(__initialize)
GTEXT(__irq_wrapper)

SECTION_FUNC(vectors, __start)
	.option norvc;
	/*
	 * Set mtvec (Machine Trap-Vector Base-Address Register)
	 * to __irq_wrapper.
	 */
	la t0, __irq_wrapper
	#ifndef CONFIG_BOARD_LITEX_PICORV32
	csrw mtvec, t0
	#else
	picorv32_setq_insn(q2, t0)
	#endif /*CONFIG_BOARD_LITEX_PICORV32*/
	/* Jump to __initialize */
	tail __initialize
