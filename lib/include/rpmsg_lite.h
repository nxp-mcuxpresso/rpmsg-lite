/*
 * Copyright (c) 2014, Mentor Graphics Corporation
 * Copyright (c) 2015 Xilinx, Inc.
 * Copyright (c) 2016 Freescale Semiconductor, Inc.
 * Copyright 2016-2025 NXP
 * Copyright 2021 ACRIOS Systems s.r.o.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RPMSG_LITE_H_
#define RPMSG_LITE_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include <stddef.h>
#include "rpmsg_compiler.h"
#include "virtqueue.h"
#include "rpmsg_env.h"
#include "llist.h"
#include "rpmsg_default_config.h"

//! @addtogroup rpmsg_lite
//! @{

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define RL_VERSION "v5.2.0" /*!< Current RPMsg Lite version */

/* Shared memory "allocator" parameters */
/*! @brief Word size used for alignment */
#define RL_WORD_SIZE (sizeof(uint32_t))
/*! @brief Align a value up to the next multiple of the word size */
#define RL_WORD_ALIGN_UP(a)                                                                                  \
    (((((uintptr_t)(a)) & (RL_WORD_SIZE - 1U)) != 0U) ? ((((uintptr_t)(a)) & (~(RL_WORD_SIZE - 1U))) + 4U) : \
                                                        ((uintptr_t)(a)))
/*! @brief Align a value down to a multiple of the word size */
#define RL_WORD_ALIGN_DOWN(a) \
    (((((uintptr_t)(a)) & (RL_WORD_SIZE - 1U)) != 0U) ? (((uintptr_t)(a)) & (~(RL_WORD_SIZE - 1U))) : ((uintptr_t)(a)))

/* Definitions for device types , null pointer, etc.*/
/*! @brief Success status code */
#define RL_SUCCESS    (0)
/*! @brief NULL pointer definition */
#define RL_NULL       ((void *)0)
/*! @brief Remote device identifier */
#define RL_REMOTE     (0)
/*! @brief Master device identifier */
#define RL_MASTER     (1)
/*! @brief Boolean true value */
#define RL_TRUE       (1UL)
/*! @brief Boolean false value */
#define RL_FALSE      (0UL)
/*! @brief Special address indicating any address */
#define RL_ADDR_ANY   (0xFFFFFFFFU)
/*! @brief Flag to release the buffer */
#define RL_RELEASE    (0)
/*! @brief Flag to hold the buffer */
#define RL_HOLD       (1)
/*! @brief Don't block and return immediately */
#define RL_DONT_BLOCK (0)
/*! @brief Block until the operation is complete */
#define RL_BLOCK      (~0UL)

/* Error macros. */
/*! @brief Base value for all RPMsg-Lite error codes */
#define RL_ERRORS_BASE   (-5000)
/*! @brief Error code: Out of memory */
#define RL_ERR_NO_MEM    (RL_ERRORS_BASE - 1)
/*! @brief Error code: Buffer size too small */
#define RL_ERR_BUFF_SIZE (RL_ERRORS_BASE - 2)
/*! @brief Error code: Invalid parameter */
#define RL_ERR_PARAM     (RL_ERRORS_BASE - 3)
/*! @brief Error code: Invalid device ID */
#define RL_ERR_DEV_ID    (RL_ERRORS_BASE - 4)
/*! @brief Error code: Maximum number of VirtQueues reached */
#define RL_ERR_MAX_VQ    (RL_ERRORS_BASE - 5)
/*! @brief Error code: No buffer available */
#define RL_ERR_NO_BUFF   (RL_ERRORS_BASE - 6)
/*! @brief Error code: System not ready */
#define RL_NOT_READY     (RL_ERRORS_BASE - 7)
/*! @brief Error code: Operation already completed */
#define RL_ALREADY_DONE  (RL_ERRORS_BASE - 8)

/* Init flags */
/*! @brief No initialization flags */
#define RL_NO_FLAGS (0U)

/*!
 * @brief Reserved field structure used in rpmsg_std_hdr
 *
 * This structure holds the idx and totlen of a buffer that is not immediately
 * returned to the vring in the receive callback function. This allows the no-copy API
 * to use this field to return the buffer later.
 */
 struct rpmsg_hdr_reserved
{
    uint16_t rfu; /* reserved for future usage */
    uint16_t idx;
};

RL_PACKED_BEGIN
/*!
 * Common header for all rpmsg messages.
 * Every message sent/received on the rpmsg bus begins with this header.
 */
