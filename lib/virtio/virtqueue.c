/*-
 * Copyright (c) 2011, Bryan Venteicher <bryanv@FreeBSD.org>
 * Copyright (c) 2016 Freescale Semiconductor, Inc.
 * Copyright 2016-2024 NXP
 * All rights reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "rpmsg_env.h"
#include "virtqueue.h"

/* Prototype for internal functions. */
static void vq_ring_update_avail(struct virtqueue *vq, uint16_t desc_idx);
static void vq_ring_update_used(struct virtqueue *vq, uint16_t head_idx, uint32_t len);
static uint16_t vq_ring_add_buffer(
    struct virtqueue *vq, struct vring_desc *desc, uint16_t head_idx, void *buffer, uint32_t length);
static int32_t vq_ring_enable_interrupt(struct virtqueue *vq, uint16_t ndesc);
static int32_t vq_ring_must_notify_host(struct virtqueue *vq);
static void vq_ring_notify_host(struct virtqueue *vq);
static uint16_t virtqueue_nused(struct virtqueue *vq);

#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
/*!
 * virtqueue_create_static - Creates new VirtIO queue - static version
 *
 * @param id        - VirtIO queue ID , must be unique
 * @param name      - Name of VirtIO queue
 * @param ring      - Pointer to vring_alloc_info control block
 * @param callback  - Pointer to callback function, invoked
 *                    when message is available on VirtIO queue
 * @param notify    - Pointer to notify function, used to notify
 *                    other side that there is job available for it
 * @param v_queue   - Created VirtIO queue.
 * @param vq_ctxt   - Statically allocated virtqueue context
 *
 * @return          - Function status
 */
int32_t virtqueue_create_static(uint16_t id,
                                const char *name,
                                struct vring_alloc_info *ring,
                                void (*callback_fc)(struct virtqueue *vq),
                                void (*notify_fc)(struct virtqueue *vq),
                                struct virtqueue **v_queue,
                                struct vq_static_context *vq_ctxt)
{
    struct virtqueue *vq    = VQ_NULL;
    volatile int32_t status = VQUEUE_SUCCESS;
    uint32_t vq_size        = 0U;

    VQ_PARAM_CHK(vq_ctxt == VQ_NULL, status, ERROR_VQUEUE_INVLD_PARAM);
    VQ_PARAM_CHK(ring == VQ_NULL, status, ERROR_VQUEUE_INVLD_PARAM);
    VQ_PARAM_CHK(ring->num_descs == 0U, status, ERROR_VQUEUE_INVLD_PARAM);
    VQ_PARAM_CHK(ring->num_descs & (ring->num_descs - 1U), status, ERROR_VRING_ALIGN);
    VQ_PARAM_CHK(ring->align > INT32_MAX, status, ERROR_VQUEUE_INVLD_PARAM);

    if ((status == VQUEUE_SUCCESS) && (ring->align > (uint32_t)INT32_MAX))
    {
        status = ERROR_VQUEUE_INVLD_PARAM;
    }

    if (status == VQUEUE_SUCCESS)
    {
        vq_size = sizeof(struct virtqueue);
        vq      = &vq_ctxt->vq;

        env_memset(vq, 0x00, vq_size);

        env_strncpy(vq->vq_name, name, VIRTQUEUE_MAX_NAME_SZ);
        vq->vq_queue_index = id;
        vq->vq_alignment   = (int32_t)(ring->align);
        vq->vq_nentries    = ring->num_descs;
        vq->callback_fc    = callback_fc;
        vq->notify_fc      = notify_fc;

        // indirect addition  is not supported
        vq->vq_ring_size = vring_size(ring->num_descs, ring->align);
        vq->vq_ring_mem  = (void *)ring->phy_addr;

        vring_init(&vq->vq_ring, vq->vq_nentries, vq->vq_ring_mem, (uint32_t)vq->vq_alignment);

        /* Cache flush initialized virt queue ring pointers */
        VQUEUE_FLUSH(vq->vq_ring.avail, sizeof(struct vring_avail));
        VQUEUE_FLUSH(vq->vq_ring.used, sizeof(struct vring_used));

        *v_queue = vq;
    }

