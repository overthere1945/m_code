/**
    @file  qupc_sfe_api.h
    @brief QUPC SFE APIs
 */
/*=============================================================================
         Copyright (c) Qualcomm Technologies, Incorporated.
                        All rights reserved.
         Qualcomm Technologies, Confidential and Proprietary.
=============================================================================*/

/**
 @mainpage QUPC SFE Module

 @section Introduction
 This document describes the public APIs for the QUPC SFE (QUP Controller for Front End) module.
 The QUPC SFE module provides interfaces for interacting with QUP Controller instances,
 specifically tailored for Bus Manager Interface (BMI) operations.

 @section Features
 - Opening and closing QUPC SFE instances for BMI.
 - Handling QUP types, Serial Engine (SE) indices, and BMI IDs.

 @section Usage
 Applications can use these APIs to manage QUPC SFE resources necessary for BMI communication.
 Refer to individual function documentation for specific usage details.

 @addtogroup qupc_sfe_api QUPC SFE APIs
 @{

 @brief QUP Controller for Front End (SFE) APIs

 @details This file defines the public APIs for the QUPC SFE module.
          These APIs provide interfaces for interacting with QUPC SFE
          instances, particularly for Bus Manager Interface (BMI)
          operations.
*/

/** @} */ /* end_addtogroup qupc_sfe_api */

#ifndef QUPC_SFE_API_H
#define QUPC_SFE_API_H

#include "com_dtypes.h"
#include "qup_common.h"

/** @addtogroup qupc_sfe_api
@{ */

/** @name QUPC SFE Driver Version
@{ */

/**
 * @brief QUPC SFE API H Version
 * @version 1
 *
 * @par             VERSION HISTORY
 * Version          Details
 * -------          --------------------------------------------------------
 * 1                Initial version.
 */
#define QUPC_SFE_API_H_VERSION 1

/** @} */ /* end_namegroup */
/** @} */ /* end_addtogroup qupc_sfe_api */


/**
 * @brief Enumerates common status codes for QUPC SFE operations.
 *
 * These status codes indicate the success or failure of various QUPC SFE
 * API calls, providing specific error information.
 */
typedef enum
{
    QUPC_SFE_SUCCESS = 0,               /**< Operation successful. */

    QUPC_SFE_ERROR_GENERIC                                 = 0x1000,
    QUPC_SFE_ERROR_INVALID_PARAMETER,   /**< An invalid parameter was provided. */
    QUPC_SFE_ERROR_OPERATION_FAILED,    /**< General operation failure. */
    QUPC_SFE_ERROR_INVALID_HANDLE,      /**< Invalid handle provided. */
    QUPC_SFE_ERROR_TIMEOUT,             /**< Operation timed out. */
    QUPC_SFE_ERROR_ABORTED,             /**< Operation was aborted. */
    QUPC_SFE_ERROR_BMI_ALREADY_OPENED,  /**< A BMI instance with the given ID is already opened. */
    QUPC_SFE_ERROR_QUPC_DT_FAILED,      /**< QUPC device tree configuration failed. */
    QUPC_SFE_ERROR_GPII_CH_CFG_FAILED,
    QUPC_SFE_ERROR_PSC_SHMEM_DT_CFG,
    QUPC_SFE_ERROR_GET_SE_HW_CTXT_FAIL,
    QUPC_SFE_ERROR_OUT_OF_GPII_CHANNELS,
    QUPC_SFE_ERROR_GPI_IFACE_INIT,

    QUPC_SFE_ERROR_QDI                                       = 0x2000,
    QUPC_SFE_ERROR_QDI_COPY_FAIL,       /**< QDI copy operation failed. */
    QUPC_SFE_ERROR_QDI_CTXT_ALLOC_FAIL, /**< QDI context allocation failed. */
    QUPC_SFE_ERROR_QDI_MEM_MAP_FAIL,    /**< QDI memory mapping failed. */

    QUPC_SFE_ERROR_PLATFORM                                  =  0x3000,
    QUPC_SFE_ERROR_PLATFORM_INIT_FAIL,
    QUPC_SFE_ERROR_PLATFORM_DEINIT_FAIL,
    QUPC_SFE_ERROR_PLATFORM_CRIT_SEC_FAIL,
    QUPC_SFE_ERROR_PLATFORM_SIGNAL_FAIL,
    QUPC_SFE_ERROR_PLATFORM_TIMER_INIT_FAIL,
    QUPC_SFE_ERROR_PLATFORM_TIMER_SET_FAIL,
    QUPC_SFE_ERROR_PLATFORM_TIMER_CLR_FAIL,
    QUPC_SFE_ERROR_PLATFORM_TIMER_DEINIT_FAIL,
    QUPC_SFE_ERROR_PLATFORM_GET_CONFIG_FAIL,
    QUPC_SFE_ERROR_PLATFORM_REG_INT_FAIL,
    QUPC_SFE_ERROR_PLATFORM_DE_REG_INT_FAIL,
    QUPC_SFE_ERROR_PLATFORM_CLOCK_ENABLE_FAIL,
    QUPC_SFE_ERROR_PLATFORM_GPIO_ENABLE_FAIL,
    QUPC_SFE_ERROR_PLATFORM_CLOCK_DISABLE_FAIL,
    QUPC_SFE_ERROR_PLATFORM_GPIO_DISABLE_FAIL,

    /* I2C & I3C Bus errors */
    QUPC_SFE_ERROR_I2C_PROTOCOL                               =  0x4000,
    QUPC_SFE_ERROR_I2C_START_STOP_UNEXPECTED,
    QUPC_SFE_ERROR_I2C_DATA_NACK,
    QUPC_SFE_ERROR_I2C_ADDR_NACK,
    QUPC_SFE_ERROR_I2C_ARBITRATION_LOST,
    QUPC_SFE_ERROR_I2C_IBI_NACK,
    QUPC_SFE_ERROR_I2C_SLAVE_READ_TERMINATED_EARLY,
    QUPC_SFE_ERROR_I2C_CRC_PARITY,
    QUPC_SFE_ERROR_I2C_BUS_NOT_IDLE,
    QUPC_SFE_ERROR_I2C_TRANSFER_TIMEOUT,

    /* SPI Bus errors */
    QUPC_SFE_ERROR_SPI_TRANSFER                               = 0x5000,
    QUPC_SFE_ERROR_SPI_TRANSFER_TIMEOUT,
    QUPC_SFE_ERROR_SPI_PENDING_TRANSFER,

    QUPC_SFE_ERROR_MEM = 0x5000,
    QUPC_SFE_ERROR_NO_MEM,              /**< No memory available for allocation. */
    QUPC_SFE_ERROR_NO_BMI_MEM,
    QUPC_SFE_ERROR_NO_GPII_CH_MEM,
    QUPC_SFE_ERROR_MAX,                 /**< Maximum enum value, for boundary checks. */
} qupc_sfe_status;

