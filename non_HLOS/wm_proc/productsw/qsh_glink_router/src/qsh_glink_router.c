/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
#include <stdlib.h>
#include <errno.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include <stdlib.h>
#include <stringl.h>
#include "sns_client_glink.pb.h"
#include "qsh_glink_priv.h"
#include "pdtsw_worker_thread.h"
#include "pdtsw_glink.h"
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log_ctrl.h>


LOG_MODULE_REGISTER(qsh_glink_router, LOG_LEVEL_DBG);

#ifdef LOG_DBG
#undef LOG_DBG
#endif
#define LOG_DBG(...)

/* Todo: have a common fatal.*/
#define PDTSW_FATAL()   do{k_panic();__builtin_unreachable();}while(0)
#define UNUSED_PARAM(x) x __attribute__((unused))


/** Variable to track the qsh glink router context.*/
qsh_glink_cxt_t qsh_glink_cxt;

/** Core functions.*/
static void qsh_glink_evt_hndlr(pdtsw_work_item_t *item);

/* Utlility functions.*/
static void *qsh_glink_work_init(size_t args_size);

static int qsh_glink_work_submit(void* args);

static void qsh_glink_clean_up_connections();

/* Client related events handlers.*/
static void qsh_glink_send_data_handler(qsh_glink_client_t* client_info,
                        void* data, size_t size);

static void qsh_glink_conn_alloc_handler(qsh_glink_client_node_t* client_node);

static void qsh_glink_conn_free_handler(qsh_glink_client_t* client_info);

/* Client list management functions*/

static int qsh_glink_compare_client(const void *a, const void *b);

static void qsh_glink_insert_client(qsh_glink_client_t* new_client);

static qsh_glink_client_t* qsh_glink_find_client(uint32_t conn_hndl);

static int qsh_glink_remove_client(uint32_t conn_hndl);

/* Glink callback handlers.*/

/* link state event callback.*/
static void qsh_glink_link_state_cb(glink_link_info_type *link_info,void* priv);

/* Channel state event callback.*/
static void qsh_glink_chnl_state_cb(glink_handle_type handle, const void *priv,
                        glink_channel_event_type event);

/* Tx done callback.*/
static void qsh_glink_tx_done_cb(glink_handle_type handle, const void *priv,
                        const void *pkt_priv, const void *ptr, size_t size);

/* Intent request callback.*/
static bool qsh_glink_rx_intent_req_cb(glink_handle_type handle,
                        const void *priv, size_t req_size);

/* Rx callback.*/
static void qsh_glink_rx_cb(glink_handle_type handle, const void *priv,
                        const void *pkt_priv, const void *ptr,
                        size_t size, size_t intent_used);

/* Glink allocator cb.*/
static glink_err_type qsh_glink_allocate_cb(glink_handle_type handle,
                const void *priv, const void *pkt_priv,
                size_t intent_size, void **buffer_ptr);

/* Glink deallocator cb.*/
static void qsh_glink_deallocate_cb(glink_handle_type handle, const void *priv,
                const void *pkt_priv, size_t intent_size, void *buffer_ptr);

/* Glink event handlers*/

/* Link state event handler */
static void qsh_glink_link_state_handler(glink_link_state_type link_state);

/* Channel state event handler.*/
static void qsh_glink_chnl_state_handler(glink_channel_event_type event);

/* Tx done event handler.*/
static void qsh_glink_tx_done_handler(glink_handle_type handle,
                const void *priv, const void *pkt_priv,
                const void *ptr, size_t size);

/* Intent request event handler.*/
static void qsh_glink_rx_intent_req_handler(glink_handle_type handle,
                    const void *priv, size_t req_size);

/* Rx event handler.*/
static void qsh_glink_rx_handler(glink_handle_type handle, const void *priv,
                    const void *pkt_priv, const void *ptr,
                    size_t size, size_t intent_used);

int qsh_glink_router_init(void)
{
    int ret_val;
    glink_err_type glink_err;
    glink_link_id_type link_info;

    LOG_DBG("qsh_glink_router init");

    GLINK_LINK_ID_STRUCT_INIT(link_info);
    link_info.link_notifier = qsh_glink_link_state_cb;
    link_info.remote_ss = QSH_GLINK_ROUTER_REMOTESS_NAME;
    link_info.xport = QSH_GLINK_ROUTER_XPORT_NAME;

    memset(&qsh_glink_cxt, 0, sizeof(qsh_glink_cxt));
    sys_slist_init(&(qsh_glink_cxt.clients_waiting));
    ret_val = k_mutex_init(&(qsh_glink_cxt.mtx));    

    if(0 != ret_val)
    {
        LOG_ERR("Mutex init failed: %d", ret_val);
        PDTSW_FATAL();
    }

    qsh_glink_cxt.channel_state = GLINK_LOCAL_DISCONNECTED;
    qsh_glink_cxt.link_state = GLINK_LINK_STATE_DOWN;

    /* Register for Q6 glink link state events.*/
    glink_err = glink_register_link_state_cb(&link_info, NULL);

    if(GLINK_STATUS_SUCCESS != glink_err)
    {
        LOG_ERR("Link state cb reg failed: %d", glink_err);
        PDTSW_FATAL();
    }

    qsh_glink_cxt.qsh_link_hndl = link_info.handle;

    LOG_DBG("qsh_glink_router init done");

    return 0;
}

