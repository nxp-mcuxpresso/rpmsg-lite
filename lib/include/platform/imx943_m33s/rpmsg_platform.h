/*
 * Copyright 2024 NXP
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef RPMSG_PLATFORM_H_
#define RPMSG_PLATFORM_H_

#include <stdint.h>

/*
 * A55 as master
 * M33S(cortex-m33 core1 in NETCMIX) as remote
 */
#define RPMSG_LITE_M33S_A55_MU      WAKEUP__MUI_A15__MUA
#define RPMSG_LITE_M33S_A55_MU_IRQn MU15_A_IRQn

/*
 * M33S(cortex-m33 core1 in NETCMIX) as master/remote
 * M71 as remote/master
 */
#define RPMSG_LITE_M33S_M71_MU      NETC__MUA1__MUB
#define RPMSG_LITE_M33S_M71_MU_IRQn MU_E1_B_IRQn

/*
 * M33S(cortex-m33 core1 in NETCMIX) as master/remote
 * M70 as remote/master
 */
#define RPMSG_LITE_M33S_M70_MU      NETC__MUA3__MUB
#define RPMSG_LITE_M33S_M70_MU_IRQn MU_E3_B_IRQn

#define RPMSG_LITE_MU_IRQ_PRIORITY (3U)

/* RPMSG MU channel index */
#define RPMSG_MU_CHANNEL (1)

#ifndef VRING_ALIGN
#if defined(CONFIG_USE_TCM_AS_RPMSG_SHMEM) && (CONFIG_USE_TCM_AS_RPMSG_SHMEM)
#define VRING_ALIGN (0x10U)
#else
#define VRING_ALIGN (0x1000U)
#endif
#endif

/* contains pool of descriptors and two circular buffers */
/* sizeof(vring_avail) = 6bytes
 * sizeof(vring_used) = 12bytes
 * sieeof(vring_desc) = 16bytes
 * sizeof(vring_used_elem) = 8bytes
 * RL_BUFFER_COUNT = 4
 *  size1 = num * sizeof(struct vring_desc); = 64byte
 *  size2 = sizeof(struct vring_avail) + (num * sizeof(uint16_t)) + sizeof(uint16_t);
 *        =  6 + (4 * 2 +2) = 16 bytes
 *  size3 = (size1 + size2 + align - 1UL) & ~(align - 1UL); = 64 + 16 = 80
 *  size4 = sizeof(struct vring_used) + (num * sizeof(struct vring_used_elem)) + sizeof(uint16_t);
 *        = 12 + (4*8 +2) = 46
 *
 * vring_size = vring_size(4, VRING_ALIGN) = size3 + size4 = 80 +46 = 126
 * RL_VRING_OVERHEAD = 2 * vring_size =  256bytes
 * RL_BUFFER_PAYLOAD_SIZE = 496
 * memsize = 4 * (496 + 16) * 2 + 0x200 = 4K + 0.5K =4.5K
 * */

#ifndef VRING_SIZE
#if defined(CONFIG_USE_TCM_AS_RPMSG_SHMEM) && (CONFIG_USE_TCM_AS_RPMSG_SHMEM)
#define VRING_ALIGN (0x10U)
#define VRING_SIZE (0x80UL)
/* define shared memory space for VRINGS per one channel
 * #define SH_MEM_TOTAL_SIZE (6144U) in main_master.c
 * remain 4K for buffers, vring overhead occupy 2Kbytes*/
#define RL_VRING_OVERHEAD ((2UL * VRING_SIZE + 0x7FFUL) & (~0x7FFUL))
#else
#define VRING_SIZE (0x8000UL)
/* define shared memory space for VRINGS per one channel */
#define RL_VRING_OVERHEAD (2UL * VRING_SIZE)
#endif
#endif

/* VQ_ID in iMX93 is defined as follows:
 *   com_id:   [4:3] communication ID, used to identify the MU instance.
 *   vring_id: [2:1] vring ID, used to identify the vring.
 *   q_id:     [0:0] queue ID, used to identify the tvq or rvq.
 *   com_id + vring_id = link_id
 */

#define RL_GET_VQ_ID(link_id, queue_id) (((queue_id)&0x1U) | (((link_id) << 1U) & 0xFFFFFFFEU))
#define RL_GET_LINK_ID(vq_id)           ((vq_id) >> 1U)
#define RL_GET_COM_ID(vq_id)            ((vq_id) >> 3U)
#define RL_GET_Q_ID(vq_id)              ((vq_id)&0x1U)

