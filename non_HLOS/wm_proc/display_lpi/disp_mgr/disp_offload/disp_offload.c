/*=================================================================================================
 * @file disp_offload.c
 *
 * This file contains the implementation of Offload events from and to Display SS
 *
 * Copyright (c) 2024-2025 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===============================================================================================*/

/*-------------------------------------------------------------------------------------------------
 * Include Files
 *-----------------------------------------------------------------------------------------------*/
#include "disp_mgr.h"
#include "disp_log.h"
#include "disp_error.h"
#include "disp_offload.h"
#include "disp_glink_apss.h"
#include "disp_glink_lpass.h"
#include "disp_asset_mgr.h"
#include "disp_dcp_drv.h"
#include "disp_heap.h"
#include "disp_ui.h"
#include "disp_rm.h"
#include "disp_platform.h"
#include "disp_battery.h"
#include "disp_ui.h"
#include "disp_dcp_drv.h"
#include "disp_sns.h"
#include "disp_sns_als_brightness_control.h"
#ifdef CONFIG_QC_DISPLAY_CLI_ENABLE
#include "disp_nema_vg_test.h"
#endif
#include "qapi_touch_service.h"

/*-------------------------------------------------------------------------------------------------
 * Macro Declarations and Constants
 *-----------------------------------------------------------------------------------------------*/
#define MAX_DEBUG_ENTRIES (20)

/*-------------------------------------------------------------------------------------------------
 * Structure & Enum Definitions
 *-----------------------------------------------------------------------------------------------*/
typedef struct {
    struct {
        disp_cmd_id_t   cmd;
        disp_osa_time_t ts;
    } cmd_hist[MAX_DEBUG_ENTRIES];
    uint8_t _cmd_hist_idx;

    uint32_t n_cmds_rcvd;
} offload_debug_t;

 /**
  * offload context should have info about metadata, asset data and configs offloaded
 */
typedef struct {
    bool                is_amb_exit_pending;    /**< Flag to check if amb exit is pending   */
    uint16_t            cur_event;              /**< Current event received                 */
    uint16_t            last_event;             /**< Last event successfully handled        */
    uint32_t            n_amb_enter;            /**< Number of ambient enters happened      */
    uint32_t            n_amb_exit;             /**< Number of ambient exits happened       */
    disp_offload_data_t data;                   /**< Offload data structure                 */
    disp_offload_cfg_t  cfg;                    /**< Offload configuration structure        */
    sns_cfg_t           sns_cfg;                /**< Sensor metric data                     */
    disp_module_desc_t  module_desc;            /**< Offload module descriptor              */

#if defined(CONFIG_QC_DISPLAY_DEBUG_ENABLE)
    offload_debug_t     debug;
#endif
} disp_offload_ctx_t;

/*-------------------------------------------------------------------------------------------------
 * Static Data Declarations
 *-----------------------------------------------------------------------------------------------*/
static disp_offload_ctx_t disp_offload_ctx;
static disp_module_desc_t * const mod_desc_p = &disp_offload_ctx.module_desc;
static uint8_t disp_version_info[DISP_LPI_VER_INFO_SIZE] = "disp_lpi_v2.0";

/*-------------------------------------------------------------------------------------------------
 * Local Function Definitions
 *-----------------------------------------------------------------------------------------------*/
// TODO: This is for dummy offload event call
#if 0
volatile bool offload_trigg_flag = false;
volatile uint32_t meta_offload_memory = 0, asset_offload_memory = 0;
void dummy_offload_event_call(void)
{

    disp_offload_data_t data = 
    {
        .meta_mem_offset = 0,
        .meta_size = 0,
        .asset_mem_offset = 0,
        .asset_size = 0,
    };

    data.meta_size = 1024 * 2;
    data.meta_mem_offset = (uint32_t)disp_malloc(DISP_HEAP_TYPE_TCM, data.meta_size);//(uint32_t)meta_data;
    meta_offload_memory = (uint32_t)data.meta_mem_offset;

    data.asset_mem_offset = (uint32_t)disp_malloc(DISP_HEAP_TYPE_AODRAM, 850000);
    data.asset_size = 850000;
    asset_offload_memory = (uint32_t)data.asset_mem_offset;

    for(int i = 0; i < 618348;)
    {
        ((uint8_t*)data.asset_mem_offset)[i++] = 0xA1;
        ((uint8_t*)data.asset_mem_offset)[i++] = 0xA2;
        ((uint8_t*)data.asset_mem_offset)[i++] = 0xA3;
    }
    
    for(int j = 0; j < 52164/4; j++)
    {
        ((uint32_t *)(data.asset_mem_offset + 618352))[j] = 0xB1B2B3B4;
    }
    for(int k = 0; k < 67392/4; k++)
    {
        ((uint32_t *)(data.asset_mem_offset + 670520))[k] = 0xC1C2C3C4;
    }

    for(int l = 0; l < 75492/4; l++)
    {
        ((uint32_t *)(data.asset_mem_offset + 737920))[l] = 0xD1D2D3D4;
    }

    while(!offload_trigg_flag)
    {
       disp_osa_thread_sleep(100);
    }

    disp_event_t event = {0};
    disp_ret_t ret = DISP_RET_SUCCESS;
    event.event_id = DISP_CMD_OFFLOAD_DATA;
    event.data_size = sizeof(disp_offload_data_t);
    event.data = disp_malloc(DISP_HEAP_TYPE_TCM, event.data_size);
    memscpy(event.data, event.data_size, &data, sizeof(disp_offload_data_t));
    DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, " Offload  Event Sending!!!");
    ret = disp_dmgr_send_event(DISP_MODULE_OFFLOAD, event);
    DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, " Offload Event ret=%d", ret);

    event.event_id = DISP_CMD_AMBIENT_PREPARE;
    event.data_size = 0;
    event.data = 0;
    DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, " Offload  Event Sending!!!");
    ret = disp_dmgr_send_event(DISP_MODULE_OFFLOAD, event);
    DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, " Offload Event ret=%d", ret);

    event.event_id = DISP_CMD_AMBIENT_ENTER;
    event.data_size = 0;
    event.data = 0;
    DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, " Offload  Event Sending!!!");
    ret = disp_dmgr_send_event(DISP_MODULE_OFFLOAD, event);
    DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, " Offload Event ret=%d", ret);

}
#endif

