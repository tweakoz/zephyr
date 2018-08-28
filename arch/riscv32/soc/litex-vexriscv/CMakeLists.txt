zephyr_include_directories(.)

zephyr_sources(../riscv-privilege/common/soc_irq.S)

if(NOT(CONFIG_SOC_SERIES_RISCV32_QEMU))
    zephyr_sources(../riscv-privilege/common/vector.S)
endif()