/**
 * @brief Callback types for QUPC BMI notifications.
 *
 * These callback types indicate the reason for the callback invocation.
 */
typedef enum
{
    QUPC_BMI_CB_TYPE_BUS_ERROR = 1,     /**< Error occurred */
} qupc_bmi_cb_type;


/**
 * @brief Callback context structure for QUPC BMI notifications.
 *
 * This structure contains context information passed to the callback function.
 */
typedef struct
{
    qupc_bmi_cb_type cb_type;               /**< Type of BMI Callback  */
    qupc_sfe_status status;               /**< Type of error (if cb_type is ERROR) */
    void* bmi_handle;                       /**< BMI handle associated with the event */
} qupc_bmi_cb_ctxt;

/**
 * @brief Callback function type for QUPC BMI notifications.
 *
 * This callback function is invoked when events occur during BMI operations.
 * The client can use this to handle errors and other events appropriately.
 *
 * @param[in] cb_ctxt Context information for the callback.
 * @param[in] user_data User-provided context data.
 */
typedef void (*qupc_bmi_callback) (qupc_bmi_cb_ctxt cb_ctxt, void* user_data);

/**
 * @brief Opens a QUPC SFE BMI instance.
 *
 * This function opens a QUPC SFE BMI instance based on QUP type, SE index, and BMI ID.
 * It also allows registration of a callback function for error notifications.
 *
 * @param[in] qup_type QUP type (e.g., QUP_2).
 * @param[in] se_index SE index (Serial Engine index).
 * @param[in] bmi_id BMI ID.
 * @param[in] callback Callback function for notifications (can be NULL).
 * @param[in] user_data User context data for callback (can be NULL).
 * @param[out] qupc_bmi_handle Pointer to store the opened QUPC BMI handle.
 * @return QUPC_SFE_SUCCESS on success, error code otherwise.
 */
qupc_sfe_status qupc_bmi_open(QUP_TYPE qup_type, uint32 se_index, uint32 bmi_id,
                               qupc_bmi_callback callback, void* user_data,
                               void ** qupc_bmi_handle);

/**
 * @brief Powers on a QUPC SFE BMI instance.
 *
 * This function powers on a previously opened QUPC SFE BMI instance.
 *
 * @param[in] qupc_bmi_handle The BMI handle to power on.
 * @return QUPC_SFE_SUCCESS on success, error code otherwise.
 */
qupc_sfe_status qupc_bmi_power_on(void* qupc_bmi_handle);

/**
 * @brief Powers off a QUPC SFE BMI instance.
 *
 * This function powers off a previously opened QUPC SFE BMI instance.
 *
 * @param[in] qupc_bmi_handle The BMI handle to power off.
 * @return QUPC_SFE_SUCCESS on success, error code otherwise.
 */
qupc_sfe_status qupc_bmi_power_off(void* qupc_bmi_handle);

/**
 * @brief Closes a QUPC BMI instance.
 *
 * This function closes a previously opened QUPC BMI instance.
 *
 * @param[in] qupc_bmi_handle The BMI handle to close.
 * @return QUPC_SFE_SUCCESS on success, error code otherwise.
 */
qupc_sfe_status qupc_bmi_close(void* qupc_bmi_handle);


#endif // QUPC_SFE_API_H
