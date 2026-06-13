/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
// $QTI_LICENSE_C$

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <stdlib.h>
#include <stringl.h>
#include "offload_mgr_priv.h"
#include "pdtsw_glink.h"
#include "pdtsw_worker_thread.h"
#include "pdtsw_platform.h"

#define OFFLOAD_MGR_ISLAND_MODE_CLIENT       "offload_mgr_island_mode_client"

#define PDTSW_FATAL(...)    while(1)

#if defined(CONFIG_PDTSW_OFFLOAD_MGR_AUDIO_ENABLE) || \
defined(CONFIG_PDTSW_OFFLOAD_MGR_HAPTICS_ENABLE) || \
defined(CONFIG_PDTSW_COMMON_UTILS_ENABLE)
extern offload_mgr_srv_client_intf_t utility_hal_offload_ctx;
#endif
#ifdef CONFIG_PDTSW_OFFLOAD_MGR_RSB_ENABLE
extern offload_mgr_srv_client_intf_t rsb_offload_cxt;
#endif

/* NPA client handle for offload mgr to vote for island mode entry and exit */
npa_client_handle offload_mgr_island_mode_handle;

/* Define the stack for RSB workqueue */
K_THREAD_STACK_DEFINE(offload_mgr_workq_stack, CONFIG_QC_PDTSW_OFFLOAD_MGR_THREAD_STACK_SIZE);

/**
 * @brief Pointer to offload manager workq
 */
static pdtsw_workq_ctx_t *p_offload_mgr_workq_ctx = NULL;

/* Variable to hold the context information.*/
offload_mgr_cxt_t offload_mgr_cxt;

/* Internal structures to send the data across the contexts*/

typedef struct 
{
    offload_mgr_evt_t evt;
    glink_link_state_type status;
}offload_mgr_link_sts_data_t;

typedef struct
{
    offload_mgr_evt_t evt;
    offload_mgr_srv_client_t client;
    glink_handle_type handle;
    glink_channel_event_type status;
}offload_mgr_chnl_sts_data_t;

typedef struct
{
    offload_mgr_evt_t evt;
    offload_mgr_srv_client_t client;
    glink_handle_type handle;
    const void *pkt_priv;
    const void *ptr;
    size_t size;
}offload_mgr_tx_done_t;

typedef struct 
{
    offload_mgr_evt_t evt;
    offload_mgr_srv_client_t client;
    size_t size;
}offload_mgr_rx_int_req_t;

typedef struct
{
    offload_mgr_evt_t evt;
    offload_mgr_srv_client_t client;
    const void *pkt_priv;
    const void *ptr;
    size_t size;
    size_t intent_used;
}offload_mgr_rx_t;

typedef struct 
{
    offload_mgr_evt_t evt;
    offload_mgr_srv_client_t client;
    size_t client_data_size;
}offload_mgr_client_work_t;

/* Forward declartions for utility functions.*/
static void *offload_work_init(size_t args_size);

static int offload_work_submit(void* args);

/* Glink callback handlers for offload manager*/

/* Apps link state event callback.*/
static void offload_mgr_link_state_cb(glink_link_info_type *link_info,void* priv)
{
    int api_retval;

    offload_mgr_link_sts_data_t *args = (offload_mgr_link_sts_data_t*)offload_work_init(
                                sizeof(offload_mgr_link_sts_data_t));
    if(NULL == args)
    {
        PDTSW_FATAL();
    }

    args->evt = OFFLOAD_MGR_EVT_GLINK_APP_LINK;
    args->status = (size_t)link_info->link_state;
    
    api_retval = offload_work_submit(args);

    if(0 != api_retval)
    {
        PDTSW_FATAL();
    }
}

