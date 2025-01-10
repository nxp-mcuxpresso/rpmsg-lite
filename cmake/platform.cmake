#
# Copyright 2024-2025 NXP
#
# SPDX-License-Identifier: BSD-3-Clause
#

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.evkmimxrt595)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imxrt500_m33/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imxrt500_m33/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imxrt500_m33/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.evkmimxrt595_fusionf1)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imxrt500_fusionf1/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imxrt500_fusionf1/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imxrt500_fusionf1/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.evkmimxrt685)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imxrt600_m33/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imxrt600_m33/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imxrt600_m33/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.evkmimxrt685_hifi4)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imxrt600_hifi4/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imxrt600_hifi4/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imxrt600_hifi4/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imxrt700)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imxrt700_m33/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imxrt700_m33/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imxrt700_m33/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imxrt700_hifi1)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imxrt700_hifi1/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imxrt700_hifi1/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imxrt700_hifi1/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imxrt700_hifi4)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imxrt700_hifi4/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imxrt700_hifi4/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imxrt700_hifi4/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imxrt700_ezhv)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imxrt700_ezhv/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imxrt700_ezhv/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imxrt700_ezhv/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imx7ulp_m4)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imx7ulp_m4/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imx7ulp_m4/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imx7ulp_m4/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imx8dxl_m4)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imx8dxl_m4/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imx8dxl_m4/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imx8dxl_m4/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imx8mm_m4)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imx8mm_m4/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imx8mm_m4/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imx8mm_m4/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imx8mn_m7)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imx8mn_m7/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imx8mn_m7/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imx8mn_m7/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imx8mp_m7)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imx8mp_m7/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imx8mp_m7/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imx8mp_m7/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imx8mq_m4)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imx8mq_m4/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imx8mq_m4/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imx8mq_m4/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imx8qm_m4)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imx8qm_m4/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imx8qm_m4/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imx8qm_m4/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imx8qx_m4)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imx8qx_cm4/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imx8qx_cm4/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imx8qx_cm4/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imx8ulp_m33)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imx8ulp_m33/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imx8ulp_m33/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imx8ulp_m33/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imx93_m33)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imx93_m33/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imx93_m33/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imx93_m33/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imx95_m7)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imx95_m7/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imx95_m7/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imx95_m7/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imx95_m33)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imx95_m33/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imx95_m33/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imx95_m33/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imx943_m70)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imx943_m70/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imx943_m70/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imx943_m70/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imx943_m71)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imx943_m71/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imx943_m71/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imx943_m71/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imx943_m33s)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imx943_m33s/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imx943_m33s/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imx943_m33s/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imxrt1160)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imxrt1160/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imxrt1160/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imxrt1160/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imxrt1170)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imxrt1170/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imxrt1170/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imxrt1170/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.imxrt1180)
    mcux_add_include(
        INCLUDES ../lib/include/platform/imxrt1180/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/imxrt1180/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/imxrt1180/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.k32l3a6)
    mcux_add_include(
        INCLUDES ../lib/include/platform/k32l3a6/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/k32l3a6/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/k32l3a6/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.k32w1)
    mcux_add_include(
        INCLUDES ../lib/include/platform/k32w1/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/k32w1/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/k32w1/rpmsg_platform.c
            ../lib/rpmsg_lite/porting/platform/k32w1/rpmsg_platform_ext.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.kw45b41)
    mcux_add_include(
        INCLUDES ../lib/include/platform/kw45b41/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/kw45b41/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/kw45b41/rpmsg_platform.c
            ../lib/rpmsg_lite/porting/platform/kw45b41/rpmsg_platform_ext.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.mcxw71x)
    mcux_add_include(
        INCLUDES ../lib/include/platform/mcxw71x/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/mcxw71x/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/mcxw71x/rpmsg_platform.c
            ../lib/rpmsg_lite/porting/platform/mcxw71x/rpmsg_platform_ext.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.kw47b42)
    mcux_add_include(
        INCLUDES ../lib/include/platform/kw47b42/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/kw47b42/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/kw47b42/rpmsg_platform.c
            ../lib/rpmsg_lite/porting/platform/kw47b42/rpmsg_platform_ext.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.kw43b43)
    mcux_add_include(
        INCLUDES ../lib/include/platform/kw43b43/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/kw43b43/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/kw43b43/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.mcxw72x)
    mcux_add_include(
        INCLUDES ../lib/include/platform/mcxw72x/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/mcxw72x/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/mcxw72x/rpmsg_platform.c
            ../lib/rpmsg_lite/porting/platform/mcxw72x/rpmsg_platform_ext.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.lpc55s69)
    mcux_add_include(
        INCLUDES ../lib/include/platform/lpc55s69/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/lpc55s69/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/lpc55s69/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.lpc54114)
    mcux_add_include(
        INCLUDES ../lib/include/platform/lpc5411x/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/lpc5411x/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/lpc5411x/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.mcxnx4x)
    mcux_add_include(
        INCLUDES ../lib/include/platform/mcxnx4x/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/mcxnx4x/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/mcxnx4x/rpmsg_platform.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.mcxl20)
    mcux_add_include(
        INCLUDES ../lib/include/platform/mcxl20/
    )

    mcux_add_source(
        SOURCES ../lib/include/platform/mcxl20/rpmsg_platform.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/platform/mcxl20/rpmsg_platform.c
    )
endif()