    return (status);
}
#else
/*!
 * virtqueue_create - Creates new VirtIO queue
 *
 * @param id        - VirtIO queue ID , must be unique
 * @param name      - Name of VirtIO queue
 * @param ring      - Pointer to vring_alloc_info control block
 * @param callback  - Pointer to callback function, invoked
 *                    when message is available on VirtIO queue
 * @param notify    - Pointer to notify function, used to notify
 *                    other side that there is job available for it
 * @param v_queue   - Created VirtIO queue.
 *
 * @return          - Function status
 */
int32_t virtqueue_create(uint16_t id,
                         const char *name,
                         struct vring_alloc_info *ring,
                         void (*callback_fc)(struct virtqueue *vq),
                         void (*notify_fc)(struct virtqueue *vq),
                         struct virtqueue **v_queue)
{
    struct virtqueue *vq    = VQ_NULL;
    volatile int32_t status = VQUEUE_SUCCESS;
    uint32_t vq_size        = 0U;

    VQ_PARAM_CHK(ring == VQ_NULL, status, ERROR_VQUEUE_INVLD_PARAM);
    VQ_PARAM_CHK(ring->num_descs == 0U, status, ERROR_VQUEUE_INVLD_PARAM);
    VQ_PARAM_CHK(ring->num_descs & (ring->num_descs - 1U), status, ERROR_VRING_ALIGN);
    VQ_PARAM_CHK(ring->align > INT32_MAX, status, ERROR_VQUEUE_INVLD_PARAM);

    if ((status == VQUEUE_SUCCESS) && (ring->align > (uint32_t)INT32_MAX))
    {
        status = ERROR_VQUEUE_INVLD_PARAM;
    }

    if (status == VQUEUE_SUCCESS)
    {
        vq_size = sizeof(struct virtqueue);
        vq      = (struct virtqueue *)env_allocate_memory(vq_size);

        if (vq == VQ_NULL)
        {
            return (ERROR_NO_MEM);
        }

        env_memset(vq, 0x00, vq_size);

        env_strncpy(vq->vq_name, name, VIRTQUEUE_MAX_NAME_SZ);
        vq->vq_queue_index = id;
        vq->vq_alignment   = (int32_t)(ring->align);
        vq->vq_nentries    = ring->num_descs;
        vq->callback_fc    = callback_fc;
        vq->notify_fc      = notify_fc;

        // indirect addition  is not supported
        vq->vq_ring_size = vring_size(ring->num_descs, ring->align);
        vq->vq_ring_mem  = (void *)ring->phy_addr;

        vring_init(&vq->vq_ring, vq->vq_nentries, vq->vq_ring_mem, (uint32_t)vq->vq_alignment);

        /* Cache flush initialized virt queue ring pointers */
        VQUEUE_FLUSH(vq->vq_ring.avail, sizeof(struct vring_avail));
        VQUEUE_FLUSH(vq->vq_ring.used, sizeof(struct vring_used));

        *v_queue = vq;
    }

    return (status);
}
#endif /* RL_USE_STATIC_API */

/*!
 * virtqueue_add_buffer()   - Enqueues new buffer in vring for consumption
 *                            by other side.
 *
 * @param vq                - Pointer to VirtIO queue control block.
 * @param head_idx          - Index of buffer to be added to the avail ring
 *
 * @return                  - Function status
 */
int32_t virtqueue_add_buffer(struct virtqueue *vq, uint16_t head_idx)
{
    volatile int32_t status = VQUEUE_SUCCESS;

    VQ_PARAM_CHK(vq == VQ_NULL, status, ERROR_VQUEUE_INVLD_PARAM);

    VQUEUE_BUSY(vq, avail_write);

    if (status == VQUEUE_SUCCESS)
    {
        VQ_RING_ASSERT_VALID_IDX(vq, head_idx);

        /*
         * Update vring_avail control block fields so that other
         * side can get buffer using it.
         */
        vq_ring_update_avail(vq, head_idx);
    }

    VQUEUE_IDLE(vq, avail_write);

    return (status);
}

/*!
 * virtqueue_fill_avail_buffers - Enqueues single buffer in vring, updates avail
 *
 * @param vq                    - Pointer to VirtIO queue control block
 * @param buffer                - Address of buffer
 * @param len                   - Length of buffer
 *
 * @return                      - Function status
 */