#define RL_GEN_LINK_ID(com_id, vring_id) (((com_id) << 2U) | (vring_id))
#define RL_GEN_MU_MSG(vq_id)             (uint32_t)(((vq_id)&0x7U) << 16U) /* com_id is discarded in msg */

#define RL_PLATFORM_IMX943_M33S_SRTM_VRING_ID (0U)
#define RL_PLATFORM_IMX943_M33S_USER_VRING_ID (1U)

#define RL_PLATFORM_IMX943_M33S_A55_COM_ID (0U)
#define RL_PLATFORM_IMX943_M33S_A55_SRTM_LINK_ID \
    RL_GEN_LINK_ID(RL_PLATFORM_IMX943_M33S_A55_COM_ID, RL_PLATFORM_IMX943_M33S_SRTM_VRING_ID)
#define RL_PLATFORM_IMX943_M33S_A55_USER_LINK_ID \
    RL_GEN_LINK_ID(RL_PLATFORM_IMX943_M33S_A55_COM_ID, RL_PLATFORM_IMX943_M33S_USER_VRING_ID)

#define RL_PLATFORM_IMX943_M33S_M70_COM_ID (1U)
#define RL_PLATFORM_IMX943_M33S_M70_USER_LINK_ID \
    RL_GEN_LINK_ID(RL_PLATFORM_IMX943_M33S_M70_COM_ID, RL_PLATFORM_IMX943_M33S_USER_VRING_ID)

#define RL_PLATFORM_IMX943_M33S_M71_COM_ID (2U)
#define RL_PLATFORM_IMX943_M33S_M71_USER_LINK_ID \
    RL_GEN_LINK_ID(RL_PLATFORM_IMX943_M33S_M71_COM_ID, RL_PLATFORM_IMX943_M33S_USER_VRING_ID)

#define RL_PLATFORM_HIGHEST_LINK_ID \
    RL_GEN_LINK_ID(RL_PLATFORM_IMX943_M33S_M71_COM_ID, RL_PLATFORM_IMX943_M33S_USER_VRING_ID)

/*
 * The CM33 subsystem local TCM start address, refer to Reference Manual for detailed information
 * +-----------------------------------------------------------------------------------+
 * |                         |   from cm33 (0, 1) view    |   from cm7 (0, 1) view     |
 * +-----------------------------------------------------------------------------------+
 * |                         |   start      |   end       |   start      |   end       |
 * +-----------------------------------------------------------------------------------+
 * | cm33 core0 code tcm     | 0x0ffc0000   |  0x0fffffff | 0x201c0000   |  0x201fffff |
 * +-----------------------------------------------------------------------------------+
 * | cm33 core0 system tcm   | 0x20000000   |  0x2003ffff | 0x20200000   |  0x2023ffff |
 * +-----------------------------------------------------------------------------------+
 * | cm33 core1 code tcm     | 0x0ffc0000   |  0x0fffffff | 0x209c0000   |  0x209fffff |
 * +-----------------------------------------------------------------------------------+
 * | cm33 core1 system tcm   | 0x20000000   |  0x2003ffff | 0x20a00000   |  0x20a3ffff |
 * +-----------------------------------------------------------------------------------+
 * | cm7 core0 itcm          | 0x203c0000   |  0x203fffff | 0x00000000   |  0x0003ffff |
 * +-----------------------------------------------------------------------------------+
 * | cm7 core0 dtcm          | 0x20400000   |  0x2043ffff | 0x20000000   |  0x2003ffff |
 * +-----------------------------------------------------------------------------------+
 * | cm7 core1 itcm          | 0x202c0000   |  0x202fffff | 0x202c0000   |  0x202fffff |
 * +-----------------------------------------------------------------------------------+
 * | cm7 core1 dtcm          | 0x20300000   |  0x2033ffff | 0x20300000   |  0x2033ffff |
 * +-----------------------------------------------------------------------------------+
 */