static void update_debug_cmd_hist(disp_cmd_id_t cmd)
{
#if defined(CONFIG_QC_DISPLAY_DEBUG_ENABLE)
    disp_offload_ctx.debug.n_cmds_rcvd++;

    uint8_t idx = disp_offload_ctx.debug._cmd_hist_idx;
    disp_offload_ctx.debug.cmd_hist[idx].cmd = cmd;
    disp_offload_ctx.debug.cmd_hist[idx].ts  = disp_osa_get_sys_curr_ticks();
    disp_offload_ctx.debug._cmd_hist_idx = (MAX_DEBUG_ENTRIES == (idx + 1)) ? 0 : (idx + 1);
#endif
}

/**
 * @brief     This is the callback func registered with disp manager for module enable
 * @param[in] args_p pointer to args
 * @return    disp_ret_t
 */
static disp_ret_t disp_offload_enable(void *args_p)
{
    DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, "offload module enabled");

    // TODO: This is for Dummy offload test
    // dummy_offload_event_call();
    return DISP_RET_SUCCESS;
}

/**
 * @brief     This is the callback func registered with disp manager for module disable
 * @param[in] args_p pointer to args
 * @return    disp_ret_t
 */
static disp_ret_t disp_offload_disable(void *args_p)
{
    DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, "offload module disabled");
    return DISP_RET_SUCCESS;
}

/**
 * @brief     This is the callback func registered with disp manager for handling ssr
 * @param[in] args_p pointer to args
 * @return    disp_ret_t
 */
static disp_ret_t disp_offload_ssr(void *args_p)
{
    DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, "offload module ssr done");
    return DISP_RET_SUCCESS;
}

/**
 * @brief     This function is used to enabled offloaded metric IDs
 * @param[in] enable true - enables offloaded metrics, false - disables offloaded metrics
 * @return    disp_ret_t
 */
static disp_ret_t disp_sns_enable_metrics(bool enable)
{
    disp_ret_t ret = DISP_RET_SUCCESS;
    sns_cfg_t *sns_cfg_p = &disp_offload_ctx.sns_cfg;

    if (true == enable)
    {
        // Register for sensor metric data
        for (uint8_t i = 0; i < sns_cfg_p->metric_id_len; i++)
        {
            uint32_t metric_id = sns_cfg_p->metric_info[i].metric_id;
            if (0 == metric_id)
            {
                DISP_PRINTF(MOD_DISP_LOG, DISP_WARN_LVL, "Metric ID not defined %d", i);
                continue;
            }

            disp_ret_t metric_ret = 
                disp_sns_update_metric_req(metric_id, true);
            if (DISP_RET_SUCCESS != metric_ret)
            {
                DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, 
                "Metric Registration failed for %d (rc=%d)",metric_id, metric_ret);
            }
            else
            {
                DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, 
                "Metric Registration success for %d", metric_id);
                ret = metric_ret;
            }

            // update the current value to UI, even though metric enable failed
            disp_ui_input_t *ui_input_p = disp_malloc(DISP_HEAP_TYPE_TCM, sizeof(disp_ui_input_t));
            if (NULL == ui_input_p)
            {
                DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "malloc failed for %d", 
                                                                    sizeof(disp_ui_input_t));
                return DISP_RET_OUT_OF_MEMORY;
            }
            ui_input_p->type = DISP_UI_INPUT_TYPE_SNS;
            ui_input_p->input_data_p = disp_malloc(DISP_HEAP_TYPE_TCM, sizeof(disp_sns_offload_metric_info_t));
            if (NULL == ui_input_p->input_data_p)
            {
                DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "malloc failed for %d", 
                                                                    sizeof(disp_sns_offload_metric_info_t));
                disp_free(ui_input_p);
                return DISP_RET_OUT_OF_MEMORY;
            }

            memscpy(ui_input_p->input_data_p, sizeof(disp_sns_offload_metric_info_t),
                    &(sns_cfg_p->metric_info[i]), sizeof(disp_sns_offload_metric_info_t));

            disp_ui_task_info_t task_info = {0};
            task_info.task     = DISP_UI_TASK_PROCESS_INPUT;
            task_info.data_p   = ui_input_p;
            task_info.data_len = sizeof(disp_ui_input_t);

            disp_ret_t rc = disp_ui_queue_task(&task_info, DISP_UI_TASK_PRIORITY_REG);
            if (DISP_RET_SUCCESS != rc)
            {
                disp_free(ui_input_p->input_data_p);
                disp_free(ui_input_p);
                DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "disp offload task queue to UI failed %d", rc);
                ret = rc;
                continue;
            }
        }
    }
    else
    {
        // De-register sensor metrics
        for (uint8_t i = 0; i < sns_cfg_p->metric_id_len; i++)
        {
            uint32_t metric_id = sns_cfg_p->metric_info[i].metric_id;

            disp_ret_t rc = disp_sns_update_metric_req(metric_id, false);
            if (DISP_RET_SUCCESS != rc)
            {
                DISP_PRINTF(MOD_DISP_LOG, DISP_WARN_LVL, 
                "Metric de-registration failed for %d (rc=%d)",metric_id, rc);
            }
            else
            {
                DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, 
                "Metric de-registrated for %d", metric_id);
            }
        }
    }

    return ret;
}