int32_t virtqueue_fill_avail_buffers(struct virtqueue *vq, void *buffer, uint32_t len)
{
    struct vring_desc *dp;
    uint16_t head_idx;

    volatile int32_t status = VQUEUE_SUCCESS;

    VQ_PARAM_CHK(vq == VQ_NULL, status, ERROR_VQUEUE_INVLD_PARAM);

    VQUEUE_BUSY(vq, avail_write);

    if (status == VQUEUE_SUCCESS)
    {
        head_idx = vq->vq_desc_head_idx;

        dp = &vq->vq_ring.desc[head_idx];
#if defined(RL_USE_ENVIRONMENT_CONTEXT) && (RL_USE_ENVIRONMENT_CONTEXT == 1)
        dp->addr = env_map_vatopa(vq->env, buffer);
#else
        dp->addr = env_map_vatopa(buffer);
#endif
        dp->len   = len;
        dp->flags = VRING_DESC_F_WRITE;

        VQUEUE_FLUSH(&vq->vq_ring.desc[head_idx], sizeof(vq->vq_ring.desc[head_idx]));

        vq->vq_desc_head_idx++;

        vq_ring_update_avail(vq, head_idx);
    }

    VQUEUE_IDLE(vq, avail_write);

    return (status);
}

/*!
 * virtqueue_get_buffer - Returns used buffers from VirtIO queue
 *
 * @param vq            - Pointer to VirtIO queue control block
 * @param len           - Length of consumed buffer
 * @param idx           - Index to buffer descriptor pool
 *
 * @return              - Pointer to used buffer
 */
void *virtqueue_get_buffer(struct virtqueue *vq, uint32_t *len, uint16_t *idx)
{
    struct vring_used_elem *uep;
    uint16_t used_idx, desc_idx;

    /* Invalidate used->idx before it is read */
    VQUEUE_INVALIDATE(&vq->vq_ring.used->idx, sizeof(vq->vq_ring.used->idx));

    if ((vq == VQ_NULL) || (vq->vq_used_cons_idx == vq->vq_ring.used->idx))
    {
        return (VQ_NULL);
    }
    VQUEUE_BUSY(vq, used_read);

    used_idx = (uint16_t)(vq->vq_used_cons_idx & ((uint16_t)(vq->vq_nentries - 1U)));
    uep      = &vq->vq_ring.used->ring[used_idx];

    env_rmb();

    /* Invalidate used->ring before it is read */
    VQUEUE_INVALIDATE(&vq->vq_ring.used->ring[used_idx], sizeof(vq->vq_ring.used->ring[used_idx]));
    desc_idx = (uint16_t)uep->id;
    if (len != VQ_NULL)
    {
        *len = uep->len;
    }

    if (idx != VQ_NULL)
    {
        *idx = desc_idx;
    }

    vq->vq_used_cons_idx++;

    VQUEUE_IDLE(vq, used_read);

#if defined(RL_USE_ENVIRONMENT_CONTEXT) && (RL_USE_ENVIRONMENT_CONTEXT == 1)
    return env_map_patova(vq->env, ((uint32_t)(vq->vq_ring.desc[desc_idx].addr)));
#else
    return env_map_patova((uint32_t)(vq->vq_ring.desc[desc_idx].addr));
#endif
}

/*!
 * virtqueue_get_buffer_length - Returns size of a buffer
 *
 * @param vq            - Pointer to VirtIO queue control block
 * @param idx           - Index to buffer descriptor pool
 *
 * @return              - Buffer length
 */
uint32_t virtqueue_get_buffer_length(struct virtqueue *vq, uint16_t idx)
{
    /* Invalidate used->ring before it is read */
    VQUEUE_INVALIDATE(&vq->vq_ring.desc[idx].len, sizeof(vq->vq_ring.desc[idx].len));
    return vq->vq_ring.desc[idx].len;
}

#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
/*!
 * virtqueue_free   - Frees VirtIO queue resources - static version
 *
 * @param vq        - Pointer to VirtIO queue control block
 *
 */
void virtqueue_free_static(struct virtqueue *vq)
{
    if (vq != VQ_NULL)
    {
        if (vq->vq_ring_mem != VQ_NULL)
        {
            vq->vq_ring_size = 0;
            vq->vq_ring_mem  = VQ_NULL;
        }
    }
}
#else
/*!
 * virtqueue_free   - Frees VirtIO queue resources
 *
 * @param vq        - Pointer to VirtIO queue control block
 *
 */
