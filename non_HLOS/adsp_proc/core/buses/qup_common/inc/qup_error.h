/**
    @file  qup_error.h
    @brief QUP error mapping
 */
/*=============================================================================
            Copyright (c) Qualcomm Technologies, Incorporated.
                              All rights reserved.
              Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

#ifndef __QUP_ERROR_H__
#define __QUP_ERROR_H__

/* *************************************************************** */
/*                          INCLUDE FILES                          */
/* *************************************************************** */

#include  "i2c_api.h"
#include  "spi_api.h"

/* *************************************************************** */
/*                         DATA STRUCTURES                         */
/* *************************************************************** */

/* Errors returned from QUP DRV Layer*/
typedef enum
{
    QUP_SUCCESS = 0,

    QUP_ERROR_INVALID_PARAMETER,
    QUP_ERROR_UNSUPPORTED_CORE_INSTANCE,
    QUP_ERROR_NO_MEM,

    QUP_ERROR_API_INVALID_EXECUTION_LEVEL,
    QUP_ERROR_API_NOT_SUPPORTED,
    QUP_ERROR_API_ASYNC_MODE_NOT_SUPPORTED,
    QUP_ERROR_API_PROTOCOL_NOT_SUPPORTED,

    QUP_ERROR_HANDLE_ALLOCATION,
    QUP_ERROR_HW_INFO_ALLOCATION,
    QUP_ERROR_IO_CTXT_ALLOCATION,

    QUP_ERROR_INPUT_FIFO_OVER_RUN,
    QUP_ERROR_OUTPUT_FIFO_UNDER_RUN,
    QUP_ERROR_INPUT_FIFO_UNDER_RUN,
    QUP_ERROR_OUTPUT_FIFO_OVER_RUN,

    QUP_ERROR_COMMAND_OVER_RUN,
    QUP_ERROR_COMMAND_ILLEGAL,
    QUP_ERROR_COMMAND_FAIL,
    QUP_ERROR_COMMAND_INVALID_OPCODE,

    QUP_ERROR_PLATFORM_INIT_FAIL,
    QUP_ERROR_PLATFORM_DEINIT_FAIL,
    QUP_ERROR_PLATFORM_CRIT_SEC_FAIL,
    QUP_ERROR_PLATFORM_SIGNAL_FAIL,
    QUP_ERROR_PLATFORM_TIMER_INIT_FAIL,
    QUP_ERROR_PLATFORM_TIMER_SET_FAIL,
    QUP_ERROR_PLATFORM_TIMER_CLR_FAIL,
    QUP_ERROR_PLATFORM_TIMER_DEINIT_FAIL,
    QUP_ERROR_PLATFORM_GET_CONFIG_FAIL,
    QUP_ERROR_PLATFORM_REG_INT_FAIL,
    QUP_ERROR_PLATFORM_DE_REG_INT_FAIL,
    QUP_ERROR_PLATFORM_CLOCK_ENABLE_FAIL,
    QUP_ERROR_PLATFORM_GPIO_ENABLE_FAIL,
    QUP_ERROR_PLATFORM_CLOCK_DISABLE_FAIL,
    QUP_ERROR_PLATFORM_GPIO_DISABLE_FAIL,
    QUP_ERROR_PLATFORM_RESOURCE_SETUP_FAIL,
    QUP_ERROR_PLATFORM_RESOURCE_RESET_FAIL,

    QUP_TRANSFER_COMPLETED,

    QUP_ERROR_HANDLE_ALREADY_IN_QUEUE,
    QUP_ERROR_DMA_REG_FAIL,
    QUP_ERROR_DMA_DE_REG_FAIL,

    QUP_ERROR_DMA_TX_CHAN_ADDRESS_MAP_FAIL,
    QUP_ERROR_DMA_RX_CHAN_ADDRESS_MAP_FAIL,
    QUP_ERROR_DMA_EV_CHAN_ADDRESS_MAP_FAIL,

    /* Alloc CMD Failures Don't Change order of below Enums*/
    QUP_ERROR_DMA_TX_CHAN_ALLOC_FAIL,
    QUP_ERROR_DMA_RX_CHAN_ALLOC_FAIL,
    QUP_ERROR_DMA_EV_CHAN_ALLOC_FAIL,

    /* Start CMD Failures Don't Change order of below Enums*/
    QUP_ERROR_DMA_TX_CHAN_START_FAIL,
    QUP_ERROR_DMA_RX_CHAN_START_FAIL,

    /* Start CMD Failures Don't Change order of below Enums*/
    QUP_ERROR_DMA_TX_CHAN_STOP_FAIL,
    QUP_ERROR_DMA_RX_CHAN_STOP_FAIL,

    /* RESET CMD Failures Don't Change order of below Enums*/
    QUP_ERROR_DMA_TX_CHAN_RESET_FAIL,
    QUP_ERROR_DMA_RX_CHAN_RESET_FAIL,

    /* DeAlloc CMD Failures Don't Change order of below Enums*/
    QUP_ERROR_DMA_TX_CHAN_DE_ALLOC_FAIL,
    QUP_ERROR_DMA_RX_CHAN_DE_ALLOC_FAIL,
    QUP_ERROR_DMA_EV_CHAN_DE_ALLOC_FAIL,

    QUP_ERROR_DMA_INSUFFICIENT_RESOURCES,
    QUP_ERROR_DMA_CONFIGURATION_FAIL,
    QUP_ERROR_DMA_PROCESS_TRANSFER_FAIL,
    QUP_ERROR_DMA_EVT_OTHER,

    QUP_ERROR_I3C_DEVICE_ALLOCATION_FAILED,

    QUP_ERROR_MAX ,
} qup_status;

#define QUP_SUCCESS(x)  (x == QUP_SUCCESS)
#define QUP_ERROR(x)    (x != QUP_SUCCESS)

/* Struture to store Mapping with I2C/SPI Error Codes */
typedef struct _qup_error_map_
{
    i2c_status     i2c_error : 16;
    spi_status_t   spi_error : 16;
}qup_error_map;

/*MACRO to return the I2C/SPI Error Code*/
#define I2C_DRIVER_ERROR(x) e_map[x].i2c_error
#define SPI_DRIVER_ERROR(x) e_map[x].spi_error

/* Mapping of Error Codes */
extern const qup_error_map e_map[];

#endif

