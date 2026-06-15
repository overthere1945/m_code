/*=================================================================================================
 * @file disp_bt.h
 *
 * This file contains the declaration of BT events from and to Display SS
 *
 * Copyright (c) 2024-2025 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===============================================================================================*/
#pragma once

/*=================================================================================================
  Include Files
  ===============================================================================================*/
#include "stdint.h"
#include "stdbool.h"

/*=================================================================================================
  Datatypes
  ===============================================================================================*/
/** Datatype to represent a type of BT notification event. */
typedef enum
{
    /** Events From BT to Display */
    DISP_EVENT_BT_RAW_NOTIFICATION,         /**< BT notifies LPI display about the notification 
                                                with all the received data */
} disp_bt_event_type_t;

/** Datatype to represent a Bluetooth notification. */
typedef struct
{
    disp_bt_event_type_t bt_event_type; /**< Type of BT event. */
    void *data;                         /**< Pointer to data (i.e. bt event data) */
    uint32_t data_size;                 /**< Size of data */
} disp_bt_notify_t;

/*=================================================================================================
  Public Function Declaration
  ===============================================================================================*/
