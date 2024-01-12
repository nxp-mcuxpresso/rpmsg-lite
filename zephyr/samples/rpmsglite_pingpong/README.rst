.. _rpmsglite_pingpong:

RPMSG-Lite Ping Pong
###########

Overview
********

The Multicore RPMsg-Lite pingpong sample that can be used with: :ref:`supported board <boards>`.

The Multicore RPMsg-Lite pingpong project is a simple demonstration program that uses the
Zephyr OS and the RPMsg-Lite library and shows how to implement the inter-core
communicaton between cores of the multicore system. The primary core releases the secondary core
from the reset and then the inter-core communication is established. Once the RPMsg is initialized
and endpoints are created the message exchange starts, incrementing a virtual counter that is part
of the message payload. The message pingpong finishes when the counter reaches the value of 100.
Then the RPMsg-Lite is deinitialized and the procedure of the data exchange is repeated again.

Building and Running
********************

This application can be built and executed on Supported Multi Core boards as follows:

Building the application for lpcxpresso54114_m4
***********************************************

.. zephyr-app-commands::
   :zephyr-app: samples/rpmsglite_pingpong
   :board: lpcxpresso54114_m4
   :goals: debug
   :west-args: --sysbuild

Building the application for lpcxpresso55s69_cpu0
*************************************************

.. zephyr-app-commands::
   :zephyr-app: samples/rpmsglite_pingpong
   :board: lpcxpresso55s69_cpu0
   :goals: debug
   :west-args: --sysbuild

Building the application for mimxrt1160_evk_cm7
***********************************************

.. zephyr-app-commands::
   :zephyr-app: samples/rpmsglite_pingpong
   :board: mimxrt1160_evk_cm7
   :goals: debug
   :west-args: --sysbuild

Building the application for mimxrt1170_evk_cm7
***********************************************

.. zephyr-app-commands::
   :zephyr-app: samples/rpmsglite_pingpong
   :board: mimxrt1170_evk_cm7
   :goals: debug
   :west-args: --sysbuild

Building the application for mimxrt1170_evkb_cm7
***********************************************

.. zephyr-app-commands::
   :zephyr-app: samples/rpmsglite_pingpong
   :board: mimxrt1170_evkb_cm7
   :goals: debug
   :west-args: --sysbuild

Sample Output
=============

Open a serial terminal (minicom, putty, etc.) and connect the board with the
following settings:

- Speed: 115200
- Data: 8 bits
- Parity: None
- Stop bits: 1

Reset the board and the following message will appear on the corresponding
serial port, one is master another is remote:

.. code-block:: console

    *** Booting Zephyr OS build zephyr-v3.5.0-2239-ga51bd53cef2b ***
    Starting application thread on Main Core!
    Primary core received a msg
    Message: Size=4, DATA = 1
    Primary core received a msg
    Message: Size=4, DATA = 3
    ...
    Primary core received a msg
    Message: Size=4, DATA = 99
    Primary core received a msg
    Message: Size=4, DATA = 101

    RPMsg demo ends
