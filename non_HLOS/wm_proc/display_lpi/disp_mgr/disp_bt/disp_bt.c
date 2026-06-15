/*=================================================================================================
 * @file disp_bt.c
 *
 * This file contains the implementation of BT events from and to Display SS
 *
 * Copyright (c) 2024-2025 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===============================================================================================*/

/*-------------------------------------------------------------------------------------------------
 * Include Files
 *-----------------------------------------------------------------------------------------------*/
#include <stringl.h>
#include "disp_bt.h"
#include "disp_mgr.h"
#include "disp_log.h"
#include "disp_error.h"
#include "disp_heap.h"
#include "disp_ui.h"

/*-------------------------------------------------------------------------------------------------
 * Structure & Enum Definitions
 *-----------------------------------------------------------------------------------------------*/

#if defined(CONFIG_QC_DISPLAY_DEBUG_ENABLE)
/** Holds some info pertaining to disp-bt event for debug purposes */
typedef struct {
    disp_bt_event_type_t type;              /**< type of disp-bt event */
    uint8_t              data_snippet[16];  /**< Some sample of the data contained in event */
    uint32_t             data_size;         /**< Size of actual data */
    disp_osa_time_t      timestamp;         /**< Timestamp of the event */
    disp_ret_t           status;
} disp_bt_evt_dbg_info_t;
#endif

typedef struct {
#if defined(CONFIG_QC_DISPLAY_DEBUG_ENABLE)
    disp_bt_evt_dbg_info_t last_processed_evt; /**< Info of Last processed event */
    disp_bt_evt_dbg_info_t curr_evt;           /**< Info of corrent received event (before processing) */
#endif 

    /* Module Descriptor */
    disp_module_desc_t module_desc; 
} disp_bt_ctx_t;

static disp_bt_ctx_t disp_bt_ctx;
static disp_module_desc_t * const mod_desc_p = &disp_bt_ctx.module_desc;

static disp_ret_t disp_bt_enable(void *args_p)
{
    DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, " Bt Module Enabled");
    return DISP_RET_SUCCESS;
}

static disp_ret_t disp_bt_disable(void *args_p)
{
    DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, " Bt Module Disable Done");
    return DISP_RET_SUCCESS;
}

static disp_ret_t disp_bt_ssr(void *args_p)
{
    DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, " Bt Module ssr Done");
    return DISP_RET_SUCCESS;
}

static disp_ret_t disp_bt_handle_events(void *args_p)
{
    DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, " Bt Module Handle Event");

    disp_event_t *event = (disp_event_t *)args_p;
    if (NULL == event || NULL == event->data || 0 == event->data_size)
    {
        return DISP_RET_INVALID_ARGUMENT;
    }

#if defined(CONFIG_QC_DISPLAY_DEBUG_ENABLE)
    disp_bt_ctx.curr_evt = (disp_bt_evt_dbg_info_t) {
        .type         = (disp_bt_event_type_t) event->event_id,
        .data_snippet = {0},
        .data_size    = event->data_size,
        .timestamp    = disp_osa_get_sys_curr_ticks()
    };
    
    // Not checking for data NULL as it is already checked for during routine entry
    memscpy(&disp_bt_ctx.curr_evt.data_snippet, sizeof(disp_bt_ctx.curr_evt.data_snippet),
        event->data, event->data_size);
#endif

    disp_ret_t ret = DISP_RET_SUCCESS;
    switch (event->event_id)
    {
        case DISP_EVENT_BT_RAW_NOTIFICATION:
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, 
                        "Rcvd BT Event: DISP_EVENT_BT_RAW_NOTIFICATION");
            
            // data_size + 1 to accomodate for null character
            uint32_t buf_size = event->data_size + 1;
            char *notif_data = (char *)disp_malloc(DISP_HEAP_TYPE_TCM, buf_size);
            if (NULL == notif_data)
            {
                DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, 
                    "Requested heap memory of %d bytes cannot be allocated.", buf_size);
                ret = DISP_RET_OUT_OF_MEMORY;
                break;
            }

            memscpy(notif_data, event->data_size, event->data, event->data_size);
            notif_data[event->data_size] = '\0';

            disp_ui_input_t *ui_input_p = disp_malloc(DISP_HEAP_TYPE_TCM, sizeof(disp_ui_input_t));
            if (NULL == ui_input_p)
            {
                DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, 
                            "No heap memory available for `disp_ui_input_t`");
                disp_free(notif_data);
                ret = DISP_RET_OUT_OF_MEMORY;
                break;
            }

            ui_input_p->type = DISP_UI_INPUT_TYPE_BT;
            ui_input_p->input_data_p = notif_data;
            
            disp_ui_task_info_t task_info = {
                .task     = DISP_UI_TASK_PROCESS_INPUT,
                .data_p   = ui_input_p,
                .data_len = event->data_size,
            };
            disp_ret_t rc = disp_ui_queue_task(&task_info, DISP_UI_TASK_PRIORITY_REG);
            if (DISP_RET_SUCCESS != rc)
            {
                DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "Failed to send BT data to UI:%d",rc);
                disp_free(ui_input_p->input_data_p);
                disp_free(ui_input_p);
                ret = DISP_RET_OVER_FLOW_ERROR;
                break;
            }
        }
        break;

        default:
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, 
                "Invalid disp-bt event type = %d", event->event_id);
            ret = DISP_RET_INVALID_ARGUMENT;
        }
        break;
    }

#if defined(CONFIG_QC_DISPLAY_DEBUG_ENABLE)
    disp_bt_ctx.curr_evt.status = ret;
    disp_bt_ctx.last_processed_evt = disp_bt_ctx.curr_evt;
#endif

    DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL,   "BT event_id=%d, data_size=%d", 
                                                event->event_id, 
                                                event->data_size);
    return ret;
}

static disp_ret_t disp_bt_init(void *args_p)
{
    DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, " Bt Module Init Done");
    return DISP_RET_SUCCESS;
}

static disp_ret_t disp_bt_deinit(void *args_p)
{
    DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, " Bt Module De-Init Done");
    return DISP_RET_SUCCESS;
}

/*-------------------------------------------------------------------------------------------------
 * Public Function Definitions
 *-----------------------------------------------------------------------------------------------*/
// Global Function to hook the Module to Event Manager
void MODULE_HOOK(DISP_MODULE_BT)
{
    // Initialize module descriptor
    *mod_desc_p = (disp_module_desc_t) {
        .name              = "disp_bt",
        .context_p         = (void *)&disp_bt_ctx,
        .init_fn_p         = disp_bt_init,
        .deinit_fn_p       = disp_bt_deinit,
        .enable_fn_p       = disp_bt_enable,
        .disable_fn_p      = disp_bt_disable,
        .ssr_cb_fn_p       = disp_bt_ssr,
        .handle_event_fn_p = disp_bt_handle_events,
        .data_p            = (void *)NULL,
    };

    disp_module_list_g[DISP_MODULE_BT] = mod_desc_p;
    DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "%s Module Hooking Done", mod_desc_p->name);
}