struct rpmsg_std_hdr
{
    uint32_t src;                       /*!< source endpoint address */
    uint32_t dst;                       /*!< destination endpoint address */
    struct rpmsg_hdr_reserved reserved; /*!< reserved for future use */
    uint16_t len;                       /*!< length of payload (in bytes) */
    uint16_t flags;                     /*!< message flags */
} /*! @brief End of packed structure */ RL_PACKED_END;

RL_PACKED_BEGIN
/*!
 * Common message structure.
 * Contains the header and the payload.
 */
struct rpmsg_std_msg
{
    struct rpmsg_std_hdr hdr; /*!< RPMsg message header */
    uint8_t data[1];          /*!< bytes of message payload data */
} RL_PACKED_END;

/*! \typedef rl_ept_rx_cb_t
    \brief Receive callback function type.
*/
typedef int32_t (*rl_ept_rx_cb_t)(void *payload, uint32_t payload_len, uint32_t src, void *priv);

/*!
 * RPMsg Lite Endpoint structure
 */
struct rpmsg_lite_endpoint
{
    uint32_t addr;        /*!< endpoint address */
    rl_ept_rx_cb_t rx_cb; /*!< ISR callback function */
    void *rx_cb_data;     /*!< ISR callback data */
    void *rfu;            /*!< reserved for future usage */
    /* 16 bytes aligned on 32bit architecture */
};

/*!
 * RPMsg Lite Endpoint static context
 */
struct rpmsg_lite_ept_static_context
{
    struct rpmsg_lite_endpoint ept; /*!< memory for endpoint structure */
    struct llist node;              /*!< memory for linked list node structure */
};

/*!
 * Structure describing the local instance
 * of RPMSG lite communication stack and
 * holds all runtime variables needed internally
 * by the stack.
 */
struct rpmsg_lite_instance
{
    struct virtqueue *rvq;                /*!< receive virtqueue */
    struct virtqueue *tvq;                /*!< transmit virtqueue */
    struct llist *rl_endpoints;           /*!< linked list of endpoints */
    LOCK *lock;                           /*!< local RPMsg Lite mutex lock */
#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
    LOCK_STATIC_CONTEXT lock_static_ctxt; /*!< Static context for lock object creation */
#endif
    uint32_t link_state;                  /*!< state of the link, up/down*/
    char *sh_mem_base;                    /*!< base address of the shared memory */
    uint32_t sh_mem_remaining;            /*!< amount of remaining unused buffers in shared memory */
    uint32_t sh_mem_total;                /*!< total amount of buffers in shared memory */
    struct virtqueue_ops const *vq_ops;   /*!< ops functions table pointer */
#if defined(RL_USE_ENVIRONMENT_CONTEXT) && (RL_USE_ENVIRONMENT_CONTEXT == 1)
    void *env;                            /*!< pointer to the environment layer context */
#endif

#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
    struct vq_static_context vq_ctxt[2];
#endif
    uint32_t link_id; /*!< linkID of this rpmsg_lite instance */
};

/*******************************************************************************
 * API
 ******************************************************************************/

/* Exported API functions */

/*!
 * @brief Initializes the RPMsg-Lite communication stack.
 * Must be called prior to any other RPMSG lite API.
 * To be called by the master side.
 *
 * @param shmem_addr       Shared memory base used for this instance of RPMsg-Lite
 * @param shmem_length     Length of memory area given by previous parameter
 * @param link_id          Link ID used to define the rpmsg-lite instance, see rpmsg_platform.h
 * @param init_flags       Initialization flags
 * @if RL_USE_ENVIRONMENT_CONTEXT
 * @param env_cfg          Initialization data for the environement RPMsg-Lite layer, used when
 *                         the environment layer uses its own context (RL_USE_ENVIRONMENT_CONTEXT)
 * @endif
 * @if RL_USE_STATIC_API
 * @param static_context   RPMsg-Lite preallocated context pointer, used in case of static api (RL_USE_STATIC_API)
 * @endif
 *
 * @return  New RPMsg-Lite instance pointer or RL_NULL.
 *
 */
#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
struct rpmsg_lite_instance *rpmsg_lite_master_init(void *shmem_addr,
                                                   size_t shmem_length,
                                                   uint32_t link_id,
                                                   uint32_t init_flags,
                                                   struct rpmsg_lite_instance *static_context);
