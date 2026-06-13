/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
// $QTI_LICENSE_C$

#ifndef __OFFLOAD_UTILITY_HAL_INTF_DEFS_H__
#define __OFFLOAD_UTILITY_HAL_INTF_DEFS_H__

/**
 * Opcodes exchanged on Audio/haptics Offload glink data channel.
 */
typedef enum
{
    GMI_UTILITY_HAL_NONE = 0xFFFFFF,
    /* See gmi_utility_hal_request_t for data associated with the event.
     * See gmi_utility_hal_response_t for data associated with the event from AM->AP */
    GMI_UTILITY_HAL_ADD_PATTERN = 1,

    /* See gmi_utility_hal_request_t for data associated with the event.
     * See gmi_utility_hal_response_t for data associated with the event from AM->AP */
    GMI_UTILITY_HAL_REMOVE_PATTERN,

    /* See gmi_utility_hal_request_t for data associated with the event.
     * See gmi_utility_hal_response_t for data associated with the event from AM->AP */
    GMI_UTILITY_HAL_FLUSH_ALL_PATTERN,
    /* Command to control AM */
    GMI_UTILITY_HAL_AM_CTRL
} gmi_data_utility_hal_opcode_t;

typedef enum
{
    USECASE_NONE = 0xFFFFFF,
    /* Only audio tone data */
    USECASE_AUDIO = 1,

    /* Only haptics tone data */
    USECASE_HAPTICS,

    /* Mix of audio and haptics data in same file */
    USECASE_AUDIO_SYNC_HAPTICS,

    /* Utility commands received from adb shell */
    USECASE_ADB_CMD,

    /* Sound model data for voice activation use case */
    USECASE_VOICE_ACTIVATION,

} gmi_usecase_type_t;

/* Enums to indicate message type */
typedef enum
{
    GMI_UTILITY_HAL_MSG_NONE = 0xFFFFFF,
    GMI_UTILITY_HAL_MSG_TYPE_REQUEST = 1,
    GMI_UTILITY_HAL_MSG_TYPE_RESPONSE
} gmi_utility_hal_msg_type_t;

/** Message packet header
 *  Since both request and response are indicated by same opcode, the msg_type will
 *  be used to identify whether it is a request or response to the request.
 */
typedef struct __attribute__((__packed__))
{
    gmi_data_utility_hal_opcode_t           opcode;            // opcode associated with the request or response
    gmi_usecase_type_t                      usecase_type;      // use case type as in audio/haptics/audio_sync_haptics
    gmi_utility_hal_msg_type_t              msg_type;          // request/response
} gmi_utility_hal_pkt_header_t;

/* Response error codes */
typedef enum
{
    GMI_UTILITY_HAL_ERR_NONE = 0xFFFFFF,
    GMI_UTILITY_HAL_SUCCESS = 1,
    GMI_UTILITY_HAL_FAILURE
} gmi_utility_hal_response_error_code_t;

/*=========================================Payloads b.w AP-AM================================================*/
/*
 * Payload for request. Structural representation.
 */
typedef struct __attribute__((__packed__))
{
    gmi_utility_hal_pkt_header_t            header;            // packet header
    uint32_t                                payload_size;      // size of data in payload
    uint8_t                                 payload[];         // Byte array carrying the packet data. Typecasted based on the use case type.
} gmi_utility_hal_request_t;

/**
 * Response payload
 */
typedef struct __attribute__((__packed__))
{
    gmi_utility_hal_pkt_header_t            header;            // packet header
    gmi_utility_hal_response_error_code_t   result;            // response
    uint32_t                                payload_size;      // size of data in payload
    uint8_t                                 payload[];         // Byte array carrying the packet data. Typecasted based on the use case type.
} gmi_utility_hal_response_t;

#endif //__OFFLOAD_UTILITY_HAL_INTF_DEFS_H__