/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
// $QTI_LICENSE_C$

#include <zephyr/logging/log.h>
#include "pdtsw_heap.h"
#include "rsb_gmi.h"
#include "offload_mgr.h"
#include "qapi_rsb_service.h"

LOG_MODULE_REGISTER(offload_rsb, LOG_LEVEL_ERR);

#define PDTSW_FATAL()       while(1);

static void rsb_offload_mgr_start(void);
static void rsb_link_evt(glink_link_state_type status);
static void rsb_channel_evt(glink_channel_event_type evt);
static void rsb_rx_evt(const void* pkt_priv, const void* data, size_t size);
static void rsb_tx_done_evt(const void* pkt_priv, const void* data, size_t size);
static void rsb_intent_req(size_t req_size);
static glink_err_type rsb_allocate_cb(const void *pkt_priv, size_t intent_size,
    void **buffer_ptr);
static void rsb_deallocate_cb(const void *pkt_priv, size_t intent_size,
    void *buffer_ptr);
static void rsb_srv_cb(qapi_rsb_evt_t evt, qapi_rsb_status_t evt_status, void *data);
static bool rsb_parse_config(rsb_gmi_config_t* input, qapi_rsb_cfg_t* cfg);

offload_mgr_srv_client_intf_t rsb_offload_cxt =
{
    .channel_name = "rsb-ctrl",
    .chnl_state = GLINK_LOCAL_DISCONNECTED,
    .is_dynamic_intent_supported = true,
    .on_offload_mgr_start = rsb_offload_mgr_start,
    .on_link_evt = rsb_link_evt,
    .on_channel_evt = rsb_channel_evt,
    .on_rx_evt = rsb_rx_evt,
    .on_tx_done_evt = rsb_tx_done_evt,
    .on_intent_req = rsb_intent_req,
    .on_allocate_cb = rsb_allocate_cb,
    .on_deallocate_cb = rsb_deallocate_cb,
    .event_hndlr = NULL,
};

void rsb_offload_mgr_start(void)
{
    rsb_offload_cxt.client_status = OFF_MGR_SRV_STATUS_STOPPED;
}

void rsb_link_evt(glink_link_state_type status)
{
    glink_err_type glink_ret = GLINK_STATUS_SUCCESS;
    if (GLINK_LINK_STATE_UP == status)
    {
        glink_ret = offload_mgr_glink_open(OFF_MGR_SRV_RSB_SERVICE);
    }
    else if (GLINK_LINK_STATE_DOWN == status)
    {
        glink_ret = offload_mgr_glink_close(OFF_MGR_SRV_RSB_SERVICE);
    }

    if (glink_ret != GLINK_STATUS_SUCCESS)
    {
        LOG_ERR("Unable to open/close glink channel");
        //TODO: panic here
    }
}

void rsb_channel_evt(glink_channel_event_type evt)
{
    glink_err_type glink_err;

    if(evt != rsb_offload_cxt.chnl_state)
    {
        if(GLINK_CONNECTED == evt)
        {
            /* Queue intent dynamically.*/
        }
        else if(GLINK_REMOTE_DISCONNECTED == evt)
        {
            /* Close the channel if already opened.*/
            if(GLINK_CONNECTED == rsb_offload_cxt.chnl_state)
            {
                glink_err = offload_mgr_glink_close(OFF_MGR_SRV_RSB_SERVICE);
            
                if(GLINK_STATUS_SUCCESS != glink_err)
                {
                    /* Todo: Log and ignore expecting the close event might be already
                        ongoing.*/
                }
            }
            rsb_offload_cxt.client_status = OFF_MGR_SRV_STATUS_STOPPED;
        }
        else
        {
            /* We are here since we have explicitly requested to close channel.*/
            rsb_offload_cxt.client_status = OFF_MGR_SRV_STATUS_STOPPED;
        }

        rsb_offload_cxt.chnl_state = evt;
    }
}

