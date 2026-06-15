/*=================================================================================================
 * @file disp_button.c
 *
 * This file contains the implementation of Button events from and to Display SS
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
#include "api_pm_button.h"
#include "pm_button.h"
#include "disp_glink_apss.h"
#include "disp_heap.h"
#include "disp_glink_util.h"
#include "disp_button.h"
#include "disp_ui.h"


/*-------------------------------------------------------------------------------------------------
 * Structure & Enum Definitions
 *-----------------------------------------------------------------------------------------------*/
typedef struct {
    // TODO: Add Context Variables 
    api_PM_Button_Handle_t disp_button_handle[DISP_BTN_ID_MAX];
    uint32_t           memory_out_range_ctr;
    /* Module Descriptor */
    disp_module_desc_t module_desc; 
} disp_button_ctx_t;

/*-------------------------------------------------------------------------------------------------
 * Static Data Declarations
 *-----------------------------------------------------------------------------------------------*/
static disp_button_ctx_t disp_button_ctx;
static disp_module_desc_t * const mod_desc_p = &disp_button_ctx.module_desc;

/*-------------------------------------------------------------------------------------------------
 * Forward function Declarations
 *-----------------------------------------------------------------------------------------------*/

disp_ret_t disp_button_notify_to_apps(uint32_t button_id);
void disp_button_cb(api_PM_Button_Instance_t    btn_instance,
                    api_PM_Button_Event_t       event_type,
                    uint64_t                    evt_timestamp_ms);

/*-------------------------------------------------------------------------------------------------
 * Local Function Definitions
 *-----------------------------------------------------------------------------------------------*/
static disp_ret_t disp_button_enable(void *args_p)
{
    DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, " Button Module Enabled");
    return DISP_RET_SUCCESS;
}

static disp_ret_t disp_button_disable(void *args_p)
{
    DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, " Button Module Disable Done");
    return DISP_RET_SUCCESS;
}

static disp_ret_t disp_button_ssr(void *args_p)
{
    DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, " Button Module ssr Done");
    return DISP_RET_SUCCESS;
}

static disp_ret_t disp_button_handle_events(void *args_p)
{
    DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, "Button Module Handle Event");
    disp_event_t* event_p = (disp_event_t*)args_p;
    if (NULL == event_p || NULL == event_p->data)
    {
        return DISP_RET_INVALID_ARGUMENT;
    }

    DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL,   "Button Event %d, %d, ID %d Done", 
                                                event_p->event_id, 
                                                event_p->data_size, 
                                                *(disp_button_idx_t*)event_p->data);

    if ( DISP_BUTTON_PRESS == event_p->event_id)
    {
        disp_button_idx_t button_id = *((disp_button_idx_t*)event_p->data);

        /* For Power button press, send notification to apps to wakeup */
        if (DISP_BTN_ID_POWER == button_id || DISP_BTN_ID_VOL_DOWN == button_id || DISP_BTN_ID_VOL_UP == button_id || DISP_BTN_ID_VOL_LEFT == button_id)
        {
            disp_ret_t ret = disp_button_notify_to_apps(button_id);
            if (DISP_RET_SUCCESS != ret)
            {
                DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL,
                    "Button: rsb btn press notify failed %d", ret);
            }
            else
            {
                DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL,
                    "Button: rsb btn press notify Success");
            }
            return ret;
        }        //else if (DISP_BTN_ID_VOL_UP == button_id)
        else
        {
            /* For up/down button press, send event to UI for changing view */
            disp_ui_input_t *ui_input_p = disp_malloc(DISP_HEAP_TYPE_TCM, sizeof(disp_ui_input_t));
            if (NULL == ui_input_p)
            {
                disp_button_ctx.memory_out_range_ctr++;
                DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "malloc failed for %d", 
                                                                    sizeof(disp_ui_input_t));
                return DISP_RET_OUT_OF_MEMORY;
            }
            ui_input_p->type = DISP_UI_INPUT_TYPE_BTN;
            ui_input_p->input_data_p = disp_malloc(DISP_HEAP_TYPE_TCM, sizeof(disp_button_idx_t));
            if (NULL == ui_input_p->input_data_p)
            {
                disp_button_ctx.memory_out_range_ctr++;
                DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "malloc failed for %d", 
                                                                    sizeof(disp_button_idx_t));
                disp_free(ui_input_p);
                return DISP_RET_OUT_OF_MEMORY;
            }

            memscpy(ui_input_p->input_data_p, sizeof(disp_button_idx_t),
                    &button_id, sizeof(disp_button_idx_t));

            disp_ui_task_info_t task_info = {0};
            task_info.task     = DISP_UI_TASK_PROCESS_INPUT;
            task_info.data_p   = ui_input_p;
            task_info.data_len = sizeof(disp_ui_input_t);

            disp_ret_t rc = disp_ui_queue_task(&task_info, DISP_UI_TASK_PRIORITY_REG);
            if (DISP_RET_SUCCESS != rc)
            {
                disp_free(ui_input_p->input_data_p);
                disp_free(ui_input_p);
                return rc;
            }
        }
        /*else
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "Unexpected button ID %d", button_id);
        }*/
    }
    else
    {
        DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "Unexpected button event %d", event_p->event_id);
    }

    return DISP_RET_SUCCESS;
}