void virtqueue_free(struct virtqueue *vq)
{
    if (vq != VQ_NULL)
    {
        if (vq->vq_ring_mem != VQ_NULL)
        {
            vq->vq_ring_size = 0;
            vq->vq_ring_mem  = VQ_NULL;
        }

        env_free_memory(vq);
    }
}
#endif /* RL_USE_STATIC_API */

/*!
 * virtqueue_get_available_buffer   - Returns buffer available for use in the
 *                                    VirtIO queue
 *
 * @param vq                        - Pointer to VirtIO queue control block
 * @param avail_idx                 - Pointer to index used in vring desc table
 * @param len                       - Length of buffer
 *
 * @return                          - Pointer to available buffer
 */
void *virtqueue_get_available_buffer(struct virtqueue *vq, uint16_t *avail_idx, uint32_t *len)
{
    uint16_t head_idx = 0;
    void *buffer;

    /* Invalidate avail->idx before it is read */
    VQUEUE_INVALIDATE(&vq->vq_ring.avail->idx, sizeof(vq->vq_ring.avail->idx));
    if (vq->vq_available_idx == vq->vq_ring.avail->idx)
    {
        return (VQ_NULL);
    }

    VQUEUE_BUSY(vq, avail_read);

    head_idx = (uint16_t)(vq->vq_available_idx++ & ((uint16_t)(vq->vq_nentries - 1U)));

    /* Invalidate avail->ring before it is read */
    VQUEUE_INVALIDATE(&vq->vq_ring.avail->ring[head_idx], sizeof(vq->vq_ring.avail->ring[head_idx]));

    *avail_idx = vq->vq_ring.avail->ring[head_idx];

    env_rmb();

#if defined(RL_USE_ENVIRONMENT_CONTEXT) && (RL_USE_ENVIRONMENT_CONTEXT == 1)
    buffer = env_map_patova(vq->env, ((uint32_t)(vq->vq_ring.desc[*avail_idx].addr)));
#else
    buffer   = env_map_patova((uint32_t)(vq->vq_ring.desc[*avail_idx].addr));
#endif
    *len = vq->vq_ring.desc[*avail_idx].len;

    VQUEUE_IDLE(vq, avail_read);

    return (buffer);
}

/*!
 * virtqueue_add_consumed_buffer - Returns consumed buffer back to VirtIO queue
 *
 * @param vq                     - Pointer to VirtIO queue control block
 * @param head_idx               - Index of vring desc containing used buffer
 * @param len                    - Length of buffer
 *
 * @return                       - Function status
 */
int32_t virtqueue_add_consumed_buffer(struct virtqueue *vq, uint16_t head_idx, uint32_t len)
{
    if (head_idx > vq->vq_nentries)
    {
        return (ERROR_VRING_NO_BUFF);
    }

    VQUEUE_BUSY(vq, used_write);
    vq_ring_update_used(vq, head_idx, len);
    VQUEUE_IDLE(vq, used_write);

    return (VQUEUE_SUCCESS);
}

/*!
 * virtqueue_fill_used_buffers - Fill used buffer ring
 *
 * @param vq                     - Pointer to VirtIO queue control block
 * @param buffer                 - Buffer to add
 * @param len                    - Length of buffer
 *
 * @return                       - Function status
 */
int32_t virtqueue_fill_used_buffers(struct virtqueue *vq, void *buffer, uint32_t len)
{
    uint16_t head_idx;
    uint16_t idx;

    VQUEUE_BUSY(vq, used_write);

    head_idx = vq->vq_desc_head_idx;
    VQ_RING_ASSERT_VALID_IDX(vq, head_idx);

    /* Enqueue buffer onto the ring. */
    idx = vq_ring_add_buffer(vq, vq->vq_ring.desc, head_idx, buffer, len);

    vq->vq_desc_head_idx = idx;

    vq_ring_update_used(vq, head_idx, len);

    VQUEUE_IDLE(vq, used_write);

    return (VQUEUE_SUCCESS);
}

/*!
 * virtqueue_enable_cb  - Enables callback generation
 *
 * @param vq            - Pointer to VirtIO queue control block
 *
 * @return              - Function status
 */
int32_t virtqueue_enable_cb(struct virtqueue *vq)
{
    /* coco begin validated: This virtqueue function does not need to be tested because it is not used in rpmsg_lite
     * implementation */
    return (vq_ring_enable_interrupt(vq, 0));
}
/* coco end */
/*!
 * virtqueue_enable_cb - Disables callback generation
 *
 * @param vq           - Pointer to VirtIO queue control block
 *
 */