/** Process the various get info request types from HLOS */
disp_ret_t process_set_info_req(disp_event_t* event_p, disp_event_t *rsp_event_p)
{
    disp_ret_t ret = DISP_RET_SUCCESS;

    disp_info_t *req_data_p = (disp_info_t *) event_p->data;
    disp_info_t res_data = {
        .type = req_data_p->type
    };

    switch (req_data_p->type)
    {
        case DISP_INFO_TYPE_VERSION:
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_WARN_LVL, "disp lpi set version : invalid");
            ret = DISP_RET_NOT_SUPPORTED_ERROR;
        }
        break;

        case DISP_INFO_TYPE_CONTEXT:
        {
            disp_ui_settings_t settings_p;
            disp_context_info_t *context_info_p = &req_data_p->context;

            disp_context_type_t ctx_type = context_info_p->context_type;
            if (CONTEXT_TYPE_MAX_CONTEXT <= ctx_type)
            {
                DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, 
                    "Invalid context type:%d",ctx_type);
                ret = DISP_RET_FAILED;
                break;
            }

            switch (ctx_type)
            {
                case CONTEXT_TYPE_ACTIVE_TILE_INFO:
                {
                    settings_p.onload_tile_id = context_info_p->active_tile.root_info_id;
                    settings_p.onload_view_id = context_info_p->active_tile.root_list_id;
                    ret = disp_ui_update_settings(&settings_p);
                    if (DISP_RET_SUCCESS != ret)
                    {
                        DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, 
                            "Update UI settings failed (rc=%d)", ret);
                        break;
                    }

                    DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, 
                        "disp lpi set context: rootlist id %d, root id %d", 
                        context_info_p->active_tile.root_info_id, 
                        context_info_p->active_tile.root_list_id);

                }
                break;

                case CONTEXT_TYPE_METRIC_INFO:
                {
                    DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, 
                        "Set context: metric info. id cnt=%d", context_info_p->sns_cfg.metric_id_len);

                    /**
                     * copy received list of metric ID and value in ctx
                     * This will be used to register for the metric during the offload
                     * The list is expected to be updated before the AMB_PREP
                    */
                    memscpy(&disp_offload_ctx.sns_cfg, sizeof(sns_cfg_t),
                            &context_info_p->sns_cfg, sizeof(sns_cfg_t));

                    disp_ui_set_sns_metric_db(&disp_offload_ctx.sns_cfg);
                }
                break;

                case CONTEXT_TYPE_BATTERY_SOC:
                {
                    uint32_t bat_val = req_data_p->context.battery_soc;
                    DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, 
                        "Set context: battery-soc val=%d", bat_val);

                    /* Send battery SoC data to UI */
                    disp_ui_input_t *ui_input_p = disp_malloc(DISP_HEAP_TYPE_TCM, sizeof(disp_ui_input_t));
                    if (NULL == ui_input_p)
                    {
                        ret = DISP_RET_OUT_OF_MEMORY;
                        break;
                    }

                    /* UI expects the battery SoC to be an "int" */
                    ui_input_p->input_data_p = disp_malloc(DISP_HEAP_TYPE_TCM, sizeof(int));
                    if (NULL == ui_input_p->input_data_p)
                    {
                        disp_free(ui_input_p);
                        ret = DISP_RET_OUT_OF_MEMORY;
                        break;
                    }

                    /* Update battery data */
                    *((int *)ui_input_p->input_data_p) = (int) bat_val;
                    ui_input_p->type = DISP_UI_INPUT_TYPE_BAT;

                    /* Enqueue task with UI */
                    disp_ui_task_info_t ui_task_info = {
                        .task     = DISP_UI_TASK_PROCESS_INPUT,
                        .data_p   = ui_input_p,
                        .data_len = sizeof(disp_ui_input_t)
                    };

                    ret = disp_ui_queue_task(&ui_task_info, DISP_UI_TASK_PRIORITY_REG);
                    if (DISP_RET_SUCCESS != ret)
                    {
                        DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, 
                            "Battery: failed to enqueue UI task ret : %d", ret);
                        disp_free(ui_input_p->input_data_p);
                        disp_free(ui_input_p);
                    }
                }
                break;

                case CONTEXT_TYPE_APP_STATE:
                {
                    // Check len
                    if (0 == context_info_p->app_state.len)
                    {
                        ret = DISP_RET_INVALID_ARGUMENT;
                    }

                    ret = disp_ui_set_app_state(context_info_p->app_state.state_name);
                    if (DISP_RET_SUCCESS != ret)
                    {
                        DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, 
                            "Set app state to %s failed (rc=%d)", 
                            context_info_p->app_state.state_name, ret
                        );
                        break;
                    }

                    DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, 
                        "disp lpi set context: app state = %s", 
                        context_info_p->app_state.state_name
                    );
                }
                break;

                default:
                {
                    DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "Unhandled Context Type %d", ctx_type);
                }
                break;
            }
        }
        break;

        case DISP_INFO_TYPE_CAPABILITIES:
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_WARN_LVL, "disp lpi set capabilities");
        }
        break;

        default:
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_WARN_LVL, 
                "Unsupported set info type - %d", req_data_p->type);
            ret = DISP_RET_NOT_SUPPORTED_ERROR;
        }
        break;
    }

    rsp_event_p->data = &res_data;
    rsp_event_p->data_size = sizeof(res_data);
    if (DISP_RET_SUCCESS != ret)
    {
        DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, 
            "Set info cmd failed! type:%d (rc=%d)", req_data_p->type, ret);
    }
    return ret;
}

