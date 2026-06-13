/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
// $QTI_LICENSE_C$

#ifndef __PDTSW_UTIL_H__
#define __PDTSW_UTIL_H__

/**
 * @brief ProductSW utility API return codes
 */
typedef enum {
    PDTSW_UTIL_SUCCESS = 0,
    PDTSW_UTIL_INVALID_ARGS_ERR,
    PDTSW_UTIL_INVALID_CMD_ERR,
    PDTSW_UTIL_ERROR
} pdtsw_util_ret_t;

/**
 * @brief Function to process the ADB utility command
 *
 * @param[in] data Pointer to input buffer.
 * @param[in] size Size of the input buffer.
 *
 * @return pdtsw_util_ret_t Status of the operation
 */
pdtsw_util_ret_t pdtsw_util_adb_cmd_process(const void* data, size_t size);

#endif // #ifndef __PDTSW_UTIL_H__