/* Channel state event callback.*/
static void offload_mgr_chnl_state_cb(glink_handle_type handle, const void *priv,
    glink_channel_event_type event)
{
    int api_retval;

    offload_mgr_chnl_sts_data_t *args = (offload_mgr_chnl_sts_data_t*)
                                            offload_work_init(
                                            sizeof(offload_mgr_chnl_sts_data_t));
    if(NULL == args)
    {
        PDTSW_FATAL();
    }

    args->evt = OFFLOAD_MGR_EVT_GLINK_CHNL;
    args->client = (offload_mgr_srv_client_t)priv;
    args->handle = handle;
    args->status = event;
    
    api_retval = offload_work_submit(args);

    if(0 != api_retval)
    {
        PDTSW_FATAL();
    }
}

/* Tx done callback.*/
static void offload_mgr_tx_done_cb(glink_handle_type handle, const void *priv,
    const void *pkt_priv, const void *ptr, size_t size)
{
    int api_retval;

    offload_mgr_tx_done_t *args = (offload_mgr_tx_done_t*)
                                    offload_work_init(
                                    sizeof(offload_mgr_tx_done_t));
    if(NULL == args)
    {
        PDTSW_FATAL();
    }

    args->evt = OFFLOAD_MGR_EVT_GLINK_TX_DONE;
    args->client = (offload_mgr_srv_client_t)priv;
    args->handle = handle;
    args->pkt_priv = pkt_priv;
    args->ptr = ptr;
    args->size = size;
    
    api_retval = offload_work_submit(args);

    if(0 != api_retval)
    {
        PDTSW_FATAL();
    }
}

/* Intent request callback.*/
static bool offload_mgr_rx_intent_req_cb(glink_handle_type handle, const void *priv,
    size_t req_size)
{
    int api_retval;

    if(true == offload_mgr_cxt.client_info[(offload_mgr_srv_client_t)priv]->
                                is_dynamic_intent_supported)
        {
        offload_mgr_rx_int_req_t *args = (offload_mgr_rx_int_req_t*)
                                            offload_work_init(
                                            sizeof(offload_mgr_rx_int_req_t));
        if(NULL == args)
        {
            PDTSW_FATAL();
        }

        args->evt = OFFLOAD_MGR_EVT_GLINK_RX_INT_REQ;
        args->client = (offload_mgr_srv_client_t)priv;
        args->size = req_size;
        
        api_retval = offload_work_submit(args);

        if(0 != api_retval)
        {
            PDTSW_FATAL();
        }
    }

    return offload_mgr_cxt.client_info[(offload_mgr_srv_client_t)priv]->
                                is_dynamic_intent_supported;
}

/* Rx callback.*/
static void offload_mgr_rx_cb(glink_handle_type handle, const void *priv,
                        const void *pkt_priv, const void *ptr,
                        size_t size, size_t intent_used)
{
    int api_retval;

    offload_mgr_rx_t *args = (offload_mgr_rx_t*)
                                    offload_work_init(
                                    sizeof(offload_mgr_rx_t));
    if(NULL == args)
    {
        PDTSW_FATAL();
    }

    args->evt = OFFLOAD_MGR_EVT_GLINK_RX;
    args->client = (offload_mgr_srv_client_t)priv;
    args->pkt_priv = pkt_priv;
    args->ptr = ptr;
    args->size = size;
    args->intent_used = intent_used;
    
    api_retval = offload_work_submit(args);

    if(0 != api_retval)
    {
        PDTSW_FATAL();
    }
}

glink_err_type offload_mgr_allocate_cb(glink_handle_type handle, const void *priv,
                            const void *pkt_priv, size_t intent_size, void **buffer_ptr)
{
    glink_err_type retval = GLINK_STATUS_SUCCESS;

    if(NULL != offload_mgr_cxt.client_info[(offload_mgr_srv_client_t)priv]->on_allocate_cb)
    {
        retval = offload_mgr_cxt.client_info[(offload_mgr_srv_client_t)priv]->on_allocate_cb(
                                        pkt_priv, intent_size, buffer_ptr);
    }
    else
    {
       *buffer_ptr = pdtsw_heap_alloc(PDTSW_COMMON_GLINK_HEAP, intent_size);

        if(NULL == *buffer_ptr)
        {
            retval = GLINK_STATUS_OUT_OF_RESOURCES;
        }
    }

    return retval;
}