#elif defined(RL_USE_ENVIRONMENT_CONTEXT) && (RL_USE_ENVIRONMENT_CONTEXT == 1)
struct rpmsg_lite_instance *rpmsg_lite_master_init(
    void *shmem_addr, size_t shmem_length, uint32_t link_id, uint32_t init_flags, void *env_cfg);
#else
struct rpmsg_lite_instance *rpmsg_lite_master_init(void *shmem_addr,
                                                   size_t shmem_length,
                                                   uint32_t link_id,
                                                   uint32_t init_flags);
#endif

/**
 * @brief Initializes the RPMsg-Lite communication stack.
 * Must be called prior to any other RPMsg-Lite API.
 * To be called by the remote side.
 *
 * @param shmem_addr       Shared memory base used for this instance of RPMsg-Lite
 * @param link_id          Link ID used to define the rpmsg-lite instance, see rpmsg_platform.h
 * @param init_flags       Initialization flags
 * @if RL_USE_ENVIRONMENT_CONTEXT
 * @param env_cfg          Initialization data for the environement RPMsg-Lite layer, used when
 *                         the environment layer uses its own context (RL_USE_ENVIRONMENT_CONTEXT)
 * @endif
 * @if RL_USE_STATIC_API
 * @param static_context   RPMsg-Lite preallocated context pointer, used in case of static api (RL_USE_STATIC_API)
 * @endif
 * @return  New RPMsg-Lite instance pointer or RL_NULL.
 *
 */
#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
struct rpmsg_lite_instance *rpmsg_lite_remote_init(void *shmem_addr,
                                                   uint32_t link_id,
                                                   uint32_t init_flags,
                                                   struct rpmsg_lite_instance *static_context);
#elif defined(RL_USE_ENVIRONMENT_CONTEXT) && (RL_USE_ENVIRONMENT_CONTEXT == 1)
struct rpmsg_lite_instance *rpmsg_lite_remote_init(void *shmem_addr,
                                                   uint32_t link_id,
                                                   uint32_t init_flags,
                                                   void *env_cfg);
#else
struct rpmsg_lite_instance *rpmsg_lite_remote_init(void *shmem_addr, uint32_t link_id, uint32_t init_flags);
#endif

/*!
 *
 * @brief Deinitialized the RPMsg-Lite communication stack
 * This function always succeeds.
 * rpmsg_lite_init() can be called again after this
 * function has been called.
 *
 * @param rpmsg_lite_dev    RPMsg-Lite instance
 *
 * @return Status of function execution, RL_SUCCESS on success.
 */
int32_t rpmsg_lite_deinit(struct rpmsg_lite_instance *rpmsg_lite_dev);

/*!
 * @brief Create a new rpmsg endpoint, which can be used
 * for communication.
 *
 * @param rpmsg_lite_dev    RPMsg-Lite instance
 * @param addr              Desired address, RL_ADDR_ANY for automatic selection
 * @param rx_cb             Callback function called on receive
 * @param rx_cb_data        Callback data pointer, passed to rx_cb
 * @if RL_USE_STATIC_API
 * @param ept_context       Endpoint preallocated context pointer, used in case of static api (RL_USE_STATIC_API)
 * @endif
 * @return RL_NULL on error, new endpoint pointer on success.
 *
 */
#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
struct rpmsg_lite_endpoint *rpmsg_lite_create_ept(struct rpmsg_lite_instance *rpmsg_lite_dev,
                                                  uint32_t addr,
                                                  rl_ept_rx_cb_t rx_cb,
                                                  void *rx_cb_data,
                                                  struct rpmsg_lite_ept_static_context *ept_context);
#else
struct rpmsg_lite_endpoint *rpmsg_lite_create_ept(struct rpmsg_lite_instance *rpmsg_lite_dev,
                                                  uint32_t addr,
                                                  rl_ept_rx_cb_t rx_cb,
                                                  void *rx_cb_data);
#endif

/*!
 * @brief This function deletes rpmsg endpoint and performs cleanup.
 *
 * @param rpmsg_lite_dev    RPMsg-Lite instance
 * @param rl_ept            Pointer to endpoint to destroy
 *
 */
int32_t rpmsg_lite_destroy_ept(struct rpmsg_lite_instance *rpmsg_lite_dev, struct rpmsg_lite_endpoint *rl_ept);

