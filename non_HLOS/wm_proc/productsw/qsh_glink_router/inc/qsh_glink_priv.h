/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
#ifndef __QSH_GLINK_PRIV_H__
#define __QSH_GLINK_PRIV_H__

#include <zephyr/kernel.h>
#include <zephyr/sys/slist.h>
#include "qapi_qsh_glink.h"
#include "glink.h"

#define QSH_GLINK_ROUTER_CHANNEL_NAME           "qsh_am"

#define QSH_GLINK_ROUTER_REMOTESS_NAME          "swm"

#define QSH_GLINK_ROUTER_XPORT_NAME             "SMEM"

/** QSH glink router internal events.*/
typedef enum qsh_glink_evt_id
{
    QSH_GLINK_EVT_ID_CONN_ALLOC_REQ,
    QSH_GLINK_EVT_ID_CONN_FREE_REQ,
    QSH_GLINK_EVT_ID_SEND_DATA,

    /* Transport events.*/
    QSH_GLINK_EVT_ID_GLINK_LINK_STATE,
    QSH_GLINK_EVT_ID_GLINK_CHANNEL_STATE,
    QSH_GLINK_EVT_ID_GLINK_RX,
    QSH_GLINK_EVT_ID_GLINK_RX_INTENT_REQ,
    QSH_GLINK_EVT_ID_GLINK_TX_DONE,

}qsh_glink_evt_id_t;

typedef enum qsh_glink_client_msg
{
    QSH_GLINK_CLIENT_MSG_NONE,
    QSH_GLINK_CLIENT_MSG_REQ_RESP,
    QSH_GLINK_CLIENT_MSG_DISCONNECT_RESP
}qsh_glink_client_msg_t;

/** Struct to hold the active connection information.*/
typedef struct qsh_glink_client
{
    uint32_t conn_hndl;
    qsh_glink_conn_cb_t cb;
    qsh_glink_client_msg_t waiting_for;
    const void *priv;
}qsh_glink_client_t;

/** Waiting client structure to store to list.*/
typedef struct qsh_glink_client_node
{
    sys_snode_t node;
    qsh_glink_client_t *client_info;
}qsh_glink_client_node_t;

/** Struct to represent internal messages.*/
typedef struct qsh_glink_msg
{
    qsh_glink_evt_id_t evt;
    qsh_glink_client_t* client_hndl;
    size_t size;
    void* data;
}qsh_glink_msg_t;

typedef struct
{
    qsh_glink_evt_id_t evt;
    size_t size;
    size_t intent_used;
    const void* pkt_priv;
    const void* data;
}qsh_glink_glink_msg_t;

typedef struct
{
    qsh_glink_status_cb_t cb;
    const void* priv;
}qsh_glink_status_clients_t;

/** Struct to represent the qsh glink router context.*/
typedef struct qsh_glink_cxt
{
    glink_link_handle_type qsh_link_hndl;
    glink_link_state_type link_state;
    glink_handle_type qsh_channel_hndl;
    glink_channel_event_type channel_state;
    struct k_mutex mtx;
    qsh_glink_msg_t last_msg;
    uint32_t msg_count;
    uint32_t decode_err_count;
    qsh_glink_status_clients_t status_clients[
        CONFIG_PDTSW_QSH_GLINK_ROUTER_MAX_STATUS_CLIENTS];
    qsh_glink_client_t *clients[
        CONFIG_PDTSW_QSH_GLINK_ROUTER_MAX_CLIENTS];
    uint16_t status_clients_count;
    uint16_t clients_count;
    sys_slist_t clients_waiting;
    uint16_t clients_waiting_count;
}qsh_glink_cxt_t;

#endif /**< __QSH_GLINK_PRIV_H__ */