void virtqueue_disable_cb(struct virtqueue *vq)
{
    VQUEUE_BUSY(vq, avail_write);

    if ((vq->vq_flags & VIRTQUEUE_FLAG_EVENT_IDX) != 0UL)
    {
        /* coco begin validated: This part does not need to be tested because VIRTQUEUE_FLAG_EVENT_IDX is not being
         * utilized in rpmsg_lite implementation */
        vring_used_event(&vq->vq_ring) = vq->vq_used_cons_idx - vq->vq_nentries - 1U;
        VQUEUE_FLUSH(&vring_used_event(&vq->vq_ring), sizeof(vring_used_event(&vq->vq_ring)));
    }
    /* coco end */
    else
    {
        vq->vq_ring.avail->flags |= (uint16_t)VRING_AVAIL_F_NO_INTERRUPT;
        VQUEUE_FLUSH(&vq->vq_ring.avail->flags, sizeof(vq->vq_ring.avail->flags));
    }

    VQUEUE_IDLE(vq, avail_write);
}

/*!
 * virtqueue_kick - Notifies other side that there is buffer available for it.
 *
 * @param vq      - Pointer to VirtIO queue control block
 */
void virtqueue_kick(struct virtqueue *vq)
{
    VQUEUE_BUSY(vq, avail_write);

    /* Ensure updated avail->idx is visible to host. */
    env_mb();

    if (0 != vq_ring_must_notify_host(vq))
    {
        vq_ring_notify_host(vq);
    }
    vq->vq_queued_cnt = 0;

    VQUEUE_IDLE(vq, avail_write);
}

/*!
 * virtqueue_dump Dumps important virtqueue fields , use for debugging purposes
 *
 * @param vq - Pointer to VirtIO queue control block
 */
void virtqueue_dump(struct virtqueue *vq)
{
    /* coco begin validated: This virtqueue function does not need to be tested because it is not used in rpmsg_lite
     * implementation */
    if (vq == VQ_NULL)
    {
        return;
    }
    /* Invalidate avail and used before read */
    VQUEUE_INVALIDATE(vq->vq_ring.avail, sizeof(struct vring_avail));
    VQUEUE_INVALIDATE(vq->vq_ring.used, sizeof(struct vring_used));

    env_print(
        "VQ: %s - size=%d; used=%d; queued=%d; "
        "desc_head_idx=%d; avail.idx=%d; used_cons_idx=%d; "
        "used.idx=%d; avail.flags=0x%x; used.flags=0x%x\r\n",
        vq->vq_name, vq->vq_nentries, virtqueue_nused(vq), vq->vq_queued_cnt, vq->vq_desc_head_idx,
        vq->vq_ring.avail->idx, vq->vq_used_cons_idx, vq->vq_ring.used->idx, vq->vq_ring.avail->flags,
        vq->vq_ring.used->flags);
}
/* coco end */

/*!
 * virtqueue_get_desc_size - Returns vring descriptor size
 *
 * @param vq            - Pointer to VirtIO queue control block
 *
 * @return              - Descriptor length
 */
uint32_t virtqueue_get_desc_size(struct virtqueue *vq)
{
    /* coco begin validated: This virtqueue function does not need to be tested because it is not used in rpmsg_lite
     * implementation */
    uint16_t head_idx;
    uint16_t avail_idx;
    uint32_t len;

    /* Invalidate avail->idx before read */
    VQUEUE_INVALIDATE(&vq->vq_ring.avail->idx, sizeof(vq->vq_ring.avail->idx));

    if (vq->vq_available_idx == vq->vq_ring.avail->idx)
    {
        return 0;
    }

    head_idx = (uint16_t)(vq->vq_available_idx & ((uint16_t)(vq->vq_nentries - 1U)));

    /* Invalidate avail->ring before read */
    VQUEUE_INVALIDATE(&vq->vq_ring.avail->ring[head_idx], sizeof(vq->vq_ring.avail->ring[head_idx]));

    avail_idx = vq->vq_ring.avail->ring[head_idx];

    /* Invalidate len before read */
    VQUEUE_INVALIDATE(&vq->vq_ring.desc[avail_idx].len, sizeof(vq->vq_ring.desc[avail_idx].len));

    len = vq->vq_ring.desc[avail_idx].len;

    return (len);
}
/* coco end */

