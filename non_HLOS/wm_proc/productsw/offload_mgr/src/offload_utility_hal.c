/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
// $QTI_LICENSE_C$

#include <stringl.h>
#include <zephyr/logging/log.h>
#include "pdtsw_heap.h"
#include "offload_utility_hal_intf_defs.h"
#include "offload_mgr.h"
#ifdef CONFIG_PDTSW_OFFLOAD_MGR_AUDIO_ENABLE
#include "audio_service.h"
#endif
#ifdef CONFIG_PDTSW_OFFLOAD_MGR_HAPTICS_ENABLE
#include "haptics_service.h"
#endif
#ifdef CONFIG_PDTSW_COMMON_UTILS_ENABLE
#include "pdtsw_util.h"
#endif
#include "pdtsw_platform.h"

LOG_MODULE_REGISTER(offload_plugin_utility_hal, LOG_LEVEL_ERR);

#define OFFLOAD_UTILITY_HAL_SUCCESS                                   (0)
#define OFFLOAD_UTILITY_HAL_FAILURE                                   (-1)

#define AS_GLINK_REMOTE_SS_APPS_CHANNEL_NAME                          "am-utility"

/** Allocated on M55 TCM. Max size of meta data received from utility_hal HAL.
 *  (sizeof(gmi_utility_hal_request_t) + max payload size)
 */
#define AM_RX_UTILITY_HAL_DATA_INTENT_SIZE_MAX                        (3072)

#if defined(CONFIG_PDTSW_AUDIO_SERVICE_ENABLE) || defined(CONFIG_PDTSW_HAPTICS_SERVICE_ENABLE)
#define UTIL_HAL_ISLAND_MODE_CLIENT       "util_hal_island_mode_client"

npa_client_handle util_hal_island_mode_handle;
#endif

/* Offload manager client interface declaration */
offload_mgr_srv_client_intf_t utility_hal_offload_ctx;

/**
 * Callback function to receive RX event from offload manager.
 */