/** Process the various get info request types from HLOS */
disp_ret_t process_get_info_req(disp_event_t* event_p, disp_event_t *rsp_event_p)
{
    disp_ret_t ret = DISP_RET_SUCCESS;

    disp_info_t *req_data_p = (disp_info_t *) event_p->data;
    disp_info_t res_data = {
        .type = req_data_p->type
    };

    switch (req_data_p->type)
    {
        case DISP_INFO_TYPE_VERSION:
        {
            disp_version_info_t ver_info;
            memscpy(ver_info.disp_lpi_version, DISP_LPI_VER_INFO_SIZE, 
                disp_version_info, sizeof(disp_version_info));

            memscpy(&res_data.version, sizeof(res_data), &ver_info, sizeof(disp_version_info_t));
            DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "disp lpi get version : %s",
                                                                ver_info.disp_lpi_version);
        }
        break;

        case DISP_INFO_TYPE_CONTEXT:
        {
            disp_context_info_t *context_info_p = &res_data.context;
            context_info_p->context_type = req_data_p->context.context_type;
            disp_context_type_t ctx_type = context_info_p->context_type;
            if (CONTEXT_TYPE_MAX_CONTEXT <= ctx_type)
            {
                DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "Invalid context type:%d", ctx_type);
                ret = DISP_RET_FAILED;
                break;
            }

            switch (ctx_type)
            {
                case CONTEXT_TYPE_ACTIVE_TILE_INFO:
                {
                    disp_ui_settings_t settings_p;
                    ret = disp_ui_get_settings(&settings_p);
                    if (DISP_RET_SUCCESS != ret)
                    {
                        DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "disp_ui_get_settings : failed");
                        break;
                    }

                    context_info_p->active_tile.root_info_id = settings_p.curr_tile_id;
                    context_info_p->active_tile.root_list_id = settings_p.curr_view_id;

                    DISP_PRINTF(
                        MOD_DISP_LOG, DISP_INFO_LVL, 
                        "disp lpi get context - active tile : root id %d rootlist id %d", 
                        context_info_p->active_tile.root_info_id,
                        context_info_p->active_tile.root_list_id
                    );
                }
                break;

                case CONTEXT_TYPE_METRIC_INFO:
                {
                    DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "Get context: metric info");

                    /**
                    * Get active list of metric ID and value
                    * This will be used to synchronize sensors context on HLOS upon ambient exit
                    */
                    ret = disp_ui_get_latest_sns_cfg(&context_info_p->sns_cfg);
                    if (DISP_RET_SUCCESS != ret)
                    {
                        DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, 
                            "disp_ui_get_latest_sns_cfg : failed");
                        break;
                    }

                    DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "Current metric cnt: %d", context_info_p->sns_cfg.metric_id_len);
                }
                break;

                case CONTEXT_TYPE_APP_STATE:
                {
                    ret = disp_ui_get_app_state(
                        context_info_p->app_state.state_name, 
                        sizeof(context_info_p->app_state.state_name)
                    );
                    if (DISP_RET_SUCCESS != ret)
                    {
                        DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, 
                            "Get app state failed (rc=%d)", ret);
                        break;
                    }

                    context_info_p->app_state.len = strnlen(
                        context_info_p->app_state.state_name, 
                        sizeof(context_info_p->app_state.state_name)
                    );
                    DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, 
                        "disp lpi get context: app state = %s", 
                        context_info_p->app_state.state_name
                    );
                }
                break;

                default:
                {
                    DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL,
                        "Unsupported Get Context Type %d", ctx_type);
                    ret = DISP_RET_NOT_SUPPORTED_ERROR;
                }
                break;
            }
        }
        break;

        case DISP_INFO_TYPE_CAPABILITIES:
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_WARN_LVL, "DISP_INFO_TYPE_CAPABILITIES");
        }
        break;

        default:
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, 
                "Unsupported get info type - %d", req_data_p->type);
            ret = DISP_RET_NOT_SUPPORTED_ERROR;
        }
        break;
    }

    rsp_event_p->data = &res_data;
    rsp_event_p->data_size = sizeof(res_data);
    if (DISP_RET_SUCCESS != ret)
    {
        DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, 
            "Get info cmd failed! type:%d (rc=%d)", req_data_p->type, ret);
    }
    return ret;
}