/**************************************************************************
 *                            Helper Functions                            *
 **************************************************************************/

/*!
 *
 * vq_ring_add_buffer
 *
 */
static uint16_t vq_ring_add_buffer(
    struct virtqueue *vq, struct vring_desc *desc, uint16_t head_idx, void *buffer, uint32_t length)
{
    struct vring_desc *dp;

    if (buffer == VQ_NULL)
    {
        return head_idx; /* coco validated: line never reached, vq_ring_add_buffer() is called from
                            rpmsg_lite_master_init() only and the buffer parameter not being null check is done before
                            passing the parameter */
    }

    VQASSERT(vq, head_idx != VQ_RING_DESC_CHAIN_END, "premature end of free desc chain");

    dp = &desc[head_idx];
#if defined(RL_USE_ENVIRONMENT_CONTEXT) && (RL_USE_ENVIRONMENT_CONTEXT == 1)
    dp->addr = env_map_vatopa(vq->env, buffer);
#else
    dp->addr = env_map_vatopa(buffer);
#endif
    dp->len   = length;
    dp->flags = VRING_DESC_F_WRITE;

    /* Flush desc after write */
    VQUEUE_FLUSH(&desc[head_idx], sizeof(desc[head_idx]));

    return ++head_idx;
}

/*!
 *
 * vq_ring_init
 *
 */
void vq_ring_init(struct virtqueue *vq)
{
    struct vring *vr;
    uint32_t i, size;

    size = (uint32_t)(vq->vq_nentries);
    vr   = &vq->vq_ring;

    for (i = 0U; i < size - 1U; i++)
    {
        vr->desc[i].next = (uint16_t)(i + 1U);
    }
    vr->desc[i].next = (uint16_t)VQ_RING_DESC_CHAIN_END;
}

/*!
 *
 * vq_ring_update_avail
 *
 */
static void vq_ring_update_avail(struct virtqueue *vq, uint16_t desc_idx)
{
    uint16_t avail_idx;

    /*
     * Place the head of the descriptor chain into the next slot and make
     * it usable to the host. The chain is made available now rather than
     * deferring to virtqueue_notify() in the hopes that if the host is
     * currently running on another CPU, we can keep it processing the new
     * descriptor.
     */
    /* Invalidate avail->idx before read */
    VQUEUE_INVALIDATE(&vq->vq_ring.avail->idx, sizeof(vq->vq_ring.avail->idx));
    avail_idx                          = (uint16_t)(vq->vq_ring.avail->idx & ((uint16_t)(vq->vq_nentries - 1U)));
    vq->vq_ring.avail->ring[avail_idx] = desc_idx;

    /* Flush avail->ring after write */
    VQUEUE_FLUSH(&vq->vq_ring.avail->ring[avail_idx], sizeof(vq->vq_ring.avail->ring[avail_idx]));

    env_wmb();

    vq->vq_ring.avail->idx++;

    /* Flush idx after write */
    VQUEUE_FLUSH(&vq->vq_ring.avail->idx, sizeof(vq->vq_ring.avail->idx));

    /* Keep pending count until virtqueue_notify(). */
    vq->vq_queued_cnt++;
}

/*!
 *
 * vq_ring_update_used
 *
 */
static void vq_ring_update_used(struct virtqueue *vq, uint16_t head_idx, uint32_t len)
{
    uint16_t used_idx;
    struct vring_used_elem *used_desc = VQ_NULL;

    /*
     * Place the head of the descriptor chain into the next slot and make
     * it usable to the host. The chain is made available now rather than
     * deferring to virtqueue_notify() in the hopes that if the host is
     * currently running on another CPU, we can keep it processing the new
     * descriptor.
     */
    /* Invalidate used->idx before read */
    VQUEUE_INVALIDATE(&vq->vq_ring.used->idx, sizeof(vq->vq_ring.used->idx));
    used_idx       = vq->vq_ring.used->idx & (vq->vq_nentries - 1U);
    used_desc      = &(vq->vq_ring.used->ring[used_idx]);
    used_desc->id  = head_idx;
    used_desc->len = len;

    /* Flush used->ring after write */
    VQUEUE_FLUSH(&(vq->vq_ring.used->ring[used_idx]), sizeof(vq->vq_ring.used->ring[used_idx]));

    env_wmb();

    vq->vq_ring.used->idx++;

    /* Flush used->idx after write */
    VQUEUE_FLUSH(&vq->vq_ring.used->idx, sizeof(vq->vq_ring.used->idx));
}

