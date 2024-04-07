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
 * MU's purpose is not hard-coded in the SoC
 * Two mu instances: MU15, MU2
 * Currently use MU1 for communication between M-Core(Cortex-M33) and A-Core(Cortex-A55)
 * use 
 */
#define RPMSG_LITE_M331_A55_MU      WAKEUP__MUI_A15__MUA
#define RPMSG_LITE_M331_A55_MU_IRQn MU15_A_IRQn

#define RPMSG_LITE_M331_M71_MU      NETC__MUA1__MUB
#define RPMSG_LITE_M331_M71_MU_IRQn MU_E1_B_IRQn

#define RPMSG_LITE_M330_M71_MU      MU7_MUB 
#define RPMSG_LITE_M330_M71_MU_IRQn MU7_B_IRQn

#define RPMSG_LITE_M331_M70_MU      NETC__MUA3__MUB
#define RPMSG_LITE_M331_M70_MU_IRQn MU_E3_B_IRQn

#define RPMSG_LITE_M330_M331_MU      MU8_MUB 
#define RPMSG_LITE_M330_M331_MU_IRQn MU8_B_IRQn

#define RPMSG_LITE_M331_M330_MU      MU8_MUA
#define RPMSG_LITE_M331_M330_MU_IRQn MU8_A_IRQn

#define RPMSG_LITE_MU_IRQ_PRIORITY (3U)

/* RPMSG MU channel index */
#define RPMSG_MU_CHANNEL (1)

/*
 * M7_0 DTCM address is 0x20400000 from M33_0/1
 * M7_0 DTCM address is 0x20400000 from M7_1
 * M7_1 DTCM address is 0x20300000 from M33_0/1
 * M7_1 DTCM address is 0x20300000 from M7_0
 */
#define M70_ALIASED_DTCM_ADDR_OFF_FOR_M33 (0x400000U)
#define M70_ALIASED_DTCM_ADDR_OFF_FOR_M71 (0x400000U)
#define M71_ALIASED_DTCM_ADDR_OFF_FOR_M33 (0x300000U)
#define M71_ALIASED_DTCM_ADDR_OFF_FOR_M70 (0x300000U)

/*
 * M33_0 DTCM address is not support to access from M33_1
 * M33_1 DTCM address is 0x20A00000 from M33_0
 */
#define M331_ALIASED_DTCM_ADDR_OFF_FOR_M33 (0xA00000U)

#ifndef VRING_ALIGN
#if !(defined(RPMSG_M331_MASTER) || defined(RPMSG_M331_REMOTE))
#define VRING_ALIGN (0x1000U)
#else
#define VRING_ALIGN (0x10U)
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
#if !(defined(RPMSG_M331_MASTER) || defined(RPMSG_M331_REMOTE))
#define VRING_SIZE (0x8000UL)
/* define shared memory space for VRINGS per one channel */
#define RL_VRING_OVERHEAD (2UL * VRING_SIZE)
#else
#define VRING_SIZE (0x80UL)
/* define shared memory space for VRINGS per one channel
 * #define SH_MEM_TOTAL_SIZE (6144U) in main_master.c
 * remain 4K for buffers, vring overhead occupy 2Kbytes*/
#define RL_VRING_OVERHEAD ((2UL * VRING_SIZE + 0x7FF) & (~0x7FF))
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

#define RL_PLATFORM_IMX943_M33_SRTM_VRING_ID (0U)
#define RL_PLATFORM_IMX943_M33_USER_VRING_ID (1U)

#define RL_PLATFORM_IMX943_M331_A55_COM_ID (0U)
#define RL_PLATFORM_IMX943_M331_A55_SRTM_LINK_ID \
    RL_GEN_LINK_ID(RL_PLATFORM_IMX943_M331_A55_COM_ID, RL_PLATFORM_IMX943_M33_SRTM_VRING_ID)
#define RL_PLATFORM_IMX943_M331_A55_USER_LINK_ID \
    RL_GEN_LINK_ID(RL_PLATFORM_IMX943_M331_A55_COM_ID, RL_PLATFORM_IMX943_M33_USER_VRING_ID)

/* define communication ID from M33-1 to M7-0 */
#define RL_PLATFORM_IMX943_M331_M70_COM_ID (1U)
#define RL_PLATFORM_IMX943_M331_M70_USER_LINK_ID \
    RL_GEN_LINK_ID(RL_PLATFORM_IMX943_M331_M70_COM_ID, RL_PLATFORM_IMX943_M33_USER_VRING_ID)

/* define communication ID from M33-1 to M7-1 */
#define RL_PLATFORM_IMX943_M331_M71_COM_ID (2U)
#define RL_PLATFORM_IMX943_M331_M71_USER_LINK_ID \
    RL_GEN_LINK_ID(RL_PLATFORM_IMX943_M331_M71_COM_ID, RL_PLATFORM_IMX943_M33_USER_VRING_ID)

/* define communication ID from M7-0 to M7-1 */
#define RL_PLATFORM_IMX943_M70_M71_COM_ID (3U)
#define RL_PLATFORM_IMX943_M70_M71_USER_LINK_ID \
    RL_GEN_LINK_ID(RL_PLATFORM_IMX943_M70_M71_COM_ID, RL_PLATFORM_IMX943_M33_USER_VRING_ID)

/* define communication ID from M33-0 to M7-1 */
#define RL_PLATFORM_IMX943_M330_M71_COM_ID (4U)
#define RL_PLATFORM_IMX943_M330_M71_USER_LINK_ID \
    RL_GEN_LINK_ID(RL_PLATFORM_IMX943_M330_M71_COM_ID, RL_PLATFORM_IMX943_M33_USER_VRING_ID)

#define RL_PLATFORM_HIGHEST_LINK_ID \
    RL_GEN_LINK_ID(RL_PLATFORM_IMX943_M70_M71_COM_ID, RL_PLATFORM_IMX943_M33_USER_VRING_ID)

/* define communication ID from M33-0 to M7-0 */
#define RL_PLATFORM_IMX943_M330_M70_COM_ID (5U)
#define RL_PLATFORM_IMX943_M330_M70_USER_LINK_ID \
    RL_GEN_LINK_ID(RL_PLATFORM_IMX943_M330_M70_COM_ID, RL_PLATFORM_IMX943_M33_USER_VRING_ID)

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
