.. _arty-litex-vexriscv:

LiteX VexRiscv
##############

Overview
********

LiteX is a Migen-based System on Chip, supporting various softcore CPUs,
including VexRiscv. The LiteX SoC with VexRiscv CPU can be deployed on e.g.
Digilent ARTY board. More information can be found on:
`LiteX's website <https://github.com/enjoy-digital/litex>`_ and
`VexRiscv's website <https://github.com/SpinalHDL/VexRiscv>`_.

Programming and debugging
*************************

Building
========

Applications for the ``arty_litex_vexriscv`` board configuration can be built as usual
(see :ref:`build_an_application`).
In order to build the application for ``arty_litex_vexriscv``, set the ``BOARD`` variable
to ``arty_litex_vexriscv``.

Booting
=======

You can boot from serial port using `flterm: <https://github.com/timvideos/flterm>`_, e.g.:

.. code-block:: bash

    flterm --port /dev/ttyUSB0 --kernel <path_to_zephyr.bin> --kernel-addr 0x40000000