static void qsh_glink_evt_hndlr(pdtsw_work_item_t *item)
{
    qsh_glink_msg_t* msg  = 
                ((qsh_glink_msg_t*)pdtsw_get_workq_handler_args(item));
    qsh_glink_cxt.last_msg = *msg;

    LOG_DBG("qsh_glink_evt_hndlr. evt: %d", msg->evt);

    switch(msg->evt)
    {
        case QSH_GLINK_EVT_ID_CONN_ALLOC_REQ:
        {
            qsh_glink_conn_alloc_handler((qsh_glink_client_node_t*)(msg->data));
            break;
        }
        case QSH_GLINK_EVT_ID_CONN_FREE_REQ:
        {
            qsh_glink_conn_free_handler(msg->client_hndl);
            break;
        }
        case QSH_GLINK_EVT_ID_SEND_DATA:
        {
            qsh_glink_send_data_handler(msg->client_hndl, msg->data,
                msg->size);
            break;
        }
        case QSH_GLINK_EVT_ID_GLINK_LINK_STATE:
        {
            qsh_glink_link_state_handler((glink_link_state_type)(uintptr_t)(msg->data));
            break;
        }
        case QSH_GLINK_EVT_ID_GLINK_CHANNEL_STATE:
        {
            qsh_glink_chnl_state_handler((glink_channel_event_type)(uintptr_t)(msg->data));
            break;
        }
        case QSH_GLINK_EVT_ID_GLINK_RX:
        {
            qsh_glink_glink_msg_t* glink_msg = (qsh_glink_glink_msg_t*)msg;
            qsh_glink_rx_handler(qsh_glink_cxt.qsh_channel_hndl, NULL,
                    glink_msg->pkt_priv, glink_msg->data, glink_msg->size,
                    glink_msg->intent_used);
            break;
        }
        case QSH_GLINK_EVT_ID_GLINK_RX_INTENT_REQ:
        {
            qsh_glink_glink_msg_t* glink_msg = (qsh_glink_glink_msg_t*)msg;
            qsh_glink_rx_intent_req_handler(
                    qsh_glink_cxt.qsh_channel_hndl, NULL, glink_msg->size);
            
            break;
        }
        case QSH_GLINK_EVT_ID_GLINK_TX_DONE:
        {
            qsh_glink_glink_msg_t* glink_msg = (qsh_glink_glink_msg_t*)msg;
            qsh_glink_tx_done_handler(qsh_glink_cxt.qsh_channel_hndl, NULL,
                    glink_msg->pkt_priv, glink_msg->data, glink_msg->size);
            break;
        }

        default:
        {
            LOG_DBG("Unknown evt: %d", msg->evt);
        }
    }

    pdtsw_free_handler_args(item, NULL);
}

int qapi_qsh_glink_register_status(qsh_glink_status_cb_t cb, const void* priv)
{
    int api_retval = 0;

    if(NULL == cb)
    {
        LOG_ERR("Cb is NULL");
        return EINVAL;
    }

    k_mutex_lock(&(qsh_glink_cxt.mtx), K_FOREVER);

    if(CONFIG_PDTSW_QSH_GLINK_ROUTER_MAX_STATUS_CLIENTS 
        < qsh_glink_cxt.status_clients_count)
    {
        LOG_ERR("List is full");
        k_mutex_unlock(&(qsh_glink_cxt.mtx));
        return ENOBUFS;
    }

    for(api_retval = 0; api_retval < CONFIG_PDTSW_QSH_GLINK_ROUTER_MAX_STATUS_CLIENTS;
        api_retval++)
    {
        if(qsh_glink_cxt.status_clients[api_retval].cb == NULL)
        {
            qsh_glink_cxt.status_clients[api_retval].cb = cb;
            qsh_glink_cxt.status_clients[api_retval].priv = priv;
            qsh_glink_cxt.status_clients_count++;
            break;
        }
    }

    k_mutex_unlock(&(qsh_glink_cxt.mtx));

    return api_retval;
}

int qapi_qsh_glink_deregister_status(qsh_glink_status_cb_t cb, const void* priv)
{
    if(NULL == cb)
    {
        LOG_ERR("Cb is NULL");
        return EINVAL;
    }

    k_mutex_lock(&(qsh_glink_cxt.mtx), K_FOREVER);

    if(qsh_glink_cxt.status_clients_count == 0)
    {
        LOG_ERR("List is empty");
        k_mutex_unlock(&(qsh_glink_cxt.mtx));
        return ENOBUFS;
    }

    for(int idx = 0; idx <
        CONFIG_PDTSW_QSH_GLINK_ROUTER_MAX_STATUS_CLIENTS;
        idx++)
    {
        if((qsh_glink_cxt.status_clients[idx].cb == cb) &&
            (qsh_glink_cxt.status_clients[idx].priv == priv))
        {
            qsh_glink_cxt.status_clients[idx].cb = NULL;
            qsh_glink_cxt.status_clients[idx].priv = NULL;
            qsh_glink_cxt.status_clients_count--;
            break;
        }
    }

    k_mutex_unlock(&(qsh_glink_cxt.mtx));

    return 0;
}