/*!
 *
 * @brief Sends a message contained in data field of length size
 * to the remote endpoint with address dst.
 * ept->addr is used as source address in the rpmsg header
 * of the message being sent.
 *
 * @param rpmsg_lite_dev    RPMsg-Lite instance
 * @param ept               Sender endpoint
 * @param dst               Remote endpoint address
 * @param data              Payload buffer
 * @param size              Size of payload, in bytes
 * @param timeout           Timeout in ms, 0 if nonblocking
 *
 * @return Status of function execution, RL_SUCCESS on success.
 *
 */
int32_t rpmsg_lite_send(struct rpmsg_lite_instance *rpmsg_lite_dev,
                        struct rpmsg_lite_endpoint *ept,
                        uint32_t dst,
                        char *data,
                        uint32_t size,
                        uintptr_t timeout);

/*!
 * @brief Function to get the link state
 *
 * @param rpmsg_lite_dev    RPMsg-Lite instance pointer
 *
 * @return RL_TRUE when link up, RL_FALSE when down.
 *
 */
uint32_t rpmsg_lite_is_link_up(struct rpmsg_lite_instance *rpmsg_lite_dev);

/*!
 * @brief Function to wait until the link is up. Returns RL_TRUE
 * once the link_state is set or RL_FALSE in case of timeout.
 *
 * @param rpmsg_lite_dev    RPMsg-Lite instance pointer
 * @param timeout           Timeout in ms, 0 if nonblocking
 *
 * @return RL_TRUE when link up, RL_FALSE when timeout.
 *
 */
uint32_t rpmsg_lite_wait_for_link_up(struct rpmsg_lite_instance *rpmsg_lite_dev, uint32_t timeout);

#if defined(RL_API_HAS_ZEROCOPY) && (RL_API_HAS_ZEROCOPY == 1)

/*!
 * @brief Releases the rx buffer for future reuse in vring.
 * This API can be called at process context when the
 * message in rx buffer is processed.
 *
 * @param rpmsg_lite_dev    RPMsg-Lite instance
 * @param rxbuf             Rx buffer with message payload
 *
 * @return Status of function execution, RL_SUCCESS on success.
 */
int32_t rpmsg_lite_release_rx_buffer(struct rpmsg_lite_instance *rpmsg_lite_dev, void *rxbuf);

/*!
 * @brief Allocates the tx buffer for message payload.
 *
 * This API can only be called at process context to get the tx buffer in vring. By this way, the
 * application can directly put its message into the vring tx buffer without copy from an application buffer.
 * It is the application responsibility to correctly fill the allocated tx buffer by data and passing correct
 * parameters to the rpmsg_lite_send_nocopy() function to perform data no-copy-send mechanism.
 *
 * @param     rpmsg_lite_dev    RPMsg-Lite instance
 * @param[in] size              Pointer to store maximum payload size available
 * @param[in] timeout           Integer, wait upto timeout ms or not for buffer to become available
 *
 * @return The tx buffer address on success and RL_NULL on failure.
 *
 * @see rpmsg_lite_send_nocopy
 */
void *rpmsg_lite_alloc_tx_buffer(struct rpmsg_lite_instance *rpmsg_lite_dev, uint32_t *size, uintptr_t timeout);

/*!
 * @brief Sends a message in tx buffer allocated by rpmsg_lite_alloc_tx_buffer()
 *
 * This function sends txbuf of length len to the remote dst address,
 * and uses ept->addr as the source address.
 * The application has to take the responsibility for:
 *  1. tx buffer allocation (rpmsg_lite_alloc_tx_buffer())
 *  2. filling the data to be sent into the pre-allocated tx buffer
 *  3. not exceeding the buffer size when filling the data
 *  4. data cache coherency
 *
 * After the rpmsg_lite_send_nocopy() function is issued the tx buffer is no more owned
 * by the sending task and must not be touched anymore unless the rpmsg_lite_send_nocopy()
 * function fails and returns an error.
 *
 * @param rpmsg_lite_dev    RPMsg-Lite instance
 * @param[in] ept           Sender endpoint pointer
 * @param[in] dst           Destination address
 * @param[in] data          TX buffer with message filled
 * @param[in] size          Length of payload
 *
 * @return 0 on success and an appropriate error value on failure.
 *
 * @see rpmsg_lite_alloc_tx_buffer
 */
int32_t rpmsg_lite_send_nocopy(struct rpmsg_lite_instance *rpmsg_lite_dev,
                               struct rpmsg_lite_endpoint *ept,
                               uint32_t dst,
                               void *data,
                               uint32_t size);
#endif /* RL_API_HAS_ZEROCOPY */

//! @}

#if defined(__cplusplus)
}
#endif

#endif /* RPMSG_LITE_H_ */
