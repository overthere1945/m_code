/*=================================================================================================
* @file disp_offload.h
*
* This file contains the declartions for dispay offload specific to LPI
*
* Copyright (c) 2024-2025 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
===============================================================================================*/
#pragma once
/*-------------------------------------------------------------------------------------------------
* Include Files
*-----------------------------------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

#define MAX_LIGHT_VALUES        (16)    /**< Number of values in lightvalue table used in ALS     */
#define MAX_METRIC_VALUES       (16)    /**< Max number of metric ids supported for sport offload */
#define DISP_LPI_VER_INFO_SIZE  (32)

/** Max size of app state string buffer */
#define APP_STATE_BUF_MAX_SIZE  (64)

/*=============================================================================
Datatypes
===========================================================================*/

/**
 * Enum for event IDs used for APSS and LPI communication.
 */
typedef enum {
    DISP_CMD_OFFLOAD_RESET      = 0x1000,   /**< Reset offload info */
    DISP_CMD_STATE_RESET,                   /**< Reset lpi state  */
    DISP_CMD_OFFLOAD_DATA,                  /**< Offload metadata and asset data */
    DISP_CMD_OFFLOAD_CONFIG,                /**< Offload all settings and other configs required */
    DISP_CMD_AMBIENT_PREPARE,               /**< Prepare LPI for ambient mode */
    DISP_CMD_AMBIENT_ENTER,                 /**< Enter into ambient mode */
    DISP_CMD_AMBIENT_EXIT,                  /**< Exit from ambient mode */
    DISP_CMD_NOTIFY_EVENT,                  /**< Notify Event info */
    DISP_CMD_SET_INFO,                      /**< Set info from APSS to LPI */
    DISP_CMD_GET_INFO,                      /**< Get info from LPI to APSS */

    DISP_CMD_BEGIN_RENDER,                  /**< Dummy enum to keep it same as LW */
    DISP_CMD_END_RENDER,                    /**< Dummy enum to keep it same as LW */

    DISP_CMD_AMBIENT_POST_EXIT,             /**< Post Exit sequence for ambient mode */
    DISP_CMD_RENDER_STOPPED,                /**< Event from Render Manger after successful Render STOP
                                                AMBIENT EXIT response has to be sent to APPS after this */
} __attribute__((packed)) disp_cmd_id_t;

/**
 * Structure to hold wrist orientation configuration.
 * 
 * Default orientation is when the watch is worn upright on said wrist
 * Retated orientation is when the watch is worn on the underside on the said wrist
 */
typedef enum {
    LEFT_WRIST_DEFAULT_ORIENTATION,               /**< Default left wrist orientation */
    LEFT_WRIST_ROTATED_ORIENTATION,               /**< Rotated left wrist orientation */
    RIGHT_WRIST_DEFAULT_ORIENTATION,              /**< Default right wrist orientation */
    RIGHT_WRIST_ROTATED_ORIENTATION,              /**< Rotated right wrist orientation */
    MAX_WRIST_DISPLAY_VAL = 0xFF,
} __attribute__((packed)) disp_wrist_orientation_t;


/** Context types */
typedef enum {
    CONTEXT_TYPE_ACTIVE_TILE_INFO,  /**< Active tile info */
    CONTEXT_TYPE_METRIC_INFO,       /**< Sensor Config    */
    CONTEXT_TYPE_BATTERY_SOC,       /**< Battery SoC      */
    CONTEXT_TYPE_APP_STATE,         /**< App state        */

    CONTEXT_TYPE_MAX_CONTEXT,
} __attribute__((packed)) disp_context_type_t;

/**
 * Structure to hold LPI offload data.
 */
typedef struct {
    uint32_t    meta_mem_offset;                /**< Metadata memoffset */
    uint32_t    meta_size;                      /**< Metadata size */
    uint32_t    asset_mem_offset;               /**< Asset data memoffset */
    uint32_t    asset_size;                     /**< Asset data size */
} __attribute__((packed)) disp_offload_data_t;

/**
 * Structure to hold ALS levels configuration.
 */
