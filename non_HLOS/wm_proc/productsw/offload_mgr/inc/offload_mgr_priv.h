/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
// $QTI_LICENSE_C$

#ifndef __PDTSW_OFFLOAD_MGR_PRIV_H__
#define __PDTSW_OFFLOAD_MGR_PRIV_H__

#include <stdint.h>
#include <stdlib.h>
#include "glink.h"
#include "offload_mgr.h"

/** offload manager internal events.*/
typedef enum
{
    OFFLOAD_MGR_EVT_GLINK_APP_LINK = 0,
    OFFLOAD_MGR_EVT_GLINK_CHNL,
    OFFLOAD_MGR_EVT_GLINK_RX,
    OFFLOAD_MGR_EVT_GLINK_RX_INT_REQ,
    OFFLOAD_MGR_EVT_GLINK_TX_DONE,

    OFFLOAD_MGR_EVT_CLIENT_CUSTOM,

    OFFLOAD_MGR_EVT_MAX
}offload_mgr_evt_t;

/** Offload manager message header */
typedef struct 
{
    offload_mgr_evt_t evt;
}offload_mgr_msg_hdr_t;

/** Offload manager context struct */
typedef struct 
{
    glink_link_handle_type apps_glink_link_hndl;
    glink_link_state_type apps_glink_link_state;
    offload_mgr_srv_client_intf_t *client_info[OFF_MGR_SRV_MAX];
}offload_mgr_cxt_t;

#endif /* __PDTSW_OFFLOAD_MGR_PRIV_H__ */