int qapi_qsh_glink_allocate_conn(qapi_qsh_glink_hndl_t *hndl,
        qsh_glink_conn_cb_t cb, const void* priv)
{
    int api_retval;

    LOG_DBG("qapi_qsh_glink_allocate_conn. cb=%p", cb);

    if(NULL == cb)
    {
        LOG_ERR("Cb is NULL");
        return EINVAL;
    }
    
    /* Todo: Allocate memory for qsh_glink_client_t from appropriate heap.*/
    qsh_glink_client_t* client_info = 
            (qsh_glink_client_t*)pdtsw_heap_alloc(PDTSW_QSH_GLINK_ROUTER_HEAP,
                sizeof(qsh_glink_client_t));
    if(NULL == client_info)
    {
        LOG_ERR("Mem alloc failed for client_info");
        return ENOMEM;
    }

    LOG_DBG("client_info address = %p", client_info);

    qsh_glink_client_node_t* client_node = 
            (qsh_glink_client_node_t*)pdtsw_heap_alloc(PDTSW_QSH_GLINK_ROUTER_HEAP, 
                sizeof(qsh_glink_client_node_t));
    if(NULL == client_node)
    {
        LOG_ERR("Mem alloc failed for client_node");
        pdtsw_heap_free(PDTSW_QSH_GLINK_ROUTER_HEAP, client_info);
        return ENOMEM;
    }

    LOG_DBG("client_node addr = %p", client_node);

    client_node->client_info = client_info;

    client_info->cb = cb;
    client_info->priv= priv;

    k_mutex_lock(&(qsh_glink_cxt.mtx), K_FOREVER);

    sys_slist_append(&(qsh_glink_cxt.clients_waiting), &(client_node->node));
    
    sys_snode_t *node = sys_slist_peek_head(&(qsh_glink_cxt.clients_waiting));

    LOG_DBG("node inserted: %p, node read: %p", &(client_node->node), node);

    if(node != &(client_node->node))
    {

    }

    ++qsh_glink_cxt.clients_waiting_count;

    k_mutex_unlock(&(qsh_glink_cxt.mtx));

    LOG_DBG("Handle allocated: %p", client_info);

    qsh_glink_msg_t *msg = (qsh_glink_msg_t*)
                        qsh_glink_work_init(sizeof(qsh_glink_msg_t));

    msg->evt = QSH_GLINK_EVT_ID_CONN_ALLOC_REQ;
    msg->data = client_node;

    api_retval = qsh_glink_work_submit(msg);

    *hndl = (qapi_qsh_glink_hndl_t)client_info;

    return 0;
}

int qapi_qsh_glink_free_conn(qapi_qsh_glink_hndl_t hndl)
{
    int api_retval, iter;

    LOG_DBG("qapi_qsh_glink_free_conn. hndl=%p", hndl);

    if(NULL == hndl)
    {
        LOG_DBG("hndl is NULL");
        return EINVAL;
    }

    qsh_glink_client_t* client_info = (qsh_glink_client_t*)hndl;

    k_mutex_lock(&(qsh_glink_cxt.mtx), K_FOREVER);

    for(iter = 0; iter < qsh_glink_cxt.clients_count; iter++)
    {
        if(qsh_glink_cxt.clients[iter] == client_info)
        {
            break;
        }
    }

    if(iter == qsh_glink_cxt.clients_count)
    {
        LOG_DBG("hndl not found in client list");
        k_mutex_unlock(&(qsh_glink_cxt.mtx));
        return EPERM;
    }

    k_mutex_unlock(&(qsh_glink_cxt.mtx));

    qsh_glink_msg_t *msg = (qsh_glink_msg_t*)
                        qsh_glink_work_init(sizeof(qsh_glink_msg_t));

    msg->evt = QSH_GLINK_EVT_ID_CONN_FREE_REQ;
    msg->client_hndl = client_info;

    api_retval = qsh_glink_work_submit(msg);

    return 0;
}

int qapi_qsh_glink_send(qapi_qsh_glink_hndl_t hndl,
            sns_client_request_msg* sns_client_data, size_t size)
{
    int api_retval, iter;
    LOG_DBG("qapi_qsh_glink_send. hndl=%p, data=%p, size=%d",
        hndl, sns_client_data, size);

    if(NULL == hndl)
    {
        LOG_DBG("hndl is NULL");
        return EINVAL;
    }

    qsh_glink_client_t* client_info = (qsh_glink_client_t*)hndl;

    k_mutex_lock(&(qsh_glink_cxt.mtx), K_FOREVER);

    for(iter = 0; iter < qsh_glink_cxt.clients_count; iter++)
    {
        if(qsh_glink_cxt.clients[iter] == client_info)
        {
            break;
        }
    }

    if(iter == qsh_glink_cxt.clients_count)
    {
        LOG_DBG("hndl not found in client list");
        k_mutex_unlock(&(qsh_glink_cxt.mtx));
        return EPERM;
    }

    k_mutex_unlock(&(qsh_glink_cxt.mtx));

    qsh_glink_msg_t *msg = (qsh_glink_msg_t*)
                        qsh_glink_work_init(sizeof(qsh_glink_msg_t));

    msg->evt = QSH_GLINK_EVT_ID_SEND_DATA;
    msg->data = sns_client_data;
    msg->size = size;
    msg->client_hndl = (qsh_glink_client_t*) hndl;

    api_retval = qsh_glink_work_submit(msg);

    return 0;
}

static void *qsh_glink_work_init(size_t args_size)
{
    void* retval = pdtsw_allocate_handler_args(args_size, NULL);

    if(NULL == retval)
    {
        PDTSW_FATAL();
    }

    return retval;
}

static int qsh_glink_work_submit(void* args)
{
    int retval = pdtsw_submit_to_workq(qsh_glink_evt_hndlr,
            args, NULL);

    if(0 != retval)
    {
        PDTSW_FATAL();
    }

    return retval;
}

static void qsh_glink_clean_up_connections()
{
    LOG_DBG("qsh_glink_clean_up_connections");
    int iter;
    qapi_qsh_glink_conn_evt_t evt;
    evt.evt_type = QSH_GLINK_EVT_CONN_HNDL_ERR;
    evt.conn_err = EHOSTUNREACH;

    k_mutex_lock(&(qsh_glink_cxt.mtx), K_FOREVER);

    for(iter = 0; iter < CONFIG_PDTSW_QSH_GLINK_ROUTER_MAX_STATUS_CLIENTS;
        iter++)
    {
        if(qsh_glink_cxt.status_clients[iter].cb != NULL)
        {
            qsh_glink_cxt.status_clients[iter].cb(QSH_GLINK_STATUS_DOWN, 
                qsh_glink_cxt.status_clients[iter].priv);
        }
    }

    k_mutex_unlock(&(qsh_glink_cxt.mtx));

    for(iter = 0; iter != qsh_glink_cxt.clients_count; 
            iter++)
    {
        qsh_glink_cxt.clients[iter]->cb(&evt,
                qsh_glink_cxt.clients[iter]->priv);
    }

    memset(&(qsh_glink_cxt.clients), 0, sizeof(qsh_glink_cxt.clients));
    qsh_glink_cxt.clients_count = 0;
}