void offload_mgr_deallocate_cb(glink_handle_type handle, const void *priv,
                            const void *pkt_priv, size_t intent_size, void *buffer_ptr)
{
    if(NULL != offload_mgr_cxt.client_info[(offload_mgr_srv_client_t)priv]->on_deallocate_cb)
    {
        offload_mgr_cxt.client_info[(offload_mgr_srv_client_t)priv]->on_deallocate_cb(
                                    pkt_priv, intent_size, buffer_ptr);
    }
    else
    {
        pdtsw_heap_free(PDTSW_COMMON_GLINK_HEAP, buffer_ptr);

    }
}

/* Glink event handlers for offload manager*/

/* Apps link state event callback.*/
static void offload_mgr_app_link_state_handler(offload_mgr_msg_hdr_t *msg)
{
    uint8_t iter;
    glink_link_state_type status = ((offload_mgr_link_sts_data_t*)(msg))->status;

    if(offload_mgr_cxt.apps_glink_link_state != status)
    {
        offload_mgr_cxt.apps_glink_link_state = status;

        /* Open the channels for the clients.*/
        for(iter = 0; iter < OFF_MGR_SRV_MAX; iter++)
        {
            if(NULL != offload_mgr_cxt.client_info[iter]->on_link_evt)
            {
                offload_mgr_cxt.client_info[iter]->on_link_evt(status);
            }
        }
    }
}

/* Channel state event callback.*/
static void offload_mgr_chnl_state_handler(offload_mgr_msg_hdr_t *msg)
{
    offload_mgr_chnl_sts_data_t *args = (offload_mgr_chnl_sts_data_t*)msg;

    if(OFF_MGR_SRV_MAX <= args->client)
    {
        /* Todo: log error and continue */
    }
    else
    {
        if(NULL != offload_mgr_cxt.client_info[args->client]->on_channel_evt)
        {
            offload_mgr_cxt.client_info[args->client]->on_channel_evt(args->status);
        }
    }
}

/* Tx done callback.*/
static void offload_mgr_tx_done_handler(offload_mgr_msg_hdr_t *msg)
{
    offload_mgr_tx_done_t *args = (offload_mgr_tx_done_t*)msg;

    if(OFF_MGR_SRV_MAX <= args->client)
    {
        /* Todo: log error and continue */
    }
    else
    {
        if(NULL != offload_mgr_cxt.client_info[args->client]->on_tx_done_evt)
        {
            offload_mgr_cxt.client_info[args->client]->on_tx_done_evt(
                                        args->pkt_priv, args->ptr, args->size);
        }
    }
}

/* Intent request callback.*/
static void offload_mgr_rx_intent_req_handler(offload_mgr_msg_hdr_t *msg)
{
    offload_mgr_rx_int_req_t *args = (offload_mgr_rx_int_req_t*)msg;

    if(OFF_MGR_SRV_MAX <= args->client)
    {
        /* Todo: log error and continue */
    }
    else
    {
        if(NULL != offload_mgr_cxt.client_info[args->client]->on_intent_req)
        {
            offload_mgr_cxt.client_info[args->client]->on_intent_req(args->size);
        }
    }
}

/* Rx callback.*/
static void offload_mgr_rx_handler(offload_mgr_msg_hdr_t *msg)
{
    offload_mgr_rx_t *args = (offload_mgr_rx_t*)msg;

    if(OFF_MGR_SRV_MAX <= args->client)
    {
        /* Todo: log error and continue */
    }
    else
    {
        if(NULL != offload_mgr_cxt.client_info[args->client]->on_rx_evt)
        {
            offload_mgr_cxt.client_info[args->client]->on_rx_evt(
                                        args->pkt_priv, args->ptr, args->size);
        }
    }
}