/*!
 *
 * vq_ring_enable_interrupt
 *
 */
static int32_t vq_ring_enable_interrupt(struct virtqueue *vq, uint16_t ndesc)
{
    /* coco begin validated: This virtqueue function does not need to be tested because it is not used in rpmsg_lite
     * implementation */
    /*
     * Enable interrupts, making sure we get the latest index of
     * what's already been consumed.
     */
    if ((vq->vq_flags & VIRTQUEUE_FLAG_EVENT_IDX) != 0UL)
    {
        vring_used_event(&vq->vq_ring) = vq->vq_used_cons_idx + ndesc;
        VQUEUE_FLUSH(&vring_used_event(&vq->vq_ring), sizeof(vring_used_event(&vq->vq_ring)));
    }
    else
    {
        vq->vq_ring.avail->flags &= ~(uint16_t)VRING_AVAIL_F_NO_INTERRUPT;
        VQUEUE_FLUSH(&vq->vq_ring.avail->flags, sizeof(vq->vq_ring.avail->flags));
    }

    env_mb();

    /*
     * Enough items may have already been consumed to meet our threshold
     * since we last checked. Let our caller know so it processes the new
     * entries.
     */
    if (virtqueue_nused(vq) > ndesc)
    {
        return (1);
    }

    return (0);
}
/* coco end */

/*!
 *
 * virtqueue_interrupt
 *
 */
void virtqueue_notification(struct virtqueue *vq)
{
    if (vq != VQ_NULL)
    {
        if (vq->callback_fc != VQ_NULL)
        {
            vq->callback_fc(vq);
        }
    }
}

/*!
 *
 * vq_ring_must_notify_host
 *
 */
static int32_t vq_ring_must_notify_host(struct virtqueue *vq)
{
    uint16_t new_idx, prev_idx;
    uint16_t event_idx;

    if ((vq->vq_flags & VIRTQUEUE_FLAG_EVENT_IDX) != 0UL)
    {
        /* coco begin validated: This part does not need to be tested because VIRTQUEUE_FLAG_EVENT_IDX is not being
         * utilized in rpmsg_lite implementation */
        /* Invalidate avail->idx before read */
        VQUEUE_INVALIDATE(&vq->vq_ring.avail->idx, sizeof(vq->vq_ring.avail->idx));
        new_idx  = vq->vq_ring.avail->idx;
        prev_idx = new_idx - vq->vq_queued_cnt;
        VQUEUE_INVALIDATE(&vring_avail_event(&vq->vq_ring), sizeof(vring_avail_event(&vq->vq_ring)));
        event_idx = (uint16_t)vring_avail_event(&vq->vq_ring);

        return ((vring_need_event(event_idx, new_idx, prev_idx) != 0) ? 1 : 0);
    }
    /* coco end */

    /* Invalidate flags before read */
    VQUEUE_INVALIDATE(&vq->vq_ring.used->flags, sizeof(vq->vq_ring.used->flags));
    return (((vq->vq_ring.used->flags & ((uint16_t)VRING_USED_F_NO_NOTIFY)) == 0U) ? 1 : 0);
}

/*!
 *
 * vq_ring_notify_host
 *
 */
static void vq_ring_notify_host(struct virtqueue *vq)
{
    if (vq->notify_fc != VQ_NULL)
    {
        vq->notify_fc(vq);
    }
}

/*!
 *
 * virtqueue_nused
 *
 */
static uint16_t virtqueue_nused(struct virtqueue *vq)
{
    /* coco begin validated: This virtqueue function does not need to be tested because it is not used in rpmsg_lite
     * implementation */
    uint16_t used_idx, nused;

    /* Invalidate used-idx before read */
    VQUEUE_INVALIDATE(&vq->vq_ring.used->idx, sizeof(vq->vq_ring.used->idx));
    used_idx = vq->vq_ring.used->idx;

    nused = (uint16_t)(used_idx - vq->vq_used_cons_idx);
    VQASSERT(vq, nused <= vq->vq_nentries, "used more than available");

    return (nused);
}
/* coco end */