volatile int qsh_send_hndlr_debug = 0;
static void qsh_glink_send_data_handler(qsh_glink_client_t* client_info,
                        void* data, size_t size)
{
    glink_err_type glink_err;
    uint8_t* buffer = NULL;
    size_t encoded_size;
    LOG_DBG("qsh_glink_send_data_handler. client=%p, data=%p, size=%d",
        client_info, data, size);
    
    if(GLINK_CONNECTED != qsh_glink_cxt.channel_state)
    {
        LOG_DBG("Channel not up. State=%d", qsh_glink_cxt.channel_state);
        qapi_qsh_glink_conn_evt_t resp;
        resp.evt_type = QSH_GLINK_EVT_SEND_RESP;
        resp.conn_resp = EHOSTUNREACH;
        client_info->cb(&resp, client_info->priv);
        return;
    }

    qsh_send_hndlr_debug++;

    sns_client_glink_req req_msg = 
                sns_client_glink_req_init_default;
    sns_client_glink_msg msg = sns_client_glink_msg_init_default;
    msg.which_msg = sns_client_glink_msg_req_tag;
    msg.msg.req = req_msg;
    msg.msg.req.has_connection_handle = true;
    msg.msg.req.connection_handle = client_info->conn_hndl;
    msg.msg.req.has_request = true;
    msg.msg.req.request = *((sns_client_request_msg*)data);

    qsh_send_hndlr_debug++;

    if(pb_get_encoded_size(&encoded_size, sns_client_glink_msg_fields, &msg))
    {
        qsh_send_hndlr_debug++;
        buffer = (uint8_t*)pdtsw_heap_alloc(
            PDTSW_COMMON_GLINK_HEAP, encoded_size);
    }
    else
    {
        LOG_DBG("Getting encode size failed");
        PDTSW_FATAL();
    }

    LOG_DBG("encoded size = %d", encoded_size);
        
    if(NULL == buffer)
    {
        LOG_DBG("Alloc failed");
        PDTSW_FATAL();
    }

    qsh_send_hndlr_debug++;

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, encoded_size);

    if(!pb_encode(&stream, sns_client_glink_msg_fields, &msg))
    {
        LOG_DBG("Encode failed");
        PDTSW_FATAL();
    }

    qsh_send_hndlr_debug++;

    glink_err = pdtsw_glink_tx(qsh_glink_cxt.qsh_channel_hndl,
                NULL, buffer, stream.bytes_written,
                GLINK_TX_REQ_INTENT);

    if(GLINK_STATUS_SUCCESS != glink_err)
    {
        LOG_DBG("glink tx failed: %d", glink_err);
        pdtsw_heap_free(PDTSW_COMMON_GLINK_HEAP, buffer);
        qapi_qsh_glink_conn_evt_t resp;
        resp.evt_type = QSH_GLINK_EVT_SEND_RESP;
        resp.conn_resp = EHOSTUNREACH;
        client_info->cb(&resp, client_info->priv);
    }

    return;
}

static void qsh_glink_conn_alloc_handler(qsh_glink_client_node_t* client_node)
{
    glink_err_type glink_err;
    uint8_t* buffer = NULL;
    size_t encoded_size;
    LOG_DBG("qsh_glink_conn_alloc_handler. client=%p", client_node);
    
    if(GLINK_CONNECTED != qsh_glink_cxt.channel_state)
    {
        LOG_DBG("Channel not up. State=%d", qsh_glink_cxt.channel_state);
        goto CLEAN_UP;
    }

    sns_client_glink_connect connect_msg = 
                sns_client_glink_connect_init_default;
    sns_client_glink_msg msg = sns_client_glink_msg_init_default;
    msg.which_msg = sns_client_glink_msg_connect_tag;
    msg.msg.connect = connect_msg;

    if(pb_get_encoded_size(&encoded_size, sns_client_glink_msg_fields, &msg))
    {
        buffer = (uint8_t*)pdtsw_heap_alloc(
            PDTSW_COMMON_GLINK_HEAP, encoded_size);
    }
    else
    {
        LOG_DBG("Getting encode size failed");
        goto CLEAN_UP;
    }

    LOG_DBG("encoded size = %d", encoded_size);

    if(NULL == buffer)
    {
        LOG_DBG("Allocation failed");
        goto CLEAN_UP;
    }

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, encoded_size);

    if(!pb_encode(&stream, sns_client_glink_msg_fields, &msg))
    {
        LOG_DBG("Encode failed");
        pdtsw_heap_free(PDTSW_COMMON_GLINK_HEAP, buffer);
        goto CLEAN_UP;
    }

    LOG_DBG("pb bytes written = %d", stream.bytes_written);

    glink_err = pdtsw_glink_tx(qsh_glink_cxt.qsh_channel_hndl,
                NULL, buffer, stream.bytes_written,
                GLINK_TX_REQ_INTENT);

    if(GLINK_STATUS_SUCCESS != glink_err)
    {
        LOG_DBG("glink tx failed: %d", glink_err);
        pdtsw_heap_free(PDTSW_COMMON_GLINK_HEAP, buffer);
        goto CLEAN_UP;
    }

    return;