/* AMB_EXIT post processing */
static disp_ret_t process_amb_post_exit(disp_event_t* event_p, disp_event_t *rsp_event_p)
{
    disp_ret_t ret = DISP_RET_SUCCESS;

    disp_panel_state_info_t *panel_info_p = (disp_panel_state_info_t *) event_p->data;
    disp_info_t res_data = {
        .type = DISP_INFO_TYPE_PANEL_STATE
    };

    DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "AOD = %d, Rx Panel State = %d, curr Panel State %d",
                                                         panel_info_p->always_on,
                                                         panel_info_p->power_state,
                                                         disp_get_current_panel_state());

    /**
     * This is to handle a case where AMB_EXIT was triggered in AOD_OFF when M55 was 
     * actively rendering and panel was turned ON.Post AMB_EXIT, HLOS sends this command
     * to ensure M55 clears its LDO vote as MSM display has taken over the panel.
     */
    if (0 == panel_info_p->always_on && DISP_PANEL_ON == disp_get_current_panel_state())
    {
        // turn off the LDO vote
        if (false == disp_dcp_drv_panel_power_vote(0))
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "Panel LDO vote to 0 failed");
            ret = DISP_RET_FAILED;
        }

    }
    rsp_event_p->data = &res_data;
    rsp_event_p->data_size = sizeof(res_data);
    return ret;
}



/**
 * @brief     This is the callback func registered with disp manager for offload events
 * @param[in] args_p pointer to args
 * @return    disp_ret_t
 */