#define RPMSG_M33S_SYS_TCM_BEGIN_FROM_M70_VIEW (0x20A00000)
#define RPMSG_M33S_SYS_TCM_END_FROM_M70_VIEW (0x20A3FFFF)
#define RPMSG_M70_DTCM_BEGIN_FROM_M70_VIEW (0x20000000)
#define RPMSG_M70_DTCM_END_FROM_M70_VIEW (0x2003FFFF)
#define RPMSG_M71_DTCM_BEGIN_FROM_M70_VIEW (0x20300000)
#define RPMSG_M71_DTCM_END_FROM_M70_VIEW (0x2033FFFF)

#define RPMSG_M33S_SYS_TCM_BEGIN_FROM_M71_VIEW (0x20A00000)
#define RPMSG_M33S_SYS_TCM_END_FROM_M71_VIEW (0x20A3FFFF)
#define RPMSG_M70_DTCM_BEGIN_FROM_M71_VIEW (0x20400000)
#define RPMSG_M70_DTCM_END_FROM_M71_VIEW (0x2043FFFF)
#define RPMSG_M71_DTCM_BEGIN_FROM_M71_VIEW (0x20000000)
#define RPMSG_M71_DTCM_END_FROM_M71_VIEW (0x2003FFFF)

#define RPMSG_M33S_SYS_TCM_BEGIN_FROM_M33S_VIEW (0x20000000)
#define RPMSG_M33S_SYS_TCM_END_FROM_M33S_VIEW (0x2003FFFF)
#define RPMSG_M70_DTCM_BEGIN_FROM_M33S_VIEW (0x20400000)
#define RPMSG_M70_DTCM_END_FROM_M33S_VIEW (0x2043FFFF)
#define RPMSG_M71_DTCM_BEGIN_FROM_M33S_VIEW (0x20300000)
#define RPMSG_M71_DTCM_END_FROM_M33S_VIEW (0x2033FFFF)

/*
 * RPMSG Share Memory Layout between MCUs
 * cm7 core1 dtcm(cm7 core0 as master, cm7 core1 as remote):         0x20039000~0x2003A7FF(from cm7 core1 view)  = 0x20339000~0x2033A7FF(from cm7 core0, cm33 core1 view)
 * cm7 core1 dtcm(cm33 core1 as master, cm7 core1 as remote):        0x2003A800~0x2003BFFF(from cm7 core1 view)  = 0x2033A800~0x2033BFFF(from cm33 core1, cm7 core0 view)
 * cm7 core0 dtcm(cm7 core1 as master, cm7 core0 as remote):         0x20039000~0x2003A7FF(from cm7 core1 view)  = 0x20439000~0x2043A7FF(from cm7 core0, cm33 core1 view)
 * cm7 core0 dtcm(cm33 core1 as master, cm7 core0 as remote):        0x2003A800~0x2003BFFF(from cm7 core1 view)  = 0x2043A800~0x2043BFFF(from cm33 core1, cm7 core0 view)
 * cm33 core1 system tcm(cm7 core0 as master, cm33 core1 as remote): 0x20039000~0x2003A7FF(from cm33 core1 view) = 0x20A39000~0x20A3A7FF(from cm7 core0, cm7 core1 view)
 * cm33 core1 system tcm(cm7 core1 as master, cm33 core1 as remote): 0x2003A800~0x2003BFFF(from cm33 core1 view) = 0x20A3A800~0x20A3BFFF(from cm7 core0, cm7 core1 view)
 */
/* M33S as rpmsg master, M70 as rpmsg remote, use DTCM of M70 as rpmsg share memory  */
#define RPMSG_SHMEM_M33S_M70_DTCM_BEGIN_FROM_M33S_VIEW (0x2043A800)
#define RPMSG_SHMEM_M33S_M70_DTCM_END_FROM_M33S_VIEW   (0x2043BFFF)
#define RPMSG_SHMEM_M33S_M70_DTCM_BEGIN_FROM_M70_VIEW (0x2003A800)
#define RPMSG_SHMEM_M33S_M70_DTCM_END_FROM_M70_VIEW   (0x2003BFFF)

/* M70 as rpmsg master, M33S as rpmsg remote, use SYSTEM TCM of M33S as rpmsg share memory  */
#define RPMSG_SHMEM_M70_M33S_SYS_TCM_BEGIN_FROM_M33S_VIEW (0x20039000)
#define RPMSG_SHMEM_M70_M33S_SYS_TCM_END_FROM_M33S_VIEW (0x2003A7FF)
#define RPMSG_SHMEM_M70_M33S_SYS_TCM_BEGIN_FROM_M70_VIEW (0x20A39000)
#define RPMSG_SHMEM_M70_M33S_SYS_TCM_END_FROM_M70_VIEW (0x20A3A7FF)