static void offload_mgr_client_evt_handler(offload_mgr_msg_hdr_t *msg)
{
    offload_mgr_client_work_t *args = (offload_mgr_client_work_t*)msg;

    if(OFF_MGR_SRV_MAX <= args->client)
    {
        /* Todo: log error and continue */
    }
    else
    {
        if(NULL != offload_mgr_cxt.client_info[args->client]->on_rx_evt)
        {
            /*Todo: use container of*/
            offload_mgr_cxt.client_info[args->client]->event_hndlr(
                                    sizeof(offload_mgr_client_work_t)+((uint8_t*)args),
                                    args->client_data_size);
        }
    }
}

/* Glink APIs for clients*/

glink_err_type offload_mgr_glink_open(offload_mgr_srv_client_t client)
{
    glink_err_type retval = GLINK_STATUS_SUCCESS;
    glink_open_config_type open_cfg;

    if((OFF_MGR_SRV_MAX <= client) ||
        (NULL == offload_mgr_cxt.client_info[client]->channel_name)||
        (NULL == offload_mgr_cxt.client_info[client]->on_channel_evt) ||
        (GLINK_CONNECTED == offload_mgr_cxt.client_info[client]->chnl_state))
    {
        retval = GLINK_STATUS_INVALID_PARAM;
    }
    else
    {
        GLINK_OPEN_CONFIG_INIT(open_cfg);
        open_cfg.name = offload_mgr_cxt.client_info[client]->channel_name;
        open_cfg.remote_ss = OFFLOAD_MGR_APPS_REMOTE_SS;
        open_cfg.transport = OFFLOAD_MGR_APPS_XPORT;
        open_cfg.notify_allocate = offload_mgr_allocate_cb;
        open_cfg.notify_deallocate = offload_mgr_deallocate_cb;
        open_cfg.notify_rx = offload_mgr_rx_cb;
        open_cfg.notify_rx_intent_req = offload_mgr_rx_intent_req_cb;
        open_cfg.notify_state = offload_mgr_chnl_state_cb;;
        open_cfg.notify_tx_done = offload_mgr_tx_done_cb;
        open_cfg.options = GLINK_OPT_CLIENT_BUFFER_ALLOCATION;
        open_cfg.priv = (void*)client;

        /* trigger island mode exit (blocking call)*/
        pdtsw_island_vote_island_exit(offload_mgr_island_mode_handle);

        retval = pdtsw_glink_open(&open_cfg, 
                        &(offload_mgr_cxt.client_info[client]->chnl_hndl));

        /* trigger island mode entry (non-blocking call)*/
        pdtsw_island_vote_island_entry(offload_mgr_island_mode_handle);
    }

    return retval;
}

glink_err_type offload_mgr_glink_close(offload_mgr_srv_client_t client)
{
    glink_err_type retval = GLINK_STATUS_SUCCESS;

    if((OFF_MGR_SRV_MAX <= client) ||
        (GLINK_LOCAL_DISCONNECTED == offload_mgr_cxt.client_info[client]->chnl_state))
    {
        retval = GLINK_STATUS_INVALID_PARAM;
    }
    else
    {
        /* trigger island mode exit (blocking call)*/
        pdtsw_island_vote_island_exit(offload_mgr_island_mode_handle);

        retval = pdtsw_glink_close(offload_mgr_cxt.client_info[client]->chnl_hndl);
        offload_mgr_cxt.client_info[client]->chnl_hndl = NULL;

        /* trigger island mode entry (non-blocking call)*/
        pdtsw_island_vote_island_entry(offload_mgr_island_mode_handle);
    }

    return retval;
}

glink_err_type offload_mgr_glink_send(offload_mgr_srv_client_t client, const void *pPkt,
        const void *pkt_priv, size_t size, uint32_t options)
{
    glink_err_type retval = GLINK_STATUS_SUCCESS;

    if((OFF_MGR_SRV_MAX <= client) ||
        (GLINK_CONNECTED != offload_mgr_cxt.client_info[client]->chnl_state))
    {
        retval = GLINK_STATUS_INVALID_PARAM;
    }
    else
    {
        /* trigger island mode exit (blocking call)*/
        pdtsw_island_vote_island_exit(offload_mgr_island_mode_handle);

        retval = pdtsw_glink_tx(offload_mgr_cxt.client_info[client]->chnl_hndl,
                                    pkt_priv, pPkt, size, options);

        /* trigger island mode entry (non-blocking call) */
        pdtsw_island_vote_island_entry(offload_mgr_island_mode_handle);
    }

    return retval;
}

