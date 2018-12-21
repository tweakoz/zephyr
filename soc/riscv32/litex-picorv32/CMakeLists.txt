zephyr_include_directories(.)
zephyr_sources(../riscv-privilege/common/soc_irq.S)
zephyr_sources(../riscv-privilege/common/vector.S)
zephyr_sources(custom_irq_interface.S)