static disp_ret_t disp_offload_handle_events(void *args_p)
{
    disp_event_t* event_p = (disp_event_t*)args_p;
    disp_event_t rsp_event = {0};
    disp_ret_t ret = DISP_RET_SUCCESS;
    if (NULL == event_p)
    {
        DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "%s: invalid argument", __func__);
        return DISP_RET_INVALID_ARGUMENT;
    }
    DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, "offload event %d, %d, %x", event_p->event_id,
                                                event_p->data_size, *(uint32_t*)event_p->data);

    update_debug_cmd_hist((disp_cmd_id_t) event_p->event_id);

    rsp_event.event_id = event_p->event_id;
    /**< update the context with current event recieved */
    disp_offload_ctx.cur_event = event_p->event_id;
    switch(event_p->event_id)
    {
        case DISP_CMD_OFFLOAD_RESET:
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "DISP_CMD_OFFLOAD_RESET");

            disp_ui_reset();

            /**<release the meta data from loaded address*/
            ret = disp_meta_release();
            if(DISP_RET_SUCCESS == ret)
            {
                /**<release the asset data from loaded address*/
                ret = disp_asset_release();
                if(DISP_RET_SUCCESS == ret)
                {
                    /** clear all the offload data and cfg ctx */
                    memset(&disp_offload_ctx.data, 0, sizeof(disp_offload_data_t));
                    memset(&disp_offload_ctx.cfg, 0, sizeof(disp_offload_cfg_t));
                }
            }
            disp_glink_apss_message_send_rsp(DISP_GLINK_APSS_CTRL_CH, &rsp_event, ret);
        }
        break;

        case DISP_CMD_STATE_RESET:
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, "DISP_CMD_STATE_RESET");
            /** nothing to do for now, add app state reset handling here */
            disp_glink_apss_message_send_rsp(DISP_GLINK_APSS_CTRL_CH, &rsp_event, ret);
        }
        break;

        case DISP_CMD_OFFLOAD_DATA:
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "DISP_CMD_OFFLOAD_DATA");
            disp_offload_data_t *disp_offload_data_p = (disp_offload_data_t *)(event_p->data);

            /** This is to make sure if APSS does not send RESET command before new offload */
            if(NULL != disp_offload_data_p)
            {
                disp_ui_reset();

                ret = disp_meta_release();
                if(DISP_RET_SUCCESS == ret)
                {
                    /**<release the asset data from loaded address*/
                    ret = disp_asset_release();
                    if(DISP_RET_SUCCESS == ret)
                    {
                        /** clear all the offload data and cfg ctx */
                        memset(&disp_offload_ctx.data, 0, sizeof(disp_offload_data_t));
                        memset(&disp_offload_ctx.cfg, 0, sizeof(disp_offload_cfg_t));
                    }
                }

                memscpy(&disp_offload_ctx.data, sizeof(disp_offload_data_t),
                                                disp_offload_data_p, sizeof(disp_offload_data_t));

                ret = disp_meta_store((uint8_t *)disp_offload_data_p->meta_mem_offset, disp_offload_data_p->meta_size);
                if(DISP_RET_SUCCESS == ret)
                {
                    ret = disp_asset_store((uint8_t *)disp_offload_data_p->asset_mem_offset, disp_offload_data_p->asset_size);
                    if(DISP_RET_SUCCESS == ret)
                    {
                        // TODO: Might be required to be moved under panel control.
                        disp_ui_task_info_t ui_task_info = {
                            .task     = DISP_UI_TASK_PARSE_METADATA,
                        };
                        disp_ui_queue_task(&ui_task_info, DISP_UI_TASK_PRIORITY_REG);
                    }
                }
            }
            else
            {
                ret = DISP_RET_INVALID_ARGUMENT;
            }

            disp_glink_apss_message_send_rsp(DISP_GLINK_APSS_CTRL_CH, &rsp_event, ret);
        }
        break;

        case DISP_CMD_OFFLOAD_CONFIG:
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "DISP_CMD_OFFLOAD_CONFIG");
            disp_offload_cfg_t *disp_offload_cfg_p = (disp_offload_cfg_t *)(event_p->data);
            memscpy(&disp_offload_ctx.cfg, sizeof(disp_offload_cfg_t),
                                                    disp_offload_cfg_p, sizeof(disp_offload_cfg_t));
            // TODO: Need to uncomment after verifying the settings to be in place
            //disp_panel_ctrl_aod_update(disp_offload_cfg_p->settings.always_on)

            bool is_ALS_enabled = disp_offload_ctx.cfg.als_cfg.is_als_enabled;
            bool is_AOD_enabled = disp_offload_ctx.cfg.settings.always_on;
            bool is_TTB_enabled = disp_offload_ctx.cfg.settings.tilt_to_bright;
            bool is_TTW_enabled = disp_offload_ctx.cfg.settings.tilt_to_wake;
            disp_wrist_orientation_t orientation = disp_offload_ctx.cfg.settings.orientation;          

            if((LEFT_WRIST_ROTATED_ORIENTATION == orientation) ||
                                                 (RIGHT_WRIST_ROTATED_ORIENTATION == orientation))
            {
                qapi_touch_svc_set_orientation(QAPI_TOUCH_ORIENT_180);
                disp_rm_set_layer_rotation(true);
            }
            else
            {
                qapi_touch_svc_set_orientation(QAPI_TOUCH_ORIENT_0);
                disp_rm_set_layer_rotation(false);
            }

            ret = disp_sns_als_set_brightness_ctx(&(disp_offload_ctx.cfg.als_cfg));
            if(DISP_RET_SUCCESS != ret)
            {
                DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "ALS set context failed: %d", ret);
            }
            else
            {
                /** @todo: Check if we can move this logic to AMBIENT_PREPARE */
                ret = disp_sns_tmr_create(is_ALS_enabled, DISP_SNS_TMR_ALS);
                if(DISP_RET_SUCCESS != ret)
                {
                    DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "ALS timer creation failed: %d", ret);
                }
            }

            ret = disp_sns_tmr_create(is_TTB_enabled, DISP_SNS_TMR_TTB);
            if(DISP_RET_SUCCESS != ret)
            {
                DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "TTB timer creation failed %d", ret);
            }

            disp_sns_timeout_update(disp_offload_ctx.cfg.settings.disp_timeout_ms);

            disp_glink_apss_message_send_rsp(DISP_GLINK_APSS_CTRL_CH, &rsp_event, ret);

            DISP_PRINTF(MOD_DISP_LOG, DISP_WARN_LVL, 
                        "ALS:%d, AOD:%d, TTB:%d, TTW:%d, Orientation:%d Panel State %d",
                        disp_offload_ctx.cfg.als_cfg.is_als_enabled,
                        disp_offload_ctx.cfg.settings.always_on,
                        disp_offload_ctx.cfg.settings.tilt_to_bright,
                        disp_offload_ctx.cfg.settings.tilt_to_wake,
                        disp_offload_ctx.cfg.settings.orientation,
                        disp_offload_ctx.cfg.offload_panel_state);
        }
        break;

        case DISP_CMD_AMBIENT_PREPARE:
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "DISP_CMD_AMBIENT_PREPARE");
            disp_panel_control_status_update(true);
            disp_panel_state_controller(DISP_PANEL_AMB_PREP);
            disp_rm_task_info_t rm_prep_task_info = {
                .task = DISP_RM_TASK_RENDER_PREPARE,
            };
            disp_rm_queue_task(&rm_prep_task_info, DISP_RM_TASK_PRIORITY_REG);

            // send request to battery soc monitor for registration 
            disp_battery_soc_monitor_start();

            // Register for all the metrics offloaded
            disp_sns_enable_metrics(true);

            disp_glink_apss_message_send_rsp(DISP_GLINK_APSS_CTRL_CH, &rsp_event, ret);
        }
        break;

        case DISP_CMD_AMBIENT_ENTER:
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "DISP_CMD_AMBIENT_ENTER");
            disp_offload_ctx.n_amb_enter = (disp_offload_ctx.n_amb_enter + 1) % UINT32_MAX;

            if(!disp_dcp_drv_start_offload())
            {
                DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "Driver Error in Start Offload!");
                ret = DISP_RET_DRIVER_ERROR;
            }

#ifdef CONFIG_QC_DISPLAY_CLI_ENABLE
            /**
             * if nemaVG test is enable, skip render start and initialize test infra
             * else, continue with the default flow
             */
            if (nema_vg_test_get_state())
            {
                nema_vg_test_init();
            }
            else