void offload_utility_hal_rx_event_cb(const void* pkt_priv, const void* data, size_t size)
{
    int32_t ret = OFFLOAD_UTILITY_HAL_SUCCESS;
    if (!data)
    {
        LOG_ERR("%s: Invalid data rx", __func__);
        return;
    }

    gmi_utility_hal_pkt_header_t *header = (gmi_utility_hal_pkt_header_t*)data;

    if (header->usecase_type == USECASE_NONE)
    {
        /* Dummy check to send failure response to utility hal if services are disabled */
        ret = OFFLOAD_UTILITY_HAL_FAILURE;
    }
#ifdef CONFIG_PDTSW_OFFLOAD_MGR_AUDIO_ENABLE
    else if (header->usecase_type == USECASE_AUDIO
#ifdef CONFIG_QC_PDTSW_STHAL_ENABLE
            || header->usecase_type == USECASE_VOICE_ACTIVATION
#endif
            )
    {
        if (header->msg_type == GMI_UTILITY_HAL_MSG_TYPE_REQUEST)
        {
            gmi_utility_hal_request_t *request = (gmi_utility_hal_request_t*)data;

            /* trigger island mode exit (blocking call)*/
            pdtsw_island_vote_island_exit(util_hal_island_mode_handle);

            /* Blocking call to audio service to copy meta data */
            audio_service_usecase_type_t uc_type =
                (USECASE_AUDIO == header->usecase_type) ? AS_UC_PLAYBACK : AS_UC_VOICE_ACTIVATION;
            audio_service_offload_event_type_t evt_type;
            if (GMI_UTILITY_HAL_ADD_PATTERN == header->opcode)
            {
                evt_type = AS_OFFLOAD_EVT_ADD;
            }
            else if (GMI_UTILITY_HAL_REMOVE_PATTERN == header->opcode)
            {
                evt_type = AS_OFFLOAD_EVT_DELETE;
            }
            else
            {
                evt_type = AS_OFFLOAD_EVT_FLUSH_ALL;
            }
            ret = audio_service_process_offload_request(uc_type,
                evt_type, request->payload, request->payload_size);

            /* trigger island mode entry (non-blocking call)*/
            pdtsw_island_vote_island_entry(util_hal_island_mode_handle);
        }
        else if (header->msg_type == GMI_UTILITY_HAL_MSG_TYPE_RESPONSE)
        {
            /* No responses expected from AP as of now */
        }
    }
#endif
#ifdef CONFIG_PDTSW_OFFLOAD_MGR_HAPTICS_ENABLE
    else if (header->usecase_type == USECASE_HAPTICS)
    {
        if (header->msg_type == GMI_UTILITY_HAL_MSG_TYPE_REQUEST)
        {
            gmi_utility_hal_request_t *request = (gmi_utility_hal_request_t*)data;

            /* trigger island mode exit (blocking call)*/
            pdtsw_island_vote_island_exit(util_hal_island_mode_handle);

            ret = haptics_service_process_offload_request(header->opcode, request->payload,
                request->payload_size);

            /* trigger island mode entry (non-blocking call)*/
            pdtsw_island_vote_island_entry(util_hal_island_mode_handle);
        }
        else if (header->msg_type == GMI_UTILITY_HAL_MSG_TYPE_RESPONSE)
        {
            /* No responses expected from haptics interface as of now */
        }
    }
#endif
#ifdef CONFIG_PDTSW_COMMON_UTILS_ENABLE
    else if(header->usecase_type == USECASE_ADB_CMD)
    {
        if((header->opcode == GMI_UTILITY_HAL_AM_CTRL) &&
            (header->msg_type == GMI_UTILITY_HAL_MSG_TYPE_REQUEST))
        {
            gmi_utility_hal_request_t *request = (gmi_utility_hal_request_t*)data;
            ret = pdtsw_util_adb_cmd_process(request->payload, request->payload_size);
        }
        else
        {
            LOG_ERR("%s: Invalid msg type:%d or opcode:%d", __func__, header->msg_type, header->opcode);
        }
    }
#endif
    else
    {
        LOG_ERR("%s: usecase type %d not supported", __func__, header->usecase_type);
        ret = OFFLOAD_UTILITY_HAL_FAILURE;
    }

    offload_mgr_glink_rx_done(OFF_MGR_SRV_UTILITY_HAL, (void*)data, false);
    /* Compose the response to be sent to utility hal */
    gmi_utility_hal_response_t *response = (gmi_utility_hal_response_t*)pdtsw_heap_alloc(PDTSW_COMMON_GLINK_HEAP,
        sizeof(gmi_utility_hal_response_t) + sizeof(int32_t));
    if (!response)
    {
        LOG_ERR("%s: Resp mem alloc failed", __func__);
        return;
    }
    response->header.opcode = header->opcode;
    response->header.msg_type = GMI_UTILITY_HAL_MSG_TYPE_RESPONSE;
    response->header.usecase_type = header->usecase_type;
    response->result = (!ret) ? GMI_UTILITY_HAL_SUCCESS : GMI_UTILITY_HAL_FAILURE;
    response->payload_size = sizeof(int32_t);
    memscpy(response->payload, response->payload_size, (void*)(&ret), sizeof(int32_t));

    glink_err_type glink_ret = offload_mgr_glink_send(OFF_MGR_SRV_UTILITY_HAL, response,
            NULL, sizeof(gmi_utility_hal_response_t) + response->payload_size, GLINK_TX_REQ_INTENT);
    if (GLINK_STATUS_SUCCESS != glink_ret)
    {
        LOG_ERR("%s: Resp tx failed", __func__);
        pdtsw_heap_free(PDTSW_COMMON_GLINK_HEAP, response);
    }
}

/**
 * Callback function to receive tx done notification from offload manager.
 */
void offload_utility_hal_tx_done_cb(const void* pkt_priv, const void* data, size_t size)
{
    if (data)
    {
        pdtsw_heap_free(PDTSW_COMMON_GLINK_HEAP, (void*)data);
    }
}

/**
 * Callback function to receive intent request from offload manager.
 */
void offload_utility_hal_intent_req_cb(size_t req_size)
{
    if (req_size <= AM_RX_UTILITY_HAL_DATA_INTENT_SIZE_MAX)
    {
        glink_err_type glink_ret = offload_mgr_glink_queue_intent(OFF_MGR_SRV_UTILITY_HAL, NULL,
            req_size);
        if (GLINK_STATUS_SUCCESS != glink_ret)
        {
            LOG_ERR("%s: queue rx intent failed", __func__);
        }
    }
}

