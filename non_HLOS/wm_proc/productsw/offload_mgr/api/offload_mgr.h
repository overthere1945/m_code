/*==============================================================================
                 Copyright (c) 2025 Qualcomm Technologies, Inc.
                                All Rights Reserved.
                 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
// $QTI_LICENSE_C$

#ifndef __PDTSW_OFFLOAD_MGR_H__
#define __PDTSW_OFFLOAD_MGR_H__

#include <stdint.h>
#include <stdlib.h>
#include "pdtsw_glink.h"
#include "errno.h"

#define OFFLOAD_MGR_APPS_REMOTE_SS      "apss"
#define OFFLOAD_MGR_APPS_XPORT          "SMEM"

/** Supported services for offloads.*/
typedef enum
{
#ifdef CONFIG_PDTSW_OFFLOAD_MGR_RSB_ENABLE
    OFF_MGR_SRV_RSB_SERVICE,
#endif
#if defined(CONFIG_PDTSW_OFFLOAD_MGR_AUDIO_ENABLE) || \
defined(CONFIG_PDTSW_OFFLOAD_MGR_HAPTICS_ENABLE) || defined(CONFIG_PDTSW_COMMON_UTILS_ENABLE)
    OFF_MGR_SRV_UTILITY_HAL,
#endif
    OFF_MGR_SRV_MAX
}offload_mgr_srv_client_t;

/** Enum to identify current status of the service.*/
typedef enum
{
    OFF_MGR_SRV_STATUS_DEINIT = 0,
    OFF_MGR_SRV_STATUS_INIT,
    OFF_MGR_SRV_STATUS_STARTED,
    OFF_MGR_SRV_STATUS_STOPPED
}offload_mgr_client_status_t;

/** struct to hold client handlers for offload events.*/
typedef struct
{
    char* channel_name;
    glink_channel_event_type chnl_state;
    bool is_dynamic_intent_supported;
    glink_handle_type chnl_hndl;
    offload_mgr_client_status_t client_status;
    void (*on_offload_mgr_start)(void);
    void (*on_link_evt)(glink_link_state_type status);
    void (*on_channel_evt)(glink_channel_event_type evt);
    void (*on_rx_evt)(const void* pkt_priv, const void* data, size_t size);
    void (*on_tx_done_evt)(const void* pkt_priv, const void* data, size_t size);
    void (*on_intent_req)(size_t req_size);
    glink_err_type (*on_allocate_cb)(const void *pkt_priv, size_t intent_size,
                                                void **buffer_ptr);
    void (*on_deallocate_cb)(const void *pkt_priv, size_t intent_size,
                                                void *buffer_ptr);
    void (*event_hndlr)(void *arg, size_t size);
}offload_mgr_srv_client_intf_t;

/** Offload mager handler */

/**
 * Open Glink channel.
 * @param[in] client Client that needs to open channel for.
 * @return standard glink errors
 */
glink_err_type offload_mgr_glink_open(offload_mgr_srv_client_t client);

/**
 * Close Glink channel.
 * @param[in] client Client that needs to close channel for.
 * @return standard glink errors
 */
glink_err_type offload_mgr_glink_close(offload_mgr_srv_client_t client);

/**
 * Send message to over glink. Lifetime of buffer should me maintained by client.
 * @param[in] client Client that is requesting for message to be sent.
 * @param[in] pPkt buffer to data that should be sent over glink.
 * @param[in] pkt_priv data to identify the packet being sent.
 * @param[in] size of the data to send.
 * @param[in] options Glink Tx options
 * @return standard glink errors
 */
glink_err_type offload_mgr_glink_send(offload_mgr_srv_client_t client, const void *pPkt,
        const void *pkt_priv, size_t size, uint32_t options);

/**
 * Queue an intent
 * @param[in] client client for which intent should be queued.
 * @param[in] pkt_priv any data associated with this intent.
 * @param[in] size size of the intent.
 * @return standard glink errors
 */
glink_err_type offload_mgr_glink_queue_intent(offload_mgr_srv_client_t client,
        void* pkt_priv, size_t size);
        
/**
 * Send Rx done event
 * @param[in] client Client that Rx done needs to be sent on.
 * @param[in] buffer Buffer on which rx done should happen.
 * @param[in] reuse reuse the same intent.
 * @return standard glink errors
 */
glink_err_type offload_mgr_glink_rx_done(offload_mgr_srv_client_t client,
        void* buffer, bool reuse);

/**
 * Init item to be queued to offload manager context
 * @param[in] arg_size Size of arguments to be passed to handler function.
 * @return Pointer to allocated memory. Null on failure
 * @note The args will be copied based on the arg_size, and these will be freed by the
 *  offload framework after client processed the work. so no need to maintain
 *  agrs memory explicitly if arg_size is correctly specified.
 */
void* offload_mgr_client_work_init(size_t arg_size);

/**
 * Queue item to offload manager context.
 * @param[in] client client that the work belongs to.
 * @param[in] args Arguments to be passed to handler function.
 * @return 0 will be returned if submission is success.
 * @note The args should be initialized with offload_mgr_client_work_init before calling
 * submit even when the size of arguments is zero or no arguments.
 */
int offload_mgr_client_work_submit(offload_mgr_srv_client_t client, void *args);

#endif /* __PDTSW_OFFLOAD_MGR_H__ */