CLEAN_UP:
    LOG_DBG("qsh_glink_conn_alloc_handler failed");
    qapi_qsh_glink_conn_evt_t resp;
    resp.evt_type = QSH_GLINK_EVT_CONN_ALLOC_RESP;
    resp.conn_resp = EHOSTUNREACH;
    client_node->client_info->cb(&resp, client_node->client_info->priv);

    k_mutex_lock(&(qsh_glink_cxt.mtx), K_FOREVER);
    if(!sys_slist_find_and_remove (&(qsh_glink_cxt.clients_waiting),
                &(client_node->node)))
    {
        LOG_DBG("Unable to find client in wait list to remove");
        PDTSW_FATAL();
    }
    --qsh_glink_cxt.clients_waiting_count;
    k_mutex_unlock(&(qsh_glink_cxt.mtx));

    pdtsw_heap_free(PDTSW_QSH_GLINK_ROUTER_HEAP, client_node->client_info);
    pdtsw_heap_free(PDTSW_QSH_GLINK_ROUTER_HEAP, client_node);
    /*Todo: free buffer*/
}

static void qsh_glink_conn_free_handler(qsh_glink_client_t* client_info)
{
    glink_err_type glink_err;
    int api_retval;
    qapi_qsh_glink_conn_evt_t resp;
    resp.evt_type = QSH_GLINK_EVT_CONN_FREE_RESP;
    resp.conn_resp = EHOSTUNREACH;
    LOG_DBG("qsh_glink_conn_free_handler. client=%p", client_info);
    
    if(GLINK_CONNECTED != qsh_glink_cxt.channel_state)
    {
        LOG_DBG("Channel not up. State=%d", qsh_glink_cxt.channel_state);
        client_info->cb(&resp, client_info->priv);
        return;
    }

    sns_client_glink_disconnect disconnect_msg = 
                sns_client_glink_disconnect_init_default;
    sns_client_glink_msg msg = sns_client_glink_msg_init_default;
    uint32_t buffer_size = 32; //TODO: Remove hardcoding
    uint8_t* buffer = (uint8_t*)pdtsw_heap_alloc(PDTSW_COMMON_GLINK_HEAP,
                        buffer_size);
    if(NULL == buffer)
    {
        LOG_DBG("Alloc failed");
        PDTSW_FATAL();
    }

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, buffer_size);

    msg.which_msg = sns_client_glink_msg_disconnect_tag;
    msg.msg.disconnect = disconnect_msg;
    msg.msg.disconnect.connection_handle = client_info->conn_hndl;

    if(!pb_encode(&stream, sns_client_glink_msg_fields, &msg))
    {
        LOG_DBG("Encode failed");
        pdtsw_heap_free(PDTSW_COMMON_GLINK_HEAP, buffer);
        PDTSW_FATAL();
    }

    glink_err = pdtsw_glink_tx(qsh_glink_cxt.qsh_channel_hndl,
                NULL, buffer, buffer_size,
                GLINK_TX_REQ_INTENT);

    if(GLINK_STATUS_SUCCESS != glink_err)
    {
        LOG_DBG("glink tx failed: %d", glink_err);
        pdtsw_heap_free(PDTSW_COMMON_GLINK_HEAP, buffer);
        client_info->cb(&resp, client_info->priv);
    }
    else
    {
        resp.conn_resp = 0;

        /* Remove from client list.*/
        api_retval = qsh_glink_remove_client(
                    msg.msg.resp.connection_handle);

        if(0 != api_retval)
        {
            LOG_DBG("Failed to remove client from wait list");
            PDTSW_FATAL();
        }
        client_info->cb(&resp, client_info->priv);
        pdtsw_heap_free(PDTSW_QSH_GLINK_ROUTER_HEAP, client_info);
    }

    return;
}

static int qsh_glink_compare_client(const void *a, const void *b)
{
    qsh_glink_client_t *client_a = *(qsh_glink_client_t **)a;
    qsh_glink_client_t *client_b = *(qsh_glink_client_t **)b;

    if (client_a == NULL) return 1;
    if (client_b == NULL) return -1;

    return (client_a->conn_hndl - client_b->conn_hndl);
}

static void qsh_glink_insert_client(qsh_glink_client_t* new_client)
{
    if (qsh_glink_cxt.clients_count >= CONFIG_PDTSW_QSH_GLINK_ROUTER_MAX_CLIENTS)
    {
        PDTSW_FATAL();
    }

    /* Insert the new client at the end of the used portion of the array.*/
    qsh_glink_cxt.clients[qsh_glink_cxt.clients_count] = new_client;
    qsh_glink_cxt.clients_count++;

    /* Sort the array to maintain order.*/
    qsort(qsh_glink_cxt.clients, qsh_glink_cxt.clients_count,
            sizeof(qsh_glink_client_t*), qsh_glink_compare_client);
}

static qsh_glink_client_t* qsh_glink_find_client(uint32_t conn_hndl)
{
    qsh_glink_client_t key = { .conn_hndl = conn_hndl };
    qsh_glink_client_t *key_ptr = &key;
    qsh_glink_client_t **result = (qsh_glink_client_t **)bsearch(&key_ptr,
        qsh_glink_cxt.clients, qsh_glink_cxt.clients_count,
        sizeof(qsh_glink_client_t*), qsh_glink_compare_client);

    if (result != NULL) {
        return *result;
    }
    return NULL;
}

