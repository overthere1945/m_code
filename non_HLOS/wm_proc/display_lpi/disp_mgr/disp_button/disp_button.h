/*=================================================================================================
 * @file disp_button.h
 *
 * This file contains the header info for button sub module 
 *
 * Copyright (c) 2024-2025 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===============================================================================================*/
#pragma once
/*-------------------------------------------------------------------------------------------------
 * Include Files
 *-----------------------------------------------------------------------------------------------*/
#include "disp_mgr.h"
#include "disp_log.h"
#include "disp_error.h"
#include "api_pm_button.h"
#include "pm_button.h"

/*-------------------------------------------------------------------------------------------------
 * Structure & Enum Definitions
 *-----------------------------------------------------------------------------------------------*/
/**
 * Button ID mapping for display
 */
typedef enum
{
    DISP_BTN_ID_POWER       = PMIC_PON_BUTTON,      /**< Power/RSB button */
    DISP_BTN_ID_VOL_DOWN    = PMIC_PON_RESIN_BUTTON,/**< Volume down button */
    DISP_BTN_ID_VOL_UP      = PMIC_BUTTON_1,        /**< Volume up button */
    DISP_BTN_ID_VOL_LEFT      = PMIC_BUTTON_2,        /**< Volume up button */
    // extend this enum to add support for more button events (if required)

    DISP_BTN_ID_MAX,        /**< Max button IDs supported */
} disp_button_idx_t;


/**
 * Event type for display manager work queue item
*/
typedef enum
{
    DISP_BUTTON_PRESS = 1,        /**< Button press event */

    DISP_BAT_SOC_MAX = 0xFF       /**< Max number event Type supported */
} disp_button_evt_type_t;
