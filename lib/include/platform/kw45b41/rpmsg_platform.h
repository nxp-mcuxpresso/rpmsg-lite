/*
 * Copyright 2020-2024 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RPMSG_PLATFORM_H_
#define RPMSG_PLATFORM_H_

#include <stdint.h>
#include "virtio_ring.h"

/*
 * No need to align the VRING as defined in Linux because kw45b41 is not intended
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

#define RL_GET_VQ_ID(link_id, queue_id) (((queue_id)&0x1U) | (((link_id) << 1U) & 0xFFFFFFFEU))
#define RL_GET_LINK_ID(id)              (((id)&0xFFFFFFFEU) >> 1U)
#define RL_GET_Q_ID(id)                 ((id)&0x1U)

#define RL_PLATFORM_KW45B41_LINK_ID (0U)
#define RL_PLATFORM_HIGHEST_LINK_ID (0U)

typedef struct rpmsg_platform_shmem_config
{
    uint32_t buffer_payload_size; /* custom buffer payload size setting that overwrites RL_BUFFER_PAYLOAD_SIZE global
                                     config, must be equal to (240, 496, 1008, ...) [2^n - 16] */
    uint16_t buffer_count; /* custom buffer count setting that overwrites RL_BUFFER_COUNT global config, must be power
                              of two (2, 4, ...) */
    uint32_t vring_size;   /* custom vring size */
    uint32_t vring_align;  /* custom vring alignment */
} rpmsg_platform_shmem_config_t;

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

/*!
 * \brief Set static shared memory configuration from application core in SMU2 to be accessible from nbu later.
 *
 */
void platform_set_static_shmem_config(void);

/*!
 * \brief API used when RL_ALLOW_CUSTOM_SHMEM_CONFIG is set to 1 in rpmsg_lite.c.
 * \details On this platform we set the macro on nbu side to take the same config of application core previously
 *          set in platform_set_static_shmem_config().
 *
 * \param[in] link_id NOT USED on this platform
 * \param[out] *config shared memory configuration
 *
 * \return int 0 if success, other if error.
 */
uint32_t platform_get_custom_shmem_config(uint32_t link_id, rpmsg_platform_shmem_config_t *config);

#endif /* RPMSG_PLATFORM_H_ */