void rsb_rx_evt(const void* pkt_priv, const void* data, size_t size)
{
    glink_err_type glink_ret;

    if(NULL != data && 0 < size)
    {
        qapi_rsb_status_t api_retval;
        rsb_gmi_msg_t *msg = (rsb_gmi_msg_t*)data;
        rsb_gmi_msg_t *response_msg = NULL;
        bool send_response = false;

        response_msg = (rsb_gmi_msg_t *)pdtsw_heap_alloc(PDTSW_COMMON_GLINK_HEAP,
            sizeof(rsb_gmi_msg_t));
        if (!response_msg)
        {
            LOG_ERR("rsb_rx_evt: Resp mem alloc failed");
            PDTSW_FATAL();
        }


        switch (msg->opcode)
        {
            case START_OFFLOAD:
            {
                qapi_rsb_cfg_t rsb_cfg;
                qapi_rsb_cfg_t *rsb_cfg_ptr = NULL;
                rsb_cfg.params = msg->start_data.config_params;

                if (false == rsb_parse_config(&(msg->start_data.config_data), &rsb_cfg))
                {
                    response_msg->opcode = ACK_OFFLOAD;
                    response_msg->ack_data.resp = RSB_GMI_CONFIG_INVALID;
                    send_response = true;
                    break;
                }
                else if (rsb_cfg.params != 0)
                {
                    rsb_cfg_ptr = &rsb_cfg;
                }

                bool power_on_req = (msg->start_data.enable_voting != 0);
                api_retval = qapi_rsb_service_enable(power_on_req, rsb_cfg_ptr, rsb_srv_cb);

                response_msg->opcode = ACK_OFFLOAD;
                if ((api_retval != QAPI_RSB_SUCCESS) && (api_retval != QAPI_RSB_SAME_STATE_ERROR))
                {
                    response_msg->ack_data.resp = RSB_GMI_FAIL;
                    send_response = true;
                }
                else if (api_retval == QAPI_RSB_SAME_STATE_ERROR)
                {
                    response_msg->ack_data.resp = RSB_GMI_SUCCESS;
                    send_response = true;
                }
                break;
            }

            case STOP_OFFLOAD:
            {
                bool power_off_req = (msg->stop_data.enable_voting == 0);
                api_retval = qapi_rsb_service_disable(power_off_req, rsb_srv_cb);

                response_msg->opcode = ACK_OFFLOAD;
                if ((api_retval != QAPI_RSB_SUCCESS) && (api_retval != QAPI_RSB_SAME_STATE_ERROR))
                {
                    response_msg->ack_data.resp = RSB_GMI_FAIL;
                    send_response = true;
                }
                else if (api_retval == QAPI_RSB_SAME_STATE_ERROR)
                {
                    response_msg->ack_data.resp = RSB_GMI_SUCCESS;
                    send_response = true;
                }
                break;
            }

            default:
                LOG_ERR("Unexpected opcode rx. %d", msg->opcode);
                break;
        }

        if (send_response)
        {
            glink_ret = offload_mgr_glink_send(OFF_MGR_SRV_RSB_SERVICE, response_msg,
                                               NULL, sizeof(rsb_gmi_msg_t), GLINK_TX_REQ_INTENT);
            if (glink_ret != GLINK_STATUS_SUCCESS)
            {
                LOG_ERR("Glink tx failed. %d", glink_ret);
                pdtsw_heap_free(PDTSW_COMMON_GLINK_HEAP, response_msg);
            }
        }
        else
        {
            pdtsw_heap_free(PDTSW_COMMON_GLINK_HEAP, response_msg);
        }
    }
    else
    {
        LOG_ERR("Invalid buffer rx. buff = %p, size = %d", data, size);
        PDTSW_FATAL();
    }

    glink_ret = offload_mgr_glink_rx_done(OFF_MGR_SRV_RSB_SERVICE, (void*)data, false);
    if(GLINK_STATUS_SUCCESS != glink_ret)
    {
        LOG_ERR("Rx done failed: %d", glink_ret);
    }
}

void rsb_tx_done_evt(const void* pkt_priv, const void* data, size_t size)
{
    /* Todo: For Tx sequences, better move the dynamic allocation to offload mgr.*/
    pdtsw_heap_free(PDTSW_COMMON_GLINK_HEAP, (void*)data);
}

void rsb_intent_req(size_t req_size)
{
    if (req_size <= sizeof(rsb_gmi_msg_t))
    {
        glink_err_type glink_ret = offload_mgr_glink_queue_intent(
            OFF_MGR_SRV_RSB_SERVICE, NULL, req_size);
        if (GLINK_STATUS_SUCCESS != glink_ret)
        {
            LOG_ERR("Unable to queue rx intent of size %d", req_size);
        }
    }
    else
    {
        /* This is an unexpected message on this channel*/
        LOG_ERR("Unexpected rx intent request of size %d", req_size);
        PDTSW_FATAL();
    }
}

