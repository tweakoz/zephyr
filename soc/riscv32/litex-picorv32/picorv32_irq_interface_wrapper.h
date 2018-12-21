#ifndef PICORV32_IRQ_INTERFACE_WRAPPER
#define PICORV32_IRQ_INTERFACE_WRAPPER

extern volatile unsigned int _irq_pending;

extern volatile unsigned int _irq_mask;

extern volatile unsigned int _irq_enabled;

extern void _irq_enable(void);

extern void _irq_disable(void);

extern void _irq_setmask(unsigned int mask);

#endif /*PICORV32_IRQ_INTERFACE_WRAPPER*/
