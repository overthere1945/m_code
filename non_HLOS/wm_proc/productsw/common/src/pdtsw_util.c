/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
// $QTI_LICENSE_C$

#include <zephyr/logging/log.h>
#include "pdtsw_util.h"

#ifdef CONFIG_PDTSW_COMMON_UTILS_DBG_LOGGING
LOG_MODULE_REGISTER(pdtsw_util, LOG_LEVEL_DBG);
#define PDTSW_UTIL_DBG(...)  LOG_DBG(__VA_ARGS__)
#else
#define PDTSW_UTIL_DBG(...)
#endif

/**
 * @brief Function to control shell UART Rx
 */
extern void shell_rx_control(bool enable);

/**
 * @brief ProductSW utility commands definition
 */
typedef enum {
    PDTSW_UTIL_CMD_INVALID = 0x0,
    PDTSW_UTIL_CMD_UART_RX_CTRL = 0x1
} pdtsw_util_cmd_t;

/**
 * @brief Function to process the ADB utility command
 *
 * @param[in] data Pointer to input buffer.
 * @param[in] size Size of the input buffer.
 *
 * @return pdtsw_util_ret_t Status of the operation
 */
pdtsw_util_ret_t pdtsw_util_adb_cmd_process(const void* data, size_t size)
{
    /* ensure the arguments are valid */
    if((data == NULL) || (size < (2 * sizeof(uint32_t))))
    {
        return PDTSW_UTIL_INVALID_ARGS_ERR;
    }

    uint32_t *pdata = (uint32_t *)data;
    /* parse the command -> byte[4:7]*/
    pdtsw_util_cmd_t cmd = (pdtsw_util_cmd_t)pdata[1];

    /* process the command */
    switch(cmd)
    {
        case PDTSW_UTIL_CMD_UART_RX_CTRL:
        {
            /* parse the uart rx state to be set -> byte[0:3] */
            bool enable = (pdata[0] == 0) ? false:true;
            shell_rx_control(enable);
            PDTSW_UTIL_DBG("Shell Rx:%d\n", enable);
            break;
        }
        case PDTSW_UTIL_CMD_INVALID:
        default:
            return PDTSW_UTIL_INVALID_CMD_ERR;
    }

    return PDTSW_UTIL_SUCCESS;
}