glink_err_type rsb_allocate_cb(const void *pkt_priv, size_t intent_size,
    void **buffer_ptr)
{
    glink_err_type glink_ret = GLINK_STATUS_SUCCESS;
    *buffer_ptr = pdtsw_heap_alloc(PDTSW_COMMON_GLINK_HEAP, intent_size);
    if (!(*buffer_ptr))
    {
        glink_ret = GLINK_STATUS_FAILURE;
    }

    return glink_ret;
}
    
void rsb_deallocate_cb(const void *pkt_priv, size_t intent_size,
    void *buffer_ptr)
{
    pdtsw_heap_free(PDTSW_COMMON_GLINK_HEAP, buffer_ptr);
}

void rsb_srv_cb(qapi_rsb_evt_t evt, qapi_rsb_status_t evt_status, void *data)
{
    glink_err_type glink_ret;

    rsb_gmi_msg_t *response_msg = NULL;
    response_msg = (rsb_gmi_msg_t *)pdtsw_heap_alloc(PDTSW_COMMON_GLINK_HEAP,
        sizeof(rsb_gmi_msg_t));
    if (!response_msg)
    {
        LOG_ERR("rsb_srv_cb: Resp mem alloc failed");
        PDTSW_FATAL();
    }

    response_msg->opcode = ACK_OFFLOAD;
    response_msg->ack_data.resp = RSB_GMI_FAIL;

    switch(evt)
    {
        case QAPI_RSB_EVT_ENABLE:
        {
            if(QAPI_RSB_SUCCESS == evt_status)
            {
                rsb_offload_cxt.client_status = OFF_MGR_SRV_STATUS_STARTED;
                response_msg->ack_data.resp = RSB_GMI_SUCCESS;
            }
            break;
        }
        case QAPI_RSB_EVT_DISABLE:
        {
            if(QAPI_RSB_SUCCESS == evt_status)
            {
                rsb_offload_cxt.client_status = OFF_MGR_SRV_STATUS_STOPPED;
                response_msg->ack_data.resp = RSB_GMI_SUCCESS;
            }
            break;
        }
        default:
        {
            pdtsw_heap_free(PDTSW_COMMON_GLINK_HEAP, response_msg);
            LOG_ERR("Invalid evt rx: %d", evt);
            return;
        }
    }

    glink_ret = offload_mgr_glink_send(OFF_MGR_SRV_RSB_SERVICE, response_msg,
        NULL, sizeof(rsb_gmi_msg_t), GLINK_TX_REQ_INTENT);
    if (GLINK_STATUS_SUCCESS != glink_ret)
    {
        pdtsw_heap_free(PDTSW_COMMON_GLINK_HEAP, response_msg);
        LOG_ERR("Glink tx failed. %d",glink_ret);
    }
}

bool rsb_parse_config(rsb_gmi_config_t* input, qapi_rsb_cfg_t* cfg)
{
    if(cfg->params == 0)
    {
        return true;
    }

    cfg->data.motion_int_threshold = input->motion_threshold;

    /* Check if the motion interval received is valid*/
    if(QAPI_RSB_MOTION_INT_INTERVAL_88MS < input->motion_interval)
    {
        return false;
    }
    cfg->data.motion_int_interval = input->motion_interval;

    if(QAPI_RSB_SLEEP123_DISABLED < input->op_mode)
    {
        return false;
    }
    cfg->data.sleep_mode = input->op_mode;

    cfg->data.x_resolution =  ((input->res_x_hi) << 8) | (input->res_x_lo);

    cfg->data.sleep_cfg[0].freq = ((input->sleep1) >> 4) & (0x0F);
    cfg->data.sleep_cfg[0].etm = (input->sleep1) & (0x0F);

    cfg->data.sleep_cfg[1].freq = ((input->sleep2) >> 4) & (0x0F);
    cfg->data.sleep_cfg[1].etm = (input->sleep2) & (0x0F);

    cfg->data.sleep_cfg[2].freq = ((input->sleep3) >> 4) & (0x0F);
    cfg->data.sleep_cfg[2].etm = (input->sleep3) & (0x0F);

    return true;
}