static int qsh_glink_remove_client(uint32_t conn_hndl)
{
    qsh_glink_client_t *client_to_remove = qsh_glink_find_client(conn_hndl);
    if (client_to_remove == NULL) {
        return ENOENT;
    }

    /* Find the index of the client to remove.*/
    uint16_t iter1 = 0, iter2 = 0;
    while((iter1 < qsh_glink_cxt.clients_count) && 
            (qsh_glink_cxt.clients[iter1]->conn_hndl != conn_hndl))
    {
        iter1++;
    }

    /* Shift elements to fill the gap left by the removed client.*/
    for (iter2 = iter1; iter2 < qsh_glink_cxt.clients_count - 1; iter2++)
    {
        qsh_glink_cxt.clients[iter2] = qsh_glink_cxt.clients[iter2 + 1];
    }
    qsh_glink_cxt.clients[qsh_glink_cxt.clients_count - 1] = NULL;  // Set the last element to NULL
    qsh_glink_cxt.clients_count--;

    /* Sort the array to maintain order.*/
    qsort(qsh_glink_cxt.clients, qsh_glink_cxt.clients_count,
            sizeof(qsh_glink_client_t*), qsh_glink_compare_client);

    return 0;
}

static void qsh_glink_link_state_cb(glink_link_info_type *link_info,void* priv)
{
    int api_retval;
    LOG_DBG("glink_link_info_type. link_state=%d", link_info->link_state);

    qsh_glink_msg_t *msg = 
                        (qsh_glink_msg_t*)qsh_glink_work_init(
                        sizeof(qsh_glink_msg_t));

    msg->evt = QSH_GLINK_EVT_ID_GLINK_LINK_STATE;
    msg->data = (void*)(uintptr_t)(link_info->link_state);
    
    api_retval = qsh_glink_work_submit(msg);
}

static void qsh_glink_chnl_state_cb(glink_handle_type handle, const void *priv,
                        glink_channel_event_type event)
{
    int api_retval;
    LOG_DBG("qsh_glink_chnl_state_cb. event=%d", event);

    qsh_glink_msg_t *msg = 
                        (qsh_glink_msg_t*)qsh_glink_work_init(
                        sizeof(qsh_glink_msg_t));

    msg->evt = QSH_GLINK_EVT_ID_GLINK_CHANNEL_STATE;
    msg->data = (void*)(uintptr_t)event;
    
    api_retval = qsh_glink_work_submit(msg);
}

static void qsh_glink_tx_done_cb(glink_handle_type handle, const void *priv,
                        const void *pkt_priv, const void *ptr, size_t size)
{
    int api_retval;
    LOG_DBG("qsh_glink_tx_done_cb. ptr=%p", ptr);

    qsh_glink_glink_msg_t *msg = 
                        (qsh_glink_glink_msg_t*)qsh_glink_work_init(
                        sizeof(qsh_glink_glink_msg_t));

    msg->evt = QSH_GLINK_EVT_ID_GLINK_TX_DONE;
    msg->pkt_priv = pkt_priv;
    msg->data = ptr;
    msg->size = size;
    
    api_retval = qsh_glink_work_submit(msg);
}

static bool qsh_glink_rx_intent_req_cb(glink_handle_type handle,
                        const void *priv, size_t req_size)
{
    int api_retval;
    LOG_DBG("qsh_glink_rx_intent_req_cb. req_size=%d", req_size);

    qsh_glink_glink_msg_t *msg = 
                        (qsh_glink_glink_msg_t*)qsh_glink_work_init(
                        sizeof(qsh_glink_glink_msg_t));

    msg->evt = QSH_GLINK_EVT_ID_GLINK_RX_INTENT_REQ;
    msg->size = req_size;
    
    api_retval = qsh_glink_work_submit(msg);

    return true;
}

static void qsh_glink_rx_cb(glink_handle_type handle, const void *priv,
                        const void *pkt_priv, const void *ptr,
                        size_t size, size_t intent_used)
{
    int api_retval;
    LOG_DBG("qsh_glink_rx_cb. ptr=%p, size=%d", ptr, size);

    qsh_glink_glink_msg_t *msg = 
                        (qsh_glink_glink_msg_t*)qsh_glink_work_init(
                        sizeof(qsh_glink_glink_msg_t));

    msg->evt = QSH_GLINK_EVT_ID_GLINK_RX;
    msg->pkt_priv = pkt_priv;
    msg->data = ptr;
    msg->size = size;
    msg->intent_used = intent_used;
    
    api_retval = qsh_glink_work_submit(msg);
}

static glink_err_type qsh_glink_allocate_cb(glink_handle_type handle,
                const void *priv, const void *pkt_priv,
                size_t intent_size, void **buffer_ptr)
{
    /* Todo: do we need to assign the parameters again?*/
    LOG_DBG("qsh_glink_allocate_cb size=%d", intent_size);

    *buffer_ptr = pdtsw_heap_alloc(PDTSW_COMMON_GLINK_HEAP, intent_size);
    if(NULL == *buffer_ptr)
    {
        PDTSW_FATAL();
    }
    return GLINK_STATUS_SUCCESS;
}

static void qsh_glink_deallocate_cb(glink_handle_type handle, const void *priv,
                const void *pkt_priv, size_t intent_size, void *buffer_ptr)
{
    pdtsw_heap_free(PDTSW_COMMON_GLINK_HEAP, buffer_ptr);
}