#endif
            {
                disp_dmgr_module_enable(DISP_MODULE_RSB, NULL);

                if (disp_offload_ctx.cfg.als_cfg.is_als_enabled)
                {
                    ret = disp_sns_register_phy_sensor(DISP_SNS_ALS, true);
                    if(DISP_RET_SUCCESS != ret)
                    {
                        DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "failed to register ALS sensor");
                    }
                }
                if (disp_offload_ctx.cfg.settings.tilt_to_bright)
                {
                    ret = disp_sns_register_phy_sensor(DISP_SNS_TILT, true);
                    if(DISP_RET_SUCCESS != ret)
                    {
                        DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "failed to register TILT sensor");
                    }
                }

                // Inform Q6 about Ambient entry (This is required for TTT feature)
                disp_ret_t rc = disp_glink_lpass_message_send_cmd(DISP_GLINK_LPASS_AUDIO_NOTIF_CH, 
                    DISP_VUI_EVENT_INFORM_AMB_ENTRY);
                if (DISP_RET_SUCCESS != rc)
                {
                    DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, 
                        "Failed to inform Q6 of ambient entry");
                }

                // TODO: Might be required to be moved under panel control.
                disp_panel_state_controller(DISP_PANEL_AMB_ENTRY);
                if (false == disp_offload_ctx.cfg.settings.always_on)
                {
                    DISP_PRINTF(MOD_DISP_LOG, DISP_WARN_LVL, "AOD OFF render will start on Tilt");
                }
            }

            disp_glink_apss_message_send_rsp(DISP_GLINK_APSS_CTRL_CH, &rsp_event, ret);
        }
        break;

        case DISP_CMD_AMBIENT_EXIT:
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "DISP_CMD_AMBIENT_EXIT");
            disp_offload_ctx.n_amb_exit = (disp_offload_ctx.n_amb_exit + 1) % UINT32_MAX;

            disp_panel_state_controller(DISP_PANEL_AMB_EXIT);

            // De-register for the metric here
            disp_sns_enable_metrics(false);

            if (disp_offload_ctx.cfg.als_cfg.is_als_enabled)
            {
                ret = disp_sns_register_phy_sensor(DISP_SNS_ALS, false);
                if(DISP_RET_SUCCESS != ret)
                {
                    DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "failed to deregister ALS sensor");
                }
                /** Stop the ALS timer */
                /** @note: As per Zephyr documentation: 
                 * Attempting to stop a timer that is not running is permitted, but has no effect on the timer.
                 */
                ret = disp_sns_als_timer_ctrl(false, 0, 0);
                if(DISP_RET_SUCCESS != ret)
                {
                    DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "failed to stop ALS timer");
                }
            }
            if (disp_offload_ctx.cfg.settings.tilt_to_bright)
            {
                ret = disp_sns_register_phy_sensor(DISP_SNS_TILT, false);
                if(DISP_RET_SUCCESS != ret)
                {
                    DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "failed to deregister TILT sensor");
                }
            }

            disp_dmgr_module_disable(DISP_MODULE_RSB, NULL);

            // Inform Q6 about Ambient entry (This is required for TTT feature)
            disp_ret_t rc = disp_glink_lpass_message_send_cmd(DISP_GLINK_LPASS_AUDIO_NOTIF_CH, 
                DISP_VUI_EVENT_INFORM_AMB_EXIT);
            if (DISP_RET_SUCCESS != rc)
            {
                DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, 
                    "Failed to inform Q6 of ambient exit");
            }

            // stop getting battery SoC events
            disp_battery_soc_monitor_stop();
            /**
             * In case of AOD_OFF scenario exit can be triggered when we are not actively rendering
             * In that case, it is not needed to wait for render stopped.
            */
            if (true == disp_rm_rendering_state())
            {
                disp_offload_ctx.is_amb_exit_pending = true;
                DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "AMBIENT_EXIT when rendering");
            }
            else
            {
                disp_panel_control_status_update(false);
                disp_dcp_drv_end_offload();

                rsp_event.event_id = DISP_CMD_AMBIENT_EXIT;
                rsp_event.data_size = sizeof(disp_info_t);
                
                disp_info_t resp_data = {
                    .type = DISP_INFO_TYPE_PANEL_STATE,
                    .panel_state = {
                        .always_on = disp_offload_ctx.cfg.settings.always_on,
                        .power_state = (DISP_PANEL_ON == disp_get_current_panel_state()?
                                       DISPLAY_PANEL_ACTIVE : DISPLAY_PANEL_OFF),
                            },
                };

                DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "always On %d state %d panel_state %d",
                                                             resp_data.panel_state.always_on,
                                                             resp_data.panel_state.power_state,
                                                             disp_get_current_panel_state());

                rsp_event.data = &resp_data;
                disp_glink_apss_message_send_rsp(DISP_GLINK_APSS_CTRL_CH, &rsp_event, ret);

                DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "DISP_CMD_AMBIENT_EXIT DONE");
            }
            
        }
        break;

        case DISP_CMD_RENDER_STOPPED:
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "DISP_CMD_RENDER_STOPPED");
            disp_panel_state_controller(DISP_PANEL_RENDER_STOPPED);

            if (true == disp_offload_ctx.is_amb_exit_pending)
            {
                disp_panel_control_status_update(false);
                disp_dcp_drv_end_offload();

                rsp_event.event_id = DISP_CMD_AMBIENT_EXIT;
                rsp_event.data_size = sizeof(disp_info_t);
                
                disp_info_t resp_data = {
                    .type = DISP_INFO_TYPE_PANEL_STATE,
                    .panel_state = {
                        .always_on = disp_offload_ctx.cfg.settings.always_on,
                        .power_state = (DISP_PANEL_ON == disp_get_current_panel_state()?
                                       DISPLAY_PANEL_ACTIVE : DISPLAY_PANEL_OFF),
                            },
                };
                DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "always On %d state %d panel_state %d", 
                                                            resp_data.panel_state.always_on,
                                                            resp_data.panel_state.power_state,
                                                            disp_get_current_panel_state());
                
                rsp_event.data = &resp_data;
                disp_glink_apss_message_send_rsp(DISP_GLINK_APSS_CTRL_CH, &rsp_event, ret);
                DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "DISP_CMD_AMBIENT_EXIT DONE");

                disp_offload_ctx.is_amb_exit_pending = false;

                // Reset UI to default
                disp_ui_reset_sns_data();

                // Clear offload sns config
                memset(&disp_offload_ctx.sns_cfg, 0, sizeof(sns_cfg_t));
            }
        }
        break;

        case DISP_CMD_NOTIFY_EVENT:
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "DISP_CMD_NOTIFY_EVENT");
            rsp_event.data_size = event_p->data_size;
            rsp_event.data = event_p->data;
            disp_glink_apss_message_send_cmd(DISP_GLINK_APSS_CTRL_CH, &rsp_event);
        }
        break;

        case DISP_CMD_SET_INFO:
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, "DISP_CMD_SET_INFO");
            ret = process_set_info_req(event_p, &rsp_event);
            disp_glink_apss_message_send_rsp(DISP_GLINK_APSS_CTRL_CH, &rsp_event, ret);
        }
        break;

        case DISP_CMD_GET_INFO:
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, "DISP_CMD_GET_INFO");
            ret = process_get_info_req(event_p, &rsp_event);
            disp_glink_apss_message_send_rsp(DISP_GLINK_APSS_CTRL_CH, &rsp_event, ret);
        }
        break;

        case DISP_CMD_AMBIENT_POST_EXIT:
        {
            /* TODO: This is just place holder currently, may or may not be present in final */
            DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "DISP_CMD_AMBIENT_POST_EXIT");
            disp_info_t *disp_info_p = (disp_info_t *)(event_p->data);
            ret = process_amb_post_exit(event_p, &rsp_event);
            disp_glink_apss_message_send_rsp(DISP_GLINK_APSS_CTRL_CH, &rsp_event, ret);

        }
        break;

        default:
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "offload event %d invalid",
                            event_p->event_id);
        }
        break;

    }
    /**< update the last event id, which is processed successfully */
    disp_offload_ctx.last_event = disp_offload_ctx.cur_event;
    DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, "offload event %d handling done", event_p->event_id);

    return DISP_RET_SUCCESS;
}