/**
 * Callback function to allocate memory for intent request from offload manager.
 */
glink_err_type offload_utility_hal_allocate_cb(const void *pkt_priv, size_t intent_size,
    void **buffer_ptr)
{
    glink_err_type glink_ret = GLINK_STATUS_SUCCESS;
    *buffer_ptr = pdtsw_heap_alloc(PDTSW_COMMON_GLINK_HEAP, intent_size);
    if (!(*buffer_ptr))
    {
        LOG_ERR("%s:%d", __func__,  __LINE__);
        glink_ret = GLINK_STATUS_FAILURE;
    }

    return glink_ret;
}

/**
 * Callback function to deallocate memory for intent request from offload manager.
 */
void offload_utility_hal_deallocate_cb(const void *pkt_priv, size_t intent_size, void *buffer_ptr)
{
    pdtsw_heap_free(PDTSW_COMMON_GLINK_HEAP, buffer_ptr);
}

void offload_utility_hal_channel_event_cb(glink_channel_event_type evt)
{
    switch (evt)
    {
        case GLINK_CONNECTED:
        {
            LOG_DBG("%s: Glink connected, %d %d", __func__, sizeof(gmi_utility_hal_request_t), sizeof(gmi_utility_hal_pkt_header_t));
            utility_hal_offload_ctx.chnl_state = evt;
            break;
        }
        case GLINK_LOCAL_DISCONNECTED:
        case GLINK_REMOTE_DISCONNECTED:
        {
            LOG_DBG("%s: Glink disconnected", __func__);
            utility_hal_offload_ctx.chnl_state = evt;
            break;
        }
        default:
        {
            LOG_ERR("%s: Unhandled evt %d", __func__, evt);
            break;
        }
    }
}

/**
 * Callback function to receive link state event from offload manager.
 * If link state is up, channel will be opened with AP for data offload. Else channel
 * will be closed.
 */
void offload_utility_hal_link_event_cb(glink_link_state_type status)
{
    glink_err_type glink_ret = GLINK_STATUS_SUCCESS;
    if (GLINK_LINK_STATE_UP == status)
    {
        glink_ret = offload_mgr_glink_open(OFF_MGR_SRV_UTILITY_HAL);
    }
    else if (GLINK_LINK_STATE_DOWN == status)
    {
        glink_ret = offload_mgr_glink_close(OFF_MGR_SRV_UTILITY_HAL);
    }

    if (glink_ret != GLINK_STATUS_SUCCESS)
    {
        LOG_ERR("%s: open/close glink ch failed", __func__);
    }
}


int offload_utility_hal_init()
{
#if defined(CONFIG_PDTSW_AUDIO_SERVICE_ENABLE) || defined(CONFIG_PDTSW_HAPTICS_SERVICE_ENABLE)
    util_hal_island_mode_handle = pdtsw_island_create_client(UTIL_HAL_ISLAND_MODE_CLIENT);
    if (util_hal_island_mode_handle == NULL)
    {
        LOG_ERR("pdtsw_island_create_client() failed\n");
        return OFFLOAD_UTILITY_HAL_FAILURE;
    }
#endif
    return OFFLOAD_UTILITY_HAL_SUCCESS;
}


/**
 * Offload manager client interface.
 * This is the interface that the offload manager uses to communicate with the audio/haptics service.
 */
offload_mgr_srv_client_intf_t utility_hal_offload_ctx =
{
    .channel_name                = AS_GLINK_REMOTE_SS_APPS_CHANNEL_NAME,
    .chnl_state                  = GLINK_LOCAL_DISCONNECTED,
    .is_dynamic_intent_supported = true,
    .on_offload_mgr_start        = NULL,
    .on_link_evt                 = offload_utility_hal_link_event_cb,
    .on_channel_evt              = offload_utility_hal_channel_event_cb,
    .on_rx_evt                   = offload_utility_hal_rx_event_cb,
    .on_tx_done_evt              = offload_utility_hal_tx_done_cb,
    .on_intent_req               = offload_utility_hal_intent_req_cb,
    .on_allocate_cb              = offload_utility_hal_allocate_cb,
    .on_deallocate_cb            = offload_utility_hal_deallocate_cb,
    .event_hndlr                 = NULL
};
