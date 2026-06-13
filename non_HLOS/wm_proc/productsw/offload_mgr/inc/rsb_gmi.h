/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
// $QTI_LICENSE_C$

#ifndef __PDTSW_RSB_GMI_H__
#define __PDTSW_RSB_GMI_H__

#include <stdint.h>

typedef enum
{
    START_OFFLOAD = 1,
    STOP_OFFLOAD = 2,
    ACK_OFFLOAD = 3
}rsb_gmi_opcode_t;

typedef enum
{
    RSB_GMI_SUCCESS = 0,
    RSB_GMI_FAIL = 1,
    RSB_GMI_CONFIG_INVALID = 2,
}rsb_gmi_resp_t;
 
typedef struct __attribute__((packed, aligned(4)))
{
    uint8_t op_mode;
    uint8_t res_x_lo;
    uint8_t res_x_hi;
    uint8_t sleep1;
    uint8_t sleep2;
    uint8_t sleep3;
    uint8_t motion_interval;
    uint8_t motion_threshold;
}rsb_gmi_config_t;
 
typedef struct __attribute__((packed, aligned(4)))
{
    uint32_t config_params;
    rsb_gmi_config_t config_data;
    uint32_t enable_voting;
}rsb_gmi_start_data_t;

typedef struct __attribute__((packed, aligned(4)))
{
    uint32_t enable_voting;
}rsb_gmi_stop_data_t;

typedef struct __attribute__((packed, aligned(4)))
{
    uint32_t resp; /**< 0 = success, non-zero on failure.*/
}rsb_gmi_ack_data_t;

/** Message type that will be exchanged over Glink.
 */
typedef struct __attribute__((packed, aligned(4)))
{
    uint32_t opcode;
    union
    {
        rsb_gmi_start_data_t start_data;    /**< Data associated with start opcode.*/
        rsb_gmi_stop_data_t stop_data;      /**< Data associated with stop opcode.*/
        rsb_gmi_ack_data_t ack_data;        /**< Data associated with ack opcode.*/
    };
}rsb_gmi_msg_t;

#endif // __PDTSW_RSB_GMI_H__