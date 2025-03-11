/*
 * Copyright 2024-2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef RPMSG_PLATFORM_H_
#define RPMSG_PLATFORM_H_

#include <stdint.h>

/*
 * No need to align the VRING as defined in Linux because RT700 is not intended
 * to run the Linux
 */
#ifndef VRING_ALIGN
#define VRING_ALIGN (0x10U)
#endif

/* contains pool of descriptors and two circular buffers */
#ifndef VRING_SIZE
/* set VRING_SIZE based on number of used buffers as calculated in vring_init */
#define VRING_DESC_SIZE (((RL_BUFFER_COUNT * sizeof(struct vring_desc)) + VRING_ALIGN - 1UL) & ~(VRING_ALIGN - 1UL))
#define VRING_AVAIL_SIZE                                                                                            \
    (((sizeof(struct vring_avail) + (RL_BUFFER_COUNT * sizeof(uint16_t)) + sizeof(uint16_t)) + VRING_ALIGN - 1UL) & \
     ~(VRING_ALIGN - 1UL))
#define VRING_USED_SIZE                                                                                     \
    (((sizeof(struct vring_used) + (RL_BUFFER_COUNT * sizeof(struct vring_used_elem)) + sizeof(uint16_t)) + \
      VRING_ALIGN - 1UL) &                                                                                  \
     ~(VRING_ALIGN - 1UL))
#define VRING_SIZE (VRING_DESC_SIZE + VRING_AVAIL_SIZE + VRING_USED_SIZE)
#endif

/* define shared memory space for VRINGS per one channel */
#define RL_VRING_OVERHEAD (2UL * VRING_SIZE)

/* VQ_ID in imxrt700 is defined as follows:
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

#define RL_PLATFORM_IMXRT700_M33_0_M33_1_COM_ID (0U)
#define RL_PLATFORM_IMXRT700_M33_0_HIFI4_COM_ID (1U)
#define RL_PLATFORM_IMXRT700_M33_1_HIFI1_COM_ID (2U)
#define RL_PLATFORM_IMXRT700_M33_0_EZHV_COM_ID  (3U)

#define RL_PLATFORM_HIGHEST_LINK_ID RL_GEN_LINK_ID(RL_PLATFORM_IMXRT700_M33_0_EZHV_COM_ID, 0U)

#define RL_PLATFORM_IMXRT700_M33_0_M33_1_LINK_ID RL_GEN_LINK_ID(RL_PLATFORM_IMXRT700_M33_0_M33_1_COM_ID, 0U)

#define RL_PLATFORM_IMXRT700_M33_0_HIFI4_LINK_ID RL_GEN_LINK_ID(RL_PLATFORM_IMXRT700_M33_0_HIFI4_COM_ID, 0U)

#define RL_PLATFORM_IMXRT700_M33_1_HIFI1_LINK_ID RL_GEN_LINK_ID(RL_PLATFORM_IMXRT700_M33_1_HIFI1_COM_ID, 0U)

#define RL_PLATFORM_IMXRT700_M33_0_EZHV_LINK_ID  RL_GEN_LINK_ID(RL_PLATFORM_IMXRT700_M33_0_EZHV_COM_ID, 0U)

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
void platform_cache_invalidate(void *data, uint32_t len);
void platform_cache_flush(void *data, uint32_t len);
uintptr_t platform_vatopa(void *addr);
void *platform_patova(uintptr_t addr);

/* platform init/deinit */
int32_t platform_init(void);
int32_t platform_deinit(void);

#endif /* RPMSG_PLATFORM_H_ */
