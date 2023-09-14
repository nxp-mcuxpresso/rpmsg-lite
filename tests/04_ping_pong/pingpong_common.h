/*
 * Copyright 2016-2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __common_h__
#define __common_h__

#define CTR_CMD_CREATE_EP (10)
#define CTR_CMD_DESTROY_EP (11)
#define CTR_CMD_SEND (12)
#define CTR_CMD_SEND_NO_COPY (13)
#define CTR_CMD_RECV (14)
#define CTR_CMD_CREATE_CHANNEL (15)
#define CTR_CMD_DESTROY_CHANNEL (16)

/* recv command modes */
#define CMD_RECV_MODE_COPY (1)
#define CMD_RECV_MODE_NOCOPY (2)

/* send command modes */
#define CMD_SEND_MODE_COPY (1)
#define CMD_SEND_MODE_NOCOPY (2)

/* acknowledge required? */
#define ACK_REQUIRED_YES (1)
#define ACK_REQUIRED_NO (0)

#define INIT_EPT_ADDR (2)
#define TEST_CNT (10)
#define TC_EPT_COUNT (6)
#define RESPONDER_APP_BUF_SIZE (30)
#define SENDER_APP_BUF_SIZE (50)
#define CMD_SEND_MSG_SIZE (20)
#define BUFFER_MAX_LENGTH (100)

#define CMD_RECV_TIMEOUT_MS (2000)

#define DESTROY_ALL_EPT (0xFFFFFFFF)

#define EP_SIGNATURE (('H' << 24) | ('D' << 16) | ('O' << 8) | ('D' << 0))

#define RPMSG_LITE_NS_ANNOUNCE_STRING "rpmsg-test-channel"

typedef struct control_message
{
    char CMD;
    char ACK_REQUIRED;
    char DATA[BUFFER_MAX_LENGTH];
} CONTROL_MESSAGE, *CONTROL_MESSAGE_PTR;

typedef struct acknowledge_message
{
    char CMD_ACK;
    int32_t RETURN_VALUE;
    int32_t TIMEOUT_MSEC;
    char RESP_DATA[BUFFER_MAX_LENGTH];
} ACKNOWLEDGE_MESSAGE, *ACKNOWLEDGE_MESSAGE_PTR;

/* create endpoint data message format */
typedef struct control_message_data_create_ept_param
{
    uint32_t ept_to_create_addr;
    uint32_t ept_to_ack_addr;
} CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM, *CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM_PTR;

/* destroy endpoint data message format */
typedef struct control_message_data_destroy_ept_param
{
    uint32_t ept_to_destroy_addr;
    uint32_t ept_to_ack_addr;
} CONTROL_MESSAGE_DATA_DESTROY_EPT_PARAM, *CONTROL_MESSAGE_DATA_DESTROY_EPT_PARAM_PTR;

/* create channel data message format */
typedef struct control_message_data_create_channel_param
{
    char name[32];
    uint32_t ep_to_ack_addr;
} CONTROL_MESSAGE_DATA_CREATE_CHANNEL_PARAM, *CONTROL_MESSAGE_DATA_CREATE_CHANNEL_PARAM_PTR;

/* delete channel data message format */
typedef struct control_message_data_delete_channel_param
{
    char name[32];
    uint32_t ep_to_ack_addr;
} CONTROL_MESSAGE_DATA_DELETE_CHANNEL_PARAM, *CONTROL_MESSAGE_DATA_DELETE_CHANNEL_PARAM_PTR;

/* send data message format */
typedef struct control_message_data_send_param
{
    uint32_t dest_addr;
    char msg[BUFFER_MAX_LENGTH - 20];
    int32_t msg_size;
    uint32_t repeat_count;
    uint32_t mode;
    uint32_t ept_to_ack_addr;
} CONTROL_MESSAGE_DATA_SEND_PARAM, *CONTROL_MESSAGE_DATA_SEND_PARAM_PTR;

/* receive data message format */
typedef struct control_message_data_recv_param
{
    struct rpmsg_endpoint *responder_ept;
    int32_t buffer_size;
    uint32_t timeout_ms;
    uint32_t mode;
    uint32_t ept_to_ack_addr;
} CONTROL_MESSAGE_DATA_RECV_PARAM, *CONTROL_MESSAGE_DATA_RECV_PARAM_PTR;

#endif // __common_h__
