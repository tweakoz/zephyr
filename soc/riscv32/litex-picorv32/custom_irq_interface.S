#include <custom_ops.S>

.global _irq_enable
_irq_enable:
  la t0, _irq_mask
  lw t0, (t0)
  picorv32_maskirq_insn(zero, t0)
  la t0, _irq_enabled
  li t1, 0xffffffff
  sw t1, (t0)
  ret

.global _irq_disable
_irq_disable:
  li t0, 0xffffffff
  picorv32_maskirq_insn(zero, t0)
  la t0, _irq_enabled
  sw zero, (t0)
  ret

.global _irq_setmask
_irq_setmask:
  # save mask
  la t0, _irq_mask
  sw a0, 0(t0)
  picorv32_maskirq_insn(zero, a0)
  ret

.global _irq_pending
_irq_pending:
  picorv32_getq_insn(q1, a0)
  ret

.global _irq_mask
_irq_mask:
  .word 0xffffffff

.global _irq_enabled
_irq_enabled:
  .word 0xffffffff