glink_err_type offload_mgr_glink_queue_intent(offload_mgr_srv_client_t client,
        void* pkt_priv, size_t size)
{
    glink_err_type retval = GLINK_STATUS_SUCCESS;

    if((OFF_MGR_SRV_MAX <= client) ||
        (GLINK_CONNECTED != offload_mgr_cxt.client_info[client]->chnl_state))
    {
        retval = GLINK_STATUS_INVALID_PARAM;
    }
    else
    {
        /* trigger island mode exit (blocking call)*/
        pdtsw_island_vote_island_exit(offload_mgr_island_mode_handle);

        retval = pdtsw_glink_queue_rx_intent(
                    offload_mgr_cxt.client_info[client]->chnl_hndl, pkt_priv, size);

        /* trigger island mode entry (non-blocking call)*/
        pdtsw_island_vote_island_entry(offload_mgr_island_mode_handle);
    }

    return retval;
}
        
glink_err_type offload_mgr_glink_rx_done(offload_mgr_srv_client_t client,
        void* buffer, bool reuse)
{
    glink_err_type retval = GLINK_STATUS_SUCCESS;

    if((OFF_MGR_SRV_MAX <= client) ||
        (GLINK_CONNECTED != offload_mgr_cxt.client_info[client]->chnl_state))
    {
        retval = GLINK_STATUS_INVALID_PARAM;
    }
    else
    {
        /* trigger island mode exit (blocking call)*/
        pdtsw_island_vote_island_exit(offload_mgr_island_mode_handle);

        retval = pdtsw_glink_rx_done(
                    offload_mgr_cxt.client_info[client]->chnl_hndl, buffer, reuse);

        /* trigger island mode entry (non-blocking call)*/
        pdtsw_island_vote_island_entry(offload_mgr_island_mode_handle);
    }

    return retval;
}

int offload_mgr_init(void)
{
    int iter;
    glink_err_type glink_err;

    offload_mgr_island_mode_handle = pdtsw_island_create_client(OFFLOAD_MGR_ISLAND_MODE_CLIENT);
    if (offload_mgr_island_mode_handle == NULL)
    {
        PDTSW_FATAL();
    }

    const struct k_work_queue_config cfg = {
        .name = "Offload Mgr Workq",
        .no_yield = false,
        .essential = false
    };

    /* create a custom workq for offload manager */
    p_offload_mgr_workq_ctx = pdtsw_custom_workq_init(PDTSW_COMMON_HEAP, offload_mgr_workq_stack,
        K_THREAD_STACK_SIZEOF(offload_mgr_workq_stack), CONFIG_QC_PDTSW_OFFLOAD_MGR_THREAD_PRIORITY, &cfg);
    if(p_offload_mgr_workq_ctx == NULL)
    {
        PDTSW_FATAL();
    }

    /* Initialize the context.*/
    memset(&offload_mgr_cxt, 0, sizeof(offload_mgr_cxt_t));
    offload_mgr_cxt.apps_glink_link_state = GLINK_LINK_STATE_DOWN;

    /* Todo: Tag all the clients initialization for offload here*/
#ifdef CONFIG_PDTSW_OFFLOAD_MGR_RSB_ENABLE
    offload_mgr_cxt.client_info[OFF_MGR_SRV_RSB_SERVICE] = &rsb_offload_cxt;
#endif
#if defined(CONFIG_PDTSW_OFFLOAD_MGR_AUDIO_ENABLE) || \
defined(CONFIG_PDTSW_OFFLOAD_MGR_HAPTICS_ENABLE) || \
defined(CONFIG_PDTSW_COMMON_UTILS_ENABLE)
    offload_mgr_cxt.client_info[OFF_MGR_SRV_UTILITY_HAL] = &utility_hal_offload_ctx;
#endif

    /* Initialize the clients.*/
    for(iter = 0; iter < OFF_MGR_SRV_MAX; iter++)
    {
        if(NULL != offload_mgr_cxt.client_info[iter]->on_offload_mgr_start)
        {
            /* Indicate the start for clients to perform any cpecific initializations.*/
            offload_mgr_cxt.client_info[iter]->client_status = OFF_MGR_SRV_STATUS_INIT;
            offload_mgr_cxt.client_info[iter]->on_offload_mgr_start();
        }
    }

    /* Register for Glink link events.*/
    glink_link_id_type link_info;
    GLINK_LINK_ID_STRUCT_INIT(link_info);
    link_info.remote_ss = OFFLOAD_MGR_APPS_REMOTE_SS;
    link_info.xport = OFFLOAD_MGR_APPS_XPORT;
    link_info.link_notifier = offload_mgr_link_state_cb;
    glink_err = glink_register_link_state_cb(&link_info, NULL);

    if(GLINK_STATUS_SUCCESS != glink_err)
    {
        PDTSW_FATAL();
    }
    offload_mgr_cxt.apps_glink_link_hndl = link_info.handle;
    return 0;
}