/**
 * @brief     This is the callback func registered with disp manager for module init
 * @param[in] args_p pointer to args
 * @return    disp_ret_t
 */
static disp_ret_t disp_offload_init(void *args_p)
{
    DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, "offload module init done");
    return DISP_RET_SUCCESS;
}

/**
 * @brief     This is the callback func registered with disp manager for module deinit
 * @param[in] args_p pointer to args
 * @return    disp_ret_t
 */
static disp_ret_t disp_offload_deinit(void *args_p)
{
    DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, "offload module de-init done");
    return DISP_RET_SUCCESS;
}

/*-------------------------------------------------------------------------------------------------
 * Public Function Definitions
 *-----------------------------------------------------------------------------------------------*/
// Global Function to hook the Module to Event Manager
void MODULE_HOOK(DISP_MODULE_OFFLOAD)
{
    // Initialize module descriptor
    *mod_desc_p = (disp_module_desc_t) {
        .name               = "disp_offload",
        .context_p          = (void*) &disp_offload_ctx,
        .init_fn_p          = disp_offload_init,
        .deinit_fn_p        = disp_offload_deinit,
        .enable_fn_p        = disp_offload_enable,
        .disable_fn_p       = disp_offload_disable,
        .ssr_cb_fn_p        = disp_offload_ssr,
        .handle_event_fn_p  = disp_offload_handle_events,
        .data_p             = (void*) NULL,
    };

    disp_module_list_g[DISP_MODULE_OFFLOAD] = mod_desc_p;
   DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "%s module hooking done", 
       mod_desc_p->name);
}

/*-------------------------------------------------------------------------------------------------
 * Public Function Definitions
 *-----------------------------------------------------------------------------------------------*/

/**
 * @brief     This function is for getting whether TTB config is active or inactive.
 * @param     None
 * @return    bool: true - if TTB is active; false - if TTB is inactive.
 */
bool disp_offload_is_ttb_enabled()
{
    return disp_offload_ctx.cfg.settings.tilt_to_bright;
}

/**
 * @brief     This function is for getting whether AOD is enabled or disabled.
 * @param     None
 * @return    bool: true - if AOD is enabled; false - if AOD is disabled.
 */
bool disp_offload_is_aod_enabled()
{
    return disp_offload_ctx.cfg.settings.always_on;
}

/**
 * @brief     This function returns the number of ambient mode entries.
 * @param     None
 * @return    uint32_t: count of ambient mode entries that have occurred.
 */
uint32_t disp_offload_no_of_amb_enter_get()
{
    return disp_offload_ctx.n_amb_enter;
}


/**
 * @brief     This function is for getting panel state
 * @param     None
 * @return    disp_offload_panel_state_t
 */
disp_offload_panel_state_t disp_get_offloaded_panel_state()
{
    return disp_offload_ctx.cfg.offload_panel_state;
}