typedef struct {
    bool        is_brightness_discrete;                      /**< Discrete or continuous brightness change */
    uint8_t     als_values_len;                              /**< Length of ALS values */
    uint32_t    light_values_up[MAX_LIGHT_VALUES];           /**< Light values for increasing brightness */
    uint32_t    light_values_down[MAX_LIGHT_VALUES];         /**< Light values for decreasing brightness */
    uint32_t    brightness_values_up[MAX_LIGHT_VALUES + 1];  /**< Brightness values corresponding to light value going up */
    uint32_t    brightness_values_down[MAX_LIGHT_VALUES + 1];/**< Brightness values corresponding to light values going down */
} __attribute__((packed)) als_levels_t;

/**
 * Structure to hold ALS configuration.
 */
typedef struct {
    bool        is_als_enabled;                 /**< Flag indicating whether als based brightness control is enabled */
    uint8_t     brighten_alpha;                 /**< Conversion of ALS value to light level, increasing brightness */
    uint8_t     dimming_alpha;                  /**< Conversion of ALS value to light level, decreasing brightness */
    als_levels_t als_levels;                    /**< Different levels of ALS values and corresponding brightness */
} __attribute__((packed)) als_cfg_t;

/**
 * Structure to hold display settings.
 */
typedef struct {
    bool        always_on;                      /**< Always on display enabled */
    bool        tilt_to_bright;                 /**< Tilt event should brighten the display */
    bool        tilt_to_wake;                   /**< Tilt event should wake the APSS */
    uint32_t    disp_timeout_ms;                /**< Display timeout in milliseconds */
    disp_wrist_orientation_t orientation;       /**< watch orientation */
} __attribute__((packed)) settings_t;

/**
* Enum used to store DisplayPanelState information in payload from MSM.
*/
typedef enum
{
    DISPLAY_PANEL_ACTIVE,    /**< Panel HW state Active.*/
    DISPLAY_PANEL_LOW_POWER, /**< Panel HW state IDLE. */
    DISPLAY_PANEL_OFF,        /**< Panel HW state OFF. */
    DISP_PANEL_MAX = 0xFFFFFFFF,
} __attribute__((packed)) disp_offload_panel_state_t;



/**
* Structure to hold panel state values to be shared to HAL
*/
typedef struct
{
    uint32_t                    always_on;  /**< AOD Config value*/
    disp_offload_panel_state_t power_state; /**< Current panel power state.*/
}  __attribute__((packed))  disp_panel_state_info_t;


/**
 * Structure to hold display offload configuration.
 */
typedef struct {
    settings_t                  settings;               /**< Display settings */
    als_cfg_t                   als_cfg;                /**< ALS configuration */
    disp_offload_panel_state_t offload_panel_state;;    /**< Panel configuration */
} __attribute__((packed)) disp_offload_cfg_t;

/**
 * Structure to hold display lpi version.
 */
typedef struct
{
    uint8_t    disp_lpi_version[DISP_LPI_VER_INFO_SIZE];   /**< display lpi version */
} __attribute__((packed)) disp_version_info_t;


/**
 * Structure to hold metric value type
 */
typedef enum
{
    DISP_SNS_METRIC_VALUE_TYPE_INT,         /**< Metric value is of int type        */
    DISP_SNS_METRIC_VALUE_TYPE_FLOAT,       /**< Metric value is of float type      */
    DISP_SNS_METRIC_VALUE_TYPE_BYTES,       /**< Metric value is of byte array type */

    DISP_SNS_METRIC_VALUE_TYPE_MAX = 0xFF,  /**< Max value */
} __attribute__((packed)) disp_sns_metric_value_type_t;

/**
 * Structure to hold byte array data
 */
typedef struct
{
    uint8_t len;                        /**< Length of data        */
    uint8_t arr[MAX_METRIC_VALUES];     /**< Placeholder for data  */
} __attribute__((packed)) disp_sns_byte_arr_t;

/**
 * Structure to hold metric data
 */
typedef struct
{
    disp_sns_metric_value_type_t metric_value_type; /**< Metric value type  */
    union
    {
        uint32_t            int_val;                /**< value in int       */
        float               float_val;              /**< value in float     */
        /* Placeholder for future use */
        disp_sns_byte_arr_t byte_arr;               /**< value in byte array */
    };
} __attribute__((packed)) disp_sns_offload_metric_data_t;