static void qsh_glink_link_state_handler(glink_link_state_type link_state)
{
    glink_err_type glink_err;
    glink_open_config_type open_cfg;
    LOG_DBG("qsh_glink_link_state_handler. link_state=%d", link_state);

    if(link_state != qsh_glink_cxt.link_state)
    {
        if(GLINK_LINK_STATE_UP == link_state)
        {
            GLINK_OPEN_CONFIG_INIT(open_cfg);
            /* Establish channel with battery firmware on Q6.*/
            open_cfg.remote_ss = QSH_GLINK_ROUTER_REMOTESS_NAME;
            open_cfg.transport = QSH_GLINK_ROUTER_XPORT_NAME;
            open_cfg.name = QSH_GLINK_ROUTER_CHANNEL_NAME;
            open_cfg.notify_rx = qsh_glink_rx_cb;
            open_cfg.notify_rx_intent_req = qsh_glink_rx_intent_req_cb;
            open_cfg.notify_state = qsh_glink_chnl_state_cb;
            open_cfg.notify_tx_done = qsh_glink_tx_done_cb;
            open_cfg.notify_allocate = qsh_glink_allocate_cb;
            open_cfg.notify_deallocate = qsh_glink_deallocate_cb;
            open_cfg.options = GLINK_OPT_CLIENT_BUFFER_ALLOCATION;
            glink_err = glink_open(&open_cfg, &(qsh_glink_cxt.qsh_channel_hndl));

            if(GLINK_STATUS_SUCCESS != glink_err)
            {
                LOG_DBG("Channel open failed: %d", glink_err);
                /* Todo: Log and raise fatal.*/
                PDTSW_FATAL();
            }

            LOG_DBG("Channel opened");
        }
        else
        {
            /* Close the channel if already opened.*/
            if(GLINK_CONNECTED == qsh_glink_cxt.channel_state)
            {
                glink_err = glink_close(qsh_glink_cxt.qsh_channel_hndl);
            
                if(GLINK_STATUS_SUCCESS != glink_err)
                {
                    /* Todo: Log and ignore expecting the close event might be already
                        ongoing.*/
                        LOG_DBG("Channel close failed. err=%d", glink_err);
                }

                LOG_DBG("Channel closed");

                qsh_glink_clean_up_connections();

            }
        }

        qsh_glink_cxt.link_state = link_state;
    }
}

static void qsh_glink_chnl_state_handler(glink_channel_event_type event)
{
    int iter;
    glink_err_type glink_err;
    LOG_DBG("qsh_glink_chnl_state_handler. evt=%d", event);

    if(event != qsh_glink_cxt.channel_state)
    {
        if(GLINK_CONNECTED == event)
        {
            /* Todo [optional]: Queue an rx intent. Max intent size.*/
            // glink_err = glink_queue_rx_intent(
            //     qsh_glink_cxt.qsh_channel_hndl, NULL, 100);

            // if(GLINK_STATUS_SUCCESS != glink_err)
            // {
            //     /* Todo: Log and raise fatal.*/
            //     LOG_DBG("Intent queuing failed. err=%d", glink_err);
            //     PDTSW_FATAL();
            // }
            // LOG_DBG("Intent queued");

            k_mutex_lock(&(qsh_glink_cxt.mtx), K_FOREVER);

            for(iter = 0; iter < CONFIG_PDTSW_QSH_GLINK_ROUTER_MAX_STATUS_CLIENTS;
                iter++)
            {
                if(qsh_glink_cxt.status_clients[iter].cb != NULL)
                {
                    qsh_glink_cxt.status_clients[iter].cb(QSH_GLINK_STATUS_UP, 
                        qsh_glink_cxt.status_clients[iter].priv);
                }
            }

            k_mutex_unlock(&(qsh_glink_cxt.mtx));

        }
        else if(GLINK_REMOTE_DISCONNECTED == event)
        {
            /* Close the channel if already opened.*/
            if(GLINK_CONNECTED == qsh_glink_cxt.channel_state)
            {
                glink_err = glink_close(qsh_glink_cxt.qsh_channel_hndl);
            
                if(GLINK_STATUS_SUCCESS != glink_err)
                {
                    /* Todo: Log and ignore expecting the close event might be already
                        ongoing.*/
                    LOG_DBG("Channel close failed: %d", glink_err);
                }
            }
            LOG_DBG("Channel closed");

            qsh_glink_clean_up_connections();
        }
        else
        {
            /* We are here since we have explicitly requested to close channel.*/
        }

        qsh_glink_cxt.channel_state = event;
    }
}

static void qsh_glink_tx_done_handler(glink_handle_type handle, const void *priv,
    const void *pkt_priv, const void *ptr, size_t size)
{
    LOG_DBG("qsh_glink_tx_done_handler. ptr=%p", ptr);

    pdtsw_heap_free(PDTSW_COMMON_GLINK_HEAP, (void*)ptr);
}

static void qsh_glink_rx_intent_req_handler(glink_handle_type handle, const void *priv,
    size_t req_size)
{
    LOG_DBG("qsh_glink_rx_intent_req_handler. req_size=%d", req_size);

    /* Queue an rx intent.*/
    glink_err_type glink_err = glink_queue_rx_intent
                (qsh_glink_cxt.qsh_channel_hndl, NULL, req_size);

    if(GLINK_STATUS_SUCCESS != glink_err)
    {
        /* Log and raise fatal since we agreed to queue an intent in callback.*/
        PDTSW_FATAL();
    }
}