/* Offload manager hanlder*/
static void offload_mgr_task(pdtsw_work_item_t *item)
{
    offload_mgr_msg_hdr_t *msg = (offload_mgr_msg_hdr_t*)pdtsw_get_workq_handler_args(item);

    switch (msg->evt)
    {
        case OFFLOAD_MGR_EVT_GLINK_APP_LINK:
        {
            offload_mgr_app_link_state_handler(msg);
            break;
        }
        case OFFLOAD_MGR_EVT_GLINK_CHNL:
        {
            offload_mgr_chnl_state_handler(msg);
            break;
        }
        case OFFLOAD_MGR_EVT_GLINK_TX_DONE:
        {
            offload_mgr_tx_done_handler(msg);
            break;
        }
        case OFFLOAD_MGR_EVT_GLINK_RX_INT_REQ:
        {
            offload_mgr_rx_intent_req_handler(msg);
            break;
        }
        case OFFLOAD_MGR_EVT_GLINK_RX:
        {
            offload_mgr_rx_handler(msg);
            break;
        }
        case OFFLOAD_MGR_EVT_CLIENT_CUSTOM:
        {
            offload_mgr_client_evt_handler(msg);
            break;
        }
        default:
            break;
    }

    pdtsw_free_handler_args(item, p_offload_mgr_workq_ctx);
}

void *offload_work_init(size_t args_size)
{
    return pdtsw_allocate_handler_args(args_size, p_offload_mgr_workq_ctx);
}

int offload_work_submit(void* args)
{
    return pdtsw_submit_to_workq(offload_mgr_task, args, p_offload_mgr_workq_ctx);
}

void* offload_mgr_client_work_init(size_t arg_size)
{
    if(0 == arg_size)
    {
        return NULL;
    }

    void *retval = pdtsw_allocate_handler_args(
                    sizeof(offload_mgr_client_work_t)+arg_size, p_offload_mgr_workq_ctx);

    if(NULL != retval)
    {
        ((offload_mgr_msg_hdr_t*)retval)->evt = OFFLOAD_MGR_EVT_CLIENT_CUSTOM;
        ((offload_mgr_client_work_t*)retval)->client_data_size = arg_size;
        retval = ((uint8_t*)(retval))+(sizeof(offload_mgr_client_work_t));
    }

    return retval;
}

int offload_mgr_client_work_submit(offload_mgr_srv_client_t client, void *args)
{
    void *buffer = ((uint8_t*)(args))-(sizeof(offload_mgr_client_work_t));
    // Todo: Use container
    return pdtsw_submit_to_workq(offload_mgr_task, buffer, p_offload_mgr_workq_ctx);
}