/* M71 as rpmsg master, M33S as rpmsg remote, use SYSTEM TCM of M33S as rpmsg share memory  */
#define RPMSG_SHMEM_M71_M33S_SYS_TCM_BEGIN_FROM_M33S_VIEW (0x2003A800)
#define RPMSG_SHMEM_M71_M33S_SYS_TCM_END_FROM_M33S_VIEW   (0x2003BFFF)
#define RPMSG_SHMEM_M71_M33S_SYS_TCM_BEGIN_FROM_M71_VIEW (0x20A3A800)
#define RPMSG_SHMEM_M71_M33S_SYS_TCM_END_FROM_M71_VIEW   (0x20A3BFFF)

/* M33S as rpmsg master, M71 as rpmsg remote, use DTCM of M71 as rpmsg share memory  */
#define RPMSG_SHMEM_M33S_M71_DTCM_BEGIN_FROM_M33S_VIEW (0x2033A800)
#define RPMSG_SHMEM_M33S_M71_DTCM_END_FROM_M33S_VIEW   (0x2033BFFF)
#define RPMSG_SHMEM_M33S_M71_DTCM_BEGIN_FROM_M71_VIEW (0x2003A800)
#define RPMSG_SHMEM_M33S_M71_DTCM_END_FROM_M71_VIEW   (0x2003BFFF)

#if defined(CONFIG_RPMSG_M71_M33S) && (CONFIG_RPMSG_M71_M33S)

#define RL_PLATFORM_IMX943_USER_LINK_ID RL_PLATFORM_IMX943_M33S_M71_USER_LINK_ID
#define RL_PLATFORM_IMX943_SHMEM RPMSG_SHMEM_M71_M33S_SYS_TCM_BEGIN_FROM_M33S_VIEW

#elif defined(CONFIG_RPMSG_M33S_M71) && (CONFIG_RPMSG_M33S_M71)

#define RL_PLATFORM_IMX943_USER_LINK_ID RL_PLATFORM_IMX943_M33S_M71_USER_LINK_ID
#define RL_PLATFORM_IMX943_SHMEM RPMSG_SHMEM_M33S_M71_DTCM_BEGIN_FROM_M33S_VIEW

#elif defined(CONFIG_RPMSG_M70_M33S) && (CONFIG_RPMSG_M70_M33S)

#define RL_PLATFORM_IMX943_USER_LINK_ID RL_PLATFORM_IMX943_M33S_M70_USER_LINK_ID
#define RL_PLATFORM_IMX943_SHMEM RPMSG_SHMEM_M70_M33S_SYS_TCM_BEGIN_FROM_M33S_VIEW

#elif defined(CONFIG_RPMSG_M33S_M70) && (CONFIG_RPMSG_M33S_M70)

#define RL_PLATFORM_IMX943_USER_LINK_ID RL_PLATFORM_IMX943_M33S_M70_USER_LINK_ID
#define RL_PLATFORM_IMX943_SHMEM RPMSG_SHMEM_M33S_M70_DTCM_BEGIN_FROM_M33S_VIEW

#endif

/* platform interrupt related functions */
int32_t platform_init_interrupt(uint32_t vector_id, void *isr_data);
int32_t platform_deinit_interrupt(uint32_t vector_id);
int32_t platform_interrupt_enable(uint32_t vector_id);
int32_t platform_interrupt_disable(uint32_t vector_id);
int32_t platform_in_isr(void);
void platform_notify(uint32_t vector_id);

/* platform low-level time-delay (busy loop) */
void platform_time_delay(uint32_t num_msec);

/* platform memory functions */
void platform_map_mem_region(uint32_t vrt_addr, uint32_t phy_addr, uint32_t size, uint32_t flags);
void platform_cache_all_flush_invalidate(void);
void platform_cache_disable(void);
uint32_t platform_vatopa(void *addr);
void *platform_patova(uint32_t addr);

/* platform init/deinit */
int32_t platform_init(void);
int32_t platform_deinit(void);

#endif /* RPMSG_PLATFORM_H_ */