/**
 * Structure to hold metric info
 */
typedef struct {
    uint32_t                       metric_id;       /**< unique metric ID */
    disp_sns_offload_metric_data_t metric_value;    /**< metric value     */
} __attribute__((packed)) disp_sns_offload_metric_info_t;


/**
 * Structure to hold sensor config
 */
typedef struct {
    uint8_t                        metric_id_len;                 /**< number of metric in array */
    disp_sns_offload_metric_info_t metric_info[MAX_METRIC_VALUES];/**< metric info placeholder   */
} __attribute__((packed)) sns_cfg_t;


/** Details pertaining to active rootlist and active root used for get & set context 
 * comms between HLOS and LPI for synchronizing UI changes during Uxoffload */
typedef struct {
    int8_t                  root_list_id;          /**< Id of active rootlist */
    uint16_t                root_info_id;          /**< Id of active root */
} __attribute__((packed)) disp_active_tile_info_t;

typedef struct {
    uint8_t len;                                /**< String length of state name */
    char    state_name[APP_STATE_BUF_MAX_SIZE]; /**< App state string */
}  __attribute__((packed)) app_state_info_t;

/** Current context - used for GET & SET CONTEXT comms */
typedef struct {
    disp_context_type_t         context_type;       /**< Type of context to be get/set */
    union {
        disp_active_tile_info_t active_tile;   /**< Active tile info */
        sns_cfg_t               sns_cfg;       /**< Sensor Configs */
        uint32_t                battery_soc;   /**< Current SoC */
        app_state_info_t        app_state;     /**< Current app state */
    };
} __attribute__((packed)) disp_context_info_t;

/**
 * display lpi capability information
 * TODO: add the lpi display capabilities here
 */
typedef struct
{
    /**<TODO: add capabilities here */
} __attribute__((packed)) disp_capability_info_t;

/**
 * display get/set info type
 */
typedef enum
{
    DISP_INFO_TYPE_VERSION      = 0x0100,       /**< display lpi version */
    DISP_INFO_TYPE_CONTEXT      = 0x0200,       /**< display lpi context info */
    DISP_INFO_TYPE_CAPABILITIES = 0x0300,       /**< display lpi capabilities */
    DISP_INFO_TYPE_PANEL_STATE  = 0x0400,       /**< display panel state info */
    DISP_INFO_TYPE_MAX          = 0xFF00,
} __attribute__((packed)) disp_info_type_t;

/**
 * display get/set info
 */
typedef struct
{
    disp_info_type_t                type;           /**< type should hold type and subtype <type>[15 - 8] <subtype>[ 7 - 0]*/
    union {
        disp_version_info_t         version;        /**< display lpi version info */
        disp_context_info_t         context;        /**< display lpi context info */
        disp_capability_info_t      capabilities;   /**< display lpi capabilities info */
        disp_panel_state_info_t     panel_state;    /**< display panel state info */
    };
} __attribute__((packed)) disp_info_t;

/*-------------------------------------------------------------------------------------------------
 * Public Function Declarations
 *-----------------------------------------------------------------------------------------------*/

/**
 * @brief     This function is for getting whether TTB is enabled or disabled.
 * @param     None
 * @return    bool: true - if TTB is enabled; false - if TTB is disabled.
 */
bool disp_offload_is_ttb_enabled();

/**
 * @brief     This function is for getting whether AOD is enabled or disabled.
 * @param     None
 * @return    bool: true - if AOD is enabled; false - if AOD is disabled.
 */
bool disp_offload_is_aod_enabled();

/**
 * @brief     This function returns the number of ambient mode entries.
 * @param     None
 * @return    uint32_t: count of ambient mode entries that have occurred.
 */
uint32_t disp_offload_no_of_amb_enter_get();


/**
 * @brief     This function is for getting panel state
 * @param     None
 * @return    disp_offload_panel_state_t
 */
disp_offload_panel_state_t disp_get_offloaded_panel_state();