static disp_ret_t disp_button_init(void *args_p)
{
    disp_ret_t ret = DISP_RET_SUCCESS;
    /* The registration of button ID is tightly coupled with api_PM_Button_Instance_t values */
    for (disp_button_idx_t i = DISP_BTN_ID_POWER; i < DISP_BTN_ID_MAX; i++)
    {
        api_Status_t pm_ret = api_PM_Button_Register(&(disp_button_ctx.disp_button_handle[i]), 
                                                    i, disp_button_cb, PMIC_BUTTON_PRESS);
        if(API_OK== pm_ret)
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "buttons %d reg successfully !",i);
        }
        else
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "buttons %d reg failed ret %d !",i, pm_ret);
            ret = DISP_RET_FAILED;
        }
    }

    DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, " Button Module Init Done");
    return ret;
}

static disp_ret_t disp_button_deinit(void *args_p)
{
    disp_ret_t ret = DISP_RET_SUCCESS;
    for (int i = 0; i < DISP_BTN_ID_MAX; i++)
    {
        api_Status_t pm_ret = api_PM_Button_Unregister(disp_button_ctx.disp_button_handle[i]);
        if(API_OK== pm_ret)
        {
          DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "buttons %d unreg successfully !",i);
        }
        else
        {
            DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "buttons %d unreg failed ret %d !",i, pm_ret);
            ret = DISP_RET_FAILED;
        }
    }
    DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, " Button Module De-Init Done");
    return ret;
}


/*-------------------------------------------------------------------------------------------------
 * Public Function Definitions
 *-----------------------------------------------------------------------------------------------*/
// Global Function to hook the Module to Event Manager
void MODULE_HOOK(DISP_MODULE_BUTTON)
{
    // Initialize module descriptor
    *mod_desc_p = (disp_module_desc_t) {
        .name               = "disp_button",
        .context_p          = (void*) &disp_button_ctx,
        .init_fn_p          = disp_button_init,
        .deinit_fn_p        = disp_button_deinit,
        .enable_fn_p        = disp_button_enable,
        .disable_fn_p       = disp_button_disable,
        .ssr_cb_fn_p        = disp_button_ssr,
        .handle_event_fn_p  = disp_button_handle_events,
        .data_p             = (void*) NULL,
    };

    disp_module_list_g[DISP_MODULE_BUTTON] = mod_desc_p;
    DISP_PRINTF(MOD_DISP_LOG, DISP_INFO_LVL, "%s Module Hooking Done", mod_desc_p->name);
}


/**
 * @brief Button callback to be registered with platform
 *
 * @param[in] btn_instance  Button instance
 * @param[in] event_type    Type of button event
 * @param[in] evt_timestamp_ms timestamp
 *
 * @return None
 */

void disp_button_cb(api_PM_Button_Instance_t    btn_instance,
                    api_PM_Button_Event_t       event_type,
                    uint64_t                    evt_timestamp_ms)
{
    DISP_PRINTF(MOD_DISP_LOG, DISP_DEBUG_LVL, " Button Event Received %d ts:%llu",
                                                 btn_instance, evt_timestamp_ms);
    disp_event_t event = {0};
    disp_ret_t ret = DISP_RET_SUCCESS;
    event.event_id = DISP_BUTTON_PRESS;
    event.data_size = sizeof(api_PM_Button_Instance_t);
    event.data = disp_malloc(DISP_HEAP_TYPE_TCM, event.data_size);
    if (NULL == event.data)
    {
        disp_button_ctx.memory_out_range_ctr++;
        DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "malloc failed for %d", event.data_size);
        return;
    }
    memscpy(event.data, event.data_size, &btn_instance, sizeof(api_PM_Button_Instance_t));
    ret = disp_dmgr_send_event(DISP_MODULE_BUTTON, event);

    if (DISP_RET_SUCCESS != ret)
    {
        DISP_PRINTF(MOD_DISP_LOG, DISP_ERROR_LVL, "Button press event send failed ret=%d", ret);
        disp_free(event.data);
    }
}


/**
 * @brief Send power button press event to apps
 *
 * @param[in] button_id This is placeholder to exchange any data, currently button ID
 *                      is shared just for demonstration.
 *
 * @return disp_ret_t  Display return code
 */
disp_ret_t disp_button_notify_to_apps(uint32_t button_id)
{
    printk("BTN: disp_button_notify_to_apps: %x", button_id);
    disp_event_t event = {0};
    event.event_id = DISP_UTIL_CMD_PWR_BUTTON_PRESS;
    event.data_size = sizeof(uint32_t);
    event.data = (void *)&button_id;
    return disp_glink_apss_message_send_cmd(DISP_GLINK_APSS_UTIL_CH, &event);
}