static void qsh_glink_rx_handler(glink_handle_type handle, const void *priv,
    const void *pkt_priv, const void *ptr,
    size_t size, size_t intent_used)
{
    bool eof = false;
    uint32_t tag;
    pb_wire_type_t wire_type;

    LOG_DBG("ptr=%p, size=%d", ptr, size);

    sns_client_glink_msg msg = sns_client_glink_msg_init_default;
    pb_istream_t stream = pb_istream_from_buffer(ptr, size);
    pb_istream_t stream_ind = pb_istream_from_buffer(ptr, size);

    if (pb_decode(&stream, sns_client_glink_msg_fields, &msg))
    {
        LOG_DBG("msg.which_msg=%d", msg.which_msg);
        switch(msg.which_msg)
        {
            case sns_client_glink_msg_connect_ack_tag:
            {
                /* Pick a client from waiting list.*/
                k_mutex_lock(&(qsh_glink_cxt.mtx), K_FOREVER);
                sys_snode_t *first_client_node = sys_slist_get(&
                        (qsh_glink_cxt.clients_waiting));

                if((NULL == first_client_node) && (0 != qsh_glink_cxt.clients_waiting_count))
                {
                    LOG_ERR("No client in wait list, client cnt: %d", 
                        qsh_glink_cxt.clients_waiting_count);
                    break;
                }
                --qsh_glink_cxt.clients_waiting_count;

                LOG_DBG("client in wait list: %p", first_client_node);

                qsh_glink_client_node_t *client_node = CONTAINER_OF(
                        first_client_node, qsh_glink_client_node_t, node);

                client_node->client_info->conn_hndl = 
                                msg.msg.connect_ack.connection_handle;

                k_mutex_unlock(&(qsh_glink_cxt.mtx));

                if((true == msg.msg.connect_ack.has_connection_handle) && 
                        (0 != msg.msg.connect_ack.connection_handle))
                {
                    LOG_DBG("Add client to clients list");
                    /* Add to client list.*/
                    qsh_glink_insert_client(client_node->client_info);

                    /* Inform success*/
                    qapi_qsh_glink_conn_evt_t resp;
                    resp.evt_type = QSH_GLINK_EVT_CONN_ALLOC_RESP;
                    resp.conn_resp = 0;
                    client_node->client_info->cb(&resp, 
                                client_node->client_info->priv);
                }
                else
                {
                    LOG_DBG("Inform failure and discard client");
                    /* Todo: send failure. Should be made to a function so that can be called from both alloc handler and response handler*/
                    qapi_qsh_glink_conn_evt_t resp;
                    resp.evt_type = ECONNREFUSED;
                    resp.conn_resp = 0;
                    client_node->client_info->cb(&resp, client_node->client_info->priv);

                    pdtsw_heap_free(PDTSW_QSH_GLINK_ROUTER_HEAP, client_node->client_info);
                }

                /* Todo: Free the node created for waiting list.*/
                pdtsw_heap_free(PDTSW_QSH_GLINK_ROUTER_HEAP, client_node);

                break;
            }
            case sns_client_glink_msg_resp_tag:
            {
                /* Fetch client info from by handle.*/
                k_mutex_lock(&(qsh_glink_cxt.mtx), K_FOREVER);
                
                qsh_glink_client_t *client_info = qsh_glink_find_client(
                            msg.msg.resp.connection_handle);

                k_mutex_unlock(&(qsh_glink_cxt.mtx));

                if(NULL == client_info)
                {
                    LOG_DBG("Unable to find client %p", client_info);
                    /* Nothing to do. drop this mesage.*/
                    break;
                }

                qapi_qsh_glink_conn_evt_t resp;
                resp.evt_type = QSH_GLINK_EVT_SEND_RESP;
                resp.conn_resp = 0;

                if(msg.msg.resp.has_error)
                {
                    resp.conn_resp = msg.msg.resp.error;
                }

                client_info->cb(&resp, client_info->priv);

                break;
            }
            case sns_client_glink_msg_ind_tag:
            {
                qapi_qsh_glink_conn_evt_t resp;
                resp.evt_type = QSH_GLINK_EVT_DATA;

                /* Fetch client info from by handle.*/
                k_mutex_lock(&(qsh_glink_cxt.mtx), K_FOREVER);
                
                qsh_glink_client_t *client_info = qsh_glink_find_client(
                            msg.msg.ind.connection_handle);

                k_mutex_unlock(&(qsh_glink_cxt.mtx));

                if(NULL == client_info)
                {
                    LOG_DBG("Unable to find client %p", client_info);
                    /* Nothing to do. drop this mesage.*/
                    break;
                }

                /*Todo: Check for optimizing to no decode by reading each tag*/
                if(pb_decode_tag(&stream_ind, &wire_type, &tag, &eof))
                {
                    if(PB_WT_STRING == wire_type && sns_client_glink_msg_ind_tag == tag)
                    {
                        pb_istream_t sub_stream;
                        if(pb_make_string_substream(&stream_ind, &sub_stream))
                        {
                            if(pb_decode_tag(&sub_stream, &wire_type, &tag, &eof))
                            {
                                if(PB_WT_32BIT == wire_type && sns_client_glink_ind_connection_handle_tag == tag)
                                {
                                    pb_skip_field(&sub_stream, wire_type);
                                    if(pb_decode_tag(&sub_stream, &wire_type, &tag, &eof))
                                    {
                                        if(PB_WT_STRING == wire_type && sns_client_glink_ind_event_tag == tag)
                                        {
                                            pb_istream_t ind_stream;
                                            if(pb_make_string_substream(&sub_stream, &ind_stream))
                                            {
                                                resp.data = &ind_stream;
                                                resp.data_sz = ind_stream.bytes_left;
                                                client_info->cb(&resp, client_info->priv);
                                                pb_close_string_substream(&sub_stream, &ind_stream);
                                            }
                                        }
                                    }
                                }
                            }
                            pb_close_string_substream(&stream_ind, &sub_stream);
                        }
                    } else {
                        LOG_DBG("Rx msg is not an indication msg\n");
                    }
                } else {
                    LOG_DBG("Failed to decode sns_client_glink_msg\n");
                }

                break;
            }
            default:
            {
                /* Nothing to do.*/
                break;
            }
        }
    }
    else
    {
        /* Todo: increase the decode error count.*/
        LOG_DBG("Decode error");
    }

    /* search from look-up and for the client info from handle and notify client.*/

    /* Rx done.*/
    glink_err_type glink_err = glink_rx_done
                (qsh_glink_cxt.qsh_channel_hndl, ptr, false);

    if(GLINK_STATUS_SUCCESS != glink_err)
    {
        /* Todo: Log and raise fatal since we agreed to queue an intent in callback.*/
        PDTSW_FATAL();
    }
}

SYS_INIT(qsh_glink_router_init, POST_KERNEL, 95);